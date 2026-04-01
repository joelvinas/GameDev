#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

#include <SDL3/SDL.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float getLightLevel(float hour) {
    return sin((hour - 6.0f) * M_PI / 12.0f);
}

const float axialTilt = 23.5f; // Degrees, adjust for your fictional planet
const float default_latitude = 45.0f; // Degrees, adjust based on player location
const int daysInYear = 365; 
const int dayOfYear = 172; // Example: 172 for June 21st (summer solstice in northern hemisphere)

float getLightLevel(float hour, float latitude, int dayOfYear) {
    // 1. Constants (Adjust for your fictional planet)
    const float maxTilt = axialTilt * (M_PI / 180.0f); // Tilt in radians
    const float latRad = latitude * (M_PI / 180.0f);

    // 2. Calculate Solar Declination 
    // This oscillates over the year. 0 at equinox, max at solstices.
    float declination = maxTilt * sin(2.0f * M_PI * (dayOfYear - 80) / daysInYear);

    // 3. The "Time" component (Your original sine wave)
    // We use cos here because it aligns better with solar noon at 12:00
    float hourAngle = (hour - 12.0f) * (M_PI / 12.0f);

    // 4. The Solar Zenith Formula
    // This combines the time of day, tilt, and position on the planet
    float lightIntensity = (sin(latRad) * sin(declination)) + 
                           (cos(latRad) * cos(declination) * cos(hourAngle));

    // Clamp between -1.0 (dead of night) and 1.0 (high noon)
    // or 0.0 to 1.0 if you just want brightness.
    return lightIntensity; 
}

bool isDaytime(int hour) {
    return getLightLevel(hour) > 0.0f;
}

bool isNighttime(int hour) {
    return getLightLevel(hour) < 0.0f;
}


std::string GetTimeOfDay(float hour, float latitude, int dayOfYear)
// This function determines the time of day category (Early Morning, Morning, Afternoon, Evening, Night, Late Night)
// Produced by ChatGPT with the following prompt:
//      You are an expert in C++ and game development. As a minor feature for the game, you'd like to dynamically return the "Time of Day" as a text value based on the luminosity function. 
//      There are 6 times of day in the game: Early Morning, Morning, Afternoon, Evening, Night, and Late Night. Ideally, these would correlate with fixed time stamps: 4-8, 8-12, 12-16, 16-20, 20-24/0, 0-4.
//      Based on the time of year and where the Actor is in the world (latitude), this "Time of Day" might not be relatively correct. The below table was built based on the Time of Day on the Equinox. 
//      Provide C++ logic to use the float hour and/or int dayOfYear to get one of the 6 "Time of Day" values. Note that the float getLightLevel(float hour, float latitude, int dayOfYear) function can be called to get the light level as a value between -1 and 1 based on the light available at a latitude and hour for a dayOfYear.""
{
    // configuration/tuning
    const float sampleResolution = 0.25f;     // hours per sample
    const float lightThreshold = 0.0f;        // crossing threshold for "day"
    const float twilightFraction = 0.15f;     // fraction of day length used as twilight padding
    
    // Build light samples across 0..24
    int samples = static_cast<int>(std::round(24.0f / sampleResolution));
    std::vector<float> times(samples);
    std::vector<float> lights(samples);
    for (int i = 0; i < samples; ++i) {
        float t = i * sampleResolution;
        times[i] = t;
        lights[i] = getLightLevel(t, latitude, dayOfYear);
    }
    
    // Find contiguous daytime intervals where light > threshold
    auto isDay = [&](int idx){ return lights[idx] > lightThreshold; };
    // find first sunrise crossing and last sunset crossing (handle wrap)
    int firstDayIdx = -1, lastDayIdx = -1;
    for (int i = 0; i < samples; ++i) {
        if (isDay(i)) { firstDayIdx = i; break; }
    }
    for (int i = samples - 1; i >= 0; --i) {
        if (isDay(i)) { lastDayIdx = i; break; }
    }
    
    bool allNight = (firstDayIdx == -1);
    bool allDay = false;
    if (!allNight) {
        // check if day covers whole circle (wrap-around)
        // if firstDayIdx == 0 and lastDayIdx == samples-1 and there's no zero crossing in middle
        bool wrap = true;
        for (int i = 0; i < samples; ++i) {
            if (!isDay(i)) { wrap = false; break; }
        }
        allDay = wrap;
    }
    
    // Handle polar cases
    if (allDay) {
        // Polar day: treat as long daytime; split into Early Morning/Morning/Afternoon/Evening with tiny night
        float normalizedHour = fmod(hour + 24.0f, 24.0f);
        // Divide 24h into four daytime-like segments plus tiny nights
        float seg = normalizedHour / 24.0f;
        if (seg < 0.125f) return "Late Night";         // tiny late night
        if (seg < 0.375f) return "Early Morning";
        if (seg < 0.625f) return "Afternoon";          // use Afternoon for midday region
        if (seg < 0.875f) return "Evening";
        return "Night";                                // tiny night
    }
    if (allNight) {
        // Polar night: always dark, map to Night/Late Night according to hour
        float h = fmod(hour + 24.0f, 24.0f);
        if (h >= 0.0f && h < 4.0f) return "Late Night";
        if (h < 8.0f) return "Night";
        if (h < 12.0f) return "Early Morning";
        if (h < 16.0f) return "Morning";
        if (h < 20.0f) return "Afternoon";
        return "Evening";
    }
    
    // Convert indices to times (hours)
    auto idxToHour = [&](int idx){ return times[idx]; };
    float sunrise = idxToHour(firstDayIdx);
    float sunset  = idxToHour(lastDayIdx);
    // handle wrap if sunrise > sunset (day wraps across midnight)
    float dayLength = 0.0f;
    if (sunrise <= sunset) dayLength = sunset - sunrise;
    else dayLength = (24.0f - sunrise) + sunset;
    
    // twilight padding derived from fraction of day length (clamped)
    float twilight = std::clamp(dayLength * twilightFraction, 0.5f, 3.0f); // 0.5..3h
    // define dawn start and end, dusk start and end
    auto addHours = [](float t, float delta){ float r = t + delta; while (r < 0) r += 24; while (r >= 24) r -= 24; return r; };
    float dawnStart = addHours(sunrise, -twilight);     // start of early-morning twilight
    float dawnEnd   = addHours(sunrise, twilight);      // end of dawn
    float duskStart = addHours(sunset, -twilight);
    float duskEnd   = addHours(sunset, twilight);
    
    // For daytime internal split: divide day (sunrise..sunset) into two halves: morning vs afternoon.
    // Further split morning into Early Morning (first half of morning half) and Morning (second half).
    // Similarly split night (sunset..sunrise next day) into Night and Late Night.
    auto inInterval = [&](float t, float a, float b) {
        if (a <= b) return t >= a && t < b;
        return t >= a || t < b; // wrap
    };
    
    float h = fmod(hour + 24.0f, 24.0f);
    
    // If within dawn twilight region treat as Early Morning
    if (inInterval(h, dawnStart, dawnEnd)) return "Early Morning";
    // If within dusk twilight region treat as Evening
    if (inInterval(h, duskStart, duskEnd)) return "Evening";
    
    // Daytime windows strictly between dawnEnd and duskStart
    // compute midpoint of daylight for morning/afternoon split
    float sunriseEffective = dawnEnd;
    float sunsetEffective  = duskStart;
    // compute effective day length
    float effectiveDayLen;
    if (inInterval(sunriseEffective, sunriseEffective, sunsetEffective))
        effectiveDayLen = (sunriseEffective <= sunsetEffective) ? (sunsetEffective - sunriseEffective) : (24.0f - sunriseEffective + sunsetEffective);
    else
        effectiveDayLen = dayLength; // fallback
    // midday point
    float midday = addHours(sunriseEffective, effectiveDayLen * 0.5f);
    
    // Early Morning (after dawnEnd until quarter of daytime)
    float morningQuarter = addHours(sunriseEffective, effectiveDayLen * 0.25f);
    if (inInterval(h, sunriseEffective, morningQuarter)) return "Early Morning";
    if (inInterval(h, morningQuarter, midday)) return "Morning";
    if (inInterval(h, midday, addHours(sunriseEffective, effectiveDayLen * 0.75f))) return "Afternoon";
    if (inInterval(h, addHours(sunriseEffective, effectiveDayLen * 0.75f), sunsetEffective)) return "Afternoon";
    
    // Night split: between duskEnd and dawnStart (wrap)
    // We'll split the night period length into two equal halves: first half = Night, second half = Late Night
    float nightStart = duskEnd;
    float nightEnd   = dawnStart;
    float nightLen;
    if (nightStart <= nightEnd) nightLen = nightEnd - nightStart;
    else nightLen = (24.0f - nightStart) + nightEnd;
    float nightMid = addHours(nightStart, nightLen * 0.5f);
    if (inInterval(h, nightStart, nightMid)) return "Night";
    return "Late Night";
}

float getNormalizedLightLevel(float hour) {
    float rawLight = getLightLevel(hour);
    float mapped = (rawLight + 1.0f) / 2.0f;
    return std::clamp(0.2f + 0.8f * mapped, 0.2f, 1.0f);
}

SDL_Color GetLightTint(float normalizedLight) {
    float t = std::clamp((normalizedLight - 0.2f) / 0.8f, 0.0f, 1.0f);
    Uint8 r = static_cast<Uint8>(51 + (255 - 51) * t);
    Uint8 g = static_cast<Uint8>(64 + (255 - 64) * t);
    Uint8 b = static_cast<Uint8>(127 + (255 - 127) * t);
    return {r, g, b, 255};
}
