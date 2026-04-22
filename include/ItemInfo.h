#pragma once
#include <string>
#include <vector>
#include <map>

struct ItemType {
    int id; // Unique identifier for the item type
    std::string name; // Name of the item type
    int maxDurability; // Maximum durability
    int decayRate; // Rate at which durability decreases
    int weight; // Weight of the item for inventory management
    int maxStack; // Maximum stack size for this item type
};

struct InventoryItem {
    ItemType itemType;
    int quantity;
    
    InventoryItem() : itemType{0, "Unknown", 0, 0, 0, 0}, quantity(0) {}
    InventoryItem(ItemType t, int q) : itemType(t), quantity(q) {}
};

struct Item {
    int id;
    std::string name; // Allows for custom names (e.g., "Sword of Doom") while typeID references the base item type (e.g., "Sword")
    int typeID; // Reference to ItemType
    int durability; // Current durability for this specific item instance
};

struct Weapon {
    int id;
    std::string name; // Custom name for the weapon
    int typeID; // Reference to ItemType (e.g., "Sword")
    int durability; // Current durability of the weapon
    int damage; // Base damage value for the weapon
    int delay; // Time delay between attacks in milliseconds
};

enum class ArmorSlot {
    Head,
    Chest,
    Shoulders,
    Arms,
    Hands,
    Legs,
    Feet
};

struct Armor {
    int id;
    std::string name; // Custom name for the armor
    ArmorSlot armorSlot; // Slot where the armor can be equipped (e.g., "Head")
    int typeID; // Reference to ItemType (e.g., "Helmet")
    int durability; // Current durability of the armor
    int defense; // Base defense value for the armor
};

struct Tool {
    int id;
    std::string name; // Custom name for the tool
    int typeID; // Reference to ItemType (e.g., "Pickaxe")
    int durability; // Current durability of the tool
    int efficiency; // Efficiency rating for resource gathering
};

struct Bag {
    int id;
    std::vector<Item> items;
    int capacity;
};

// inline const std::map<std::string, TradeData>& getTradeData() {
//     static std::map<std::string, TradeData> trades = {
//         {"Lumberjack", {"Lumberjack", {
//             {"Stick", 1, 10},
//             {"Wood", 1, 3},
//         }}},
//         {"Hunter", {"Hunter", {
//             {"Raw Meat", 1, 5},
//             {"Hide", 0, 1},
//             {"Pelt", 0, 1},
//             {"Bone", 0, 3},
//         }}},
//     };
//     return trades;
// }
