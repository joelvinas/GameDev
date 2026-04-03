//Holds the state variables and 
// isolated AI update ticks.

#ifndef AGENT_H
#define AGENT_H

#include <vector>
#include <string>
#include <map>
#include <SDL3/SDL.h>
#include "Constants.h"
#include "ItemInfo.h"

struct InventoryItem {
    ItemType itemType;
    int quantity;
    
    InventoryItem() : itemType{0, "Unknown", 0, 0, 0, 0}, quantity(0) {}
    InventoryItem(ItemType t, int q) : itemType(t), quantity(q) {}
};

struct Agent {
    int id;
    std::string name;
    Point gridPos;
    Point realPos;
    std::vector<Point> currentPath;
    bool isMoving = false;
    float moveProgress = 0.0f;
    size_t currentPathIndex = 0;
    float waitTimer = 0.0f;
    float waitDuration = 0.0f;
    bool hasHouse = false;
    Point housePos = { -1, -1 };
    bool isBuildingHouse = false;
    Point plotPos = { -1, -1 };

    std::vector<std::string> knownJobs = { "Lumberjack" };
    std::string currentJob = "Lumberjack";
    bool isGoingToWork = false;
    bool isWorking = false;
    bool isReturningHome = false;
    Point workTarget = { -1, -1 };
    std::map<int, InventoryItem> inventory;

    void Work(int& targetX, int& targetY);
    void WanderInSettlement(int& targetX, int& targetY);
    void WanderInWilderness(int& targetX, int& targetY);  // TODO: Implement this

    void Update(float deltaTime);
    void Draw(SDL_Renderer* renderer);
};

#endif
