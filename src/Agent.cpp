#include "Agent.h"
#include "Navigation.h"
#include "Constants.h"
#include "EntityManager.h"
#include <random>
#include <algorithm>

void Agent::Update(float deltaTime) {
    if (isMoving && currentPathIndex + 1 < currentPath.size()) {
        float dist = getDistance(currentPath[currentPathIndex], currentPath[currentPathIndex + 1]);
        if (dist == 0.0f) dist = 1.0f;
        moveProgress += deltaTime * 4.0f * (1.0f / getMoveCost(gridPos.x, gridPos.y)) / dist; 

        if (moveProgress >= 1.0f) {
            moveProgress = 0.0f;
            currentPathIndex++;
            gridPos = currentPath[currentPathIndex];
            if (currentPathIndex + 1 >= currentPath.size()) {
                isMoving = false;
                currentPath.clear();
                waitDuration = (std::rand() % 400) / 100.0f + 2.0f; // 2.0 to 5.99
                waitTimer = 0.0f;
            }
        }

        if (isMoving) {
            int i0 = std::max(0, (int)currentPathIndex - 1);
            int i1 = currentPathIndex;
            int i2 = std::min((int)currentPath.size() - 1, (int)currentPathIndex + 1);
            int i3 = std::min((int)currentPath.size() - 1, (int)currentPathIndex + 2);

            Point p0 = currentPath[i0];
            Point p1 = currentPath[i1];
            Point p2 = currentPath[i2];
            Point p3 = currentPath[i3];

            float rx = getSplineVal((float)p0.x, (float)p1.x, (float)p2.x, (float)p3.x, moveProgress);
            float ry = getSplineVal((float)p0.y, (float)p1.y, (float)p2.y, (float)p3.y, moveProgress);

            realPos.x = (int)(rx * CELL_SIZE + CELL_SIZE / 2);
            realPos.y = (int)(ry * CELL_SIZE + CELL_SIZE / 2);
        }
    } else {
        realPos.x = gridPos.x * CELL_SIZE + CELL_SIZE / 2;
        realPos.y = gridPos.y * CELL_SIZE + CELL_SIZE / 2;
        
        waitTimer += deltaTime;
        if (waitTimer >= waitDuration) {
            if (!isMoving) {
                int targetX = gridPos.x;
                int targetY = gridPos.y;
                if (!settlementFound && grid[gridPos.y][gridPos.x].type == GRASS) {
                    bool validSettlement = true;
                    for (int dy = -SETTLEMENT_RADIUS; dy <= SETTLEMENT_RADIUS; dy++) {
                        for (int dx = -SETTLEMENT_RADIUS; dx <= SETTLEMENT_RADIUS; dx++) {
                            if (dx*dx + dy*dy <= SETTLEMENT_RADIUS*SETTLEMENT_RADIUS) {
                                int cx = gridPos.x + dx;
                                int cy = gridPos.y + dy;
                                if (cx >= 0 && cx < GRID_SIZE && cy >= 0 && cy < GRID_SIZE) {
                                    if (grid[cy][cx].type == WATER) {
                                        validSettlement = false;
                                        break;
                                    }
                                } else {
                                    validSettlement = false;
                                    break;
                                }
                            }
                        }
                        if (!validSettlement) break;
                    }

                    if (validSettlement) {
                        settlementFound = true;
                        settlementPos = gridPos;
                        SaveSettlement();
                        EntityManager::AlertFoundSettlement(settlementPos);
                    }
                }

                if (settlementFound) {
                    if (!hasHouse && !isBuildingHouse) {
                        for (int attempt = 0; attempt < 50; attempt++) {
                            int dx = (std::rand() % (SETTLEMENT_RADIUS * 2 + 1)) - SETTLEMENT_RADIUS;
                            int dy = (std::rand() % (SETTLEMENT_RADIUS * 2 + 1)) - SETTLEMENT_RADIUS;
                            if (dx*dx + dy*dy <= SETTLEMENT_RADIUS*SETTLEMENT_RADIUS) {
                                int hx = settlementPos.x + dx;
                                int hy = settlementPos.y + dy;
                                if (hx >= 0 && hx < GRID_SIZE && hy >= 0 && hy < GRID_SIZE && grid[hy][hx].type == GRASS) {
                                    isBuildingHouse = true;
                                    plotPos = {hx, hy};
                                    break;
                                }
                            }
                        }
                    }

                    if (isBuildingHouse && !isMoving) {
                        int dX = std::abs(gridPos.x - plotPos.x);
                        int dY = std::abs(gridPos.y - plotPos.y);
                        if (dX <= 1 && dY <= 1 && (dX != 0 || dY != 0)) {
                            hasHouse = true;
                            isBuildingHouse = false;
                            housePos = plotPos;
                            agentHouses[name] = housePos;
                            grid[housePos.y][housePos.x].type = OBSTACLE;
                            grid[housePos.y][housePos.x].r = 139;
                            grid[housePos.y][housePos.x].g = 69;
                            grid[housePos.y][housePos.x].b = 19;
                            SaveSettlement();
                        } else {
                            bool foundPath = false;
                            for (int adjY = -1; adjY <= 1; adjY++) {
                                for (int adjX = -1; adjX <= 1; adjX++) {
                                    if (adjX == 0 && adjY == 0) continue;
                                    if (isPassable(plotPos.x + adjX, plotPos.y + adjY, name)) {
                                        targetX = plotPos.x + adjX;
                                        targetY = plotPos.y + adjY;
                                        foundPath = true;
                                        break;
                                    }
                                }
                                if (foundPath) break;
                            }
                            if (!foundPath) isBuildingHouse = false;
                        }
                    }
                    
                    if (!isBuildingHouse) {
                        int dx = (std::rand() % (SETTLEMENT_RADIUS * 2 + 1)) - SETTLEMENT_RADIUS;
                        int dy = (std::rand() % (SETTLEMENT_RADIUS * 2 + 1)) - SETTLEMENT_RADIUS;
                        if (dx*dx + dy*dy <= SETTLEMENT_RADIUS*SETTLEMENT_RADIUS) {
                            targetX = settlementPos.x + dx;
                            targetY = settlementPos.y + dy;
                        }
                    }
                } else {
                    int dx = (std::rand() % 21) - 10;
                    int dy = (std::rand() % 21) - 10;
                    targetX = gridPos.x + dx;
                    targetY = gridPos.y + dy;
                }

                if (isPassable(targetX, targetY, name)) {
                    std::vector<Point> p = findPath(gridPos, {targetX, targetY}, name);
                    if (p.size() > 1) {
                        currentPath = p;
                        isMoving = true;
                        currentPathIndex = 0;
                        moveProgress = 0.0f;
                    }
                }
                waitTimer = 0.0f;
                waitDuration = (std::rand() % 400) / 100.0f + 2.0f;
            }
        }
    }
}

void Agent::Draw(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 255, 165, 0, 255); // Orange
    DrawFilledCircle(renderer, (float)realPos.x, (float)realPos.y, PLAYER_RADIUS);
    
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(renderer, (float)realPos.x - 10, (float)realPos.y + PLAYER_RADIUS + 2, name.c_str());
}