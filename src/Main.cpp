// Handles SDL loop, rendering passes, and inputs.
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <vector>
#include <chrono>
#include <random>
#include <fstream>
#include <string>
#include <filesystem>
#include "Constants.h"
#include "MapGeneration.h"
#include "Navigation.h"
#include "EntityManager.h"

// Global State Implementations
Point playerPos = { 10, 10 }; 
Point targetPos = { 10, 10 };
std::vector<Point> currentPath;
bool isMoving = false;
float moveProgress = 0.0f;
size_t currentPathIndex = 0;
Point realPlayerPos = { 10 * CELL_SIZE + CELL_SIZE / 2, 10 * CELL_SIZE + CELL_SIZE / 2 };

bool settlementFound = false;
Point settlementPos = { -1, -1 };
std::map<std::string, Point> agentHouses;

const char* SETTLEMENT_FILENAME = "Settlement.dat";
const char* MAP_FILENAME = "GameMap.map";

unsigned currentSeed = 0;
char seedString[64] = "";

// SDL App State
struct AppState {
    SDL_Window* window;
    SDL_Renderer* renderer;
};

// Help to draw filled circle
void DrawFilledCircle(SDL_Renderer* renderer, float x, float y, float r) {
    for (int w = 0; w < r * 2; w++) {
        for (int h = 0; h < r * 2; h++) {
            int dx = (int)r - w; // horizontal offset
            int dy = (int)r - h; // vertical offset
            if ((dx*dx + dy*dy) <= (r * r)) {
                SDL_RenderPoint(renderer, x + dx, y + dy);
            }
        }
    }
}

// Help to draw a hollow hexagon
void DrawHexagon(SDL_Renderer* renderer, float x, float y, float r) {
    float prevX = x + r;
    float prevY = y;
    for (int i = 1; i <= 6; i++) {
        float angle = i * (3.14159265f / 3.0f);
        float nextX = x + r * std::cos(angle);
        float nextY = y + r * std::sin(angle);
        SDL_RenderLine(renderer, prevX, prevY, nextX, nextY);
        prevX = nextX;
        prevY = nextY;
    }
}

// Save settlement to file
void SaveSettlement() {
    std::string path = std::filesystem::exists("bin") ? "bin\\Settlement.dat" : "Settlement.dat";
    std::ofstream outFile(path, std::ios::binary);
    if (outFile.is_open()) {
        outFile.write(reinterpret_cast<const char*>(&settlementPos), sizeof(Point));
        size_t numHouses = agentHouses.size();
        outFile.write(reinterpret_cast<const char*>(&numHouses), sizeof(size_t));
        for (const auto& pair : agentHouses) {
            size_t nameLen = pair.first.size();
            outFile.write(reinterpret_cast<const char*>(&nameLen), sizeof(size_t));
            outFile.write(pair.first.c_str(), nameLen);
            outFile.write(reinterpret_cast<const char*>(&pair.second), sizeof(Point));
        }
        outFile.close();
        SDL_Log("Settlement saved to %s", path.c_str());
    }
}

// Load settlement from file
bool LoadSettlement() {
    std::string path = std::filesystem::exists("bin") ? "bin\\Settlement.dat" : "Settlement.dat";
    std::ifstream inFile(path, std::ios::binary);
    if (inFile.is_open()) {
        inFile.read(reinterpret_cast<char*>(&settlementPos), sizeof(Point));
        int readBytes = inFile.gcount();
        if (readBytes == sizeof(Point)) {
            settlementFound = true;
            SDL_Log("Settlement loaded from %s: %d, %d", path.c_str(), settlementPos.x, settlementPos.y);
            
            size_t numHouses = 0;
            if (inFile.read(reinterpret_cast<char*>(&numHouses), sizeof(size_t))) {
                agentHouses.clear();
                for (size_t i = 0; i < numHouses; ++i) {
                    size_t nameLen = 0;
                    inFile.read(reinterpret_cast<char*>(&nameLen), sizeof(size_t));
                    std::string name(nameLen, '\0');
                    inFile.read(&name[0], nameLen);
                    Point housePos;
                    inFile.read(reinterpret_cast<char*>(&housePos), sizeof(Point));
                    agentHouses[name] = housePos;
                }
            }
            
            inFile.close();
            return true;
        }
        inFile.close();
        SDL_Log("Settlement file %s was empty or corrupted.", path.c_str());
    }
    return false;
}

// Initialize Game
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    AppState* as = new AppState();
    if (!SDL_CreateWindowAndRenderer("PathfindingGame (SDL3)", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &as->window, &as->renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    *appstate = as;

    std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    unsigned seed = (GIVEN_SEED != 0) ? GIVEN_SEED : rng();
    currentSeed = seed;
    SDL_snprintf(seedString, sizeof(seedString), "Seed: %u", currentSeed);

    if (!LoadMap()) {
        SDL_Log("No save found. Generating new map...");
        GenerateMap(seed);
        SaveMap();
    }

    LoadSettlement();

    playerPos = { 10, 10 };
    if (!isPassable(10, 10)) {
        grid[10][10].type = GRASS;
        grid[10][10].altitude = 0.5f;
        grid[10][10].r = 34; grid[10][10].g = 139; grid[10][10].b = 34;
    }
    targetPos = playerPos;

    EntityManager::Initialize();

    return SDL_APP_CONTINUE;
}

// Handle User Input Events
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event->button.button == SDL_BUTTON_LEFT) {
            float px = event->button.x;
            float py = event->button.y;
            if (px < MAP_SIZE && py < MAP_SIZE) {
                int targetX = (int)(px / CELL_SIZE);
                int targetY = (int)(py / CELL_SIZE);
                if (targetX >= 0 && targetX < GRID_SIZE && targetY >= 0 && targetY < GRID_SIZE && isPassable(targetX, targetY)) {
                    std::vector<Point> newPath = findPath(playerPos, { targetX, targetY });
                    if (newPath.size() > 1) {
                        targetPos = { targetX, targetY };
                        currentPath = newPath;
                        isMoving = true;
                        currentPathIndex = 0;
                        moveProgress = 0.0f;
                    }
                }
            }
        }
    }
    return SDL_APP_CONTINUE;
}

// Update Game State
SDL_AppResult SDL_AppIterate(void* appstate) {
    AppState* as = (AppState*)appstate;

    static Uint64 lastTime = SDL_GetTicksNS();
    Uint64 currentTime = SDL_GetTicksNS();
    float deltaTime = (currentTime - lastTime) / 1e9f;
    lastTime = currentTime;

    if (isMoving && currentPathIndex + 1 < currentPath.size()) {
        float dist = getDistance(currentPath[currentPathIndex], currentPath[currentPathIndex + 1]);
        if (dist == 0.0f) dist = 1.0f;
        moveProgress += deltaTime * 5.0f * (1.0f / getMoveCost(playerPos.x, playerPos.y)) / dist; 

        if (moveProgress >= 1.0f) {
            moveProgress = 0.0f;
            currentPathIndex++;
            playerPos = currentPath[currentPathIndex];
            if (currentPathIndex + 1 >= currentPath.size()) {
                isMoving = false;
                currentPath.clear();
            }
        }

        if (isMoving) {
            int i0 = std::max(0, (int)currentPathIndex - 1);
            int i1 = currentPathIndex;
            int i2 = std::min((int)currentPath.size() - 1, (int)currentPathIndex + 1);
            int i3 = std::min((int)currentPath.size() - 1, (int)currentPathIndex + 2);

            Point p0 = currentPath[i0];
            Point p1 = currentPath[i1];
            Point p2 = currentPath[i2];
            Point p3 = currentPath[i3];

            float rx = getSplineVal((float)p0.x, (float)p1.x, (float)p2.x, (float)p3.x, moveProgress);
            float ry = getSplineVal((float)p0.y, (float)p1.y, (float)p2.y, (float)p3.y, moveProgress);

            realPlayerPos.x = (int)(rx * CELL_SIZE + CELL_SIZE / 2);
            realPlayerPos.y = (int)(ry * CELL_SIZE + CELL_SIZE / 2);
        }
    } else {
        realPlayerPos.x = playerPos.x * CELL_SIZE + CELL_SIZE / 2;
        realPlayerPos.y = playerPos.y * CELL_SIZE + CELL_SIZE / 2;
    }

    // Update NPCs
    EntityManager::UpdateAll(deltaTime);

    // Draw Map
    SDL_SetRenderDrawColor(as->renderer, 50, 50, 50, 255);
    SDL_RenderClear(as->renderer);

    // Draw Cells
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            Cell& c = grid[y][x];
            SDL_FRect cellRect = { (float)(x * CELL_SIZE), (float)(y * CELL_SIZE), (float)CELL_SIZE, (float)CELL_SIZE };
            SDL_SetRenderDrawColor(as->renderer, c.r, c.g, c.b, 255);
            SDL_RenderFillRect(as->renderer, &cellRect);

            if (c.type == TREE) {
                SDL_SetRenderDrawColor(as->renderer, 20, 100, 20, 255);
                DrawFilledCircle(as->renderer, x * CELL_SIZE + CELL_SIZE / 2.0f, y * CELL_SIZE + CELL_SIZE / 2.0f, CELL_SIZE / 3.0f);
            }
        }
    }

    // Draw Path
    if (currentPath.size() > 1) {
        SDL_SetRenderDrawColor(as->renderer, 255, 255, 0, 255);
        for (size_t i = 0; i + 1 < currentPath.size(); i++) {
            float x1 = currentPath[i].x * CELL_SIZE + CELL_SIZE / 2.0f;
            float y1 = currentPath[i].y * CELL_SIZE + CELL_SIZE / 2.0f;
            float x2 = currentPath[i+1].x * CELL_SIZE + CELL_SIZE / 2.0f;
            float y2 = currentPath[i+1].y * CELL_SIZE + CELL_SIZE / 2.0f;
            SDL_RenderLine(as->renderer, x1, y1, x2, y2);
        }
    }

    if (settlementFound) {
        SDL_SetRenderDrawColor(as->renderer, 255, 165, 0, 255); 
        float cx = settlementPos.x * CELL_SIZE + CELL_SIZE / 2.0f;
        float cy = settlementPos.y * CELL_SIZE + CELL_SIZE / 2.0f;
        float r = (SETTLEMENT_RADIUS + 0.5f) * CELL_SIZE; 
        DrawHexagon(as->renderer, cx, cy, r);
    }

    // Draw Target
    SDL_SetRenderDrawColor(as->renderer, 255, 0, 0, 255); 
    DrawFilledCircle(as->renderer, targetPos.x * CELL_SIZE + CELL_SIZE / 2.0f, targetPos.y * CELL_SIZE + CELL_SIZE / 2.0f, PLAYER_RADIUS * 0.5f);

    // Draw Player
    SDL_SetRenderDrawColor(as->renderer, 0, 0, 255, 255);
    DrawFilledCircle(as->renderer, (float)realPlayerPos.x, (float)realPlayerPos.y, PLAYER_RADIUS);

    // Draw NPCs
    EntityManager::DrawAll(as->renderer);
    
    // Draw House Owner Initials
    SDL_SetRenderDrawColor(as->renderer, 255, 255, 255, 255);
    for (const auto& pair : agentHouses) {
        float hx = pair.second.x * CELL_SIZE + CELL_SIZE / 2.0f - 4.0f;
        float hy = pair.second.y * CELL_SIZE + CELL_SIZE / 2.0f - 4.0f;
        if (!pair.first.empty()) {
            std::string initial = pair.first.substr(0, 1);
            SDL_RenderDebugText(as->renderer, hx, hy, initial.c_str());
        }
    }
    
    // Draw Seed
    SDL_SetRenderDrawColor(as->renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(as->renderer, MAP_SIZE + 20, 20, seedString);

    // Draw Agent Inventories
    int panelY = 50;
    for (const auto& agent : EntityManager::npcs) {
        std::string header = agent.name + " (" + agent.currentJob + "):";
        SDL_RenderDebugText(as->renderer, MAP_SIZE + 20, panelY, header.c_str());
        panelY += 20;
        
        if (agent.inventory.empty()) {
            SDL_RenderDebugText(as->renderer, MAP_SIZE + 30, panelY, "- Empty");
            panelY += 20;
        } else {
            for (const auto& item : agent.inventory) {
                std::string line = "- " + item.first + ": " + std::to_string(item.second);
                SDL_RenderDebugText(as->renderer, MAP_SIZE + 30, panelY, line.c_str());
                panelY += 20;
            }
        }
        panelY += 10;
    }

    // Present
    SDL_RenderPresent(as->renderer);

    return SDL_APP_CONTINUE;
}

// Quit
void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    SaveSettlement();
    EntityManager::SaveNPCs();
    if (appstate) {
        AppState* as = (AppState*)appstate;
        delete as;
    }
}
