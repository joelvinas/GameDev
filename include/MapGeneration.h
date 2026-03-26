#ifndef MAP_GENERATION_H
#define MAP_GENERATION_H

#include <SDL3/SDL.h>
#include <string>

// Constants
constexpr int GIVEN_SEED = 0;
constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;
constexpr int MAP_SIZE = 900;
constexpr int GRID_SIZE = 60;
constexpr int CELL_SIZE = MAP_SIZE / GRID_SIZE;
constexpr float SQUISH_RADIUS = 5.0f;
constexpr float MUD_SPEED = 2.0f;

enum CellType { GRASS = 0, OBSTACLE, SWAMP, WATER, MOUNTAIN, TREE };

struct Cell {
    CellType type;
    float altitude; 
    Uint8 r, g, b;
};

// Global grid array
extern Cell grid[GRID_SIZE][GRID_SIZE];

void GenerateMap(unsigned seed);
void SaveMap();
bool LoadMap();

#endif
