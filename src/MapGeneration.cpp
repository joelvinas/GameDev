#include "MapGeneration.h"
#include <cmath>
#include <fstream>
#include <algorithm>
#include <random>

const char* MAP_FILENAME = "GameMap.map";

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

std::string GetMapFilePath() {
    const char* basePath = SDL_GetBasePath();
    std::string path;
    if (basePath) {
        path = std::string(basePath) + MAP_FILENAME;
        SDL_free((void*)basePath);
    } else {
        path = MAP_FILENAME; 
    }
    return path;
}

void SaveMap() {
    std::string path = GetMapFilePath();
    std::ofstream outFile(path, std::ios::binary);
    if (outFile.is_open()) {
        outFile.write(reinterpret_cast<const char*>(grid), sizeof(grid));
        outFile.close();
        SDL_Log("Map saved successfully to %s", path.c_str());
    }
}

bool LoadMap() {
    std::string path = GetMapFilePath();
    std::ifstream inFile(path, std::ios::binary);
    if (inFile.is_open()) {
        inFile.read(reinterpret_cast<char*>(grid), sizeof(grid));
        inFile.close();
        SDL_Log("Map loaded successfully from %s", path.c_str());
        return true;
    }
    return false;
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
