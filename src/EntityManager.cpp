#include "EntityManager.h"
#include "Navigation.h"
#include "Constants.h"
#include <random>
#include <chrono>

std::vector<Agent> EntityManager::npcs;

void EntityManager::Initialize() {
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
