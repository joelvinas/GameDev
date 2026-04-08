#include "Constants.h"

std::vector<WorldObject> worldObjects;

WorldObject* GetObjectAt(int x, int y) {
    for (auto& obj : worldObjects) {
        if (obj.gridPos.x == x && obj.gridPos.y == y) {
            return &obj;
        }
    }
    return nullptr;
}
