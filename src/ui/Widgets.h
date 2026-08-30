#pragma once
#include <cstdint>
#include <raylib.h>

namespace ls::ui {

// A small immediate-mode layer, hand-rolled per GDD 13.4: the player-facing
// screens are rects, lines, text and hit-testing, and a framework would only
// add a dependency and a programmer-art tell. Dear ImGui stays for developer
// tooling, when rlImGui finally tags a release.

// Keyboard-first focus. Every screen is a vertical list of items; the mouse
// is a shortcut, never the only way in. Hovering with the mouse MOVES focus,
// so the two input methods never disagree about what is selected.
struct Focus {
    int index = 0;
    int count = 0;

    void begin(int itemCount);
    void move(int delta);
    bool isFocused(int item) const { return item == index; }
};

// Sound hooks so widgets can be audible without ui/ knowing about audio/.
struct WidgetFeedback {
    bool moved    = false;   // focus changed this frame
    bool accepted = false;   // an item was activated
};

void beginFrame(WidgetFeedback& feedback);

// --- primitives ------------------------------------------------------------

// A framed panel with a title rule. The frame is the game's one piece of
// chrome, so it is drawn once here and never re-invented per screen.
void panel(Rectangle bounds, float alpha = 1.0f);
void panelTitled(Rectangle bounds, const char* title, float alpha = 1.0f);

void rule(float x, float y, float width, float alpha = 1.0f);
void text(const char* s, float x, float y, int size, Color color);
void textCentered(const char* s, float centerX, float y, int size, Color color);
void textRight(const char* s, float rightX, float y, int size, Color color);

// A labelled horizontal bar, used for base health and the damage breakdown.
void bar(Rectangle bounds, float fraction, Color fill, Color track);

// --- interactive -----------------------------------------------------------

// Returns true on the frame it is activated (ENTER/SPACE while focused, or a
// left click). `enabled` false draws it dim and never activates.
bool button(Rectangle bounds, const char* label, Focus& focus, int item,
            bool enabled = true);

// A row that shows a value and takes LEFT/RIGHT to change it. Returns true
// when the value changed. Clamped to [lo, hi], stepped by `step`.
bool slider(Rectangle bounds, const char* label, int& value, int lo, int hi,
            int step, Focus& focus, int item);

// A row that toggles on ENTER/SPACE/LEFT/RIGHT or a click.
bool toggle(Rectangle bounds, const char* label, bool& value, Focus& focus,
            int item);

// A list row for the upgrade tree: name, level, cost and description, drawn
// bright when affordable and dim-but-visible when not, because seeing what you
// cannot afford yet is the motivation (GDD 13.1).
bool treeRow(Rectangle bounds, const char* name, const char* levelText,
             const char* description, uint32_t cost, bool affordable,
             bool owned, Focus& focus, int item);

// True if the mouse is inside — screens use it for hover states the focus
// model does not cover.
bool hovered(Rectangle bounds);

}  // namespace ls::ui
