//Shares mathematical structures, 
// rendering prototypes, 
// and state globals.

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <SDL3/SDL.h>
#include <map>
#include <string>
#include <filesystem>

// Core Map and UI
constexpr int GIVEN_SEED = 0;
constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;
constexpr int MAP_SIZE = 900;
constexpr int GRID_SIZE = 60;
constexpr int CELL_SIZE = MAP_SIZE / GRID_SIZE;
constexpr float PLAYER_RADIUS = 5.0f;
constexpr float MUD_SPEED = 2.0f;
constexpr int SETTLEMENT_ID = 1;

// Settlement
constexpr int SETTLEMENT_RADIUS = 3;

// Database filenames
constexpr const char* SETTLEMENT_FILENAME = "Settlement.dat";
//constexpr const char* MAP_FILENAME = "GameMap.map";
constexpr const char* WORLD_DATA_FILENAME = "WorldData.db";
constexpr const char* SAVE_DATA_FILENAME = "SaveData.db";
constexpr const char* MAP_DATA_FILENAME = "MapData.db";

// Database helper
inline std::string GetDBPath(const std::string& dbName) {
    // If "data" folder exists linearly here, we are running from Terminal root.
    // If not, we fall back to relative parent pathing assuming we are in /bin/.
    if (std::filesystem::exists("data")) {
        return "data\\" + dbName;
    } else {
        return "..\\data\\" + dbName;
    }
}

// Core Types
struct Point {
    int x, y;
    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
    bool operator!=(const Point& other) const { return !(*this == other); }
};

enum ObjectType { TREE, STUMP, HOUSE, WALL, STORAGE, WAREHOUSE_PLOT };

struct WorldObject {
    int id;
    ObjectType type;
    Point gridPos;
    float health;
    float maxHealth;
    int resourceYield; // For Trees/Mines
    int ownerId;       // To track which Agent owns the house
};

enum CellType { GRASS = 0, OBSTACLE, SWAMP, WATER, MOUNTAIN };

struct Cell {
    CellType type;
    float altitude; 
    unsigned char r, g, b;
};

// Global Collections
extern Cell grid[GRID_SIZE][GRID_SIZE];
extern std::vector<WorldObject> worldObjects;
WorldObject* GetObjectAt(int x, int y);



// Rendering Helpers
void DrawFilledCircle(SDL_Renderer* renderer, float x, float y, float r);
void DrawHexagon(SDL_Renderer* renderer, float x, float y, float r);

// Settlement State
extern bool settlementFound;
extern Point settlementPos;

// Warehouse State
extern bool warehouseFound;
extern Point warehousePos;
extern bool warehousePlotFound;
extern Point warehousePlotPos;
extern int warehouseWoodCount;
extern int warehouseStickCount;
extern std::map<int, std::map<int, struct InventoryItem>> settlementInventories;

void SaveSettlement();

#endif