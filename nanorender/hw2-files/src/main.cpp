#include "MiniFB.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#ifndef HW2_ASSETS_DIR
#define HW2_ASSETS_DIR "assets"
#endif

// ---- Part 1: Mesh data structures ----

struct Vertex { float x, y, z; };
struct Face   { int v[3]; };  // 0-based vertex indices

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<Face>   faces;
};

// Parses a .obj file into vertices and triangular faces.
// Handles all face formats: "f v", "f v/vt", "f v//vn", "f v/vt/vn"
// by reading only the first (vertex) index from each token.
static bool load_obj(const char* path, Mesh& mesh) {
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "load_obj: cannot open '%s'\n", path);
        return false;
    }
    mesh.vertices.clear();
    mesh.faces.clear();

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 'v' && line[1] == ' ') {
            Vertex vtx;
            if (sscanf(line + 2, "%f %f %f", &vtx.x, &vtx.y, &vtx.z) == 3)
                mesh.vertices.push_back(vtx);

        } else if (line[0] == 'f' && line[1] == ' ') {
            // Walk the line token by token; read the leading integer of each
            // (the part before any '/'), convert from 1-based to 0-based.
            int indices[3], count = 0;
            const char* p = line + 2;
            while (count < 3 && *p) {
                while (*p == ' ' || *p == '\t') p++;
                if (*p == '\0' || *p == '\n') break;
                int idx;
                if (sscanf(p, "%d", &idx) == 1)
                    indices[count++] = idx - 1;
                while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
            }
            if (count == 3) {
                Face face;
                face.v[0] = indices[0];
                face.v[1] = indices[1];
                face.v[2] = indices[2];
                mesh.faces.push_back(face);
            }
        }
    }
    fclose(f);
    printf("load_obj: '%s' — %d vertices, %d faces\n",
           path, (int)mesh.vertices.size(), (int)mesh.faces.size());
    return true;
}

// ---- Part 2: Bounding-box normalization ----

struct BBox {
    float min_x, min_y, min_z;
    float max_x, max_y, max_z;
};

static BBox compute_bbox(const Mesh& mesh) {
    BBox bb = { 1e30f, 1e30f, 1e30f, -1e30f, -1e30f, -1e30f };
    for (const Vertex& v : mesh.vertices) {
        if (v.x < bb.min_x) bb.min_x = v.x;  if (v.x > bb.max_x) bb.max_x = v.x;
        if (v.y < bb.min_y) bb.min_y = v.y;  if (v.y > bb.max_y) bb.max_y = v.y;
        if (v.z < bb.min_z) bb.min_z = v.z;  if (v.z > bb.max_z) bb.max_z = v.z;
    }
    return bb;
}

// Projects a 3D point to 2D screen coordinates using:
//   1. Translate: move model center to origin
//   2. Uniform scale: fit largest XY extent into 80% of the smaller screen dimension
//   3. Translate: move to screen center; flip Y so model +Y points up
static void to_screen(float vx, float vy, const BBox& bb, float& sx, float& sy) {
    float cx = (bb.min_x + bb.max_x) * 0.5f;
    float cy = (bb.min_y + bb.max_y) * 0.5f;

    float extent = std::max(bb.max_x - bb.min_x, bb.max_y - bb.min_y);
    float fit    = std::min(WIDTH, HEIGHT) * 0.8f;
    float scale  = (extent > 0.0f) ? fit / extent : 1.0f;

    sx =  (vx - cx) * scale + WIDTH  * 0.5f;
    sy = -(vy - cy) * scale + HEIGHT * 0.5f;
}

// ---- Drawing ----

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

    // Part 1: Load mesh once at startup
    Mesh mesh;
    const char* obj_path = HW2_ASSETS_DIR "/test_pyramid.obj";
    bool mesh_loaded = load_obj(obj_path, mesh);

    // Extract just the filename for the GUI label
    const char* mesh_name = strrchr(obj_path, '/');
    mesh_name = mesh_name ? mesh_name + 1 : obj_path;

    // Part 2: compute bounding box once from the raw mesh
    BBox bbox = { 0,0,0,0,0,0 };
    if (mesh_loaded && !mesh.vertices.empty())
        bbox = compute_bbox(mesh);

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

        // Part 2: project every vertex to screen space using bbox normalization
        std::vector<int> sx(mesh.vertices.size()), sy(mesh.vertices.size());
        if (mesh_loaded) {
            for (int i = 0; i < (int)mesh.vertices.size(); i++) {
                float fx, fy;
                to_screen(mesh.vertices[i].x, mesh.vertices[i].y, bbox, fx, fy);
                sx[i] = (int)(fx + 0.5f);
                sy[i] = (int)(fy + 0.5f);
            }
        }

        // TODO Part 3: draw wireframe edges with draw_line() using sx/sy
        // TODO Part 4: Build Local/World transformation matrix GUI
        // TODO Part 5: Apply transformation matrices before projection
        // TODO Part 6: Keyboard/mouse input to modify transform state

        // 3. UI
        mu_begin(ctx);
        if (mu_begin_window(ctx, "HW2 - 3D Viewer", mu_rect(20, 20, 320, 240))) {
            int w[] = {-1};

            // Mesh name
            mu_layout_row(ctx, 1, w, 0);
            char name_buf[128];
            snprintf(name_buf, sizeof(name_buf), "Mesh: %s",
                     mesh_loaded ? mesh_name : "(failed to load)");
            mu_label(ctx, name_buf);

            // Vertex / face counts
            mu_layout_row(ctx, 1, w, 0);
            char info_buf[64];
            snprintf(info_buf, sizeof(info_buf), "Vertices: %d   Faces: %d",
                     (int)mesh.vertices.size(), (int)mesh.faces.size());
            mu_label(ctx, info_buf);

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
