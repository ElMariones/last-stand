#pragma once
#include <cstdint>

namespace ls {

// Player options. Stored as integer percentages rather than floats: the save
// is a fixed-width binary format, and integers keep it trivially portable and
// trivially diffable.
struct Settings {
    int  masterVolume     = 80;    // 0..100
    int  sfxVolume        = 90;    // 0..100
    int  musicVolume      = 55;    // 0..100
    int  shakeScale       = 100;   // 0..200 — some people want more, some none
    int  defaultTimeScale = 1;     // 1, 2 or 4
    bool hitstop          = true;
    bool damageNumbers    = true;
    bool levelOfDetail    = true;
    bool debugOverlay     = false;
    // Whether the first-run tutorial has been completed or skipped. It lives
    // with the settings rather than with progress because it is a property of
    // the player, not of the campaign - NEW GAME does not put it back.
    // Carried in a spare bit of the existing flag word, so adding it cost no
    // save version: an older save reads it as false, which is exactly right.
    bool tutorialDone     = false;

    // Display. Stored so the game reopens the way the player left it.
    int  windowWidth      = 1280;
    int  windowHeight     = 720;
    int  uiScale          = 100;   // 75..150, percent
    bool fullscreen       = false;

    float master() const { return static_cast<float>(masterVolume) * 0.01f; }
    float sfx() const { return static_cast<float>(sfxVolume) * 0.01f; }
    float music() const { return static_cast<float>(musicVolume) * 0.01f; }
    float shake() const { return static_cast<float>(shakeScale) * 0.01f; }
    float ui() const { return static_cast<float>(uiScale) * 0.01f; }
};

// Forces every field into its legal range. Applied on load, so a hand-edited
// or truncated save can never put the game into a state the UI cannot show.
void clampSettings(Settings& s);

// Packs the four booleans into one word for serialization.
uint32_t packSettingFlags(const Settings& s);
void     unpackSettingFlags(uint32_t bits, Settings& s);

// Lifetime totals across every run the player has ever made. Kept beside the
// settings because both are "things about this save that are not progress",
// and both need the same versioned round-trip.
struct Stats {
    uint32_t runs           = 0u;
    uint32_t victories      = 0u;
    uint32_t kills          = 0u;
    uint32_t scrapEarned    = 0u;
    uint32_t secondsPlayed  = 0u;
    uint32_t bestRunKills   = 0u;
    uint32_t turretsBought  = 0u;
    uint32_t nodesBought    = 0u;

    float winRate() const {
        return (runs == 0u) ? 0.0f
                            : static_cast<float>(victories) /
                                  static_cast<float>(runs);
    }
};

// The window sizes the options screen cycles through.
int  windowSizeCount();
void windowSizeAt(int index, int& width, int& height);
int  nearestWindowSize(int width, int height);

}  // namespace ls
