#include "MapGeneration.h"
#include <cmath>
#include <fstream>
#include <algorithm>
#include <random>
#include <sqlite3.h>
#include <vector>
#include "Constants.h"

// Terrain Config
const float TERRAIN_NOISE_SCALE = 3.0f;       
const float TERRAIN_HEIGHT_OFFSET = 0.0f;     
const float WATER_LEVEL = 0.45f;              
const float MOUNTAIN_LEVEL = 0.75f;           
const int TERRAIN_OCTAVES = 4;                

// New Terrain Config for Trees
const float TREE_DENSITY = 0.4f;       
const float TREE_NOISE_SCALE = 5.0f;   
const float TREE_LINE = 0.7f;          

Cell grid[GRID_SIZE][GRID_SIZE];

float lerp(float a, float b, float t) { return a + t * (b - a); }
float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

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

float fractalNoise(float x, float y, unsigned seed, int octaves) {
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
    float normalized = total / maxVal; 
    return (normalized + 1.0f) * 0.5f; 
}

//std::string GetMapFilePath() {
//    const char* basePath = SDL_GetBasePath();
//    std::string path;
//    if (basePath) {
//        path = std::string(basePath) + MAP_FILENAME;
//        SDL_free((void*)basePath);
//    } else {
//        path = MAP_FILENAME; 
//    }
//    return path;
//}

void SaveMapToDB() {
    sqlite3* db;
    int rc = sqlite3_open(GetDBPath(MAP_DATA_FILENAME).c_str(), &db);

    if (rc != SQLITE_OK) {
        SDL_Log("Cannot open database: %s", sqlite3_errmsg(db));
        return;
    }

    // 1. Create the table
    const char* createTableSQL = 
        "CREATE TABLE IF NOT EXISTS map_cells ("
        "x INTEGER, y INTEGER, altitude REAL, type INTEGER, r INTEGER, g INTEGER, b INTEGER,"
        "PRIMARY KEY (x, y));";
    sqlite3_exec(db, createTableSQL, nullptr, nullptr, nullptr);

    // 2. Start a Transaction (Crucial for performance!)
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    // 3. Prepare the Insert Statement
    const char* insertSQL = "INSERT OR REPLACE INTO map_cells (x, y, altitude, type, r, g, b) VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, insertSQL, -1, &stmt, nullptr);

    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            Cell& cell = grid[y][x];
            sqlite3_bind_int(stmt, 1, x);
            sqlite3_bind_int(stmt, 2, y);
            sqlite3_bind_double(stmt, 3, cell.altitude);
            sqlite3_bind_int(stmt, 4, cell.type);
            sqlite3_bind_int(stmt, 5, cell.r);
            sqlite3_bind_int(stmt, 6, cell.g);
            sqlite3_bind_int(stmt, 7, cell.b);

            sqlite3_step(stmt);
            sqlite3_reset(stmt); // Reset for next iteration
        }
    }

    // 4. Finalize and Commit
    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
    sqlite3_close(db);
    SDL_Log("Map saved to SQLite database successfully.");
}

bool LoadMapFromDB() {
    sqlite3* db;
    if (sqlite3_open(GetDBPath(MAP_DATA_FILENAME).c_str(), &db) != SQLITE_OK) return false;

    const char* query = "SELECT x, y, altitude, type, r, g, b FROM map_cells;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int x = sqlite3_column_int(stmt, 0);
        int y = sqlite3_column_int(stmt, 1);
        
        if (x < GRID_SIZE && y < GRID_SIZE) {
            grid[y][x].altitude = (float)sqlite3_column_double(stmt, 2);
            grid[y][x].type = (CellType)sqlite3_column_int(stmt, 3);
            grid[y][x].r = (Uint8)sqlite3_column_int(stmt, 4);
            grid[y][x].g = (Uint8)sqlite3_column_int(stmt, 5);
            grid[y][x].b = (Uint8)sqlite3_column_int(stmt, 6);
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return true;
}

void GenerateMap(unsigned seed) {
    std::mt19937 rng(seed);
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            float scaleX = (float)x / GRID_SIZE * TERRAIN_NOISE_SCALE;
            float scaleY = (float)y / GRID_SIZE * TERRAIN_NOISE_SCALE;
            float alt = fractalNoise(scaleX, scaleY, seed, TERRAIN_OCTAVES) + TERRAIN_HEIGHT_OFFSET;
            alt = std::max(0.0f, std::min(1.0f, alt)); // Clamp altitude
            
            float moisture = fractalNoise(scaleX + 10.0f, scaleY + 10.0f, seed+1, TERRAIN_OCTAVES);
            float forestNoise = fractalNoise((float)x / GRID_SIZE * TREE_NOISE_SCALE, 
                                            (float)y / GRID_SIZE * TREE_NOISE_SCALE, 
                                            seed + 2, 2); 

            Cell& cell = grid[y][x];
            cell.altitude = alt;

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
                cell.type = GRASS;
                float t = (alt - WATER_LEVEL) / (MOUNTAIN_LEVEL - WATER_LEVEL);
                cell.r = 34 + (Uint8)(t * 20); cell.g = 100 + (Uint8)(t * 50); cell.b = 34 + (Uint8)(t * 20);

                if (alt < TREE_LINE && forestNoise > (1.0f - TREE_DENSITY)) {
                    if (std::uniform_real_distribution<float>(0, 1)(rng) < 0.8f) {
                        cell.type = TREE;
                        cell.r = 10; cell.g = 60; cell.b = 10; 
                    }
                } 
            }
        }
    }
}
