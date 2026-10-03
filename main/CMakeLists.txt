set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(HALPP_SOURCES
  "../deps/halpp/src/halpp/display/backlight.cpp"
  "../deps/halpp/src/halpp/display/display.cpp"
  "../deps/halpp/src/halpp/display/generic_display.cpp"
  "../deps/halpp/src/halpp/i2c/i2c_master.cpp"
  "../deps/halpp/src/halpp/ledc/channel.cpp"
  "../deps/halpp/src/halpp/ledc/timer.cpp"
)

set(PROJECT_SOURCES
  "hal/display/board_display.cpp"
  "hal/exio/exio.cpp"
  "lavalamp_display.cpp"
  "lavalamp.cpp"
)

set(PROJECT_INCLUDE_DIRS
  "."
  "../deps/halpp/src"
)

set(PROJECT_REQUIRES
  esp_driver_i2s
  esp_driver_ledc
  esp_io_expander_tca9554
  esp_lcd
  esp_lcd_st77916
  esp_lcd_touch_cst816s
  espbase
)

idf_component_register(
  SRCS ${PROJECT_SOURCES} ${HALPP_SOURCES}
  INCLUDE_DIRS ${PROJECT_INCLUDE_DIRS}
  REQUIRES ${PROJECT_REQUIRES}
)
