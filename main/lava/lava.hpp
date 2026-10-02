#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "halpp/config.hpp"

constexpr int SCREEN_WIDTH = 360;
constexpr int SCREEN_HEIGHT = 360;
constexpr int CENTER_X = 180;
constexpr int CENTER_Y = 180;
constexpr int RADIUS = 180;

constexpr int TILE_SIZE = 30;
constexpr int GRID_W = SCREEN_WIDTH / TILE_SIZE;
constexpr int GRID_H = SCREEN_HEIGHT / TILE_SIZE;

constexpr uint16_t rgbTo565(uint8_t r, uint8_t g, uint8_t b) {
  uint16_t rgb = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);

  if (halpp::config::lvgl::USE_RGB565_SWAPPED) {
    return rgb << 8 | (rgb >> 8);
  } else {
    return rgb;
  }
}
// Bayer matrix for perfectly smooth gradients on 16-bit displays
const uint8_t bayer4x4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5},
};

// Lerp helper for smooth color transitions
inline int lerp(int a, int b, float t) {
  return a + (b - a) * t;
}

struct Blob {
  float x, y;
  float vx, vy;
  float radius;
  float rSq, invRSq;  // Pre-calculated for speed

  void setRadius(float r) {
    radius = r;
    rSq = r * r;
    invRSq = 1.0f / rSq;
  }
};

class LavaLampAnimator {
 private:
  std::vector<Blob> blobs;
  bool previousLava[GRID_H][GRID_W];
  uint16_t renderBuffer[TILE_SIZE * TILE_SIZE];
  float speedScale = 0.3f;  // 1.0 is normal, 0.5 is half speed, 0.2 is very slow

 public:
  LavaLampAnimator(int numBlobs = 5, float speedScale_ = 0.3f) {
    for (int i = 0; i < numBlobs; ++i) {
      Blob b;
      // Spread blobs vertically like a real lava lamp
      b.x = CENTER_X + (rand() % 60 - 30);
      b.y = CENTER_Y + (rand() % 200 - 100);
      b.vx = (float)(rand() % 10 - 5) / 15.0f;
      b.vy = (float)(rand() % 20 - 10) / 10.0f;

      // Mix of big main blobs and smaller break-off pieces
      b.setRadius(50.0f + (rand() % 45));
      blobs.push_back(b);
    }

    // Force full redraw on frame 1 to draw the background
    memset(previousLava, 1, sizeof(previousLava));
    speedScale = speedScale_;
  }

  template <typename DrawCallback>
  void updateAndRender(DrawCallback drawCallback) {
    bool currentLava[GRID_H][GRID_W] = {};

    // 1. Update Physics (Vertical Lava Flow)
    for (auto& blob : blobs) {
      blob.x += (blob.vx * speedScale);
      blob.y += (blob.vy * speedScale);

      // Heat at bottom makes them rise, cooling at top makes them sink
      if (blob.y < blob.radius) blob.vy += 0.05f;
      if (blob.y > SCREEN_HEIGHT - blob.radius) blob.vy -= 0.05f;

      // Keep horizontally contained
      float dx = blob.x - CENTER_X;
      if (std::abs(dx) > (RADIUS * 0.6f)) {
        blob.vx -= (dx * 0.002f);
      }

      // Enforce speed limits
      blob.vy = std::clamp(blob.vy, -1.5f, 1.5f);
      blob.vx = std::clamp(blob.vx, -0.5f, 0.5f);

      // 2. Mark intersecting tiles
      int minTx = std::max(0, (int)(blob.x - blob.radius) / TILE_SIZE);
      int maxTx = std::min(GRID_W - 1, (int)(blob.x + blob.radius) / TILE_SIZE);
      int minTy = std::max(0, (int)(blob.y - blob.radius) / TILE_SIZE);
      int maxTy = std::min(GRID_H - 1, (int)(blob.y + blob.radius) / TILE_SIZE);

      for (int ty = minTy; ty <= maxTy; ++ty) {
        for (int tx = minTx; tx <= maxTx; ++tx) {
          currentLava[ty][tx] = true;
        }
      }
    }

    // 3. Render tiles that either have lava now, OR had lava last frame (to clean up trails)
    for (int ty = 0; ty < GRID_H; ++ty) {
      for (int tx = 0; tx < GRID_W; ++tx) {
        if (currentLava[ty][tx] || previousLava[ty][tx]) {
          renderTile(tx, ty, drawCallback);
        }
        // Store state for next frame's trail cleanup
        previousLava[ty][tx] = currentLava[ty][tx];
      }
    }
  }

 private:
  template <typename DrawCallback>
  void renderTile(int tx, int ty, DrawCallback drawCallback) {
    int rectX = tx * TILE_SIZE;
    int rectY = ty * TILE_SIZE;
    int bufIdx = 0;

    for (int py = 0; py < TILE_SIZE; ++py) {
      int y = rectY + py;
      int dy_center = y - CENTER_Y;
      int dy_center_sq = dy_center * dy_center;

      for (int px = 0; px < TILE_SIZE; ++px) {
        int x = rectX + px;

        // Mask pixels outside the circular screen
        int dx_center = x - CENTER_X;
        if ((dx_center * dx_center + dy_center_sq) > (RADIUS * RADIUS)) {
          renderBuffer[bufIdx++] = 0x0000;
          continue;
        }

        // Calculate finite-support metaball field
        float field = 0.0f;
        for (const auto& blob : blobs) {
          float dx = x - blob.x;
          float dy = y - blob.y;
          float distSq = dx * dx + dy * dy;

          if (distSq < blob.rSq) {
            // Wyvill-inspired polynomial curve. Bounded perfectly to radius!
            float v = 1.0f - (distSq * blob.invRSq);
            field += v * v * v;  // Smooth cubic falloff
          }
        }

        int r = 0, g = 0, b = 0;

        // --- COLOR MAPPING PALETTE ---
        if (field < 0.1f) {
          // 1. Background gradient (Deep Purple to Black)
          float bgMap = (float)y / SCREEN_HEIGHT;
          r = lerp(30, 5, bgMap);
          g = 0;
          b = lerp(60, 15, bgMap);
        } else if (field < 0.2f) {
          // 2. Anti-aliased Lava Edge (Blend Background -> Deep Red)
          float t = (field - 0.1f) / 0.1f;  // Normalize 0 to 1
          r = lerp(30, 255, t);
          g = lerp(0, 50, t);
          b = lerp(60, 0, t);
        } else {
          // 3. Lava Core (Blend Deep Red -> Bright Yellow)
          float t = std::min(1.0f, (field - 0.2f) / 0.8f);
          r = 255;
          g = lerp(50, 220, t);  // Pushes towards yellow
          b = 0;
        }

        // Dither and write
        int dither = bayer4x4[y % 4][x % 4] - 8;
        r = std::clamp(r + dither, 0, 255);
        g = std::clamp(g + dither, 0, 255);
        b = std::clamp(b + dither, 0, 255);

        renderBuffer[bufIdx++] = rgbTo565(r, g, b);
      }
    }

    drawCallback(rectX, rectY, TILE_SIZE, TILE_SIZE, renderBuffer);
  }
};