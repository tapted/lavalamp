#include "esp_io_expander_tca9554.h"

#include "hal/exio/exio.hpp"
#include "halpp/i2c/i2c_master.hpp"
#include "hal/board.hpp"

namespace HAL {

static constexpr uint8_t TCA9554_ADDRESS = HAL::I2CConfig::ADDR_EXIO;  // 0x20

EspResult<esp_io_expander_handle_t> init_board_exio() {
  esp_io_expander_handle_t handle;
  esp_err_t err = esp_io_expander_new_i2c_tca9554(I2CMaster::instance().get_bus_handle(),
                                                  TCA9554_ADDRESS, &handle);
  if (err != ESP_OK) return err;
  return handle;
}

}  // namespace HAL