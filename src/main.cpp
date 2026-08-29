#include <raylib.h>

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "LAST STAND");
    SetTargetFPS(0);  // vsync governs; never cap with a sleep

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(Color{12, 10, 10, 255});
        DrawText("LAST STAND", 40, 40, 40, Color{220, 235, 255, 255});
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
