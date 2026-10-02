#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>  // for memset
#include <vector>

#include "halpp/config.hpp"

constexpr int SCREEN_WIDTH = 360;
constexpr int SCREEN_HEIGHT = 360;
constexpr int CENTER_X = 180;
constexpr int CENTER_Y = 180;
constexpr int RADIUS = 180;

// Tile grid settings
constexpr int TILE_SIZE = 30;  // 30x30 = 900 pixels (fits perfectly in fast RAM)
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

const uint8_t bayer4x4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5},
};

struct Rect {
  int x, y, w, h;
};

struct Blob {
  float x, y;
  float vx, vy;
  float radius;
  uint8_t baseR, baseG, baseB;

  Rect getBoundingBox() const {
    // Tightened margin to reduce unnecessary dirty tiles
    int margin = (int)(radius * 1.2f);
    return Rect{(int)x - margin, (int)y - margin, (int)(margin * 2), (int)(margin * 2)};
  }
};

class LavaLampAnimator {
 private:
  std::vector<Blob> blobs;
  bool dirtyGrid[GRID_H][GRID_W];
  uint16_t renderBuffer[TILE_SIZE * TILE_SIZE];  // 1.8KB, lives in fast internal RAM!

  void markDirty(const Rect& r) {
    // Convert rect bounds to grid coordinates and clamp to screen
    int startX = std::max(0, r.x / TILE_SIZE);
    int startY = std::max(0, r.y / TILE_SIZE);
    int endX = std::min(GRID_W - 1, (r.x + r.w - 1) / TILE_SIZE);
    int endY = std::min(GRID_H - 1, (r.y + r.h - 1) / TILE_SIZE);

    for (int y = startY; y <= endY; ++y) {
      for (int x = startX; x <= endX; ++x) {
        dirtyGrid[y][x] = true;
      }
    }
  }

 public:
  LavaLampAnimator(int numBlobs = 4) {
    for (int i = 0; i < numBlobs; ++i) {
      Blob b;
      b.x = CENTER_X + (rand() % 100 - 50);
      b.y = CENTER_Y + (rand() % 100 - 50);
      b.vx = (float)(rand() % 20 - 10) / 10.0f;
      b.vy = (float)(rand() % 20 - 10) / 10.0f;
      b.radius = 45.0f + (rand() % 25);  // Slightly smaller to prevent full-screen fills

      b.baseR = 200 + (rand() % 55);
      b.baseG = 20 + (rand() % 60);
      b.baseB = 10;

      blobs.push_back(b);
    }

    // Force full redraw on first frame
    memset(dirtyGrid, 1, sizeof(dirtyGrid));
  }

  // Combines physics update and rendering to avoid holding state between calls
  template <typename DrawCallback>
  void updateAndRender(DrawCallback drawCallback) {
    // 1. Mark current positions as dirty (to clear trails)
    for (auto& blob : blobs) {
      markDirty(blob.getBoundingBox());
    }

    // 2. Update physics
    for (auto& blob : blobs) {
      blob.x += blob.vx;
      blob.y += blob.vy;

      float dx = blob.x - CENTER_X;
      float dy = blob.y - CENTER_Y;
      float distFromCenter = std::sqrt(dx * dx + dy * dy);

      if (distFromCenter > (RADIUS - blob.radius)) {
        blob.vx = -blob.vx + (rand() % 5 - 2) * 0.1f;
        blob.vy = -blob.vy + (rand() % 5 - 2) * 0.1f;
      }

      // 3. Mark new positions as dirty
      markDirty(blob.getBoundingBox());
    }

    // 4. Render only the tiles marked as dirty
    for (int ty = 0; ty < GRID_H; ++ty) {
      for (int tx = 0; tx < GRID_W; ++tx) {
        if (dirtyGrid[ty][tx]) {
          renderTile(tx, ty, drawCallback);
        }
      }
    }

    // 5. Clear grid for next frame
    memset(dirtyGrid, 0, sizeof(dirtyGrid));
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

        // Mask pixels outside the physical circular screen
        int dx_center = x - CENTER_X;
        if ((dx_center * dx_center + dy_center_sq) > (RADIUS * RADIUS)) {
          renderBuffer[bufIdx++] = 0x0000;
          continue;
        }

        // Metaball Field Calculation
        float field = 0.0f;
        float accumR = 0.0f, accumG = 0.0f, accumB = 0.0f;

        for (const auto& blob : blobs) {
          float distX = x - blob.x;
          float distY = y - blob.y;
          float distSq = distX * distX + distY * distY;
          if (distSq < 1.0f) distSq = 1.0f;

          float value = (blob.radius * blob.radius) / distSq;
          field += value;

          accumR += blob.baseR * value;
          accumG += blob.baseG * value;
          accumB += blob.baseB * value;
        }

        // Background gradient mapping
        float bgFactor = (float)y / SCREEN_HEIGHT;
        float finalR = 20 + bgFactor * 30;
        float finalG = 10;
        float finalB = 40 + (1.0f - bgFactor) * 50;

        // Thresholding/Blending
        if (field > 0.8f) {
          float normalize = 1.0f / field;
          finalR = accumR * normalize;
          finalG = accumG * normalize;
          finalB = accumB * normalize;
        }

        // Bayer Dithering
        int dither = (bayer4x4[y % 4][x % 4] - 8);
        int r = std::clamp((int)finalR + dither, 0, 255);
        int g = std::clamp((int)finalG + dither, 0, 255);
        int b = std::clamp((int)finalB + dither, 0, 255);

        renderBuffer[bufIdx++] = rgbTo565(r, g, b);
      }
    }

    // Push the 30x30 chunk to the SPI bus
    drawCallback(rectX, rectY, TILE_SIZE, TILE_SIZE, renderBuffer);
  }
};