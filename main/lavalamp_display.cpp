#include "lavalamp_display.hpp"

#include "espbase/boot/ota_rollback_watchdog.hpp"
#include "espbase/esp_result.hpp"
#include "espbase/main_loop_task.hpp"
#include "hal/display/board_display.hpp"
#include "halpp/config.hpp"
#include "halpp/display/display.hpp"
#include "lava/lava.hpp"

static constexpr char TAG[] = "LavalampDisplay";

static constinit MainLoopTask<int> lava_task;
static LavaLampAnimator animator(5);  // 5 organic blobs

static std::optional<uint32_t> lava_step_function(MainLoopTask<int>&) {
  static halpp::Display& display = halpp::Display::instance();
  animator.updateAndRender([](int x, int y, int w, int h, uint16_t* buffer) {
    display.draw_bitmap(x, y, w, h, buffer);
  });
  return 15;
}

void init_lavalamp_display() {
  wait_for_reset_display_and_touch();
  if (EspError err = halpp::Display::init_default()) {
    err.log(TAG, "Failed to init ST77916 display; won't start lvgl task");
    return;
  }
  lava_task.start(0, lava_step_function);

  // if (EspError err = halpp::Display::instance().init_lvgl()) {
  //   err.log(TAG, "Failed to init LVGL display.");
  //   return;
  // }
  startup_gate_passed("Lavalamp ST77916 Display Initialized");
}