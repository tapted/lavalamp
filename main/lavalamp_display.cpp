#include "lavalamp_display.hpp"

#include <esp_lcd_touch_cst816s.h>

#include "backlight_controller.hpp"
#include "espbase/boot/ota_rollback_watchdog.hpp"
#include "espbase/esp_result.hpp"
#include "espbase/main_loop_task.hpp"
#include "hal/display/board_display.hpp"
#include "halpp/config.hpp"
#include "halpp/display/display.hpp"
#include "halpp/display/touch.hpp"
#include "lava/lava.hpp"

using halpp::config;

static constexpr char TAG[] = "LavalampDisplay";

static constinit MainLoopTask<int> lava_task;
static LavaLampAnimator animator(5);  // 5 organic blobs
static BacklightController* backlight_controller = nullptr;
static constinit halpp::display::Touch touch;

static std::optional<uint32_t> lava_step_function(MainLoopTask<int>&) {
  static bool was_ui_visible = false;
  bool is_ui_visible = backlight_controller->isVisible();
  if (was_ui_visible && !is_ui_visible) {
    animator.forceRedraw();
  }
  was_ui_visible = is_ui_visible;
  if (!is_ui_visible) {
    animator.updateAndRender([](int x, int y, int w, int h, uint16_t* buffer) {
      halpp::Display::instance().draw_bitmap(x, y, w, h, buffer);
    });
  }
  return 15;
}

static void set_backlight(int brightness) {
  halpp::Display::instance().set_backlight(halpp::BacklightState::On, brightness, 0);
}

void init_lavalamp_display() {
  wait_for_reset_display_and_touch();
  if (EspError err = halpp::Display::init_default()) {
    err.log(TAG, "Failed to init ST77916 display; won't start lvgl task");
    return;
  }
  if (EspError err = halpp::Display::instance().init_lvgl()) {
    err.log(TAG, "Failed to init LVGL display.");
    return;
  }
  if (EspError err = touch.begin(esp_lcd_touch_new_i2c_cst816s, nullptr)) {
    err.log(TAG, "Failed to initialize touch");
  }

  lava_task.start(0, lava_step_function);
  backlight_controller = new BacklightController(set_backlight, config::Display::BACKLIGHT_DEFAULT);
  startup_gate_passed("Lavalamp ST77916 Display Initialized");
}