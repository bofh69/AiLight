/**
 * Ai-Thinker RGBW Light Firmware - overriding configuration
 *
 * This file is part of the Ai-Thinker RGBW Light Firmware.
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.

 * Created by Sacha Telgenhof <stelgenhof at gmail dot com>
 * (https://www.sachatelgenhof.nl)
 * Copyright (c) 2016 - 2017 Sacha Telgenhof
 */

#include "config-default.h"

/* Override settings from config-default.h */

#define HOSTNAME "WindLight"
#define ADMIN_PASSWORD "hinotori"

/**
 * WiFi
 * ---------------------------
 * Use the below variables to set the default WiFi settings of your Ai-Thinker
 * RGBW Light. These will be used as the factory defaults of your device. If no
 * SSID/PSK are provided, your Ai-Thinker RGBW light will start in AP mode.
 */
#define WIFI_SSID ""
#define WIFI_PSK ""
#define WIFI_OUTPUT_POWER 20.5 // 20.5 is the maximum output power
