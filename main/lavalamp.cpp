#include "espbase/boot/check_crash_loop.hpp"
#include "espbase/boot/delayed_pm_enable.hpp"
#include "espbase/boot/ota_rollback_watchdog.hpp"
#include "espbase/main_loop.hpp"
#include "lavalamp_display.hpp"

static constexpr const char TAG[] = "lavalamp";

extern "C" void app_main(void) {
  check_crash_loop();
  // delayed_pm_enable();
  start_ota_rollback_watchdog(2);

  init_lavalamp_display();

  ESP_LOGI(TAG, "Starting main loop...");
  main_loop.push_func([](void*) { startup_gate_passed("Main Loop running"); });
  main_loop.run_forever();
}