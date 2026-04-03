#pragma once
#include <string>
#include <vector>
#include <random>
#include "ItemInfo.h"

struct LootOutcome {
    int quantity;
    int weight; // Probability weight
};

struct ItemDistribution {
    ItemType itemType;
    std::vector<LootOutcome> outcomes;
};

struct LootTable {
    int id;
    std::string name;
    std::vector<ItemDistribution> itemDistributions;
};

LootTable LoadLootTableFromDB(const std::string& dbPath, int lootTableId);
std::vector<std::pair<ItemType, int>> RollLootTable(const LootTable& table, std::mt19937& rng);
