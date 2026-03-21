#pragma once
#include <string>
#include <vector>
#include <map>

struct ProduceItem {
    std::string name;
    int minAmount;
    int maxAmount;
};

struct TradeData {
    std::string name;
    std::vector<ProduceItem> produce;
};

inline const std::map<std::string, TradeData>& getTradeData() {
    static std::map<std::string, TradeData> trades = {
        {"Lumberjack", {"Lumberjack", {
            {"Stick", 1, 10},
            {"Wood", 1, 3},
        }}},
        {"Hunter", {"Hunter", {
            {"Raw Meat", 1, 5},
            {"Hide", 0, 1},
            {"Pelt", 0, 1},
            {"Bone", 0, 3},
        }}},
    };
    return trades;
}
