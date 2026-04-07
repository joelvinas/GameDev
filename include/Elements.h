#ifndef ELEMENTS_H
#define ELEMENTS_H

#include <vector>
#include <string>
#include <unordered_map>

struct Element {
    int id;
    std::string name;
    std::string symbol;
};

class ElementalSystem {
private:
    // A 2D array where matrix[attacker_id][defender_id] = multiplier
    // We use size 8 to map IDs 1-7 directly to indices
    float relationshipMatrix[8][8];
    std::unordered_map<int, Element> elementData;

public:
    void LoadElementsFromDb();
    float getMultiplier(int attackerId, int defenderId) const;
};

#endif