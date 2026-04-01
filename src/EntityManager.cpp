#include "EntityManager.h"
#include "Navigation.h"
#include "Constants.h"
#include <random>
#include <chrono>
#include <fstream>
#include <filesystem>

std::vector<Agent> EntityManager::npcs;

void EntityManager::Initialize() {
    if (LoadNPCs()) {
        return;
    }
    std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    std::vector<std::string> names = {"Andy", "Belle", "Curtis"};
    int spawned = 0;
    for (int r = 1; r < 30 && spawned < 3; r++) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                int nx = 30 + dx;
                int ny = 30 + dy;
                if (isPassable(nx, ny)) {
                    Agent a;
                    a.name = names[spawned];
                    a.gridPos = { nx, ny };
                    a.realPos = { nx * CELL_SIZE + CELL_SIZE / 2, ny * CELL_SIZE + CELL_SIZE / 2 };
                    a.waitDuration = std::uniform_real_distribution<float>(2.0f, 5.0f)(rng);
                    if (agentHouses.find(a.name) != agentHouses.end()) {
                        a.hasHouse = true;
                        a.housePos = agentHouses[a.name];
                        grid[a.housePos.y][a.housePos.x].type = OBSTACLE;
                        grid[a.housePos.y][a.housePos.x].r = 139;
                        grid[a.housePos.y][a.housePos.x].g = 69;
                        grid[a.housePos.y][a.housePos.x].b = 19;
                    }
                    npcs.push_back(a);
                    spawned++;
                    if (spawned >= 3) break;
                }
            }
            if (spawned >= 3) break;
        }
    }
}

void EntityManager::UpdateAll(float deltaTime) {
    for (auto& agent : npcs) {
        agent.Update(deltaTime);
    }
}

void EntityManager::DrawAll(SDL_Renderer* renderer) {
    for (auto& agent : npcs) {
        agent.Draw(renderer);
    }
}

void EntityManager::AlertFoundSettlement(Point settlementPos) {
    for (auto& a : npcs) {
        std::vector<Point> p = findPath(a.gridPos, settlementPos, a.name);
        if (p.size() > 1) {
            a.currentPath = p;
            a.isMoving = true;
            a.currentPathIndex = 0;
            a.moveProgress = 0.0f;
        }
    }
}

void EntityManager::SaveNPCs() {
    std::string path = std::filesystem::exists("bin") ? "bin\\NPCs.dat" : "NPCs.dat";
    std::ofstream outFile(path, std::ios::binary);
    if (outFile.is_open()) {
        size_t count = npcs.size();
        outFile.write(reinterpret_cast<const char*>(&count), sizeof(size_t));
        for (const auto& a : npcs) {
            size_t nameLen = a.name.size();
            outFile.write(reinterpret_cast<const char*>(&nameLen), sizeof(size_t));
            outFile.write(a.name.c_str(), nameLen);
            
            outFile.write(reinterpret_cast<const char*>(&a.gridPos), sizeof(Point));
            outFile.write(reinterpret_cast<const char*>(&a.hasHouse), sizeof(bool));
            outFile.write(reinterpret_cast<const char*>(&a.housePos), sizeof(Point));
            outFile.write(reinterpret_cast<const char*>(&a.isBuildingHouse), sizeof(bool));
            outFile.write(reinterpret_cast<const char*>(&a.plotPos), sizeof(Point));
            
            size_t jobLen = a.currentJob.size();
            outFile.write(reinterpret_cast<const char*>(&jobLen), sizeof(size_t));
            outFile.write(a.currentJob.c_str(), jobLen);

            outFile.write(reinterpret_cast<const char*>(&a.isGoingToWork), sizeof(bool));
            outFile.write(reinterpret_cast<const char*>(&a.isWorking), sizeof(bool));
            outFile.write(reinterpret_cast<const char*>(&a.isReturningHome), sizeof(bool));
            outFile.write(reinterpret_cast<const char*>(&a.workTarget), sizeof(Point));

            size_t invSize = a.inventory.size();
            outFile.write(reinterpret_cast<const char*>(&invSize), sizeof(size_t));
            for (const auto& item : a.inventory) {
                size_t itemLen = item.first.size();
                outFile.write(reinterpret_cast<const char*>(&itemLen), sizeof(size_t));
                outFile.write(item.first.c_str(), itemLen);
                outFile.write(reinterpret_cast<const char*>(&item.second), sizeof(int));
            }
        }
        outFile.close();
        SDL_Log("NPCs saved to %s", path.c_str());
    }
}

bool EntityManager::LoadNPCs() {
    std::string path = std::filesystem::exists("bin") ? "bin\\NPCs.dat" : "NPCs.dat";
    std::ifstream inFile(path, std::ios::binary);
    if (inFile.is_open()) {
        size_t count = 0;
        if (inFile.read(reinterpret_cast<char*>(&count), sizeof(size_t))) {
            npcs.clear();
            for (size_t i = 0; i < count; ++i) {
                Agent a;
                size_t nameLen = 0;
                inFile.read(reinterpret_cast<char*>(&nameLen), sizeof(size_t));
                a.name.resize(nameLen);
                inFile.read(&a.name[0], nameLen);

                inFile.read(reinterpret_cast<char*>(&a.gridPos), sizeof(Point));
                a.realPos = { a.gridPos.x * CELL_SIZE + CELL_SIZE / 2, a.gridPos.y * CELL_SIZE + CELL_SIZE / 2 };

                inFile.read(reinterpret_cast<char*>(&a.hasHouse), sizeof(bool));
                inFile.read(reinterpret_cast<char*>(&a.housePos), sizeof(Point));
                inFile.read(reinterpret_cast<char*>(&a.isBuildingHouse), sizeof(bool));
                inFile.read(reinterpret_cast<char*>(&a.plotPos), sizeof(Point));

                size_t jobLen = 0;
                if (inFile.read(reinterpret_cast<char*>(&jobLen), sizeof(size_t))) {
                    a.currentJob.resize(jobLen);
                    inFile.read(&a.currentJob[0], jobLen);
                } else {
                    a.currentJob = "Lumberjack";
                }

                inFile.read(reinterpret_cast<char*>(&a.isGoingToWork), sizeof(bool));
                inFile.read(reinterpret_cast<char*>(&a.isWorking), sizeof(bool));
                inFile.read(reinterpret_cast<char*>(&a.isReturningHome), sizeof(bool));
                inFile.read(reinterpret_cast<char*>(&a.workTarget), sizeof(Point));

                size_t invSize = 0;
                if (inFile.read(reinterpret_cast<char*>(&invSize), sizeof(size_t))) {
                    for (size_t j = 0; j < invSize; ++j) {
                        size_t itemLen = 0;
                        inFile.read(reinterpret_cast<char*>(&itemLen), sizeof(size_t));
                        std::string itemName(itemLen, '\0');
                        inFile.read(&itemName[0], itemLen);
                        int itemCount = 0;
                        inFile.read(reinterpret_cast<char*>(&itemCount), sizeof(int));
                        a.inventory[itemName] = itemCount;
                    }
                }
                
                a.waitDuration = (std::rand() % 400) / 100.0f + 2.0f;
                a.waitTimer = 0.0f;
                
                if (a.hasHouse) {
                    agentHouses[a.name] = a.housePos;
                    grid[a.housePos.y][a.housePos.x].type = OBSTACLE;
                    grid[a.housePos.y][a.housePos.x].r = 139;
                    grid[a.housePos.y][a.housePos.x].g = 69;
                    grid[a.housePos.y][a.housePos.x].b = 19;
                }

                npcs.push_back(a);
            }
            inFile.close();
            SDL_Log("NPCs loaded from %s", path.c_str());
            return true;
        }
        inFile.close();
    }
    return false;
}
