#pragma once

#include <cstdint>

#include "espbase/esp_result.hpp"
#include "esp_err.h"
#include "esp_io_expander.h"

namespace HAL {

EspResult<esp_io_expander_handle_t> init_board_exio();

class EXIO {
  static esp_io_expander_handle_t handle;
  esp_err_t init();
  esp_err_t _deinit();

 public:
  static EXIO& instance() {
    static EXIO inst;
    return inst;
  }
  static esp_err_t deinit() { return handle ? instance()._deinit() : ESP_OK; }

  // pins: bitwise OR of IO_EXPANDER_PIN_NUM_XXX values.
  EspResult<void> set_pins_mode(uint32_t pin_num_mask, esp_io_expander_dir_t dir);

  /**
   * @brief Read a single pin level
   * @param pin Pin number (1-8)
   * @return EspResult containing the pin state (0 or 1) on success
   */
  EspResult<uint8_t> read_pin(uint8_t pin);

  /**
   * @brief Read all pins at once
   * @param states Pointer to store all pin states
   * @return EspResult containing all pin states on success
   */
  EspResult<uint32_t> read_pins();

  /**
   * @brief Set a single pin output level
   * @param pin Pin number (1-8)
   * @param state Output state (true=high, false=low)
   * @return ESP_OK on success
   */
  EspResult<void> write_pin(uint8_t pin, bool state);

  /**
   * @brief Set multiple pins output levels at once
   * @param pin_mask Bitmask of pins to set (1 << (pin_num - 1))
   * @param levels Bitmask of output levels for the specified pins
   * @return ESP_OK on success
   */
  EspResult<void> write_pins(uint32_t pin_mask, uint32_t levels);

 private:
  EXIO();
  ~EXIO();
};

}  // namespace HAL
