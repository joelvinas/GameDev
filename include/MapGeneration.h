//Isolates the terrain Perlin noise logic.

#ifndef MAP_GENERATION_H
#define MAP_GENERATION_H

#include <string>

void GenerateMap(unsigned seed);
void GenerateTreesForMap(unsigned seed);
void SaveMapToDB();
bool LoadMapFromDB();

#endif
