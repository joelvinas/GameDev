//Isolates the terrain Perlin noise logic.

#ifndef MAP_GENERATION_H
#define MAP_GENERATION_H

#include <string>
#include "Constants.h"

void GenerateMap(unsigned seed);
void SaveMap();
bool LoadMap();

#endif
