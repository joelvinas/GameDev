//Shares mathematical structures, 
// rendering prototypes, 
// and state globals.

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <SDL3/SDL.h>
#include <map>
#include <string>
// Core Map and UI
constexpr int GIVEN_SEED = 0;
constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;
constexpr int MAP_SIZE = 900;
constexpr int GRID_SIZE = 60;
constexpr int CELL_SIZE = MAP_SIZE / GRID_SIZE;
constexpr float PLAYER_RADIUS = 5.0f;
constexpr float MUD_SPEED = 2.0f;

// Settlement
constexpr int SETTLEMENT_RADIUS = 3;
extern const char* SETTLEMENT_FILENAME;
extern const char* MAP_FILENAME;

// Core Types
struct Point {
    int x, y;
    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Point& other) const { return !(*this == other); }
};

enum CellType { GRASS = 0, OBSTACLE, SWAMP, WATER, MOUNTAIN, TREE };

struct Cell {
    CellType type;
    float altitude; 
    unsigned char r, g, b;
};

// Global Grid
extern Cell grid[GRID_SIZE][GRID_SIZE];

// Rendering Helpers
void DrawFilledCircle(SDL_Renderer* renderer, float x, float y, float r);
void DrawHexagon(SDL_Renderer* renderer, float x, float y, float r);

// Settlement State
extern bool settlementFound;
extern Point settlementPos;
extern std::map<int, Point> agentHouses;
void SaveSettlement();

#endif
