#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include <string>
#include <random>
#include <algorithm>
#include <iomanip>

using namespace std;

// Random number generator utilities
static random_device rd;
static mt19937 gen(rd());

float randomFloat(float min, float max) {
    uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

float myClamp(float val, float min_val, float max_val) {
    return std::max(min_val, std::min(val, max_val));
}

// 1. Core Architecture
enum class JobRole { Gatherer, Crafter, Quartermaster, Guard };
enum class AIState { Idle, Gathering, Crafting, Distributing, Patrolling };

string roleToString(JobRole role) {
    switch(role) {
        case JobRole::Gatherer: return "Gatherer";
        case JobRole::Crafter: return "Crafter";
        case JobRole::Quartermaster: return "Quartermaster";
        case JobRole::Guard: return "Guard";
        default: return "Unknown";
    }
}

string stateToString(AIState state) {
    switch(state) {
        case AIState::Idle: return "Idle";
        case AIState::Gathering: return "Gathering";
        case AIState::Crafting: return "Crafting";
        case AIState::Distributing: return "Distributing";
        case AIState::Patrolling: return "Patrolling";
        default: return "Unknown";
    }
}

struct Culture {
    float aggression;    // 0.0 to 1.0
    float innovation;    // 0.0 to 1.0
    float collectivism;  // 0.0 to 1.0
};

struct Resources {
    float wood = 0;
    float ore = 0;
    float stone = 0;
    float food = 0;
};

// 2. The Quartermaster & Economy
class Tool {
public:
    float efficiency;
    int durability;
    int complexity;

    Tool(float eff, int dur, int comp) : efficiency(eff), durability(dur), complexity(comp) {}

    // Tool mechanics: break at 0 durability
    bool isBroken() const {
        return durability <= 0;
    }

    void use() {
        if (durability > 0) {
            durability--;
        }
    }
    
    float getEffectiveMultiplier() const {
        return isBroken() ? 1.0f : efficiency;
    }
};

class Faction;

class Member {
public:
    int id;
    Culture personalValues;
    JobRole jobRole;
    AIState currentState;
    shared_ptr<Tool> currentTool;

    Member(int id, Culture factionCulture, JobRole role) : id(id), jobRole(role), currentState(AIState::Idle), currentTool(nullptr) {
        // variance of +/- 10% from faction culture
        personalValues.aggression = myClamp(factionCulture.aggression + randomFloat(-0.1f, 0.1f), 0.0f, 1.0f);
        personalValues.innovation = myClamp(factionCulture.innovation + randomFloat(-0.1f, 0.1f), 0.0f, 1.0f);
        personalValues.collectivism = myClamp(factionCulture.collectivism + randomFloat(-0.1f, 0.1f), 0.0f, 1.0f);
    }

    void updateState(Faction& faction);
    void performAction(Faction& faction);
};

class Faction {
public:
    int id;
    Culture startingCulture;
    Culture dominantCulture;
    vector<Member> population;
    map<int, float> alignmentMatrix; // FactionID -> -10 to +10
    
    // Central villageStockpile
    Resources resources;
    vector<shared_ptr<Tool>> inventory; 
    
    int territorySize = 1;
    int housingLevel = 1;

    int nextMemberId = 1;

    Faction(int id, Culture startCulture) : id(id), startingCulture(startCulture), dominantCulture(startCulture) {
        // Initial resources to start
        resources.food = 50.0f;
        resources.wood = 50.0f;
        resources.stone = 50.0f;
        
        // Add one of each role to jumpstart the colony
        addMember(JobRole::Gatherer);
        addMember(JobRole::Gatherer);
        addMember(JobRole::Crafter);
        addMember(JobRole::Quartermaster);
        addMember(JobRole::Guard);
    }
    
    void addMember(JobRole role) {
        population.emplace_back(nextMemberId++, dominantCulture, role);
    }

    void tick() {
        // Calculate drift
        updateCulturalDrift();
        
        // Immigration logic
        if (resources.food > (population.size() * 2) && housingLevel > population.size()) {
            // New member joins!
            addMember(JobRole::Gatherer); // default as gatherer
        }
        
        // Faction-level Decision Making based on Innovation parameter
        // If innovation is high, prioritize housing (upgrading). If low, prioritize territory (expansion).
        if (dominantCulture.innovation > 0.5f) {
            if (resources.wood >= 20.0f && resources.stone >= 20.0f) {
                housingLevel++;
                resources.wood -= 20.0f;
                resources.stone -= 20.0f;
            }
        } else {
            if (resources.wood >= 30.0f && resources.food >= 10.0f) {
                territorySize++;
                resources.wood -= 30.0f;
                resources.food -= 10.0f;
            }
        }

        // Each member updates and performs
        for (auto& member : population) {
            member.updateState(*this);
            member.performAction(*this);
        }
        
        // Basic population consumption
        for (size_t i = 0; i < population.size(); ++i) {
            if (resources.food > 0) {
                resources.food -= 0.5f; // Eat
            }
        }
    }

    void updateCulturalDrift() {
        if (population.empty()) return;

        float sumAgg = 0, sumInn = 0, sumCol = 0;
        for (const auto& member : population) {
            sumAgg += member.personalValues.aggression;
            sumInn += member.personalValues.innovation;
            sumCol += member.personalValues.collectivism;
        }

        dominantCulture.aggression = sumAgg / population.size();
        dominantCulture.innovation = sumInn / population.size();
        dominantCulture.collectivism = sumCol / population.size();
    }

    float getAverageHappiness() const;
};

void Member::updateState(Faction& faction) {
    // Basic State Machine mapping based on JobRole
    switch (jobRole) {
        case JobRole::Gatherer:
            currentState = AIState::Gathering;
            break;
        case JobRole::Crafter:
            currentState = AIState::Crafting;
            break;
        case JobRole::Quartermaster:
            currentState = AIState::Distributing;
            break;
        case JobRole::Guard:
            currentState = AIState::Patrolling;
            break;
    }
}

void Member::performAction(Faction& faction) {
    float multiplier = 1.0f;
    if (currentTool) {
        multiplier = currentTool->getEffectiveMultiplier();
        currentTool->use(); // decreases durability
    }

    switch (currentState) {
        case AIState::Gathering:
            // Gatherers add to the central resources
            faction.resources.food += 2.0f * multiplier;
            faction.resources.wood += 1.0f * multiplier;
            faction.resources.stone += 0.5f * multiplier;
            faction.resources.ore += 0.2f * multiplier;
            break;

        case AIState::Crafting:
            // Crafters synthesize resources into Tools. 
            // Innovation allows crafting of tools with better stats using rarer resources.
            if (faction.resources.wood >= 5.0f && faction.resources.stone >= 5.0f) {
                faction.resources.wood -= 5.0f;
                faction.resources.stone -= 5.0f;
                
                float newEff = 1.5f;
                int newDur = 20;
                int newComp = 1;

                if (faction.dominantCulture.innovation > 0.6f && faction.resources.ore >= 2.0f) {
                    // Use ore for better tools
                    faction.resources.ore -= 2.0f;
                    newEff = 2.5f;
                    newDur = 50;
                    newComp = 2;
                }

                // Add to central villageStockpile
                faction.inventory.push_back(make_shared<Tool>(newEff, newDur, newComp));
            }
            break;

        case AIState::Distributing:
            // Quartermaster prioritizes tool distribution to workers with lowest efficiency or broken tools.
            if (!faction.inventory.empty()) {
                Member* targetMember = nullptr;
                float lowestEff = 9999.0f;

                for (auto& m : faction.population) {
                    // Gatherers and crafters need tools most
                    if (m.jobRole == JobRole::Gatherer || m.jobRole == JobRole::Crafter) {
                        float eff = m.currentTool ? m.currentTool->getEffectiveMultiplier() : 0.0f;
                        if (eff < lowestEff) {
                            lowestEff = eff;
                            targetMember = &m;
                        }
                    }
                }

                if (targetMember != nullptr) {
                    // Give the best tool available in inventory
                    // Sort inventory by efficiency descending
                    sort(faction.inventory.begin(), faction.inventory.end(), [](const shared_ptr<Tool>& a, const shared_ptr<Tool>& b) {
                        return a->efficiency > b->efficiency;
                    });
                    
                    targetMember->currentTool = faction.inventory.front();
                    faction.inventory.erase(faction.inventory.begin());
                }
            }
            break;

        case AIState::Patrolling:
            // Guards patrol (no resource change but maybe consumes extra food or prevents loss in a fuller simulation)
            break;
            
        case AIState::Idle:
            break;
    }
}

// 4. Reporting & Simulation Heartbeat
class SimulationTimer {
    int ticks = 0;
    vector<Faction> factions;

public:
    void addFaction(const Faction& f) {
        factions.push_back(f);
    }

    void tickAll() {
        for (auto& f : factions) {
            f.tick();
        }
        ticks++;

        // Trigger a console report every 100 ticks (one "Season")
        if (ticks % 100 == 0) {
            printSeasonReport();
        }
    }

    void printSeasonReport() {
        cout << "\n=========================================\n";
        cout << " SEASON REPORT - Tick: " << ticks << "\n";
        cout << "=========================================\n";

        for (auto& f : factions) {
            cout << "Faction ID: " << f.id << "\n";
            cout << "Population: " << f.population.size() << " | Happiness: " << f.getAverageHappiness() << " (Est)\n";
            cout << "Territory size: " << f.territorySize << " | Housing Level: " << f.housingLevel << "\n";
            
            cout << "--- Resources ---\n";
            cout << "Food:  " << fixed << setprecision(1) << f.resources.food << "  Wood: " << f.resources.wood << "\n";
            cout << "Stone: " << f.resources.stone << "  Ore:  " << f.resources.ore << "\n";

            int brokenTools = 0;
            int functionalTools = 0;
            for (const auto& m : f.population) {
                if (m.currentTool) {
                    if (m.currentTool->isBroken()) brokenTools++;
                    else functionalTools++;
                }
            }

            cout << "--- Economic Health ---\n";
            cout << "Assigned Working Tools: " << functionalTools << " | Assigned Broken Tools: " << brokenTools << "\n";
            cout << "Unassigned Tools in Stockpile: " << f.inventory.size() << "\n";
            
            cout << "--- Cultural Drift ---\n";
            cout << "Original Culture -> Avg Aggression: " << f.startingCulture.aggression 
                 << " Innovation: " << f.startingCulture.innovation 
                 << " Collectivism: " << f.startingCulture.collectivism << "\n";
            cout << "Current Culture  -> Avg Aggression: " << f.dominantCulture.aggression 
                 << " Innovation: " << f.dominantCulture.innovation 
                 << " Collectivism: " << f.dominantCulture.collectivism << "\n";
        }
        cout << "=========================================\n";
    }
    
    int getTicks() const { return ticks; }
};

// Implement average happiness (dummy calculation for now, based on food and housing)
float Faction::getAverageHappiness() const {
    float happiness = 50.0f; 
    if (resources.food > population.size()) happiness += 10.0f;
    else happiness -= 20.0f;
    
    if (housingLevel >= population.size()) happiness += 20.0f;
    return myClamp(happiness, 0.0f, 100.0f);
}

int main() {
    cout << "Initializing Society Simulation...\n";
    
    // Setup Faction with starting values
    Culture startCulture = {0.4f, 0.8f, 0.6f}; // High innovation
    Faction wolfClan(1, startCulture);
    
    // Setup Diplomacy / Alignment Matrix
    wolfClan.alignmentMatrix[2] = 0.0f; // Neutral to Faction 2

    // Simulation Engine
    SimulationTimer sim;
    sim.addFaction(wolfClan);

    // Run for a few seasons (300 ticks)
    cout << "Running 300 ticks (3 Seasons)...\n";
    for (int i = 0; i < 300; i++) {
        sim.tickAll();
    }
    
    cout << "Simulation Complete.\n";
    cout << "Press Enter to exit...";
    cin.get();
    return 0;
}