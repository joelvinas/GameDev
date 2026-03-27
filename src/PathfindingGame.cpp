#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <vector>
#include <queue>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <random>
#include <string>
#include <cstdlib>
#include <fstream>
#include "MapGeneration.h"

// Point structure for grid coordinates
struct Point {
    int x, y;
    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Point& other) const { return !(*this == other); }
};

// Node structure for A* algorithm
struct Node {
    Point pos;
    float gCost, hCost;
    Point parent;

    float fCost() const { return gCost + hCost; }
    bool operator>(const Node& other) const { return fCost() > other.fCost(); }
};

// Global State
Point playerPos = { 10, 10 }; // Grid coordinates
Point targetPos = { 10, 10 };
std::vector<Point> currentPath;
bool isMoving = false;
float moveProgress = 0.0f;
size_t currentPathIndex = 0;
Point realPlayerPos = { 10 * CELL_SIZE + CELL_SIZE / 2, 10 * CELL_SIZE + CELL_SIZE / 2 };

// Agent structure for NPCs
struct Agent {
    std::string name;
    Point gridPos;
    Point realPos;
    std::vector<Point> currentPath;
    bool isMoving = false;
    float moveProgress = 0.0f;
    size_t currentPathIndex = 0;
    float waitTimer = 0.0f;
    float waitDuration = 0.0f;
};
std::vector<Agent> npcs;

bool settlementFound = false;
Point settlementPos = { -1, -1 };
const int SETTLEMENT_RADIUS = 3;

const char* SETTLEMENT_FILENAME = "Settlement.dat";


unsigned currentSeed = 0;
char seedString[64] = "";

// Get Euclidean distance between two points
float getDistance(Point p1, Point p2) {
    return std::sqrt(std::pow((float)p1.x - p2.x, 2) + std::pow((float)p1.y - p2.y, 2));
}

// Check if a cell is passable
bool isPassable(int x, int y) {
    if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE) return false;
    CellType t = grid[y][x].type;
    if (t == WATER || t == TREE || t == OBSTACLE) return false;
    if (t == MOUNTAIN && grid[y][x].altitude > 0.85f) return false;
    return true;
}

// Check if a cell is valid for a settlement
bool isValidSettlement(int cx, int cy) {
    if (cx < SETTLEMENT_RADIUS || cx >= GRID_SIZE - SETTLEMENT_RADIUS || 
        cy < SETTLEMENT_RADIUS || cy >= GRID_SIZE - SETTLEMENT_RADIUS) {
        return false;
    }
    for (int dy = -SETTLEMENT_RADIUS; dy <= SETTLEMENT_RADIUS; dy++) {
        for (int dx = -SETTLEMENT_RADIUS; dx <= SETTLEMENT_RADIUS; dx++) {
            if (dx*dx + dy*dy <= SETTLEMENT_RADIUS*SETTLEMENT_RADIUS) {
                int px = cx + dx;
                int py = cy + dy;
                if (!isPassable(px, py) || grid[py][px].type != GRASS) {
                    return false;
                }
            }
        }
    }
    return true;
}

// Get movement cost for a cell
float getMoveCost(int x, int y) {
    float baseCost = 1.0f;
    CellType t = grid[y][x].type;
    if (t == SWAMP) baseCost *= 2.0f;
    if (t == MOUNTAIN) {
        float alt = grid[y][x].altitude;
        baseCost *= (1.0f + (alt - 0.7f) * 10.0f); // increasingly expensive
    }
    return baseCost;
}

// Bresenham's line algorithm for line of sight
bool lineOfSight(Point p0, Point p1) {
    int x0 = p0.x, y0 = p0.y;
    int x1 = p1.x, y1 = p1.y;
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int x = x0, y = y0;
    int n = 1 + dx + dy;
    int x_inc = (x1 > x0) ? 1 : -1;
    int y_inc = (y1 > y0) ? 1 : -1;
    int error = dx - dy;
    dx *= 2;
    dy *= 2;

    for (; n > 0; --n) {
        if (!isPassable(x, y)) return false;
        if (getMoveCost(x, y) > 1.0f) return false;

        if (error > 0) {
            x += x_inc;
            error -= dy;
        } else if (error < 0) {
            y += y_inc;
            error += dx;
        } else {
            x += x_inc;
            error -= dy;
            y += y_inc;
            error += dx;
            --n; 
        }
    }
    return true;
}

// String pulling algorithm to smooth paths
std::vector<Point> stringPull(const std::vector<Point>& path) {
    if (path.size() <= 2) return path;
    std::vector<Point> smoothPath;
    smoothPath.push_back(path.front());
    size_t currentIndex = 0;
    while (currentIndex + 1 < path.size()) {
        size_t farthestVisible = currentIndex + 1;
        for (size_t i = currentIndex + 2; i < path.size(); i++) {
            if (lineOfSight(path[currentIndex], path[i])) {
                farthestVisible = i;
            }
        }
        smoothPath.push_back(path[farthestVisible]);
        currentIndex = farthestVisible;
    }
    return smoothPath;
}

// Catmull-Rom spline interpolation
float getSplineVal(float p0, float p1, float p2, float p3, float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
    );
}

// A* Implementation
std::vector<Point> findPath(Point start, Point end) {
    if (!isPassable(end.x, end.y)) return {};

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openSet;
    std::vector<std::vector<float>> gCosts(GRID_SIZE, std::vector<float>(GRID_SIZE, INFINITY));
    std::vector<std::vector<Point>> parents(GRID_SIZE, std::vector<Point>(GRID_SIZE, { -1, -1 }));

    gCosts[start.y][start.x] = 0;
    openSet.push({ start, 0, getDistance(start, end), { -1, -1 }});

    while (!openSet.empty()) {
        Node current = openSet.top();
        openSet.pop();

        if (current.pos == end) {
            std::vector<Point> path;
            Point currPos = end;
            while (currPos.x != -1) {
                path.push_back(currPos);
                currPos = parents[currPos.y][currPos.x];
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                
                Point neighbor = { current.pos.x + dx, current.pos.y + dy };
                if (isPassable(neighbor.x, neighbor.y)) {
                    float moveCost = (dx == 0 || dy == 0) ? 1.0f : 1.414f;
                    moveCost *= getMoveCost(neighbor.x, neighbor.y);
                    float newGCost = gCosts[current.pos.y][current.pos.x] + moveCost;

                    if (newGCost < gCosts[neighbor.y][neighbor.x]) {
                        gCosts[neighbor.y][neighbor.x] = newGCost;
                        parents[neighbor.y][neighbor.x] = current.pos;
                        openSet.push({ neighbor, newGCost, getDistance(neighbor, end), current.pos});
                    }
                }
            }
        }
    }

    return {};
}

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
    std::string path = "bin\\Settlement.dat";
    std::ofstream outFile(path, std::ios::binary);
    if (outFile.is_open()) {
        outFile.write(reinterpret_cast<const char*>(&settlementPos), sizeof(Point));
        outFile.close();
        SDL_Log("Settlement saved to %s", path.c_str());
    }
}

// Load settlement from file
bool LoadSettlement() {
    std::string path = "bin\\Settlement.dat";
    std::ifstream inFile(path, std::ios::binary);
    if (inFile.is_open()) {
        inFile.read(reinterpret_cast<char*>(&settlementPos), sizeof(Point));
        int readBytes = inFile.gcount();
        if (readBytes == sizeof(Point)) {
            settlementFound = true;
            SDL_Log("Settlement loaded from %s: %d, %d", path.c_str(), settlementPos.x, settlementPos.y);
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
        // Generate Terrain
        GenerateMap(seed);
        SaveMap();
    }

    LoadSettlement();

    // Ensure start is valid
    playerPos = { 10, 10 };
    if (!isPassable(10, 10)) {
        grid[10][10].type = GRASS;
        grid[10][10].altitude = 0.5f;
        grid[10][10].r = 34; grid[10][10].g = 139; grid[10][10].b = 34;
    }
    targetPos = playerPos;

    // Initialize NPCs
    std::vector<std::string> names = {"Andy", "Belle", "Curtis"};
    int spawned = 0;
    for (int r = 1; r < 30 && spawned < 3; r++) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                int nx = 30 + dx;
                int ny = 30 + dy;
                if (isPassable(nx, ny)) {
                    Agent a;
                    a.name = names[spawned];
                    a.gridPos = { nx, ny };
                    a.realPos = { nx * CELL_SIZE + CELL_SIZE / 2, ny * CELL_SIZE + CELL_SIZE / 2 };
                    a.waitDuration = std::uniform_real_distribution<float>(2.0f, 5.0f)(rng);
                    npcs.push_back(a);
                    spawned++;
                    if (spawned >= 3) break;
                }
            }
            if (spawned >= 3) break;
        }
    }

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
    for (auto& agent : npcs) {
        if (agent.isMoving && agent.currentPathIndex + 1 < agent.currentPath.size()) {
            float dist = getDistance(agent.currentPath[agent.currentPathIndex], agent.currentPath[agent.currentPathIndex + 1]);
            if (dist == 0.0f) dist = 1.0f;
            agent.moveProgress += deltaTime * 4.0f * (1.0f / getMoveCost(agent.gridPos.x, agent.gridPos.y)) / dist; 

            if (agent.moveProgress >= 1.0f) {
                agent.moveProgress = 0.0f;
                agent.currentPathIndex++;
                agent.gridPos = agent.currentPath[agent.currentPathIndex];
                if (agent.currentPathIndex + 1 >= agent.currentPath.size()) {
                    agent.isMoving = false;
                    agent.currentPath.clear();
                    agent.waitDuration = (std::rand() % 400) / 100.0f + 2.0f; // 2.0 to 5.99
                    agent.waitTimer = 0.0f;
                }
            }

            if (agent.isMoving) {
                int i0 = std::max(0, (int)agent.currentPathIndex - 1);
                int i1 = agent.currentPathIndex;
                int i2 = std::min((int)agent.currentPath.size() - 1, (int)agent.currentPathIndex + 1);
                int i3 = std::min((int)agent.currentPath.size() - 1, (int)agent.currentPathIndex + 2);

                Point p0 = agent.currentPath[i0];
                Point p1 = agent.currentPath[i1];
                Point p2 = agent.currentPath[i2];
                Point p3 = agent.currentPath[i3];

                float rx = getSplineVal((float)p0.x, (float)p1.x, (float)p2.x, (float)p3.x, agent.moveProgress);
                float ry = getSplineVal((float)p0.y, (float)p1.y, (float)p2.y, (float)p3.y, agent.moveProgress);

                agent.realPos.x = (int)(rx * CELL_SIZE + CELL_SIZE / 2);
                agent.realPos.y = (int)(ry * CELL_SIZE + CELL_SIZE / 2);
            }
        } else {
            agent.realPos.x = agent.gridPos.x * CELL_SIZE + CELL_SIZE / 2;
            agent.realPos.y = agent.gridPos.y * CELL_SIZE + CELL_SIZE / 2;
            
            agent.waitTimer += deltaTime;
            if (agent.waitTimer >= agent.waitDuration) {
                if (!settlementFound) {
                    bool found = false;
                    for (int dy = -15; dy <= 15 && !found; dy++) {
                        for (int dx = -15; dx <= 15 && !found; dx++) {
                            if (dx*dx + dy*dy <= 15*15) {
                                int cx = agent.gridPos.x + dx;
                                int cy = agent.gridPos.y + dy;
                                if (isValidSettlement(cx, cy)) {
                                    settlementFound = true;
                                    settlementPos = { cx, cy };
                                    SaveSettlement();
                                    found = true;
                                    SDL_Log("%s found a settlement at %d, %d!", agent.name.c_str(), cx, cy);
                                }
                            }
                        }
                    }
                    if (settlementFound) {
                        for (auto& a : npcs) {
                            std::vector<Point> p = findPath(a.gridPos, settlementPos);
                            if (p.size() > 1) {
                                a.currentPath = p;
                                a.isMoving = true;
                                a.currentPathIndex = 0;
                                a.moveProgress = 0.0f;
                            }
                        }
                        break; 
                    }
                }

                if (!agent.isMoving) {
                    int targetX = agent.gridPos.x;
                    int targetY = agent.gridPos.y;
                    
                    if (settlementFound) {
                        int dx = (std::rand() % (SETTLEMENT_RADIUS * 2 + 1)) - SETTLEMENT_RADIUS;
                        int dy = (std::rand() % (SETTLEMENT_RADIUS * 2 + 1)) - SETTLEMENT_RADIUS;
                        if (dx*dx + dy*dy <= SETTLEMENT_RADIUS*SETTLEMENT_RADIUS) {
                            targetX = settlementPos.x + dx;
                            targetY = settlementPos.y + dy;
                        }
                    } else {
                        int dx = (std::rand() % 21) - 10;
                        int dy = (std::rand() % 21) - 10;
                        targetX = agent.gridPos.x + dx;
                        targetY = agent.gridPos.y + dy;
                    }

                    if (isPassable(targetX, targetY)) {
                        std::vector<Point> p = findPath(agent.gridPos, {targetX, targetY});
                        if (p.size() > 1) {
                            agent.currentPath = p;
                            agent.isMoving = true;
                            agent.currentPathIndex = 0;
                            agent.moveProgress = 0.0f;
                        }
                    }
                    agent.waitTimer = 0.0f;
                    agent.waitDuration = (std::rand() % 400) / 100.0f + 2.0f;
                }
            }
        }
    }

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
    for (const auto& agent : npcs) {
        // Draw NPC
        SDL_SetRenderDrawColor(as->renderer, 255, 165, 0, 255); // Orange
        DrawFilledCircle(as->renderer, (float)agent.realPos.x, (float)agent.realPos.y, PLAYER_RADIUS);
        
        // Draw NPC Name
        SDL_SetRenderDrawColor(as->renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(as->renderer, (float)agent.realPos.x - 10, (float)agent.realPos.y + PLAYER_RADIUS + 2, agent.name.c_str());
    }
    
    // Draw Seed
    SDL_SetRenderDrawColor(as->renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(as->renderer, MAP_SIZE + 20, 20, seedString);

    // Present
    SDL_RenderPresent(as->renderer);

    return SDL_APP_CONTINUE;
}

// Quit
void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    if (appstate) {
        AppState* as = (AppState*)appstate;
        delete as;
    }
}