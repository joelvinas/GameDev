//Holds the state variables and 
// isolated AI update ticks.

#ifndef AGENT_H
#define AGENT_H

#include <vector>
#include <string>
#include <SDL3/SDL.h>
#include "Constants.h"

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

    void Update(float deltaTime);
    void Draw(SDL_Renderer* renderer);
};

#endif
