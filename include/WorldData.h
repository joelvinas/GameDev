#pragma once
#include <cmath>
#include <string>
#include <SDL3/SDL.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Time and Day/Night Cycle Helpers
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
float getLightLevel(float hour);
float getLightLevel(float hour, float latitude, int dayOfYear);

bool isDaytime(int hour);
bool isNighttime(int hour);

//     // 2. Calculate Solar Declination 
//     // This oscillates over the year. 0 at equinox, max at solstices.
//     float declination = maxTilt * sin(2.0f * M_PI * (dayOfYear - 80) / daysInYear);

//     // 3. The "Time" component (Your original sine wave)
//     // We use cos here because it aligns better with solar noon at 12:00
//     float hourAngle = (hour - 12.0f) * (M_PI / 12.0f);

//     // 4. The Solar Zenith Formula
//     // This combines the time of day, tilt, and position on the planet
//     float lightIntensity = (sin(latRad) * sin(declination)) + 
//                         (cos(latRad) * cos(declination) * cos(hourAngle));

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

// Standard simple time format
std::string getTimeOfDay(int hour);

// Advanced latitudinal time format (from TimeOfDay.cpp)
std::string GetTimeOfDay(float hour, float latitude, int dayOfYear);

std::string getTimeOfDay(int hour) {
    if (hour >= 4 && hour < 8) return "Early Morning";
    else if (hour >= 8 && hour < 12) return "Morning";
    else if (hour >= 12 && hour < 16) return "Afternoon";
    else if (hour >= 16 && hour < 20) return "Evening";
    else if (hour >= 20 && hour <= 23) return "Night";
    else if (hour >= 0 && hour < 4) return "Late Night";
    else return "Unknown";
}

// Enhanced cycle functions
float getNormalizedLightLevel(float hour);
SDL_Color GetLightTint(float normalizedLight);