#include "Agent.h"
#include "Navigation.h"
#include "Constants.h"
#include "EntityManager.h"
#include "LootSystem.h"
#include <random>
#include <algorithm>
#include "sqlite3.h"
#include <chrono>

void Agent::BuildNewHouse(int& targetX, int& targetY) {
    int dX = std::abs(gridPos.x - plotPos.x);
    int dY = std::abs(gridPos.y - plotPos.y);
    if (dX <= 1 && dY <= 1 && (dX != 0 || dY != 0)) {
        hasHouse = true;
        isBuildingHouse = false;
        housePos = plotPos;



        sqlite3* db;
        if (sqlite3_open(GetDBPath(SAVE_DATA_FILENAME).c_str(), &db) == SQLITE_OK) {
            sqlite3_stmt* stmt;
            const char* insertSql = "INSERT INTO Structures (name, structureType, x, y) VALUES (?, ?, ?, ?);";
            if (sqlite3_prepare_v2(db, insertSql, -1, &stmt, 0) == SQLITE_OK) {
                std::string houseName = name + "'s House";
                sqlite3_bind_text(stmt, 1, houseName.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(stmt, 2, 1);
                sqlite3_bind_int(stmt, 3, housePos.x);
                sqlite3_bind_int(stmt, 4, housePos.y);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);

                long long newId = sqlite3_last_insert_rowid(db);
                ownedStructureIds.insert(static_cast<int>(newId));
            }
            sqlite3_close(db);
        }

        grid[housePos.y][housePos.x].type = OBSTACLE;
        grid[housePos.y][housePos.x].r = 139;
        grid[housePos.y][housePos.x].g = 69;
        grid[housePos.y][housePos.x].b = 19;
        SaveSettlement();
    }
    else {
        // Find a path to the plot
        FindPathway(targetX, targetY, plotPos, id);
    }
}

void Agent::BuildSettlement(int& targetX, int& targetY){
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

    // If the current position is a valid settlement position, set it as the settlement position
    if (validSettlement) {
        settlementFound = true;
        settlementPos = gridPos;
        SaveSettlement();
        EntityManager::AlertFoundSettlement(settlementPos);
    }
}

void Agent::FindHousePlot(Point& plotPos, int& id) {
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

bool Agent::FindTarget(int& targetX, int& targetY, int& id, CellType targetType) {
    bool foundTarget = false;
    for (int r = 1; r < 30; ++r) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (std::abs(dx) == r || std::abs(dy) == r) {
                    int tx = gridPos.x + dx;
                    int ty = gridPos.y + dy;
                    if (tx >= 0 && tx < GRID_SIZE && ty >= 0 && ty < GRID_SIZE && grid[ty][tx].type == targetType) {
                        for (int ay = -1; ay <= 1; ay++) {
                            for (int ax = -1; ax <= 1; ax++) {
                                if (ax == 0 && ay == 0) continue;
                                if (isPassable(tx + ax, ty + ay, id)) {
                                    workTarget = {tx + ax, ty + ay};
                                    isGoingToWork = true;
                                    targetX = workTarget.x;
                                    targetY = workTarget.y;
                                    foundTarget = true;
                                    break;
                                }
                            }
                            if (foundTarget) break;
                        }
                    }
                }
                if (foundTarget) break;
            }
            if (foundTarget) break;
        }
        if (foundTarget) break;
    }
    return foundTarget;
}

void Agent::FindPathway(int& targetX, int& targetY, Point& plotPos, int& id) {
    bool foundPath = false;
    for (int adjY = -1; adjY <= 1; adjY++) {
        for (int adjX = -1; adjX <= 1; adjX++) {
            if (adjX == 0 && adjY == 0) continue;
            if (isPassable(plotPos.x + adjX, plotPos.y + adjY, id)) {
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

void Agent::WanderInSettlement(int& targetX, int& targetY) {
    // Move to random position in settlement
    int dx = (std::rand() % (SETTLEMENT_RADIUS * 2 + 1)) - SETTLEMENT_RADIUS;
    int dy = (std::rand() % (SETTLEMENT_RADIUS * 2 + 1)) - SETTLEMENT_RADIUS;
    if (dx*dx + dy*dy <= SETTLEMENT_RADIUS*SETTLEMENT_RADIUS) {
        targetX = settlementPos.x + dx;
        targetY = settlementPos.y + dy;
    }
}

void Agent::Work(int& targetX, int& targetY) {
    try {

        LootTable treeLoot = LoadLootTableFromDB(GetDBPath("WorldData.db"), 1); // 1 = Tree
        
        std::mt19937 rng(std::chrono::system_clock::now().time_since_epoch().count());
        auto drops = RollLootTable(treeLoot, rng);
        
        for (const auto& drop : drops) {
            int itemId = drop.first.id;
            int qty = drop.second;
            if (inventory.find(itemId) == inventory.end()) {
                inventory[itemId] = InventoryItem(drop.first, 0);
            }
            inventory[itemId].quantity += qty;
        }
    } catch (const std::exception& e) {
        SDL_Log("Loot error: %s", e.what());
    }
    isWorking = false;
    isReturningHome = true;
    targetX = housePos.x;
    targetY = housePos.y;
}

void Agent::Update(float deltaTime) {
    if (isMoving && currentPathIndex + 1 < currentPath.size()) {
        float dist = getDistance(currentPath[currentPathIndex], currentPath[currentPathIndex + 1]);
        if (dist == 0.0f) dist = 1.0f;
        moveProgress += deltaTime * 4.0f * (1.0f / getMoveCost(gridPos.x, gridPos.y)) / dist; 

        // Update move progress
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

        // Update position
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

                // Check if the current position is a valid settlement position
                if (!settlementFound && grid[gridPos.y][gridPos.x].type == GRASS) {
                    BuildSettlement(targetX, targetY);
                }

                if (settlementFound) { 
                    if (!hasHouse && !isBuildingHouse) {
                        // Find a plot for the house
                        FindHousePlot(plotPos, id);
                    }

                    if (isBuildingHouse && !isMoving) {
                        BuildNewHouse(targetX, targetY);
                    }
                    
                    if (!isBuildingHouse) {
                        if (hasHouse && currentJob == "Lumberjack") {
                            // Check if at home
                            bool atHome = std::abs(gridPos.x - housePos.x) <= 1 && std::abs(gridPos.y - housePos.y) <= 1;
                            
                            // Check if going to work
                            if (!isGoingToWork && !isWorking && !isReturningHome) {
                                if (atHome) {
                                    // Find a target to chop
                                    bool foundTarget = FindTarget(targetX, targetY, id, TREE);
                                    
                                    if (!foundTarget) {
                                        targetX = housePos.x;
                                        targetY = housePos.y;
                                    }
                                } else {
                                    isReturningHome = true;
                                    targetX = housePos.x;
                                    targetY = housePos.y;
                                }
                            } else if (isGoingToWork && !isMoving) {
                                isGoingToWork = false;
                                isWorking = true;
                            } else if (isWorking) {
                                // Work for 5 seconds
                                Work(targetX, targetY);
                            } else if (isReturningHome && !isMoving) {
                                isReturningHome = false;
                                targetX = housePos.x;
                                targetY = housePos.y;
                            }
                        } else {
                            // Move to random position in settlement
                            WanderInSettlement(targetX, targetY);
                        }
                    }
                } else {
                    int dx = (std::rand() % 21) - 10;
                    int dy = (std::rand() % 21) - 10;
                    targetX = gridPos.x + dx;
                    targetY = gridPos.y + dy;
                }

                if (isPassable(targetX, targetY, id) && (targetX != gridPos.x || targetY != gridPos.y)) {
                    std::vector<Point> p = findPath(gridPos, {targetX, targetY}, id);
                    if (p.size() > 1) {
                        currentPath = p;
                        isMoving = true;
                        currentPathIndex = 0;
                        moveProgress = 0.0f;
                    }
                }
                waitTimer = 0.0f;
                if (isWorking) {
                    waitDuration = 5.0f;
                } else {
                    waitDuration = (std::rand() % 400) / 100.0f + 2.0f;
                }
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