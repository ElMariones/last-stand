#include "ui/Widgets.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "render/Theme.h"

namespace ls::ui {

namespace {

WidgetFeedback* g_feedback = nullptr;
float g_scale = 1.0f;

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

void setScale(float s) { g_scale = std::clamp(s, 0.5f, 3.0f); }
float scale() { return g_scale; }
int sz(int baseSize) {
    return std::max(8, static_cast<int>(static_cast<float>(baseSize) * g_scale));
}
float px(float baseLength) { return baseLength * g_scale; }

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
    text(title, bounds.x + px(theme::kGutter), bounds.y + px(theme::kUnit * 1.5f),
         sz(theme::kSmall), theme::withAlpha(theme::kColdDim, alpha));
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
                     static_cast<float>(sz(theme::kBody)) * 0.5f,
                 sz(theme::kBody), ink);
    return rowActivated(bounds, isFocused, enabled);
}

namespace {

bool sliderImpl(Rectangle bounds, const char* label, int& value, int lo, int hi,
                int step, Focus& focus, int item, bool showValue,
                bool disabled) {
    const bool isFocused = rowChrome(bounds, focus, item, !disabled);
    const Color ink = disabled ? theme::kInkFaint
                               : (isFocused ? theme::kInk : theme::kInkDim);

    const float padX = px(theme::kGutter);
    const float textY =
        bounds.y + bounds.height * 0.5f - static_cast<float>(sz(theme::kBody)) * 0.5f;
    text(label, bounds.x + padX, textY, sz(theme::kBody), ink);

    const float trackW = px(200.0f);
    const float trackX = bounds.x + bounds.width - padX - trackW - px(64.0f);
    const Rectangle track{trackX, bounds.y + bounds.height * 0.5f - 3.0f,
                          trackW, 6.0f};
    const float frac = (hi > lo) ? static_cast<float>(value - lo) /
                                       static_cast<float>(hi - lo)
                                 : 0.0f;
    bar(track, frac,
        disabled ? theme::kColdDeep
                 : (isFocused ? theme::kCold : theme::kColdDim),
        theme::kColdDeep);

    if (showValue) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", value);
        textRight(buf, bounds.x + bounds.width - padX, textY, sz(theme::kBody),
                  ink);
    }

    if (!isFocused || disabled) return false;

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

}  // namespace

bool slider(Rectangle bounds, const char* label, int& value, int lo, int hi,
            int step, Focus& focus, int item) {
    return sliderImpl(bounds, label, value, lo, hi, step, focus, item, true,
                      false);
}

bool sliderQuiet(Rectangle bounds, const char* label, int& value, int lo,
                 int hi, int step, Focus& focus, int item, bool disabled) {
    return sliderImpl(bounds, label, value, lo, hi, step, focus, item, false,
                      disabled);
}

bool toggle(Rectangle bounds, const char* label, bool& value, Focus& focus,
            int item) {
    const bool isFocused = rowChrome(bounds, focus, item, true);
    const Color ink = isFocused ? theme::kInk : theme::kInkDim;
    const float padX = px(theme::kGutter);
    const float textY =
        bounds.y + bounds.height * 0.5f - static_cast<float>(sz(theme::kBody)) * 0.5f;

    text(label, bounds.x + padX, textY, sz(theme::kBody), ink);
    textRight(value ? "ON" : "OFF", bounds.x + bounds.width - padX, textY,
              sz(theme::kBody), value ? theme::kCold : theme::kInkFaint);

    bool changed = false;
    if (isFocused && (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT))) {
        changed = true;
    }
    if (rowActivated(bounds, isFocused, true)) changed = true;
    if (changed) value = !value;
    return changed;
}

bool scrollbar(Rectangle track, Rectangle content, int& scroll, int visible,
               int total) {
    if (total <= visible) return false;
    const int maxScroll = total - visible;
    const int before = scroll;

    // The wheel is what most people reach for first, so it works over the
    // whole list rather than only over the bar.
    if (hovered(content)) {
        const float wheel = GetMouseWheelMove();
        if (wheel > 0.0f) --scroll;
        if (wheel < 0.0f) ++scroll;
    }

    DrawRectangleRec(track, theme::withAlpha(theme::kColdDeep, 0.5f));

    const float fraction =
        static_cast<float>(visible) / static_cast<float>(total);
    const float thumbH = std::max(px(24.0f), track.height * fraction);
    const float travel = track.height - thumbH;
    const float t = (maxScroll > 0)
                        ? static_cast<float>(scroll) / static_cast<float>(maxScroll)
                        : 0.0f;
    const Rectangle thumb{track.x, track.y + travel * t, track.width, thumbH};

    const bool over = hovered(track) || hovered(thumb);
    DrawRectangleRec(thumb, over ? theme::kCold : theme::kColdDim);

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && hovered(track) && travel > 0.0f) {
        const float grab =
            std::clamp((GetMousePosition().y - track.y - thumbH * 0.5f) / travel,
                       0.0f, 1.0f);
        scroll = static_cast<int>(std::lround(grab * static_cast<float>(maxScroll)));
    }

    scroll = std::clamp(scroll, 0, maxScroll);
    return scroll != before;
}

bool closeButton(Rectangle panelBounds) {
    const float size = px(28.0f);
    const Rectangle r{panelBounds.x + panelBounds.width - size - px(8.0f),
                      panelBounds.y + px(8.0f), size, size};
    const bool over = hovered(r);
    DrawRectangleLinesEx(r, 1.0f,
                         over ? theme::kInk : theme::withAlpha(theme::kColdDeep, 1.0f));
    const float inset = size * 0.3f;
    const Color ink = over ? theme::kInk : theme::kInkDim;
    DrawLineEx(Vector2{r.x + inset, r.y + inset},
               Vector2{r.x + r.width - inset, r.y + r.height - inset}, 1.6f, ink);
    DrawLineEx(Vector2{r.x + r.width - inset, r.y + inset},
               Vector2{r.x + inset, r.y + r.height - inset}, 1.6f, ink);
    if (over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        noteAccept();
        return true;
    }
    return false;
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

    const float padX = px(theme::kUnit * 2.0f);
    const float textY =
        bounds.y + bounds.height * 0.5f - static_cast<float>(sz(theme::kSmall)) * 0.5f;

    text(name, bounds.x + padX, textY, sz(theme::kSmall), nameInk);
    text(levelText, bounds.x + padX + px(180.0f), textY, sz(theme::kMicro),
         theme::kInkDim);
    text(description, bounds.x + padX + px(250.0f), textY, sz(theme::kMicro),
         affordable ? theme::kInkDim : theme::kInkFaint);

    if (owned) {
        textRight("OWNED", bounds.x + bounds.width - padX, textY,
                  sz(theme::kMicro), theme::kGood);
    } else {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%u", cost);
        textRight(buf, bounds.x + bounds.width - padX, textY, sz(theme::kSmall),
                  costInk);
    }

    return rowActivated(bounds, isFocused, true);
}

}  // namespace ls::ui
