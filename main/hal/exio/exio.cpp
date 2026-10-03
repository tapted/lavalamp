#include "hal/exio/exio.hpp"

#include "halpp/config.hpp"
#include "halpp/i2c/i2c_master.hpp"

static constexpr const char TAG[] = "EXIO";

using halpp::config;

namespace HAL {

esp_io_expander_handle_t EXIO::handle = nullptr;

EXIO::EXIO() {
  init();
}

EXIO::~EXIO() {
  _deinit();
}

esp_err_t EXIO::init() {
  if (handle) {
    ESP_LOGW(TAG, "EXIO already initialized");
    return ESP_OK;
  }

  if (EspError err = config::Exio::NEW_EXIO_FUNC(I2CMaster::instance().get_bus_handle(),
                                                 config::Exio::I2C_ADDRESS, &handle)) {
    return err.log(TAG, "Failed to initialize EXIO device");
  }

  if (auto err = EspError::check(set_pins_mode(0xff, IO_EXPANDER_OUTPUT))) {
    deinit();
    return err.log(TAG, "Failed to set EXIO pin modes");
  }

  ESP_LOGI(TAG, "EXIO I2C device added");
  return ESP_OK;
}

esp_err_t EXIO::_deinit() {
  if (!handle) return ESP_OK;

  esp_io_expander_handle_t temp_handle = handle;
  handle = nullptr;
  if (auto err = EspError::check(esp_io_expander_del(temp_handle))) {
    return err.log(TAG, "Failed to delete EXIO device");
  }

  ESP_LOGI(TAG, "EXIO deinitialized");
  return ESP_OK;
}

EspResult<> EXIO::set_pins_mode(uint32_t pin_num_mask, esp_io_expander_dir_t dir) {
  return esp_io_expander_set_dir(handle, pin_num_mask, dir);
}

EspResult<uint8_t> EXIO::read_pin(uint8_t pin) {
  auto all_pins = read_pins();
  if (!all_pins) return all_pins.error();
  return EspResult<uint8_t>::ok((*all_pins >> (pin - 1)) & 0x01);
}

EspResult<uint32_t> EXIO::read_pins() {
  uint32_t level_mask;
  if (auto err = EspError::check(esp_io_expander_get_level(handle, 0xff, &level_mask))) {
    return err.log(TAG, "Failed to read pin levels");
  }
  return EspResult<uint32_t>::ok(level_mask);
}

EspResult<> EXIO::write_pin(uint8_t pin, bool state) {
  if (pin == 0 || pin > 32) {
    ESP_LOGE(TAG, "Invalid pin number: %d. Must be between 1 and 32.", pin);
    return ESP_ERR_INVALID_ARG;
  }
  EspResult<> err = esp_io_expander_set_level(handle, 1 << (pin - 1), state ? 1 : 0);
  return err.log_error(TAG, "Failed to write pin");
}

EspResult<> EXIO::write_pins(uint32_t pin_mask, uint32_t levels) {
  // Isolate which pins in the mask need to be set HIGH / LOW
  uint32_t high_mask = pin_mask & levels;
  uint32_t low_mask = pin_mask & ~levels;

  if (high_mask) {
    if (auto err = EspError::check(esp_io_expander_set_level(handle, high_mask, 1))) {
      return err.log(TAG, "Failed to set pins high");
    }
  }

  if (low_mask) {
    if (auto err = EspError::check(esp_io_expander_set_level(handle, low_mask, 0))) {
      return err.log(TAG, "Failed to set pins low");
    }
  }

  return ESP_OK;
}

}  // namespace HAL