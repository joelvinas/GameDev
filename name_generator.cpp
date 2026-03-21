#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>

// Consider https://github.com/alxgiraud/fantasygen for more advanced name generation based on patterns in existing names
// Consider https://github.com/Tw1ddle/MarkovNameGenerator for more advanced name generation based on patterns in existing names

// Function to get a random element from a vector of strings
std::string getRandomSyllable(const std::vector<std::string>& syllables) {
    int index = std::rand() % syllables.size();
    return syllables[index];
}

int main() {
    // 1. Seed the random number generator
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    // 2. Define your syllable lists
    const std::vector<std::string> start_syllables = {"El", "Gor", "Tar", "Aethel", "Bor", "Cy"};
    const std::vector<std::string> mid_syllables = {"dor", "fin", "gal", "mir", "wen", "dan"};
    const std::vector<std::string> end_syllables = {"ion", "as", "dir", "os", "a", "yn"};

    // 3. Generate a name
    std::string generatedName = getRandomSyllable(start_syllables) +
                                getRandomSyllable(mid_syllables) +
                                getRandomSyllable(end_syllables);

    // 4. Output the name
    std::cout << "Generated Fantasy Name: " << generatedName << std::endl;

    return 0;
}
