#include <iostream>
#include <string>
#include <random>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <map>
#include <set>
#include "JobData.h"
#include "WorldData.h"
//#include <sqlite3.h>

using namespace std;

// #ifndef M_PI
// #define M_PI 3.14159265358979323846
// #endif

// // Time and Day/Night Cycle Helpers
// float getLightLevel(int hour) {
//     return sin((hour - 6.0f) * M_PI / 12.0f);
// }

// const float axialTilt = 23.5f; // Degrees, adjust for your fictional planet
// const float default_latitude = 45.0f; // Degrees, adjust based on player location
// const int daysInYear = 365; 
// const int dayOfYear = 172; // Example: 172 for June 21st (summer solstice in northern hemisphere)

// float getLightLevel(float hour, float latitude, int dayOfYear) {
//     // 1. Constants (Adjust for your fictional planet)
//     const float maxTilt = axialTilt * (M_PI / 180.0f); // Tilt in radians
//     const float latRad = latitude * (M_PI / 180.0f);

//     // 2. Calculate Solar Declination 
//     // This oscillates over the year. 0 at equinox, max at solstices.
//     float declination = maxTilt * sin(2.0f * M_PI * (dayOfYear - 80) / daysInYear);

//     // 3. The "Time" component (Your original sine wave)
//     // We use cos here because it aligns better with solar noon at 12:00
//     float hourAngle = (hour - 12.0f) * (M_PI / 12.0f);

//     // 4. The Solar Zenith Formula
//     // This combines the time of day, tilt, and position on the planet
//     float lightIntensity = (sin(latRad) * sin(declination)) + 
//                            (cos(latRad) * cos(declination) * cos(hourAngle));

//     // Clamp between -1.0 (dead of night) and 1.0 (high noon)
//     // or 0.0 to 1.0 if you just want brightness.
//     return lightIntensity; 
// }

// bool isDaytime(int hour) {
//     return getLightLevel(hour) > 0.0f;
// }

// bool isNighttime(int hour) {
//     return getLightLevel(hour) < 0.0f;
// }

// std::string getTimeOfDay(int hour) {
//     if (hour >= 4 && hour < 8) return "Early Morning";
//     else if (hour >= 8 && hour < 12) return "Morning";
//     else if (hour >= 12 && hour < 16) return "Afternoon";
//     else if (hour >= 16 && hour < 20) return "Evening";
//     else if (hour >= 20 && hour <= 23) return "Night";
//     else if (hour >= 0 && hour < 4) return "Late Night";
//     else return "Unknown";
// }

static random_device rd;
// Use a fixed seed or truly random. We'll use rd() for random.
static mt19937 gen(rd());

float randomFloat(float min, float max) {
    uniform_real_distribution<float> dis(min, max);
    return dis(gen);
}

int randomInt(int min, int max) {
    uniform_int_distribution<int> dis(min, max);
    return dis(gen);
}

// The High-Level Categories
enum class StateCategory {
    WakingUp,
    Eating,
    Commuting,
    Working,
    Relaxing,
    Sleeping
};

enum class MealType { Breakfast, Lunch, Dinner, Meal };

enum class RecreationType { Socialize, Read, Exercise, Gamble, Write, Study, Craft };

MealType getMealType(int hour) {
    if (hour >= 4 && hour < 9) return MealType::Breakfast;
    else if (hour >= 10 && hour < 15) return MealType::Lunch;
    else if (hour >= 16 && hour < 21) return MealType::Dinner;
    else return MealType::Meal;
}

enum class Destination { Work, Home };

// The Container for the Agent's current status
struct AgentState {
    StateCategory category;
    
    // Sub-state data (only used when relevant)
    union {
        MealType currentMeal;
        struct {
            Destination origin;
            Destination destination;
        } commuteInfo;
    };
};

std::string stateToString(StateCategory agentCat, AgentState agentState) {
    switch (agentCat) {
        
        case StateCategory::WakingUp: return "Waking Up";
        case StateCategory::Sleeping: return "Sleeping";
        case StateCategory::Relaxing: return "Relaxing";

        case StateCategory::Eating:
            switch (agentState.currentMeal) {
                case MealType::Breakfast: return "Breakfast";
                case MealType::Lunch: return "Lunch";
                case MealType::Dinner: return "Dinner";
                case MealType::Meal: return "Meal";
                default: return "a snack";
            }
        case StateCategory::Commuting:
            if (agentState.commuteInfo.origin == Destination::Home && agentState.commuteInfo.destination == Destination::Work) {
                return "Commuting to Work";
            } else if (agentState.commuteInfo.origin == Destination::Work && agentState.commuteInfo.destination == Destination::Home) {
                return "Commuting Home";
            } else {
                return "Commuting";
            }
        case StateCategory::Working: return "Working";
    }
    return "Unknown";
}

enum class skills { Gatherer, Defender, Crafter, Trader };

struct Skill {
    int id;
    std::string name;

    // The 'set' needs this to sort and find unique items
    bool operator<(const Skill& other) const {
        return id < other.id;
    }
};


class Agent {
public:
    std::string name;
    std::string jobRole;

    float tiredness;
    float stomach;
    float foodInventory;
    float workSatisfaction;

    // Thresholds
    float wakeThreshold; // Triggers wake up
    float sleepDuration; 
    float stomachMax;     
    float stomachThreshold;
    float workSatisfactionThreshold;
    int commuteDistance;
    int commuteDistanceToWork;
    int commuteDistanceToHome;

    AgentState currentState;
    AgentState previousState;
    int commuteProgress;
    bool requiresDaylight;

    // A dynamic list of skills
    std::set<Skill> knownSkills;

    void learnRole(Skill newSkill) {
        knownSkills.insert(newSkill);
    }

    bool hasRole(Skill skill) const {
        return knownSkills.find(skill) != knownSkills.end();
    }

    
    // Status

    std::map<string, int> currentTripInventory;
    std::map<string, int> grandTotalInventory;
    
    void HandleSleeping() {
        // Handle sleep logic
        // 1. Update Vitals
        tiredness += (wakeThreshold / sleepDuration);
        stomach -= 0.5f; // Sleep makes them slightly hungry over time

        // 2. Calculate and Display Progress
        double restedPct = (tiredness / wakeThreshold) * 100;
        std::cout << "Sleeping... (Restfulness: " << std::min(100, (int)round(restedPct)) << "%)";

        // 3. Transition Logic
        if (tiredness >= wakeThreshold) {
            tiredness = wakeThreshold;
            currentState.category = StateCategory::WakingUp;

            workSatisfaction = 0.0f; // Reset work satisfaction for a new day
            std::cout << " -> Waking up fully rested!";
        }
        
        std::cout << "\n";

    };
    
    void HandleEating(int currentHour) {
        // This function is now simplified since the main tick loop handles the actual eating logic
        // It can be used for any additional effects or checks related to eating if needed in the future
                      
        previousState = currentState;   // Store the state we were in before eating, so we can return to it after the meal is done
        currentState.category = StateCategory::Eating;
        MealType mealType = getMealType(currentHour);
        currentState.currentMeal = mealType;
        if (previousState.category == StateCategory::WakingUp) {
            cout << "Woke up hungry. Time to eat " << stateToString(currentState.category, currentState) << "... ";
        } else if (previousState.category == StateCategory::Working) {
            cout << "\n";
            cout << "Got hungry while working. Time to eat " << stateToString(currentState.category, currentState) << "... ";
        } else {
            cout << "Eating " << stateToString(currentState.category, currentState) << "... ";
        }

        foodInventory -= 1.0f;  // Consumes 1 unit of food per meal
        tiredness -= 0.5f; // Eating is still an action, albeit less tiring than working
        stomach = stomachMax;
        cout << "Stomach is full at " << stomachMax << ". Food stores left: " << foodInventory << "\n";
        currentState = previousState; // Return to previous state after eating
        return;
    }

    void HandleCommuting() {
        // This function can be used for any additional effects or checks related to commuting if needed in the future
        tiredness -= 1.0f;
        stomach -= 0.2f; 
        commuteProgress++;
        if (currentState.commuteInfo.origin == Destination::Home && currentState.commuteInfo.destination == Destination::Work) {
            cout << "Commuting to work (" << commuteProgress << "/" << commuteDistanceToWork << ")...\n";

            if (commuteProgress >= commuteDistanceToWork) {
                currentState.category = StateCategory::Working;
                currentState.commuteInfo.origin = Destination::Work;
                currentState.commuteInfo.destination = Destination::Work; // Clear commute info since we're now at work
                cout << "Arrived at work!\n";
            }
        } 
        else if (currentState.commuteInfo.origin == Destination::Work && currentState.commuteInfo.destination == Destination::Home) {
            cout << "Commuting home (" << commuteProgress << "/" << commuteDistanceToHome << ")...\n";

            if (commuteProgress >= commuteDistanceToHome) {
                currentState.commuteInfo.origin = Destination::Home;
                currentState.commuteInfo.destination = Destination::Home; // Clear commute info since we're now at home

                currentState.category = StateCategory::Relaxing;
                cout << "Arrived at home!";

                // Drop off any gathered resources from the trip
                if (!currentTripInventory.empty()) {
                cout << " Dropped off: ";
                bool first = true;
                for (const auto& kv : currentTripInventory) {
                    grandTotalInventory[kv.first] += kv.second;
                    if (!first) cout << ", ";
                    cout << kv.second << " " << kv.first;
                    first = false;
                }
                cout << "\n";
                currentTripInventory.clear();
            }
            }
        }
        else {
            cout << "\n";

        }  
    }

    Agent(string name) : name(name) {
        vector<string> roles = {"Lumberjack", "Hunter"};
        if (jobRole.empty()) {
                jobRole = roles[randomInt(0, 1)];
        }

        // Gathering trades generally require daylight for now
        requiresDaylight = true;
        
        sleepDuration = randomFloat(4.0f, 12.0f);
        wakeThreshold = 24.0f - sleepDuration; 
        
        stomachThreshold = randomFloat(0.5f, 2.0f);
        // Stomach full lasts exactly 4 hours of intense work
        stomachMax = stomachThreshold + 4.0f; 
        
        workSatisfactionThreshold = randomFloat(6.0f, 10.0f);
        
        commuteDistance = randomInt(1, 3);
        commuteDistanceToWork = commuteDistance;
        commuteDistanceToHome = commuteDistance;
        
        foodInventory = 100.0f;
        
        // Initial state
        tiredness = wakeThreshold;
        stomach = 0.0f; // Initially very hungry to trigger Breakfast
        workSatisfaction = 0.0f;
        
        currentState = AgentState{StateCategory::WakingUp};
        previousState = AgentState{StateCategory::Sleeping};

        commuteProgress = 0;
        
        cout << "Initialized NPC: " << name << " (" << jobRole << ")\n";
        cout << "  Wakefulness Threshold:  " << fixed << setprecision(1) << wakeThreshold << " (Sleeps ~" << sleepDuration << "h)\n";
        cout << "  Stomach Threshold:       " << stomachThreshold << " (Max: " << stomachMax << ")\n";
        cout << "  Satisfaction Threshold: " << workSatisfactionThreshold << " hours of work\n";
        cout << "  Commute Distance (Out/In):  " << commuteDistanceToWork << "h / " << commuteDistanceToHome << "h\n\n";
    }
    
    void tick(int currentHour, int currentDay) {
        cout << "[Day " << currentDay << " | Hour " << setfill('0') << setw(2) << currentHour << "] " << name << " | ";

process_state:
        // 1. Process Sleeping State
        if (currentState.category == StateCategory::Sleeping) {
            HandleSleeping();
            return;
        }
        
        // 2. Process Exhaustion (Any awake state can lead to exhaustion)
        if (tiredness <= 0.0f) {
            if(currentState.commuteInfo.origin == Destination::Home) {
                currentState.category = StateCategory::Sleeping;
                tiredness = 0.0f;
                cout << "Exhausted! Going to sleep... ";    
                goto process_state;            
            } else if(currentState.commuteInfo.origin == Destination::Work && currentState.category == StateCategory::Working) {
                currentState.commuteInfo.destination = Destination::Home; // Change destination to home
                currentState.category = StateCategory::Commuting;
                commuteProgress = 0; // Reset commute progress to start heading home    
                cout << "Exhausted! Heading home to sleep... ";
                goto process_state;
            } else {
                cout << "Exhausted but already commuting home. Can't sleep until we get there... ";
                 // HandleCommuting(); // Process the commute hour as normal, which will eventually lead to sleeping when they arrive home
            }
        }

        // 3. Process Eating State (1 hour duration)
        if (currentState.category == StateCategory::Eating) {
            HandleEating(currentHour);
            goto process_state;
        }
        
        // 4. State Machine logic for Awake Agent
        switch (currentState.category) {
            case StateCategory::WakingUp:
                if (stomach < stomachThreshold) {
                    HandleEating(currentHour);
                    goto process_state;
                } else if (requiresDaylight && !isDaytime(currentHour)) {
                    cout << "It's still dark out. Relaxing at home until dawn...\n";
                    tiredness -= 1.0f;
                    stomach -= 0.2f;
                } else {
                    currentState.category = StateCategory::Commuting;
                    currentState.commuteInfo.origin = Destination::Home;
                    currentState.commuteInfo.destination = Destination::Work;   
                    commuteProgress = 0;
                    cout << "Not hungry. Heading out to work. ";
                    goto process_state;
                }
                break;
                
            case StateCategory::Commuting:
                HandleCommuting();
                break;      
                
            case StateCategory::Working:
                if(currentState.commuteInfo.origin == Destination::Work) {
                    // Work consumes 1 tiredness and 1 stomach per hour
                    tiredness -= 1.0f;
                    stomach -= 1.0f; 

                    cout << "Working hard as a " << jobRole << " | ";

                    // Simulate gathering resources based on job role and skills
                    {
                        const auto& trades = getTradeData();
                        auto it = trades.find(jobRole);
                        std::string producedStr = "";
                        if (it != trades.end()) {
                            for (const auto& prod : it->second.produce) {
                                int amt = randomInt(prod.minAmount, prod.maxAmount);
                                if (amt > 0) {
                                    currentTripInventory[prod.name] += amt;
                                    if (!producedStr.empty()) producedStr += ", ";
                                    producedStr += std::to_string(amt) + " " + prod.name;
                                }
                            }
                        }
                        if (!producedStr.empty()) {
                            cout << "Gathered: " << producedStr << " | ";
                        }
                    }                    

                    workSatisfaction += 1.0f;
                    cout << "Satisfaction: " 
                        << setprecision(1) << workSatisfaction << "/" << workSatisfactionThreshold 
                        << " | Stomach: " << stomach << " | Tiredness: " << tiredness;              
                }

                if (requiresDaylight && !isDaytime(currentHour)) {
                    cout << "\n";
                    cout << "It's too dark to continue gathering. Heading home for the day. ";
                    currentState.category = StateCategory::Commuting;
                    currentState.commuteInfo.origin = Destination::Work;
                    currentState.commuteInfo.destination = Destination::Home;
                    commuteProgress = 0;
                    goto process_state;
                }
                if (stomach < stomachThreshold) {
                    HandleEating(currentHour);
                    goto process_state;
                }                
                // Check if it's time to go home based on satisfaction     
                if (workSatisfaction >= workSatisfactionThreshold) {
                    previousState = currentState;
                    currentState.category = StateCategory::Commuting;
                    currentState.commuteInfo.origin = Destination::Work;
                    currentState.commuteInfo.destination = Destination::Home;
                    commuteProgress = 0;
                    cout << "\n";
                    cout << "Satisfied with today's work! Heading home.";
                }
                cout << "\n";
                break;
                
            case StateCategory::Relaxing:
                tiredness -= 1.0f;
                stomach -= 0.2f;
                cout << "Relaxing at home. Unwinding from the day. (Tiredness: " << tiredness << ")\n";
                break;
                
            default:
                break;
        }
    }
};

int main() {
    // 1. Ensure /AgentLog directory exists
    std::filesystem::create_directory("AgentLog");
    
    // 2. Generate timestamp (YY-MM-DD-HH-mm-ss)
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_struct;
#ifdef _WIN32
    localtime_s(&tm_struct, &t);
#else
    localtime_r(&t, &tm_struct);
#endif
    
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y%m%d-%H%M%S", &tm_struct);
    std::string filepath = "AgentLog/" + std::string(buffer) + "_AgentLog.txt";
    
    // 3. Open file and redirect stdout
    std::ofstream logFile(filepath);
    std::streambuf* originalCoutBuffer = std::cout.rdbuf();
    if (logFile) {
        std::cout.rdbuf(logFile.rdbuf());
    } else {
        std::cerr << "Failed to create log file: " << filepath << "\n";
    }

    cout << "Starting 'A Day in the Life' Simulation...\n";
    cout << "==========================================\n\n";
    
    // Initialize exactly NPC Agents
    std::vector<Agent> npcAgents;
    npcAgents.emplace_back("Bob");
    npcAgents.emplace_back("Alice");
    npcAgents.emplace_back("Charlie");
    npcAgents.emplace_back("Diana");

    npcAgents[0].jobRole = "Lumberjack"; // Force specific roles for testing
    npcAgents[1].jobRole = "Hunter";    
    npcAgents[2].jobRole = "Lumberjack";
    npcAgents[3].jobRole = "Hunter";
   
    // Simulate exactly 7 cycles (days)
    int cycles = 7;
    int totalSimulationHours = cycles * 24;
    
    int currentDay = 1;
    int currentHour = 6; // 0 to 23
    
    for (int t = 0; t < totalSimulationHours; t++) {
        
        for (auto& agent : npcAgents) {
            agent.tick(currentHour, currentDay);
        }
        
        currentHour++;
        if (currentHour >= 24) {
            currentHour = 0;
            currentDay++;
            cout << "\n--- End of Day " << currentDay - 1 << " ---\n\n";
        }
    }
    
    cout << "\nSimulation Complete. 7 Cycles processed.\n";
    cout << "--- 7-Day Grand Total Gathered ---\n";

    for (const auto& agent : npcAgents) {
        cout << agent.name << " (" << agent.jobRole << "):\n";
        for (const auto& kv : agent.grandTotalInventory) {
            cout << " " << kv.first << ": " << kv.second << "\n";
        }
    } 

    // for (const auto& kv : npc1.grandTotalInventory) {
    //     cout << "  " << kv.first << ": " << kv.second << "\n";
    // }
    // for (const auto& kv : npc2.grandTotalInventory) {
    //     cout << "  " << kv.first << ": " << kv.second << "\n";
    // }
    cout << "----------------------------------\n";
    
    // Restore original cout buffer
    if (logFile) {
        std::cout.rdbuf(originalCoutBuffer);
    }
    
    return 0;
}