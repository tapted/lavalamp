#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <limits>
#include <random>
#include <vector>

#include "halpp/config.hpp"
#include "halpp/display/display.hpp"

constexpr uint16_t SCREEN_WIDTH = halpp::config::Display::WIDTH;
constexpr uint16_t SCREEN_HEIGHT = halpp::config::Display::HEIGHT;
constexpr uint16_t CENTER_X = SCREEN_WIDTH / 2;
constexpr uint16_t CENTER_Y = SCREEN_HEIGHT / 2;
constexpr uint16_t RADIUS = SCREEN_WIDTH / 2;

constexpr uint16_t TILE_SIZE = 40;
constexpr uint16_t GRID_W = SCREEN_WIDTH / TILE_SIZE;
constexpr uint16_t GRID_H = SCREEN_HEIGHT / TILE_SIZE;

static_assert(SCREEN_WIDTH % TILE_SIZE == 0, "SCREEN_WIDTH must be divisible by TILE_SIZE");
static_assert(SCREEN_HEIGHT % TILE_SIZE == 0, "SCREEN_HEIGHT must be divisible by TILE_SIZE");

// Modern standard-compliant URBG (Uniform Random Bit Generator) for esp_random
struct EspRandomGenerator {
  using result_type = uint32_t;
  static constexpr result_type min() { return 0; }
  static constexpr result_type max() { return std::numeric_limits<uint32_t>::max(); }
  result_type operator()() const { return esp_random(); }
};

constexpr uint16_t rgbTo565(uint8_t r, uint8_t g, uint8_t b) {
  uint16_t rgb = static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));

  if (halpp::config::lvgl::USE_RGB565_SWAPPED) {
    return static_cast<uint16_t>((rgb << 8) | (rgb >> 8));
  }
  return rgb;
}

constexpr std::array<std::array<int, 4>, 4> bayer4x4 = {{
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5},
}};

struct Blob {
  float x{0.0f}, y{0.0f};
  float vx{0.0f}, vy{0.0f};
  float radius{0.0f};
  float rSq{0.0f}, invRSq{0.0f};

  constexpr void setRadius(float r) noexcept {
    radius = r;
    rSq = r * r;
    invRSq = 1.0f / rSq;
  }
};

class LavaLampAnimator {
 private:
  std::vector<Blob> blobs;
  std::array<std::array<bool, GRID_W>, GRID_H> previousLava{};
  uint16_t* bufferA = nullptr;
  uint16_t* bufferB = nullptr;
  float speedScale{0.3f};

 public:
  uint64_t total_draw_time = 0;
  uint64_t total_wait_for_dma_time = 0;
  uint64_t total_idle_time = 0;
  uint64_t last_clock_check = 0;

 public:
  explicit LavaLampAnimator(int numBlobs = 5, float speedScale_ = 0.3f) : speedScale{speedScale_} {
    EspRandomGenerator gen;

    // Use modern distribution classes instead of modulo arithmetic and casting
    std::uniform_real_distribution<float> xDist(
        static_cast<float>(CENTER_X) - (SCREEN_WIDTH * 0.25f),
        static_cast<float>(CENTER_X) + (SCREEN_WIDTH * 0.25f));
    std::uniform_real_distribution<float> yDist(
        static_cast<float>(CENTER_Y) - (SCREEN_HEIGHT * 0.375f),
        static_cast<float>(CENTER_Y) + (SCREEN_HEIGHT * 0.375f));
    std::uniform_real_distribution<float> vxDist(-0.333f, 0.333f);
    std::uniform_real_distribution<float> vyDist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> rDist(50.0f, 95.0f);

    blobs.reserve(static_cast<size_t>(numBlobs));
    for (int i = 0; i < numBlobs; ++i) {
      Blob b;
      b.x = xDist(gen);
      b.y = yDist(gen);
      b.vx = vxDist(gen);
      b.vy = vyDist(gen);
      b.setRadius(rDist(gen));
      blobs.push_back(b);
    }

    for (auto& row : previousLava) {
      std::ranges::fill(row, true);
    }
    bufferA = static_cast<uint16_t*>(
        heap_caps_malloc(TILE_SIZE * TILE_SIZE * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    bufferB = static_cast<uint16_t*>(
        heap_caps_malloc(TILE_SIZE * TILE_SIZE * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  }

  template <typename DrawCallback>
  void updateAndRender(DrawCallback drawCallback) {
    std::array<std::array<bool, GRID_W>, GRID_H> currentLava{};
    if (last_clock_check != 0) {
      uint64_t now = esp_timer_get_time();
      total_idle_time += now - last_clock_check;
      last_clock_check = now;
    }

    // 1. Update Physics (Vertical Lava Flow)
    for (auto& blob : blobs) {
      blob.x += (blob.vx * speedScale);
      blob.y += (blob.vy * speedScale);

      // Heat at bottom makes them rise, cooling at top makes them sink
      if (blob.y < blob.radius) blob.vy += 0.05f;
      if (blob.y > static_cast<float>(SCREEN_HEIGHT) - blob.radius) blob.vy -= 0.05f;

      // Keep horizontally contained
      float dx = blob.x - static_cast<float>(CENTER_X);
      if (std::abs(dx) > (static_cast<float>(RADIUS) * 0.6f)) {
        blob.vx -= (dx * 0.002f);
      }

      // Enforce speed limits
      blob.vy = std::clamp(blob.vy, -1.5f, 1.5f);
      blob.vx = std::clamp(blob.vx, -0.5f, 0.5f);

      // 2. Mark intersecting tiles
      int minTx = std::max(0, static_cast<int>((blob.x - blob.radius) / TILE_SIZE));
      int maxTx = std::min(GRID_W - 1, static_cast<int>((blob.x + blob.radius) / TILE_SIZE));
      int minTy = std::max(0, static_cast<int>((blob.y - blob.radius) / TILE_SIZE));
      int maxTy = std::min(GRID_H - 1, static_cast<int>((blob.y + blob.radius) / TILE_SIZE));

      for (int ty = minTy; ty <= maxTy; ++ty) {
        for (int tx = minTx; tx <= maxTx; ++tx) {
          currentLava[ty][tx] = true;
        }
      }
    }

    // 3. Render tiles that either have lava now, OR had lava last frame (to clean up trails)

    uint16_t* computeBuffer = bufferA;
    uint16_t* dmaBuffer = bufferB;
    for (int ty = 0; ty < GRID_H; ++ty) {
      for (int tx = 0; tx < GRID_W; ++tx) {
        if (currentLava[ty][tx] || previousLava[ty][tx]) {
          int rectX = tx * TILE_SIZE;
          int rectY = ty * TILE_SIZE;

          renderTile(rectX, rectY, drawCallback, computeBuffer);

          uint64_t draw_end = esp_timer_get_time();
          total_draw_time += draw_end - last_clock_check;
          last_clock_check = draw_end;

          halpp::Display::instance().ensure_flushed();

          uint64_t flush_end = esp_timer_get_time();
          total_wait_for_dma_time += flush_end - last_clock_check;
          last_clock_check = flush_end;

          std::swap(computeBuffer, dmaBuffer);
          drawCallback(rectX, rectY, TILE_SIZE, TILE_SIZE, dmaBuffer);

          last_clock_check = esp_timer_get_time();
        }
        // Store state for next frame's trail cleanup
        previousLava[ty][tx] = currentLava[ty][tx];
      }
    }
  }

  void forceRedraw() {
    // Marks all tiles as needing a redraw for the next frame
    for (auto& row : previousLava) {
      std::ranges::fill(row, true);
    }
  }

 private:
  template <typename DrawCallback>
  void renderTile(int rectX, int rectY, DrawCallback drawCallback, uint16_t* renderBuffer) {
    size_t bufIdx = 0;

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
          float dx = static_cast<float>(x) - blob.x;
          float dy = static_cast<float>(y) - blob.y;
          float distSq = dx * dx + dy * dy;

          if (distSq < blob.rSq) {
            // Wyvill-inspired polynomial curve. Bounded perfectly to radius!
            float v = 1.0f - (distSq * blob.invRSq);
            field += v * v * v;  // Smooth cubic falloff
          }
        }

        int r = 0, g = 0, b = 0;

        if (field < 0.1f) {
          // 1. Background gradient (Deep Purple to Black)
          float bgMap = static_cast<float>(y) / static_cast<float>(SCREEN_HEIGHT);
          r = static_cast<int>(std::lerp(30.0f, 5.0f, bgMap));
          g = 0;
          b = static_cast<int>(std::lerp(60.0f, 15.0f, bgMap));
        } else if (field < 0.2f) {
          // 2. Anti-aliased Lava Edge (Blend Background -> Deep Red)
          float t = (field - 0.1f) / 0.1f;
          r = static_cast<int>(std::lerp(30.0f, 255.0f, t));
          g = static_cast<int>(std::lerp(0.0f, 50.0f, t));
          b = static_cast<int>(std::lerp(60.0f, 0.0f, t));
        } else {
          // 3. Lava Core (Blend Deep Red -> Bright Yellow)
          float t = std::min(1.0f, (field - 0.2f) / 0.8f);
          r = 255;
          g = static_cast<int>(std::lerp(50.0f, 220.0f, t));
          b = 0;
        }

        // Dither and write
        int dither = bayer4x4[y % 4][x % 4] - 8;
        r = std::clamp(r + dither, 0, 255);
        g = std::clamp(g + dither, 0, 255);
        b = std::clamp(b + dither, 0, 255);

        renderBuffer[bufIdx++] =
            rgbTo565(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b));
      }
    }
  }
};