#include "Elements.h"
#include "Constants.h"
#include "sqlite3.h"
#include <SDL3/SDL.h>

void ElementalSystem::LoadElementsFromDb() {
    sqlite3* db;
    std::string dbPath = GetDBPath(WORLD_DATA_FILENAME);
    int rc = sqlite3_open(dbPath.c_str(), &db);

    if (rc) {
        SDL_Log("Can't open database: %s", sqlite3_errmsg(db));
        return;
    }

    // Initialize matrix with 1.0f (Neutral damage)
    for(int i = 0; i < 8; i++) {
        for(int j = 0; j < 8; j++) {
            relationshipMatrix[i][j] = 1.0f;
        }
    }

    sqlite3_stmt* stmt;
    
    // 1. Fetch elements and populate elementData map
    // We query from the [Elements] table as discussed.
    const char* elementsSQL = "SELECT id, name FROM Elements";
    if (sqlite3_prepare_v2(db, elementsSQL, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Element e;
            e.id = sqlite3_column_int(stmt, 0);
            const unsigned char* nameText = sqlite3_column_text(stmt, 1);
            if (nameText) {
                e.name = reinterpret_cast<const char*>(nameText);
            }
            elementData[e.id] = e;
        }
    } else {
        SDL_Log("Failed to prepare statement for Elements: %s", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);

    // 2. Fetch relationships and populate matrix
    // We assume columns: attacker_id, defender_id, damage_multiplier on [Element_Balance] table
    const char* balanceSQL = "SELECT * FROM Element_Balance";
    if (sqlite3_prepare_v2(db, balanceSQL, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            // Adjust indices based on actual columns if different. Assuming 0, 1, 2.
            int attacker = sqlite3_column_int(stmt, 0);
            int defender = sqlite3_column_int(stmt, 1);
            float multiplier = (float)sqlite3_column_double(stmt, 2);
            
            if (attacker >= 0 && attacker < 8 && defender >= 0 && defender < 8) {
                relationshipMatrix[attacker][defender] = multiplier;
            }
        }
    } else {
        SDL_Log("Failed to prepare statement for Element_Balance: %s", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);

    sqlite3_close(db);
    SDL_Log("Successfully loaded Elements and Element Balance from WorldData.db");
}

float ElementalSystem::getMultiplier(int attackerId, int defenderId) const {
    if (attackerId >= 0 && attackerId < 8 && defenderId >= 0 && defenderId < 8) {
        return relationshipMatrix[attackerId][defenderId];
    }
    return 1.0f;
}
