#include "MiniFB.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <math.h>
#include <vector>
#include <algorithm>
extern "C" {
#include "microui.h"
}
#include "ui_bridge.h"
#include "ui_renderer.h"

#define WIDTH 1600
#define HEIGHT 1200

static uint32_t g_buffer[WIDTH * HEIGHT];
static int g_visual_mode = 0;
static uint32_t g_frame_counter = 0;

// Part 5 State Variables: Custom application variables bound to UI controls
static float g_color_speed = 1.0f;
static float g_wave_frequency = 0.05f;
static int g_invert_colors = 0;
static float g_center_shift_x = 0.0f;
static float g_center_shift_y = 0.0f;

// Part 6 State Variables: Dynamic Interactive Line Drawing Canvas
struct Line {
  int x0, y0, x1, y1;
  uint32_t color;
  bool wu;  // Part 7.3: draw with Xiaolin Wu anti-aliasing when true
};

static std::vector<Line> g_lines;
static bool g_drawing_line = false;
static int g_line_start_x = 0;
static int g_line_start_y = 0;

static float g_line_r = 255.0f;
static float g_line_g = 255.0f;
static float g_line_b = 255.0f;

static int g_brush_mode = 0;
static int g_drawing_enabled = 1;
static int g_spirograph_active = 0;
static float g_spiro_R = 300.0f;
static float g_spiro_r = 180.0f;
static float g_spiro_p = 110.0f;
static float g_spiro_density = 400.0f;

// Part 7.1: Circle drawing state
struct Circle {
  int xc, yc, r;
  uint32_t color;
};
static std::vector<Circle> g_circles;
static int g_circle_mode = 0;
static bool g_drawing_circle = false;
static int g_circle_center_x = 0, g_circle_center_y = 0;

// Part 7.3: Anti-aliasing mode (0 = Bresenham, 1 = Xiaolin Wu)
static int g_use_wu = 0;

// ---- Drawing functions ----

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

// Part 7.1: Bresenham's circle — compute one octant and mirror to all eight via 8-way symmetry
static void draw_circle(int xc, int yc, int r, uint32_t color) {
  if (r <= 0) return;
  auto put = [color](int x, int y) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
      g_buffer[y * WIDTH + x] = color;
  };
  auto plot8 = [&](int x, int y) {
    put(xc+x, yc+y); put(xc-x, yc+y); put(xc+x, yc-y); put(xc-x, yc-y);
    put(xc+y, yc+x); put(xc-y, yc+x); put(xc+y, yc-x); put(xc-y, yc-x);
  };
  int x = 0, y = r, d = 3 - 2 * r;
  plot8(x, y);
  while (y >= x) {
    x++;
    if (d > 0) { y--; d += 4 * (x - y) + 10; }
    else        {      d += 4 * x + 6; }
    plot8(x, y);
  }
}

// Part 7.2: Naive float-based line drawing — uses slope y=mx+b instead of integer error
static void draw_line_naive(int x0, int y0, int x1, int y1, uint32_t color) {
  int adx = abs(x1 - x0), ady = abs(y1 - y0);
  if (adx >= ady) {
    if (x0 > x1) { std::swap(x0, x1); std::swap(y0, y1); }
    float m = (x1 != x0) ? (float)(y1 - y0) / (x1 - x0) : 0.0f;
    for (int x = x0; x <= x1; x++) {
      int y = (int)roundf(y0 + m * (x - x0));
      if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
        g_buffer[y * WIDTH + x] = color;
    }
  } else {
    if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
    float m = (y1 != y0) ? (float)(x1 - x0) / (y1 - y0) : 0.0f;
    for (int y = y0; y <= y1; y++) {
      int x = (int)roundf(x0 + m * (y - y0));
      if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
        g_buffer[y * WIDTH + x] = color;
    }
  }
}

// Part 7.2: Benchmark — draw 100k identical random lines with each algorithm and log timing
static void run_benchmark() {
  const int N = 100000;
  struct BL { int x0, y0, x1, y1; };
  std::vector<BL> lines(N);
  srand(12345);
  for (int i = 0; i < N; i++)
    lines[i] = { rand() % WIDTH, rand() % HEIGHT, rand() % WIDTH, rand() % HEIGHT };

  LARGE_INTEGER freq, t0, t1;
  QueryPerformanceFrequency(&freq);

  QueryPerformanceCounter(&t0);
  for (auto& l : lines) draw_line(l.x0, l.y0, l.x1, l.y1, 0xFFFFFF);
  QueryPerformanceCounter(&t1);
  double ms_b = (t1.QuadPart - t0.QuadPart) * 1000.0 / freq.QuadPart;

  QueryPerformanceCounter(&t0);
  for (auto& l : lines) draw_line_naive(l.x0, l.y0, l.x1, l.y1, 0xFFFFFF);
  QueryPerformanceCounter(&t1);
  double ms_n = (t1.QuadPart - t0.QuadPart) * 1000.0 / freq.QuadPart;

  printf("[Benchmark] %d lines -- Bresenham: %.2f ms | Naive float: %.2f ms | Ratio: %.2fx\n",
    N, ms_b, ms_n, ms_n / ms_b);
}

// Part 7.3: Xiaolin Wu anti-aliased line drawing
static inline float wu_fpart(float v)  { return v - floorf(v); }
static inline float wu_rfpart(float v) { return 1.0f - wu_fpart(v); }

// Blend line color against existing g_buffer pixel using fractional coverage intensity.
// When two Wu lines cross, the intersection is blended twice (once per line). This is
// correct behavior given that we replay all lines over a fresh background every frame —
// the background contribution at crossing pixels is slightly reduced compared to a
// single-pass composite, which is the inherent trade-off of per-line alpha blending.
static void plot_wu_pixel(int x, int y, uint32_t color, float intensity) {
  if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
  uint32_t bg = g_buffer[y * WIDTH + x];
  uint8_t bg_r = (bg >> 16) & 0xFF, bg_g = (bg >> 8) & 0xFF, bg_b = bg & 0xFF;
  uint8_t c_r  = (color >> 16) & 0xFF, c_g = (color >> 8) & 0xFF, c_b = color & 0xFF;
  g_buffer[y * WIDTH + x] = MFB_RGB(
    (uint8_t)(c_r * intensity + bg_r * (1.0f - intensity)),
    (uint8_t)(c_g * intensity + bg_g * (1.0f - intensity)),
    (uint8_t)(c_b * intensity + bg_b * (1.0f - intensity))
  );
}

static void draw_line_wu(int x0, int y0, int x1, int y1, uint32_t color) {
  bool steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep)   { std::swap(x0, y0); std::swap(x1, y1); }
  if (x0 > x1) { std::swap(x0, x1); std::swap(y0, y1); }

  float dx = (float)(x1 - x0);
  float dy = (float)(y1 - y0);
  float grad = (dx == 0.0f) ? 1.0f : dy / dx;

  // First endpoint
  float yend = (float)y0 + grad * (roundf((float)x0) - x0);
  float xgap = wu_rfpart(x0 + 0.5f);
  int xpxl1 = x0, ypxl1 = (int)floorf(yend);
  if (steep) { plot_wu_pixel(ypxl1,   xpxl1, color, wu_rfpart(yend) * xgap);
               plot_wu_pixel(ypxl1+1, xpxl1, color, wu_fpart(yend)  * xgap); }
  else       { plot_wu_pixel(xpxl1, ypxl1,   color, wu_rfpart(yend) * xgap);
               plot_wu_pixel(xpxl1, ypxl1+1, color, wu_fpart(yend)  * xgap); }
  float intery = yend + grad;

  // Second endpoint
  yend = (float)y1 + grad * (roundf((float)x1) - x1);
  xgap = wu_fpart(x1 + 0.5f);
  int xpxl2 = x1, ypxl2 = (int)floorf(yend);
  if (steep) { plot_wu_pixel(ypxl2,   xpxl2, color, wu_rfpart(yend) * xgap);
               plot_wu_pixel(ypxl2+1, xpxl2, color, wu_fpart(yend)  * xgap); }
  else       { plot_wu_pixel(xpxl2, ypxl2,   color, wu_rfpart(yend) * xgap);
               plot_wu_pixel(xpxl2, ypxl2+1, color, wu_fpart(yend)  * xgap); }
  (void)ypxl2;

  // Main loop — plot two straddling pixels per column with complementary intensities
  for (int x = xpxl1 + 1; x < xpxl2; x++) {
    if (steep) { plot_wu_pixel((int)intery,   x, color, wu_rfpart(intery));
                 plot_wu_pixel((int)intery+1, x, color, wu_fpart(intery)); }
    else       { plot_wu_pixel(x, (int)intery,   color, wu_rfpart(intery));
                 plot_wu_pixel(x, (int)intery+1, color, wu_fpart(intery)); }
    intery += grad;
  }
}

static void draw_spirograph() {
  float R = g_spiro_R;
  float r = g_spiro_r;
  if (r == 0.0f) r = 1.0f;
  float p = g_spiro_p;
  int limit = (int)g_spiro_density;
  if (limit <= 0) return;

  float cx = WIDTH / 2.0f;
  float cy = HEIGHT / 2.0f;
  uint32_t color = MFB_RGB((uint8_t)g_line_r, (uint8_t)g_line_g, (uint8_t)g_line_b);

  int prev_x = -1, prev_y = -1;
  for (int i = 0; i <= limit; i++) {
    float theta = (i * 2.0f * 3.14159265f * 12.0f) / limit;
    float x = (R - r) * cosf(theta) + p * cosf((R - r) * theta / r) + cx;
    float y = (R - r) * sinf(theta) - p * sinf((R - r) * theta / r) + cy;

    int px = (int)x;
    int py = (int)y;
    if (i > 0 && prev_x >= 0 && prev_x < WIDTH && prev_y >= 0 && prev_y < HEIGHT &&
        px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT) {
      draw_line(prev_x, prev_y, px, py, color);
    }
    prev_x = px;
    prev_y = py;
  }
}

static void on_custom_btn_click(){
  MessageBoxA(nullptr, "This button has been clicked!", "Debug", MB_OK);
}

int main() {
  struct mfb_window *window =
      mfb_open_ex("MiniGUI Platform", WIDTH, HEIGHT, MFB_WF_RESIZABLE);
  if (!window)
    return 1;

  mu_Context *ctx = (mu_Context *)malloc(sizeof(mu_Context));
  mu_init(ctx);

  ctx->text_width = [](mu_Font font, const char *str, int len) {
    return (len < 0 ? (int)strlen(str) : len) * 8;
  };
  ctx->text_height = [](mu_Font font) { return 8; };

  UIRenderer renderer(WIDTH, HEIGHT);

  mfb_set_char_input_callback(
      [](struct mfb_window *w, unsigned int c) {
        extern void ui_bridge_char_input(struct mfb_window *, unsigned int);
        if (c == 'v' || c == 'V') {
          g_visual_mode = (g_visual_mode + 1) % 4;
          printf("Keyboard callback: Intercepted '%c', toggling visual mode to %d\n", (char)c, g_visual_mode);
          return;
        }
        ui_bridge_char_input(w, c);
      },
      window);

  while (mfb_update_events(window) != MFB_STATE_EXIT) {
    // 1. Input
    ui_bridge_input(ctx, window);

    uint32_t current_line_color = MFB_RGB((uint8_t)g_line_r, (uint8_t)g_line_g, (uint8_t)g_line_b);

    if (g_drawing_enabled) {
      if (!ctx->hover_root) {
        if (ctx->mouse_pressed & MU_MOUSE_LEFT) {
          if (g_circle_mode) {
            // Part 7.1: first click sets circle center; drag defines radius
            g_drawing_circle = true;
            g_circle_center_x = ctx->mouse_pos.x;
            g_circle_center_y = ctx->mouse_pos.y;
          } else {
            g_drawing_line = true;
            g_line_start_x = ctx->mouse_pos.x;
            g_line_start_y = ctx->mouse_pos.y;
          }
        }
      }

      // Part 7.1: commit circle on mouse release
      if (g_drawing_circle) {
        if (!(ctx->mouse_down & MU_MOUSE_LEFT)) {
          int ddx = ctx->mouse_pos.x - g_circle_center_x;
          int ddy = ctx->mouse_pos.y - g_circle_center_y;
          int cr = (int)sqrtf((float)(ddx*ddx + ddy*ddy));
          if (cr > 0)
            g_circles.push_back({ g_circle_center_x, g_circle_center_y, cr, current_line_color });
          g_drawing_circle = false;
        }
      }

      // Part 6: line drawing (brush or click-release)
      if (g_drawing_line) {
        if (g_brush_mode) {
          int curr_x = ctx->mouse_pos.x;
          int curr_y = ctx->mouse_pos.y;
          if (curr_x != g_line_start_x || curr_y != g_line_start_y) {
            g_lines.push_back({ g_line_start_x, g_line_start_y, curr_x, curr_y, current_line_color, (bool)g_use_wu });
            g_line_start_x = curr_x;
            g_line_start_y = curr_y;
          }
        }
        if (!(ctx->mouse_down & MU_MOUSE_LEFT)) {
          if (!g_brush_mode) {
            g_lines.push_back({ g_line_start_x, g_line_start_y, ctx->mouse_pos.x, ctx->mouse_pos.y, current_line_color, (bool)g_use_wu });
          }
          g_drawing_line = false;
        }
      }
    } else {
      g_drawing_line = false;
      g_drawing_circle = false;
    }

    // 2. Scene Rendering (Background)
    g_frame_counter++;
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
      int x = i % WIDTH;
      int y = i / WIDTH;
      uint8_t r = 0, g = 0, b = 0;

      float cx = (WIDTH / 2.0f) + g_center_shift_x;
      float cy = (HEIGHT / 2.0f) + g_center_shift_y;

      if (g_visual_mode == 0) {
        int squareSize = 350;
        int left = (int)cx - squareSize / 2;
        int top = (int)cy - squareSize / 2;
        int right = left + squareSize;
        int bottom = top + squareSize;
        if (x >= left && x < right && y >= top && y < bottom) {
          float u = (x - left) / (float)(squareSize - 1);
          float v = (y - top)  / (float)(squareSize - 1);
          r = (uint8_t)(255.0f * (1.0f - u));
          g = (uint8_t)(255.0f * u);
          b = (uint8_t)(255.0f * v);
        } else {
          r = (uint8_t)(x * 255 / WIDTH);
          g = (uint8_t)(y * 255 / HEIGHT);
          b = (uint8_t)(64 * g_color_speed);
        }
      } else if (g_visual_mode == 1) {
        float dx = x - cx;
        float dy = y - cy;
        float dist = sqrtf(dx * dx + dy * dy);
        float wave = sinf(dist * g_wave_frequency - g_frame_counter * 0.1f * g_color_speed);
        r = (uint8_t)((wave + 1.0f) * 0.5f * 255);
        g = (uint8_t)((sinf(dist * (g_wave_frequency * 0.6f) + g_frame_counter * 0.05f * g_color_speed) + 1.0f) * 0.5f * 255);
        b = (uint8_t)((cosf(dist * (g_wave_frequency * 0.4f) - g_frame_counter * 0.08f * g_color_speed) + 1.0f) * 0.5f * 255);
      } else if (g_visual_mode == 2) {
        int col = x / 16;
        int speed = (int)(((col * 17) % 13 + 5) * g_color_speed);
        if (speed <= 0) speed = 1;
        int offset = (g_frame_counter * speed) % HEIGHT;
        int intensity = (y + offset) % HEIGHT;
        if (intensity < 150) {
          g = 255 - (intensity * 255 / 150);
          r = g > 150 ? (g - 150) : 0;
          b = r;
        } else {
          g = 0; r = 0; b = 0;
        }
        if (x % 32 == 0 || y % 40 == 0)
          g = (g / 2) + 40;
      } else if (g_visual_mode == 3) {
        float dx = x - cx;
        float dy = y - cy;
        float angle = atan2f(dy, dx);
        float dist = sqrtf(dx * dx + dy * dy);
        float value = sinf(angle * 8.0f + logf(dist + 1.0f) * (g_wave_frequency * 100.0f) - g_frame_counter * 0.15f * g_color_speed);
        r = (uint8_t)((value + 1.0f) * 0.5f * 128 + 64);
        g = (uint8_t)((sinf(g_frame_counter * 0.1f * g_color_speed) + 1.0f) * 0.5f * 128);
        b = (uint8_t)((cosf(angle - g_frame_counter * 0.05f * g_color_speed) + 1.0f) * 0.5f * 200);
      }

      if (g_invert_colors) { r = 255 - r; g = 255 - g; b = 255 - b; }
      g_buffer[i] = MFB_RGB(r, g, b);
    }

    // Part 6 / 7: Render spirograph, committed shapes, and live previews on top of background
    if (g_spirograph_active)
      draw_spirograph();

    // Part 7.1: draw committed circles
    for (const auto& c : g_circles)
      draw_circle(c.xc, c.yc, c.r, c.color);

    // Draw committed lines — each line remembers which algorithm drew it
    for (const auto& line : g_lines) {
      if (line.wu) draw_line_wu(line.x0, line.y0, line.x1, line.y1, line.color);
      else         draw_line   (line.x0, line.y0, line.x1, line.y1, line.color);
    }

    // Live preview: line (click-release mode only)
    if (g_drawing_line && !g_brush_mode) {
      if (g_use_wu) draw_line_wu(g_line_start_x, g_line_start_y, ctx->mouse_pos.x, ctx->mouse_pos.y, current_line_color);
      else          draw_line   (g_line_start_x, g_line_start_y, ctx->mouse_pos.x, ctx->mouse_pos.y, current_line_color);
    }

    // Part 7.1: live circle preview
    if (g_drawing_circle) {
      int ddx = ctx->mouse_pos.x - g_circle_center_x;
      int ddy = ctx->mouse_pos.y - g_circle_center_y;
      int cr = (int)sqrtf((float)(ddx*ddx + ddy*ddy));
      if (cr > 0) draw_circle(g_circle_center_x, g_circle_center_y, cr, current_line_color);
    }

    // 3. UI Logic
    static float slider_val = 50.0f;
    static float number_val = 3.14f;
    static int checkbox_a = 0;
    static int checkbox_b = 1;
    static char textbox_buf[128] = "edit me";
    static bool quit_requested = false;

    mu_begin(ctx);

    // --- Widgets window ---
    if (mu_begin_window(ctx, "Widgets", mu_rect(20, 20, 360, 540))) {
      int w1[] = {-1};

      mu_layout_row(ctx, 1, w1, 0);
      mu_label(ctx, "mu_label: plain static text");
      mu_text(ctx, "mu_text: word-wrapped longer text that will reflow inside "
                   "the window width automatically.");

      mu_layout_row(ctx, 1, w1, 0);
      if (mu_button(ctx, "mu_button: click me"))
        quit_requested = false;
      if (mu_button(ctx, "Custom Made Button"))
        on_custom_btn_click();

      mu_layout_row(ctx, 1, w1, 0);
      mu_checkbox(ctx, "mu_checkbox A (off)", &checkbox_a);
      mu_checkbox(ctx, "mu_checkbox B (on)", &checkbox_b);

      mu_layout_row(ctx, 1, w1, 0);
      mu_label(ctx, "mu_textbox:");
      mu_textbox(ctx, textbox_buf, sizeof(textbox_buf));

      mu_layout_row(ctx, 1, w1, 0);
      mu_label(ctx, "mu_slider (0-100):");
      mu_slider(ctx, &slider_val, 0, 100);

      // Part 5: Custom state binding controls
      if (mu_header(ctx, "Custom Background Morph Controls")) {
        mu_layout_row(ctx, 1, w1, 0);
        mu_checkbox(ctx, "Invert Background Colors", &g_invert_colors);

        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, "Animation Speed:");
        mu_slider(ctx, &g_color_speed, 0, 5);

        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, "Wave Frequency:");
        mu_slider(ctx, &g_wave_frequency, 0.005f, 0.2f);

        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, "Shift X Coordinate:");
        mu_slider(ctx, &g_center_shift_x, -400.0f, 400.0f);

        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, "Shift Y Coordinate:");
        mu_slider(ctx, &g_center_shift_y, -300.0f, 300.0f);
      }

      mu_layout_row(ctx, 1, w1, 0);
      mu_label(ctx, "mu_number (step 0.1):");
      mu_number(ctx, &number_val, 0.1f);

      if (mu_header(ctx, "mu_header: collapsible section")) {
        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, "Content inside the header.");
      }

      if (mu_begin_treenode(ctx, "mu_treenode: root")) {
        mu_layout_row(ctx, 1, w1, 0);
        mu_label(ctx, "child item A");
        if (mu_begin_treenode(ctx, "nested node")) {
          mu_layout_row(ctx, 1, w1, 0);
          mu_label(ctx, "deeply nested item");
          mu_end_treenode(ctx);
        }
        mu_end_treenode(ctx);
      }

      mu_layout_row(ctx, 1, w1, 0);
      if (mu_button(ctx, "Quit"))
        quit_requested = true;

      mu_end_window(ctx);
    }

    // --- Panel window ---
    if (mu_begin_window(ctx, "Panel Demo", mu_rect(395, 20, 380, 200))) {
      int w2[] = {-1};
      mu_layout_row(ctx, 1, w2, 120);
      mu_begin_panel(ctx, "scrollable panel");
      int wp[] = {-1};
      for (int i = 1; i <= 12; i++) {
        mu_layout_row(ctx, 1, wp, 0);
        char line_buf[32];
        snprintf(line_buf, sizeof(line_buf), "Panel row %d", i);
        mu_label(ctx, line_buf);
      }
      mu_end_panel(ctx);
      mu_end_window(ctx);
    }

    // --- Popup demo window ---
    if (mu_begin_window(ctx, "Popup Demo", mu_rect(395, 235, 380, 80))) {
      int w3[] = {-1};
      mu_layout_row(ctx, 1, w3, 0);
      if (mu_button(ctx, "Open popup")) {
        mu_Container *popup = mu_get_container(ctx, "my popup");
        popup->rect = mu_rect(ctx->mouse_pos.x, ctx->mouse_pos.y, 260, 84);
        popup->open = 1;
        ctx->hover_root = ctx->next_hover_root = popup;
        mu_bring_to_front(ctx, popup);
      }
      int popup_opt = MU_OPT_POPUP | MU_OPT_NORESIZE | MU_OPT_NOSCROLL |
                      MU_OPT_NOTITLE | MU_OPT_CLOSED;
      if (mu_begin_window_ex(ctx, "my popup", mu_rect(0, 0, 260, 84), popup_opt)) {
        int wp[] = {-1};
        mu_layout_row(ctx, 1, wp, 0);
        mu_label(ctx, "mu_popup: click outside to close");
        if (mu_button(ctx, "Close"))
          mu_get_current_container(ctx)->open = 0;
        mu_end_window(ctx);
      }
      mu_end_window(ctx);
    }

    // --- Interactive Drawing Tool window ---
    if (mu_begin_window(ctx, "Interactive Line Drawer", mu_rect(790, 20, 380, 680))) {
      int wd[] = {-1};
      mu_layout_row(ctx, 1, wd, 0);
      mu_label(ctx, "Left Click & Drag on background to draw!");

      mu_layout_row(ctx, 1, wd, 0);
      mu_checkbox(ctx, "Enable Drawing Click", &g_drawing_enabled);

      mu_layout_row(ctx, 1, wd, 0);
      mu_checkbox(ctx, "Continuous Brush Mode", &g_brush_mode);

      mu_layout_row(ctx, 1, wd, 0);
      mu_label(ctx, "RGB Line / Brush Color:");
      mu_slider(ctx, &g_line_r, 0, 255);
      mu_slider(ctx, &g_line_g, 0, 255);
      mu_slider(ctx, &g_line_b, 0, 255);

      char hex_col[64];
      snprintf(hex_col, sizeof(hex_col), "Active Color: (R:%d, G:%d, B:%d)",
               (int)g_line_r, (int)g_line_g, (int)g_line_b);
      mu_layout_row(ctx, 1, wd, 0);
      mu_text(ctx, hex_col);

      mu_layout_row(ctx, 1, wd, 0);
      if (mu_button(ctx, "Clear Canvas")) {
        g_lines.clear();
        g_circles.clear();
      }
      if (mu_button(ctx, "Undo Last Line")) {
        if (!g_lines.empty()) g_lines.pop_back();
      }

      // Part 7.1: Circle drawing mode
      if (mu_header(ctx, "Part 7.1 - Circle Mode")) {
        mu_layout_row(ctx, 1, wd, 0);
        mu_checkbox(ctx, "Circle Mode (drag = center+radius)", &g_circle_mode);

        mu_layout_row(ctx, 1, wd, 0);
        if (mu_button(ctx, "Undo Last Circle")) {
          if (!g_circles.empty()) g_circles.pop_back();
        }
      }

      // Part 7.3: Anti-aliasing toggle
      if (mu_header(ctx, "Part 7.3 - Anti-Aliasing")) {
        mu_layout_row(ctx, 1, wd, 0);
        mu_checkbox(ctx, "Xiaolin Wu Anti-Aliasing", &g_use_wu);
        mu_layout_row(ctx, 1, wd, 0);
        mu_text(ctx, g_use_wu
          ? "Wu mode: smooth sub-pixel edges, blends with background."
          : "Bresenham mode: crisp integer pixels, no blending.");
      }

      // Part 7.2: Performance benchmark
      if (mu_header(ctx, "Part 7.2 - Performance Benchmark")) {
        mu_layout_row(ctx, 1, wd, 0);
        mu_text(ctx, "Draws 100k random lines with each algo. Results logged to console.");
        mu_layout_row(ctx, 1, wd, 0);
        if (mu_button(ctx, "Run Benchmark (100k lines)"))
          run_benchmark();
      }

      // Spirograph section
      if (mu_header(ctx, "Advanced Spirograph Generator")) {
        mu_layout_row(ctx, 1, wd, 0);
        mu_checkbox(ctx, "Activate Spirograph Overlay", &g_spirograph_active);

        mu_layout_row(ctx, 1, wd, 0);
        mu_label(ctx, "Outer Ring R:");
        mu_slider(ctx, &g_spiro_R, 10, 500);

        mu_layout_row(ctx, 1, wd, 0);
        mu_label(ctx, "Inner Wheel r:");
        mu_slider(ctx, &g_spiro_r, 10, 500);

        mu_layout_row(ctx, 1, wd, 0);
        mu_label(ctx, "Pen Distance p:");
        mu_slider(ctx, &g_spiro_p, 10, 500);

        mu_layout_row(ctx, 1, wd, 0);
        mu_label(ctx, "Line Segment Density:");
        mu_slider(ctx, &g_spiro_density, 50, 2000);
      }
      mu_end_window(ctx);
    }

    mu_end(ctx);

    if (quit_requested) {
      mfb_close(window);
      break;
    }

    // 4. UI Rendering
    renderer.render(ctx, g_buffer);

    // 5. Display
    mfb_update_state state = mfb_update_ex(window, g_buffer, WIDTH, HEIGHT);
    if (state < 0)
      break;

    mfb_wait_sync(window);
  }

  mfb_close(window);
  free(ctx);
  return 0;
}
