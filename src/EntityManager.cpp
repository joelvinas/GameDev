#include "EntityManager.h"
#include "Navigation.h"
#include "Constants.h"
#include <random>
#include <chrono>
#include <fstream>
#include <filesystem>
#include "sqlite3.h"

extern const char* SAVE_DATA_FILENAME;
extern std::string GetDBPath(const std::string& dbName);

static void check_sqlite_res(int res, char* errMsg) {
    if (res != SQLITE_OK) {
        SDL_Log("SQLite Error: %s", errMsg);
        if (errMsg) sqlite3_free(errMsg);
    }
}

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
                    //a.name = names[spawned];
                    a.gridPos = { nx, ny };
                    a.realPos = { nx * CELL_SIZE + CELL_SIZE / 2, ny * CELL_SIZE + CELL_SIZE / 2 };
                    a.waitDuration = std::uniform_real_distribution<float>(2.0f, 5.0f)(rng);
                    if (agentHouses.find(a.id) != agentHouses.end()) {
                        a.hasHouse = true;
                        a.housePos = agentHouses[a.id];
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
        std::vector<Point> p = findPath(a.gridPos, settlementPos, a.id);
        if (p.size() > 1) {
            a.currentPath = p;
            a.isMoving = true;
            a.currentPathIndex = 0;
            a.moveProgress = 0.0f;
        }
    }
}

void EntityManager::SaveNPCs() {
    sqlite3* db;
    char* errMsg = 0;
    std::string dbPath = GetDBPath(SAVE_DATA_FILENAME);
    int rc = sqlite3_open(dbPath.c_str(), &db);

    if (rc) {
        SDL_Log("Can't open database: %s", sqlite3_errmsg(db));
        return;
    }

    const char* createTablesSQL =
        "CREATE TABLE IF NOT EXISTS NPCs (id INT PRIMARY KEY, name TEXT, gridX INT, gridY INT, hasHouse INT, housePosX INT, housePosY INT, isBuildingHouse INT, plotPosX INT, plotPosY INT, currentJob TEXT, isGoingToWork INT, isWorking INT, isReturningHome INT, workTargetX INT, workTargetY INT);"
        "CREATE TABLE IF NOT EXISTS NPCInventory (npc_id INT, item_id INTEGER, itemCount INT);";

    rc = sqlite3_exec(db, createTablesSQL, 0, 0, &errMsg);
    check_sqlite_res(rc, errMsg);

    sqlite3_stmt* stmtNPC;
    const char* insertNPCSQL = "INSERT OR REPLACE INTO NPCs (id, name, gridX, gridY, hasHouse, housePosX, housePosY, isBuildingHouse, plotPosX, plotPosY, currentJob, isGoingToWork, isWorking, isReturningHome, workTargetX, workTargetY) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_prepare_v2(db, insertNPCSQL, -1, &stmtNPC, 0);

    sqlite3_stmt* stmtInv;
    const char* insertInvSQL = "INSERT OR REPLACE INTO NPCInventory (npc_id, item_id, itemCount) VALUES (?, ?, ?);";
    sqlite3_prepare_v2(db, insertInvSQL, -1, &stmtInv, 0);

    // Insert NPC data
    for (const auto& a : npcs) {
        sqlite3_bind_int(stmtNPC, 1, a.id);
        sqlite3_bind_text(stmtNPC, 2, a.name.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmtNPC, 3, a.gridPos.x);
        sqlite3_bind_int(stmtNPC, 4, a.gridPos.y);
        sqlite3_bind_int(stmtNPC, 5, a.hasHouse ? 1 : 0);
        sqlite3_bind_int(stmtNPC, 6, a.housePos.x);
        sqlite3_bind_int(stmtNPC, 7, a.housePos.y);
        sqlite3_bind_int(stmtNPC, 8, a.isBuildingHouse ? 1 : 0);
        sqlite3_bind_int(stmtNPC, 9, a.plotPos.x);
        sqlite3_bind_int(stmtNPC, 10, a.plotPos.y);
        sqlite3_bind_text(stmtNPC, 11, a.currentJob.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmtNPC, 12, a.isGoingToWork ? 1 : 0);
        sqlite3_bind_int(stmtNPC, 13, a.isWorking ? 1 : 0);
        sqlite3_bind_int(stmtNPC, 14, a.isReturningHome ? 1 : 0);
        sqlite3_bind_int(stmtNPC, 15, a.workTarget.x);
        sqlite3_bind_int(stmtNPC, 16, a.workTarget.y);

        sqlite3_step(stmtNPC);
        sqlite3_reset(stmtNPC);

        for (const auto& item : a.inventory) {
            sqlite3_bind_int(stmtInv, 1, a.id);
            sqlite3_bind_int(stmtInv, 2, item.first);
            sqlite3_bind_int(stmtInv, 3, item.second.quantity);
            
            sqlite3_step(stmtInv);
            sqlite3_reset(stmtInv);
        }
    }

    sqlite3_finalize(stmtNPC);
    sqlite3_finalize(stmtInv);
    sqlite3_close(db);
    SDL_Log("NPCs saved to SQLite database.");
}

bool EntityManager::LoadNPCs() {
    sqlite3* db;
    std::string dbPath = GetDBPath(SAVE_DATA_FILENAME);
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) return false;

    sqlite3_stmt* stmt;
    std::string selectSQL = "SELECT id, name, gridX, gridY, hasHouse, housePosX, housePosY, isBuildingHouse, plotPosX, plotPosY, currentJob, isGoingToWork, isWorking, isReturningHome, workTargetX, workTargetY FROM NPCs;";

    if (sqlite3_prepare_v2(db, selectSQL.c_str(), -1, &stmt, 0) == SQLITE_OK) {
        bool hasRecords = false;
        std::vector<Agent> loadedNPCs;
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            hasRecords = true;
            Agent a;
            a.id = sqlite3_column_int(stmt, 0);
            a.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            a.gridPos.x = sqlite3_column_int(stmt, 2);
            a.gridPos.y = sqlite3_column_int(stmt, 3);
            a.hasHouse = sqlite3_column_int(stmt, 4) != 0;
            a.housePos.x = sqlite3_column_int(stmt, 5);
            a.housePos.y = sqlite3_column_int(stmt, 6);
            a.isBuildingHouse = sqlite3_column_int(stmt, 7) != 0;
            a.plotPos.x = sqlite3_column_int(stmt, 8);
            a.plotPos.y = sqlite3_column_int(stmt, 9);
            
            const char* jobText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 10));
            a.currentJob = jobText ? jobText : "Lumberjack";
            
            a.isGoingToWork = sqlite3_column_int(stmt, 11) != 0;
            a.isWorking = sqlite3_column_int(stmt, 12) != 0;
            a.isReturningHome = sqlite3_column_int(stmt, 13) != 0;
            a.workTarget.x = sqlite3_column_int(stmt, 14);
            a.workTarget.y = sqlite3_column_int(stmt, 15);

            a.realPos = { a.gridPos.x * CELL_SIZE + CELL_SIZE / 2, a.gridPos.y * CELL_SIZE + CELL_SIZE / 2 };
            a.waitDuration = (std::rand() % 400) / 100.0f + 2.0f;
            a.waitTimer = 0.0f;
            
            if (a.hasHouse) {
                agentHouses[a.id] = a.housePos;
                grid[a.housePos.y][a.housePos.x].type = OBSTACLE;
                grid[a.housePos.y][a.housePos.x].r = 139;
                grid[a.housePos.y][a.housePos.x].g = 69;
                grid[a.housePos.y][a.housePos.x].b = 19;
            }

            loadedNPCs.push_back(a);
        }
        sqlite3_finalize(stmt);

        if (hasRecords) {
            // Load Inventories

            // 1. Attach the WorldData database to your existing SaveData connection
            std::string worldDbPath = GetDBPath("WorldData.db");
            std::string attachSql = "ATTACH DATABASE '" + worldDbPath + "' AS world;";
            if (sqlite3_exec(db, attachSql.c_str(), NULL, NULL, NULL) != SQLITE_OK) {
                SDL_Log("Could not attach WorldData.db: %s", sqlite3_errmsg(db));
            }

            // 2. Modify your SELECT to join with the Items table in the world schema
            std::string selectSQL = 
                "SELECT n.npc_id, n.item_id, n.itemCount, "
                "w.name, w.maxDurability, w.decayRate, w.weight, w.maxStack "
                "FROM NPCInventory n "
                "JOIN world.Items w ON n.item_id = w.id;";

            if (sqlite3_prepare_v2(db, selectSQL.c_str(), -1, &stmt, 0) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    int npcId = sqlite3_column_int(stmt, 0);
                    int itemId = sqlite3_column_int(stmt, 1);
                    int itemCount = sqlite3_column_int(stmt, 2);
                    const char* itemName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                    int maxDurability = sqlite3_column_int(stmt, 4);
                    int decayRate = sqlite3_column_int(stmt, 5);
                    int weight = sqlite3_column_int(stmt, 6);
                    int maxStack = sqlite3_column_int(stmt, 7);
                    
                    for (auto& a : loadedNPCs) {
                        if (a.id == npcId) {
                            ItemType type = {itemId, itemName, maxDurability, decayRate, weight, maxStack};
                            a.inventory[itemId] = InventoryItem(type, itemCount);
                            break;
                        }
                    }
                }
                sqlite3_finalize(stmt);
            }
            
            npcs = loadedNPCs;
            sqlite3_close(db);
            SDL_Log("NPCs loaded from SQLite database.");
            return true;
        }
    }

    sqlite3_close(db);
    return false;
}
