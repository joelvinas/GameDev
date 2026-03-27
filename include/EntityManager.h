//Batches the spawn creation, 
// alerting, 
// and looping logic safely.

#ifndef ENTITYMANAGER_H
#define ENTITYMANAGER_H

#include <vector>
#include <SDL3/SDL.h>
#include "Agent.h"
#include "Constants.h"

class EntityManager {
public:
    static std::vector<Agent> npcs;
    static void Initialize();
    static void UpdateAll(float deltaTime);
    static void DrawAll(SDL_Renderer* renderer);
    static void AlertFoundSettlement(Point settlementPos);
};

#endif
