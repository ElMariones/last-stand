#include "ui/Widgets.h"

#include <algorithm>
#include <cstdio>

#include "render/Theme.h"

namespace ls::ui {

namespace {

WidgetFeedback* g_feedback = nullptr;

void noteMove() {
    if (g_feedback != nullptr) g_feedback->moved = true;
}
void noteAccept() {
    if (g_feedback != nullptr) g_feedback->accepted = true;
}

bool pressedAccept() {
    return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
           IsKeyPressed(KEY_KP_ENTER);
}

}  // namespace

void Focus::begin(int itemCount) {
    count = itemCount;
    if (count <= 0) {
        index = 0;
        return;
    }
    index = ((index % count) + count) % count;
}

void Focus::move(int delta) {
    if (count <= 0) return;
    const int before = index;
    index = ((index + delta) % count + count) % count;
    if (index != before) noteMove();
}

void beginFrame(WidgetFeedback& feedback) {
    feedback = WidgetFeedback{};
    g_feedback = &feedback;
}

bool hovered(Rectangle bounds) {
    return CheckCollisionPointRec(GetMousePosition(), bounds);
}

void panel(Rectangle bounds, float alpha) {
    DrawRectangleRec(bounds, theme::withAlpha(Color{16, 15, 18, 232}, alpha));
    DrawRectangleLinesEx(bounds, 1.0f,
                         theme::withAlpha(theme::kColdDeep, alpha));
    // A short accent along the top-left corner: enough chrome to feel built,
    // not enough to become decoration.
    DrawLineEx(Vector2{bounds.x, bounds.y},
               Vector2{bounds.x + 46.0f, bounds.y}, 2.0f,
               theme::withAlpha(theme::kCold, alpha * 0.85f));
}

void panelTitled(Rectangle bounds, const char* title, float alpha) {
    panel(bounds, alpha);
    text(title, bounds.x + theme::kGutter, bounds.y + theme::kUnit * 1.5f,
         theme::kSmall, theme::withAlpha(theme::kColdDim, alpha));
    rule(bounds.x + theme::kGutter, bounds.y + theme::kUnit * 4.5f,
         bounds.width - theme::kGutter * 2.0f, alpha * 0.6f);
}

void rule(float x, float y, float width, float alpha) {
    DrawRectangleV(Vector2{x, y}, Vector2{width, 1.0f},
                   theme::withAlpha(theme::kColdDeep, alpha));
}

void text(const char* s, float x, float y, int size, Color color) {
    DrawText(s, static_cast<int>(x), static_cast<int>(y), size, color);
}

void textCentered(const char* s, float centerX, float y, int size,
                  Color color) {
    const int w = MeasureText(s, size);
    DrawText(s, static_cast<int>(centerX) - w / 2, static_cast<int>(y), size,
             color);
}

void textRight(const char* s, float rightX, float y, int size, Color color) {
    const int w = MeasureText(s, size);
    DrawText(s, static_cast<int>(rightX) - w, static_cast<int>(y), size, color);
}

void bar(Rectangle bounds, float fraction, Color fill, Color track) {
    const float f = std::clamp(fraction, 0.0f, 1.0f);
    DrawRectangleRec(bounds, track);
    DrawRectangleRec(Rectangle{bounds.x, bounds.y, bounds.width * f,
                               bounds.height},
                     fill);
}

namespace {

// Shared chrome for every focusable row: the focus bar on the left, the
// hover/focus wash, and the mouse-moves-focus rule.
bool rowChrome(Rectangle bounds, Focus& focus, int item, bool enabled) {
    const bool mouseOver = enabled && hovered(bounds);
    if (mouseOver && focus.index != item) {
        focus.index = item;
        noteMove();
    }
    const bool isFocused = focus.isFocused(item) && enabled;

    if (isFocused) {
        DrawRectangleRec(bounds, theme::withAlpha(theme::kCold, 0.10f));
        DrawRectangleV(Vector2{bounds.x, bounds.y},
                       Vector2{3.0f, bounds.height}, theme::kCold);
    }
    return isFocused;
}

bool rowActivated(Rectangle bounds, bool isFocused, bool enabled) {
    if (!enabled) return false;
    if (isFocused && pressedAccept()) {
        noteAccept();
        return true;
    }
    if (hovered(bounds) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        noteAccept();
        return true;
    }
    return false;
}

}  // namespace

bool button(Rectangle bounds, const char* label, Focus& focus, int item,
            bool enabled) {
    const bool isFocused = rowChrome(bounds, focus, item, enabled);
    const Color ink = !enabled ? theme::kInkFaint
                               : (isFocused ? theme::kInk : theme::kInkDim);
    textCentered(label, bounds.x + bounds.width * 0.5f,
                 bounds.y + bounds.height * 0.5f -
                     static_cast<float>(theme::kBody) * 0.5f,
                 theme::kBody, ink);
    return rowActivated(bounds, isFocused, enabled);
}

bool slider(Rectangle bounds, const char* label, int& value, int lo, int hi,
            int step, Focus& focus, int item) {
    const bool isFocused = rowChrome(bounds, focus, item, true);
    const Color ink = isFocused ? theme::kInk : theme::kInkDim;

    const float padX = theme::kGutter;
    const float textY =
        bounds.y + bounds.height * 0.5f - static_cast<float>(theme::kBody) * 0.5f;
    text(label, bounds.x + padX, textY, theme::kBody, ink);

    const float trackW = 220.0f;
    const float trackX = bounds.x + bounds.width - padX - trackW - 70.0f;
    const Rectangle track{trackX, bounds.y + bounds.height * 0.5f - 3.0f,
                          trackW, 6.0f};
    const float frac = (hi > lo) ? static_cast<float>(value - lo) /
                                       static_cast<float>(hi - lo)
                                 : 0.0f;
    bar(track, frac, isFocused ? theme::kCold : theme::kColdDim,
        theme::kColdDeep);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", value);
    textRight(buf, bounds.x + bounds.width - padX, textY, theme::kBody, ink);

    if (!isFocused) return false;

    int delta = 0;
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) delta = -step;
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) delta = step;
    // Dragging the track is the obvious mouse gesture; support it.
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && hovered(bounds)) {
        const float t = std::clamp(
            (GetMousePosition().x - track.x) / track.width, 0.0f, 1.0f);
        const int wanted =
            lo + static_cast<int>(t * static_cast<float>(hi - lo));
        if (wanted != value) {
            value = std::clamp(wanted, lo, hi);
            return true;
        }
    }
    if (delta == 0) return false;

    const int before = value;
    value = std::clamp(value + delta, lo, hi);
    if (value != before) noteMove();
    return value != before;
}

bool toggle(Rectangle bounds, const char* label, bool& value, Focus& focus,
            int item) {
    const bool isFocused = rowChrome(bounds, focus, item, true);
    const Color ink = isFocused ? theme::kInk : theme::kInkDim;
    const float padX = theme::kGutter;
    const float textY =
        bounds.y + bounds.height * 0.5f - static_cast<float>(theme::kBody) * 0.5f;

    text(label, bounds.x + padX, textY, theme::kBody, ink);
    textRight(value ? "ON" : "OFF", bounds.x + bounds.width - padX, textY,
              theme::kBody, value ? theme::kCold : theme::kInkFaint);

    bool changed = false;
    if (isFocused && (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT))) {
        changed = true;
    }
    if (rowActivated(bounds, isFocused, true)) changed = true;
    if (changed) value = !value;
    return changed;
}

bool treeRow(Rectangle bounds, const char* name, const char* levelText,
             const char* description, uint32_t cost, bool affordable,
             bool owned, Focus& focus, int item) {
    const bool isFocused = rowChrome(bounds, focus, item, true);

    // Affordable nodes glow; unaffordable ones stay dim but readable, because
    // seeing what you cannot afford yet is the motivation (GDD 13.1).
    const Color nameInk = owned ? theme::kGood
                                : (affordable ? theme::kInk : theme::kInkFaint);
    const Color costInk =
        owned ? theme::kInkFaint : (affordable ? theme::kScrap : theme::kInkFaint);

    const float padX = theme::kUnit * 2.0f;
    const float textY =
        bounds.y + bounds.height * 0.5f - static_cast<float>(theme::kSmall) * 0.5f;

    text(name, bounds.x + padX, textY, theme::kSmall, nameInk);
    text(levelText, bounds.x + padX + 190.0f, textY, theme::kMicro,
         theme::kInkDim);
    text(description, bounds.x + padX + 260.0f, textY, theme::kMicro,
         affordable ? theme::kInkDim : theme::kInkFaint);

    if (owned) {
        textRight("OWNED", bounds.x + bounds.width - padX, textY,
                  theme::kMicro, theme::kGood);
    } else {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%u", cost);
        textRight(buf, bounds.x + bounds.width - padX, textY, theme::kSmall,
                  costInk);
    }

    return rowActivated(bounds, isFocused, true);
}

}  // namespace ls::ui
