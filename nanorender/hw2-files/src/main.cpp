#include "MiniFB.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
extern "C" {
#include "microui.h"
}
#include "ui_bridge.h"
#include "ui_renderer.h"

#define WIDTH 1600
#define HEIGHT 1200

static uint32_t g_buffer[WIDTH * HEIGHT];

static void draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        if (x0 >= 0 && x0 < WIDTH && y0 >= 0 && y0 < HEIGHT)
            g_buffer[y0 * WIDTH + x0] = color;
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

int main() {
    // Part 0: GLM demo
    glm::vec3 v(1.0f, 2.0f, 3.0f);
    glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0, 1, 0));
    glm::vec4 result = rot * glm::vec4(v, 1.0f);
    printf("[GLM] Rotated (1,2,3) by 45 deg around Y: (%.3f, %.3f, %.3f)\n",
           result.x, result.y, result.z);

    struct mfb_window *window =
        mfb_open_ex("HW2 - Wireframe Viewer", WIDTH, HEIGHT, MFB_WF_RESIZABLE);
    if (!window) return 1;

    mu_Context *ctx = (mu_Context *)malloc(sizeof(mu_Context));
    mu_init(ctx);
    ctx->text_width  = [](mu_Font, const char *s, int l) { return (l < 0 ? (int)strlen(s) : l) * 8; };
    ctx->text_height = [](mu_Font) { return 8; };

    UIRenderer renderer(WIDTH, HEIGHT);

    mfb_set_char_input_callback(
        [](struct mfb_window *w, unsigned int c) {
            extern void ui_bridge_char_input(struct mfb_window *, unsigned int);
            ui_bridge_char_input(w, c);
        },
        window);

    while (mfb_update_events(window) != MFB_STATE_EXIT) {
        // 1. Input
        ui_bridge_input(ctx, window);

        // 2. Clear background
        for (int i = 0; i < WIDTH * HEIGHT; i++)
            g_buffer[i] = MFB_RGB(30, 30, 30);

        // TODO Part 1: Load .obj mesh (vertices + faces)
        // TODO Part 2: Normalize mesh to viewport via bounding box
        // TODO Part 3: Orthographic projection + wireframe with draw_line()
        // TODO Part 4: Build Local/World transformation matrix GUI
        // TODO Part 5: Apply transformation matrices before projection
        // TODO Part 6: Keyboard/mouse input to modify transform state

        // 3. UI
        mu_begin(ctx);
        if (mu_begin_window(ctx, "HW2 - 3D Viewer", mu_rect(20, 20, 320, 240))) {
            int w[] = {-1};
            mu_layout_row(ctx, 1, w, 0);
            mu_label(ctx, "Mesh: (none loaded)");
            mu_layout_row(ctx, 1, w, 0);
            mu_label(ctx, "Vertices: 0   Faces: 0");
            // TODO: add transformation sliders here (Parts 4-5)
            mu_end_window(ctx);
        }
        mu_end(ctx);

        // 4. UI Rendering
        renderer.render(ctx, g_buffer);

        // 5. Display
        if (mfb_update_ex(window, g_buffer, WIDTH, HEIGHT) < 0) break;
        mfb_wait_sync(window);
    }

    mfb_close(window);
    free(ctx);
    return 0;
}
