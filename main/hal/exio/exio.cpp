#include "hal/exio/exio.hpp"

#include "halpp/config.hpp"
#include "halpp/i2c/i2c_master.hpp"

static constexpr const char TAG[] = "Exio";

using halpp::config;

namespace halpp {

EspResult<> Exio::init() {
  if (io_handle_) return ESP_OK;

  if (EspError err = config::Exio::NEW_EXIO_FUNC(I2CMaster::instance().get_bus_handle(),
                                                 config::Exio::I2C_ADDRESS, &io_handle_)) {
    return err.log(TAG, "Failed to initialize EXIO device");
  }

  if (EspError err = set_pins_mode(0xff, IO_EXPANDER_OUTPUT)) {
    return err.log(TAG, "Failed to set EXIO pin modes");
  }

  ESP_LOGI(TAG, "EXIO I2C device added at address 0x%02X", config::Exio::I2C_ADDRESS);
  return ESP_OK;
}

Exio::~Exio() {
  if (!io_handle_) return;

  if (EspError err = esp_io_expander_del(io_handle_)) {
    err.log(TAG, "Failed to delete EXIO device");
  } else {
    ESP_LOGI(TAG, "EXIO deinitialized");
  }
}

EspResult<> Exio::set_pins_mode(uint32_t pin_num_mask, esp_io_expander_dir_t dir) {
  return esp_io_expander_set_dir(io_handle_, pin_num_mask, dir);
}

EspResult<uint8_t> Exio::read_pin(uint8_t pin) {
  auto all_pins = read_pins();
  if (!all_pins) return all_pins.error();
  return EspResult<uint8_t>::ok((*all_pins >> (pin - 1)) & 0x01);
}

EspResult<uint32_t> Exio::read_pins() {
  uint32_t level_mask;
  if (EspError err = esp_io_expander_get_level(io_handle_, 0xff, &level_mask)) {
    return err.log(TAG, "Failed to read pin levels");
  }
  return EspResult<uint32_t>::ok(level_mask);
}

EspResult<> Exio::write_pin(uint8_t pin, bool state) {
  if (pin == 0 || pin > 32) {
    ESP_LOGE(TAG, "Invalid pin number: %d. Must be between 1 and 32.", pin);
    return ESP_ERR_INVALID_ARG;
  }
  EspResult<> err = esp_io_expander_set_level(io_handle_, 1 << (pin - 1), state ? 1 : 0);
  return err.log_error(TAG, "Failed to write pin");
}

EspResult<> Exio::write_pins(uint32_t pin_mask, uint32_t levels) {
  // Isolate which pins in the mask need to be set HIGH / LOW
  uint32_t high_mask = pin_mask & levels;
  uint32_t low_mask = pin_mask & ~levels;

  if (high_mask) {
    if (EspError err = esp_io_expander_set_level(io_handle_, high_mask, 1)) {
      return err.log(TAG, "Failed to set pins high");
    }
  }

  if (low_mask) {
    if (EspError err = esp_io_expander_set_level(io_handle_, low_mask, 0)) {
      return err.log(TAG, "Failed to set pins low");
    }
  }

  return ESP_OK;
}

}  // namespace halpp