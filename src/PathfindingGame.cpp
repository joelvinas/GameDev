#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <vector>
#include <queue>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <random>

// Constants
const int GIVEN_SEED = 65100292;
const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;
const int MAP_SIZE = 900;
const int GRID_SIZE = 60; // Doubled resolution for better procedural map
const int CELL_SIZE = MAP_SIZE / GRID_SIZE; // 12
const float SQUISH_RADIUS = 5.0f;
const float MUD_SPEED = 2.0f;

// Terrain Config
const float TERRAIN_NOISE_SCALE = 3.0f;       // Higher = more varied/rugged, Lower = flatter/smoother [3]
const float TERRAIN_HEIGHT_OFFSET = 0.0f;     // > 0.0 = higher land (more mountains), < 0.0 = lower land (more lakes) [0.0]
const float WATER_LEVEL = 0.45f;              // Altitude below this is water [0.35]
const float MOUNTAIN_LEVEL = 0.75f;           // Altitude above this is mountain [0.75]
const int TERRAIN_OCTAVES = 4;                // More octaves = more high-frequency detail (more jagged) [4]

// New Terrain Config for Trees
const float TREE_DENSITY = 0.5f;       // 0.0 to 1.0 (Higher = larger/denser forests)
const float TREE_NOISE_SCALE = 5.0f;   // Scale of the forest patches
const float TREE_LINE = 0.7f;          // Max altitude for trees (below mountains)

enum CellType { GRASS = 0, OBSTACLE, SWAMP, WATER, MOUNTAIN, TREE };

struct Cell {
    CellType type;
    float altitude; 
    Uint8 r, g, b;
};

struct Point {
    int x, y;
    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Point& other) const { return !(*this == other); }
};

struct Node {
    Point pos;
    float gCost, hCost;
    Point parent;

    float fCost() const { return gCost + hCost; }
    bool operator>(const Node& other) const { return fCost() > other.fCost(); }
};

// Global State
Point squishPos = { 10, 10 }; // Grid coordinates
Point targetPos = { 10, 10 };
std::vector<Point> currentPath;
bool isMoving = false;
float moveProgress = 0.0f;
size_t currentPathIndex = 0;
Cell grid[GRID_SIZE][GRID_SIZE];
Point realSquishPos = { 10 * CELL_SIZE + CELL_SIZE / 2, 10 * CELL_SIZE + CELL_SIZE / 2 };
unsigned currentSeed = 0;
char seedString[64] = "";

// Helper to get Euclidean distance
float getDistance(Point p1, Point p2) {
    return std::sqrt(std::pow((float)p1.x - p2.x, 2) + std::pow((float)p1.y - p2.y, 2));
}

bool isPassable(int x, int y) {
    if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE) return false;
    CellType t = grid[y][x].type;
    if (t == WATER || t == TREE || t == OBSTACLE) return false;
    if (t == MOUNTAIN && grid[y][x].altitude > 0.85f) return false;
    return true;
}

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

// Procedural Terrain Helpers
float lerp(float a, float b, float t) { return a + t * (b - a); } // Linear interpolation
float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); } // Smoothstep

struct Vector2 { float x, y; };
Vector2 randomGradient(int ix, int iy, unsigned seed) {
    unsigned a = ix ^ seed, b = iy ^ (seed * 1911520717);
    a *= 3284157443; b ^= a << 16 | a >> 16;
    b *= 1911520717; a ^= b << 16 | b >> 16;
    a *= 2048419325;
    float random = a * (3.14159265f / ~(~0u >> 1));
    return {cosf(random), sinf(random)};
}

float dotGridGradient(int ix, int iy, float x, float y, unsigned seed) {
    Vector2 gradient = randomGradient(ix, iy, seed);
    float dx = x - (float)ix;
    float dy = y - (float)iy;
    return (dx*gradient.x + dy*gradient.y);
}

// Perlin noise returning -1.0 to 1.0 roughly
float perlin(float x, float y, unsigned seed) {
    int x0 = (int)std::floor(x);
    int x1 = x0 + 1;
    int y0 = (int)std::floor(y);
    int y1 = y0 + 1;
    float sx = x - (float)x0;
    float sy = y - (float)y0;
    float n0 = dotGridGradient(x0, y0, x, y, seed);
    float n1 = dotGridGradient(x1, y0, x, y, seed);
    float ix0 = lerp(n0, n1, smoothstep(sx));
    n0 = dotGridGradient(x0, y1, x, y, seed);
    n1 = dotGridGradient(x1, y1, x, y, seed);
    float ix1 = lerp(n0, n1, smoothstep(sx));
    return lerp(ix0, ix1, smoothstep(sy));
}

// Fractal noise returning 0.0 to 1.0
float fractalNoise(float x, float y, unsigned seed, int octaves = 4) {
    float total = 0.0f;
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float maxVal = 0.0f;
    for(int i = 0; i < octaves; i++) {
        total += perlin(x * frequency, y * frequency, seed) * amplitude;
        maxVal += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    float normalized = total / maxVal; // roughly -1.0 to 1.0
    return (normalized + 1.0f) * 0.5f; // scale to 0.0 to 1.0
}

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

    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            float scaleX = (float)x / GRID_SIZE * TERRAIN_NOISE_SCALE;
            float scaleY = (float)y / GRID_SIZE * TERRAIN_NOISE_SCALE;
            float alt = fractalNoise(scaleX, scaleY, seed, TERRAIN_OCTAVES) + TERRAIN_HEIGHT_OFFSET;
            alt = std::max(0.0f, std::min(1.0f, alt)); // Clamp altitude
            float moisture = fractalNoise(scaleX + 10.0f, scaleY + 10.0f, seed+1, TERRAIN_OCTAVES);

            Cell& cell = grid[y][x];
            cell.altitude = alt;

            if (alt < WATER_LEVEL) {
                cell.type = WATER;
                // Deep blue to light blue
                float t = alt / WATER_LEVEL;
                cell.r = 0;
                cell.g = (Uint8)(t * 100);
                cell.b = (Uint8)(100 + t * 155);
            } else if (alt > MOUNTAIN_LEVEL) {
                cell.type = MOUNTAIN;
                // Grey to White
                float t = (alt - MOUNTAIN_LEVEL) / (1.0f - MOUNTAIN_LEVEL);
                Uint8 c = (Uint8)(100 + t * 155);
                cell.r = c; cell.g = c; cell.b = c;
            } else {
                cell.type = GRASS;
                // Green shades
                float t = (alt - WATER_LEVEL) / (MOUNTAIN_LEVEL - WATER_LEVEL);
                cell.r = 34 + (Uint8)(t * 20);
                cell.g = 100 + (Uint8)(t * 50);
                cell.b = 34 + (Uint8)(t * 20);

                // // Add procedural trees & swamp
                for (int y = 0; y < GRID_SIZE; y++) {
                    for (int x = 0; x < GRID_SIZE; x++) {
                        float scaleX = (float)x / GRID_SIZE * TERRAIN_NOISE_SCALE;
                        float scaleY = (float)y / GRID_SIZE * TERRAIN_NOISE_SCALE;
                        float alt = fractalNoise(scaleX, scaleY, seed, TERRAIN_OCTAVES) + TERRAIN_HEIGHT_OFFSET;
                        alt = std::max(0.0f, std::min(1.0f, alt));
                        
                        // 1. Generate Moisture and Forest density noise
                        float moisture = fractalNoise(scaleX + 10.0f, scaleY + 10.0f, seed + 1, TERRAIN_OCTAVES);
                        float forestNoise = fractalNoise((float)x / GRID_SIZE * TREE_NOISE_SCALE, 
                                                        (float)y / GRID_SIZE * TREE_NOISE_SCALE, 
                                                        seed + 2, 2); // Lower octaves for smoother clusters

                        Cell& cell = grid[y][x];
                        cell.altitude = alt;

                        // --- Terrain Determination ---
                        if (alt < WATER_LEVEL) {
                            cell.type = WATER;
                            float t = alt / WATER_LEVEL;
                            cell.r = 0; cell.g = (Uint8)(t * 100); cell.b = (Uint8)(100 + t * 155);
                        } else if (alt > MOUNTAIN_LEVEL) {
                            cell.type = MOUNTAIN;
                            float t = (alt - MOUNTAIN_LEVEL) / (1.0f - MOUNTAIN_LEVEL);
                            Uint8 c = (Uint8)(100 + t * 155);
                            cell.r = c; cell.g = c; cell.b = c;
                        } else {
                            // Default Grass
                            cell.type = GRASS;
                            float t = (alt - WATER_LEVEL) / (MOUNTAIN_LEVEL - WATER_LEVEL);
                            cell.r = 34 + (Uint8)(t * 20); cell.g = 100 + (Uint8)(t * 50); cell.b = 34 + (Uint8)(t * 20);

                            // --- Intelligent Tree Placement ---
                            // Must be: above water, below tree line, and within a noise cluster
                            if (alt < TREE_LINE && forestNoise > (1.0f - TREE_DENSITY)) {
                                // Optional: Add a small jitter so it's not a solid block of trees
                                if (std::uniform_real_distribution<float>(0, 1)(rng) < 0.8f) {
                                    cell.type = TREE;
                                    cell.r = 10; cell.g = 60; cell.b = 10; 
                                }
                            } 
                            // // Mud placement (keeping your original logic or slightly tweaking)
                            // else if (moisture > 0.5f && alt < 0.5f && std::uniform_real_distribution<float>(0,1)(rng) < 0.2f) {
                            //     cell.type = SWAMP;
                            //     //cell.r = 60; cell.g = 41; cell.b = 15;
                            //     cell.r = 1; cell.g = 46; cell.b = 42;
                            // }
                        }
                    }
                }
            }
        }
    }

    // Ensure start is valid
    squishPos = { 10, 10 };
    if (!isPassable(10, 10)) {
        grid[10][10].type = GRASS;
        grid[10][10].altitude = 0.5f;
        grid[10][10].r = 34; grid[10][10].g = 139; grid[10][10].b = 34;
    }
    targetPos = squishPos;

    return SDL_APP_CONTINUE;
}

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
                    std::vector<Point> newPath = findPath(squishPos, { targetX, targetY });
                    if (!newPath.empty()) {
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

SDL_AppResult SDL_AppIterate(void* appstate) {
    AppState* as = (AppState*)appstate;

    static Uint64 lastTime = SDL_GetTicksNS();
    Uint64 currentTime = SDL_GetTicksNS();
    float deltaTime = (currentTime - lastTime) / 1e9f;
    lastTime = currentTime;

    if (isMoving && currentPathIndex < currentPath.size() - 1) {
        moveProgress += deltaTime * 5.0f * (1.0f / getMoveCost(squishPos.x, squishPos.y)); 
        if (moveProgress >= 1.0f) {
            moveProgress = 0.0f;
            currentPathIndex++;
            squishPos = currentPath[currentPathIndex];
            if (currentPathIndex >= currentPath.size() - 1) {
                isMoving = false;
                currentPath.clear();
            }
        }

        if (isMoving) {
            Point p1 = currentPath[currentPathIndex];
            Point p2 = currentPath[currentPathIndex + 1];
            realSquishPos.x = (int)((p1.x + (p2.x - p1.x) * moveProgress) * CELL_SIZE + CELL_SIZE / 2);
            realSquishPos.y = (int)((p1.y + (p2.y - p1.y) * moveProgress) * CELL_SIZE + CELL_SIZE / 2);
        }
    } else {
        realSquishPos.x = squishPos.x * CELL_SIZE + CELL_SIZE / 2;
        realSquishPos.y = squishPos.y * CELL_SIZE + CELL_SIZE / 2;
    }

    SDL_SetRenderDrawColor(as->renderer, 50, 50, 50, 255);
    SDL_RenderClear(as->renderer);

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

    if (!currentPath.empty()) {
        SDL_SetRenderDrawColor(as->renderer, 255, 255, 0, 255);
        for (size_t i = 0; i < currentPath.size() - 1; i++) {
            float x1 = currentPath[i].x * CELL_SIZE + CELL_SIZE / 2.0f;
            float y1 = currentPath[i].y * CELL_SIZE + CELL_SIZE / 2.0f;
            float x2 = currentPath[i+1].x * CELL_SIZE + CELL_SIZE / 2.0f;
            float y2 = currentPath[i+1].y * CELL_SIZE + CELL_SIZE / 2.0f;
            SDL_RenderLine(as->renderer, x1, y1, x2, y2);
        }
    }

    SDL_SetRenderDrawColor(as->renderer, 255, 0, 0, 255); 
    DrawFilledCircle(as->renderer, targetPos.x * CELL_SIZE + CELL_SIZE / 2.0f, targetPos.y * CELL_SIZE + CELL_SIZE / 2.0f, SQUISH_RADIUS * 0.5f);

    SDL_SetRenderDrawColor(as->renderer, 0, 0, 255, 255);
    DrawFilledCircle(as->renderer, (float)realSquishPos.x, (float)realSquishPos.y, SQUISH_RADIUS);
    
    SDL_SetRenderDrawColor(as->renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(as->renderer, MAP_SIZE + 20, 20, seedString);

    SDL_RenderPresent(as->renderer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    if (appstate) {
        AppState* as = (AppState*)appstate;
        delete as;
    }
}
