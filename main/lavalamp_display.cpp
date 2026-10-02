#include "lavalamp_display.hpp"

#include "espbase/boot/ota_rollback_watchdog.hpp"
#include "espbase/esp_result.hpp"
#include "halpp/config.hpp"
#include "halpp/display/display.hpp"
#include "hal/display/board_display.hpp"

static constexpr char TAG[] = "LavalampDisplay";

void init_lavalamp_display() {
  wait_for_reset_display_and_touch();
  if (EspError err = halpp::Display::init_default()) {
    err.log(TAG, "Failed to init ST77916 display; won't start lvgl task");
    return;
  }
  // if (EspError err = halpp::Display::instance().init_lvgl()) {
  //   err.log(TAG, "Failed to init LVGL display.");
  //   return;
  // }
  startup_gate_passed("Lavalamp ST77916 Display Initialized");
}