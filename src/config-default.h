/**
 * Ai-Thinker RGBW Light Firmware - Configuration
 *
 * This file is part of the Ai-Thinker RGBW Light Firmware.
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.

 * Created by Sacha Telgenhof <stelgenhof at gmail dot com>
 * (https://www.sachatelgenhof.nl)
 * Copyright (c) 2016 - 2017 Sacha Telgenhof
 */

/**
 * Light
 * ---------------------------
 * Use the below variables to set the default behaviour of your Ai-Thinker RGBW
 * Light. These will be used as the factory defaults of your device.
 */
#define LIGHT_STATE false
#define LIGHT_BRIGHTNESS 0
#define LIGHT_COLOR_TEMPERATURE 0
#define LIGHT_COLOR_RED 64
#define LIGHT_COLOR_GREEN 64
#define LIGHT_COLOR_BLUE 64
#define LIGHT_COLOR_WHITE 0

#define HOSTNAME "WindLight"
#define ADMIN_PASSWORD "hinotori"

/**
 * OTA (Over The Air) Updates
 * ---------------------------
 */
#define OTA_PORT 8266
#define UPDATE_URL "http://windlight.se/firmware/from-" APP_VERSION "/firmware.bin"

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

/**
 * Holfuy
 * ---------------------------
 * The default values for the holfuy module.
 */
#define HOLFUY_SAMPLES_TO_KEEP      15
#define HOLFUY_TIME_BETWEEN_SAMPLES 60000 /* ms */
#define HOLFUY_ENABLED  false
#define HOLFUY_URL      "http://api.holfuy.com/live/"
#define HOLFUY_PASS     "pass"
#define HOLFUY_ID       101
#define HOLFUY_WIND_MIN 3.0
#define HOLFUY_WIND_MAX 7.5
#define HOLFUY_DIR_FROM 270
#define HOLFUY_DIR_TO   30
#define HOLFUY_WIND_SPEED_UNIT "m_s"

/**
 * MQTT
 * ---------------------------
 * Use the below variables to set the default MQTT settings of your Ai-Thinker
 * RGBW Light. These will be used as the factory defaults of your device and
 * making the connection to your Home Assistant instance. Most of these settings
 * can also be changed in the UI environment.
 */
#define MQTT_PORT 1883
#define MQTT_SERVER ""
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_RECONNECT_TIME 10000
#define MQTT_QOS_LEVEL 0
#define MQTT_RETAIN false
#define MQTT_KEEPALIVE 30

#define MQTT_PAYLOAD_ON "ON"
#define MQTT_PAYLOAD_OFF "OFF"

#define MQTT_STATUS_ONLINE "online"
#define MQTT_STATUS_OFFLINE "offline"

#define MQTT_HOMEASSISTANT_DISCOVERY_ENABLED false
#define MQTT_HOMEASSISTANT_DISCOVERY_PREFIX "homeassistant"

/**
 * HTTP
 * ---------------------------
 * Use the below variables to set HTTP settings of your Ai-Thinker
 * RGBW Light. These will be used as the factory defaults of your device.
 */
#define REST_API_ENABLED false
