#pragma once

#include <esp_lcd_st77916.h>

extern st77916_vendor_config_t st77916_vendor_config;

void start_reset_display_and_touch();
void wait_for_reset_display_and_touch();