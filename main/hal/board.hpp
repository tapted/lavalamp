#pragma once

#include <esp_io_expander.h>
#include <esp_io_expander_tca9554.h>
#include <esp_lcd_st77916.h>

#include "hal/display/board_display.hpp"
#include "halpp/config_defaults.hpp"

namespace HAL {

// ============================================================================
// I2C Bus Configuration (Shared by Touch, RTC, and IO Expander)
// ============================================================================
namespace I2CConfig {
// Onboard Device Addresses
constexpr uint8_t ADDR_RTC = 0x51;  // PCF85063A
}  // namespace I2CConfig

// ============================================================================
// SD Card Interface (SDIO)
// ============================================================================
namespace SDCardConfig {
constexpr gpio_num_t PIN_CLK = GPIO_NUM_14;  // SD Clock
constexpr gpio_num_t PIN_CMD = GPIO_NUM_17;  // SD Command
constexpr gpio_num_t PIN_D0 = GPIO_NUM_16;   // SD Data 0
constexpr int PIN_D3 = 3;                    // Handled by EXIO_3 (TCA9554)
}  // namespace SDCardConfig

// ============================================================================
// Extended I/O Pins (TCA9554 Port Expander)
// ============================================================================
namespace EXIOConfig {
// These are pins on the TCA9554 (Address 0x20)
constexpr uint8_t TP_RST = 1;   // Touch Reset
constexpr uint8_t LCD_RST = 2;  // LCD Reset
constexpr uint8_t SD_D3 = 3;    // SD Card Data 3
constexpr uint8_t RTC_INT = 4;  // RTC Interrupt

// User Accessible Extraction Header (J1)
constexpr uint8_t PIN_EXT5 = 5;  // J7 header pin 28
constexpr uint8_t PIN_EXT6 = 6;  // J7 header pin 26
constexpr uint8_t PIN_EXT7 = 7;  // J7 header pin 24
constexpr uint8_t PIN_EXT8 = 8;  // J7 header pin 22
}  // namespace EXIOConfig

// ============================================================================
// Miscellaneous & Power
// ============================================================================
namespace SystemConfig {
constexpr gpio_num_t PIN_BAT_ADC = GPIO_NUM_8;  // Battery Voltage Sensing

// Extraction Pins (General Purpose Headers J9)
constexpr gpio_num_t PIN_EXT1 = GPIO_NUM_1;
constexpr gpio_num_t PIN_EXT3 = GPIO_NUM_3;
constexpr gpio_num_t PIN_EXT6 = GPIO_NUM_6;
constexpr gpio_num_t PIN_EXT7 = GPIO_NUM_7;
constexpr gpio_num_t PIN_EXT12 = GPIO_NUM_12;
constexpr gpio_num_t PIN_EXT13 = GPIO_NUM_13;

// J9 Secondary Header Pins
constexpr gpio_num_t PIN_HDR_33 = GPIO_NUM_33;
constexpr gpio_num_t PIN_HDR_34 = GPIO_NUM_34;
constexpr gpio_num_t PIN_HDR_35 = GPIO_NUM_35;
constexpr gpio_num_t PIN_HDR_36 = GPIO_NUM_36;
constexpr gpio_num_t PIN_HDR_37 = GPIO_NUM_37;
}  // namespace SystemConfig
}  // namespace HAL

namespace halpp::board {
struct config : detail::Defaults {
  struct I2CConfig : detail::Defaults::I2CConfig {
    static constexpr gpio_num_t PIN_SDA = GPIO_NUM_11;
    static constexpr gpio_num_t PIN_SCL = GPIO_NUM_10;
  };

  struct Exio : detail::Defaults::Exio {
    static constexpr auto NEW_EXIO_FUNC = esp_io_expander_new_i2c_tca9554;
  };

  struct Display : detail::Defaults::Display {
    static constexpr gpio_num_t PIN_TEARING_EFFECT = GPIO_NUM_18;
    static constexpr gpio_num_t PIN_BACKLIGHT_PWM = GPIO_NUM_5;

    static constexpr esp_io_expander_pin_num_t PIN_MASK_RESET = IO_EXPANDER_PIN_NUM_1;
    static constexpr uint8_t LCD_COMMAND_BITS = 32;

    static constexpr uint16_t WIDTH = 360;
    static constexpr uint16_t HEIGHT = 360;
    static constexpr uint8_t BACKLIGHT_DEFAULT = 90;

    static constexpr auto NEW_PANEL_FUNC = esp_lcd_new_panel_st77916;
    static constexpr void* VENDOR_CONFIG = (void*)&st77916_vendor_config;
    static constexpr bool SKIP_RESET = true;  // Reset is done via EXIO, not GPIO.
    static constexpr bool INVERT_COLORS = true;
  };

  struct lvgl : detail::Defaults::lvgl {
    static constexpr uint32_t BUFFER_FRACTION = 10;
    static constexpr bool USE_RGB565_SWAPPED = true;
  };

  struct Touch : detail::Defaults::Touch {
    static constexpr esp_io_expander_pin_num_t PIN_MASK_RESET = IO_EXPANDER_PIN_NUM_0;
  };

  struct Audio : detail::Defaults::Audio {
    static constexpr uint8_t DEFAULT_VOLUME = 70;
  };
};  // struct config

static_assert(GPIO_NUM_0 == config::System::PIN_BOOT);  // Boot mode control strapping pin
static_assert(GPIO_NUM_1 == HAL::SystemConfig::PIN_EXT1);
static_assert(GPIO_NUM_2 == config::Audio::PIN_MCLK);  // I2S Master Clock (MCLK)
static_assert(GPIO_NUM_3 == HAL::SystemConfig::PIN_EXT3);
static_assert(GPIO_NUM_4 == config::Touch::PIN_INTERRUPT);  // Touch Interrupt
static_assert(GPIO_NUM_5 == config::Display::PIN_BACKLIGHT_PWM);
static_assert(GPIO_NUM_6 == HAL::SystemConfig::PIN_EXT6);
static_assert(GPIO_NUM_7 == HAL::SystemConfig::PIN_EXT7);
static_assert(GPIO_NUM_8 == HAL::SystemConfig::PIN_BAT_ADC);
// What's Pin 9? The docs said it was the backlight PWM, but it's actually GPIO 5.
// 5 was "internal occupancy" maybe that's meant to be 9.
// static_assert(GPIO_NUM_9 == config::Display::PIN_BACKLIGHT_PWM);  // Backlight PWM control
static_assert(GPIO_NUM_10 == config::I2CConfig::PIN_SCL);
static_assert(GPIO_NUM_11 == config::I2CConfig::PIN_SDA);
static_assert(GPIO_NUM_12 == HAL::SystemConfig::PIN_EXT12);
static_assert(GPIO_NUM_13 == HAL::SystemConfig::PIN_EXT13);
static_assert(GPIO_NUM_14 == HAL::SDCardConfig::PIN_CLK);
static_assert(GPIO_NUM_15 == config::Audio::PIN_AMP_ENABLE);  // Speaker Amplifier Mute/Enable
static_assert(GPIO_NUM_16 == HAL::SDCardConfig::PIN_D0);
static_assert(GPIO_NUM_17 == HAL::SDCardConfig::PIN_CMD);
static_assert(GPIO_NUM_18 == config::Display::PIN_TEARING_EFFECT);  // Tearing Effect (TE)
static_assert(GPIO_NUM_19 == config::Usb::PIN_USB_DM);              // USB D-
static_assert(GPIO_NUM_20 == config::Usb::PIN_USB_DP);              // USB D+
static_assert(GPIO_NUM_21 == config::Display::PIN_CHIP_SELECT);     // Chip Select (CS)

// woah where are 22-32?

static_assert(GPIO_NUM_33 == HAL::SystemConfig::PIN_HDR_33);
static_assert(GPIO_NUM_34 == HAL::SystemConfig::PIN_HDR_34);
static_assert(GPIO_NUM_35 == HAL::SystemConfig::PIN_HDR_35);
static_assert(GPIO_NUM_36 == HAL::SystemConfig::PIN_HDR_36);
static_assert(GPIO_NUM_37 == HAL::SystemConfig::PIN_HDR_37);
static_assert(GPIO_NUM_38 == config::Audio::PIN_WS);
static_assert(GPIO_NUM_39 == config::Audio::PIN_DATA_IN);        // Data from Mic (ES7210)
static_assert(GPIO_NUM_40 == config::SpiBus::PIN_SERIAL_CLOCK);  // Serial Clock (SCK)
static_assert(GPIO_NUM_41 == config::SpiBus::PIN_QSPI_SDA_3);
static_assert(GPIO_NUM_42 == config::SpiBus::PIN_QSPI_SDA_2);
static_assert(GPIO_NUM_43 == config::Usb::PIN_UART_TX);
static_assert(GPIO_NUM_44 == config::Usb::PIN_UART_RX);
static_assert(GPIO_NUM_45 == config::SpiBus::PIN_QSPI_SDA_1);
static_assert(GPIO_NUM_46 == config::SpiBus::PIN_QSPI_SDA_0);
static_assert(GPIO_NUM_47 == config::Audio::PIN_DATA_OUT);  // Data to Speaker (ES8311)
static_assert(GPIO_NUM_48 == config::Audio::PIN_BCK);       // I2S Bit Clock

static_assert(IO_EXPANDER_PIN_NUM_0 == config::Touch::PIN_MASK_RESET);
static_assert(IO_EXPANDER_PIN_NUM_1 == config::Display::PIN_MASK_RESET);

}  // namespace halpp::board
