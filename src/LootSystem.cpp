#include "LootSystem.h"
#include <sqlite3.h>
#include <stdexcept>
#include <map>

LootTable LoadLootTableFromDB(const std::string& dbPath, int lootTableId) {
    sqlite3* db;
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        throw std::runtime_error("Failed to open database: " + std::string(sqlite3_errmsg(db)));
    }

    const char* sql = 
        "SELECT " 
        " lt.id, "
        " lt.name, "
        " le.item_id, "
        " i.name, "
        " i.maxDurability, "
        " i.decayRate, "
        " i.weight, "
        " i.maxStack, "
        " le.quantity, "
        " le.weight "
        "FROM LootTable lt "
        "JOIN LootEntries le ON lt.id = le.lootTable_id "
        "JOIN Items i ON le.item_id = i.id "
        "WHERE lt.id = ? "
        "ORDER BY le.item_id ASC, le.quantity ASC;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        throw std::runtime_error("Failed to prepare statement");
    }

    sqlite3_bind_int(stmt, 1, lootTableId);

    LootTable table{};
    bool foundAny = false;
    std::map<int, std::size_t> itemIndexById;

    while (true) {
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            foundAny = true;

            if (table.id == 0) {
                table.id = sqlite3_column_int(stmt, 0);
                table.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            }

            int itemId = sqlite3_column_int(stmt, 2);
            const char* itemNameText = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            int maxDurability = sqlite3_column_int(stmt, 4);
            int decayRate = sqlite3_column_int(stmt, 5);
            int itemWeight = sqlite3_column_int(stmt, 6);
            int maxStack = sqlite3_column_int(stmt, 7);

            int quantity = sqlite3_column_int(stmt, 8);
            int entryWeight = sqlite3_column_int(stmt, 9);

            auto it = itemIndexById.find(itemId);
            if (it == itemIndexById.end()) {
                ItemDistribution dist;
                dist.itemType = {
                    itemId,
                    (itemNameText != nullptr) ? itemNameText : "",
                    maxDurability,
                    decayRate,
                    itemWeight,
                    maxStack
                };
                dist.outcomes.push_back({quantity, entryWeight});

                table.itemDistributions.push_back(std::move(dist));
                itemIndexById[itemId] = table.itemDistributions.size() - 1;
            } else {
                table.itemDistributions[it->second].outcomes.push_back({quantity, entryWeight});
            }
        } else if (rc == SQLITE_DONE) {
            break;
        } else {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            throw std::runtime_error("Error executing statement");
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if(!foundAny) {
        throw std::runtime_error("No loot table found for id: " + std::to_string(lootTableId));
    }
    return table;
}

std::vector<std::pair<ItemType, int>> RollLootTable(const LootTable& table, std::mt19937& rng) {
    std::vector<std::pair<ItemType, int>> results;

    for (const ItemDistribution& dist : table.itemDistributions) {
        int totalWeight = 0;
        for (const LootOutcome& outcome : dist.outcomes) {
            totalWeight += outcome.weight;
        }

        if (totalWeight <= 0) continue;

        std::uniform_int_distribution<int> rollDist(1, totalWeight);
        int roll = rollDist(rng);

        int cumulative = 0;
        int rolledQuantity = 0;
        for (const LootOutcome& outcome : dist.outcomes) {
            cumulative += outcome.weight;
            if (roll <= cumulative) {
                rolledQuantity = outcome.quantity;
                break;
            }
        }

        if (rolledQuantity > 0) {
            results.push_back({dist.itemType, rolledQuantity});
        }
    }

    return results;
}
