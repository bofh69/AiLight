/**
 * Ai-Thinker RGBW Light Firmware
 *
 * This file is part of the Ai-Thinker RGBW Light Firmware.
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.

 * Created by Sacha Telgenhof <stelgenhof at gmail dot com>
 * (https://www.sachatelgenhof.nl)
 * Copyright (c) 2016 - 2017 Sacha Telgenhof
 */

#define APP_NAME "WindLight"
#define APP_VERSION "0.0.5-dev"
#define APP_AUTHOR "sebastian@bittr.nu"

#define DEVICE_MANUFACTURER "Ai-Thinker"
#define DEVICE_MODEL "RGBW Light"

#include "config.h"

// Fallback
#ifndef MQTT_HOMEASSISTANT_DISCOVERY_ENABLED
#define MQTT_HOMEASSISTANT_DISCOVERY_ENABLED false
#endif

#ifndef MQTT_HOMEASSISTANT_DISCOVERY_PREFIX
#define MQTT_HOMEASSISTANT_DISCOVERY_PREFIX "homeassistant"
#endif

#ifndef REST_API_ENABLED
#define REST_API_ENABLED false
#endif

#include <pgmspace.h>
#include "AiLight.hpp"
#include "ArduinoOTA.h"
#include <ArduinoJson.h>
#include <AsyncMqttClient.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Hash.h>
#include <Ticker.h>
#include <WiFiUdp.h>
#include <vector>

extern "C" {
#include "spi_flash.h"
}

#include "html.gz.h"

#define EEPROM_START_ADDRESS 0
static const int BUFFER_SIZE = JSON_OBJECT_SIZE(10);
#define RECONNECT_TIME 10

// Key names as used internally and in the WebUI
#define KEY_SETTINGS "s"
#define KEY_DEVICE "d"

#define KEY_STATE "state"
#define KEY_TYPE "type"
#define KEY_BRIGHTNESS "brightness"
#define KEY_WHITE "white_value"
#define KEY_COLORTEMP "color_temp"
#define KEY_FLASH "flash"
#define KEY_COLOR "color"
#define KEY_COLOR_R "r"
#define KEY_COLOR_G "g"
#define KEY_COLOR_B "b"
#define KEY_GAMMA_CORRECTION "gamma"
#define KEY_TRANSITION "transition"

#define KEY_HOSTNAME "hostname"
#define KEY_WIFI_SSID "wifi_ssid"
#define KEY_WIFI_PSK "wifi_psk"

#define KEY_HOLFUY_ENABLED "switch_holfuy"
#define KEY_HOLFUY_URL "holfuy_url"
#define KEY_HOLFUY_WIND_SPEED_UNIT "holfuy_wind_speed_unit"
#define KEY_HOLFUY_STATIONS "holfuy_stations"
#define KEY_HOLFUY_PASS "holfuy_pass"
#define KEY_HOLFUY_ID "holfuy_id"
#define KEY_HOLFUY_WIND_MIN "holfuy_wind_min"
#define KEY_HOLFUY_WIND_MAX "holfuy_wind_max"
#define KEY_HOLFUY_DIR_FROM "holfuy_dir_from"
#define KEY_HOLFUY_DIR_TO "holfuy_dir_to"

#define KEY_MQTT_SERVER "mqtt_server"
#define KEY_MQTT_PORT "mqtt_port"
#define KEY_MQTT_USER "mqtt_user"
#define KEY_MQTT_PASSWORD "mqtt_password"
#define KEY_MQTT_STATE_TOPIC "mqtt_state_topic"
#define KEY_MQTT_COMMAND_TOPIC "mqtt_command_topic"
#define KEY_MQTT_LWT_TOPIC "mqtt_lwt_topic"
#define KEY_MQTT_HA_USE_DISCOVERY "switch_ha_discovery"
#define KEY_MQTT_HA_IS_DISCOVERED "mqtt_ha_is_discovered"
#define KEY_MQTT_HA_DISCOVERY_PREFIX "mqtt_ha_discovery_prefix"

#define KEY_REST_API_ENABLED "switch_rest_api"
#define KEY_REST_API_KEY "api_key"

// MQTT Event type definitions
#define MQTT_EVENT_CONNECT 0
#define MQTT_EVENT_DISCONNECT 1
#define MQTT_EVENT_MESSAGE 2

// HTTP
#define HTTP_WEB_INDEX "index.html"
#define HTTP_API_ROOT "api"
#define HTTP_HEADER_APIKEY "API-Key"
#define HTTP_HEADER_SERVER "Server"
#define HTTP_HEADER_CONTENTTYPE "Content-Type"
#define HTTP_HEADER_ALLOW "Allow"
#define HTTP_MIMETYPE_HTML "text/html"
#define HTTP_MIMETYPE_JSON "application/json"

const char *SERVER_SIGNATURE = APP_NAME "/" APP_VERSION;

const char *HTTP_ROUTE_INDEX = "/" HTTP_WEB_INDEX;
const char *HTTP_APIROUTE_ROOT = "/" HTTP_API_ROOT;
const char *HTTP_APIROUTE_ABOUT = "/" HTTP_API_ROOT "/about";
const char *HTTP_APIROUTE_LIGHT = "/" HTTP_API_ROOT "/light";

AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");
AsyncWebServer *server;
AsyncMqttClient mqtt;
std::vector<void (*)(uint8_t, const char *, const char *)> _mqtt_callbacks;
Ticker wifiReconnectTimer;
Ticker mqttReconnectTimer;

const uint8_t INIT_CONFIG_OLD_42 = 0x42;
const uint8_t INIT_CONFIG_NEW = 0x44;

// Configuration structure that gets stored to the EEPROM
struct config_42_t {
  uint8_t ic;                 // Initialization check
  bool is_on;                 // Operational state (true == on)
  uint8_t brightness;         // Brightness level
  uint8_t color_temp;         // Colour temperature
  Color color;                // RGBW channel levels
  uint16_t mqtt_port;         // MQTT Broker port
  char hostname[128];         // Hostname/Identifier
  char wifi_ssid[32];         // WiFi SSID
  char wifi_psk[63];          // WiFi Passphrase Key
  char mqtt_server[128];      // Server/hostname of the MQTT Broker
  char mqtt_user[64];         // Username used for connecting to the MQTT Broker
  char mqtt_password[64];     // Password used for connecting to the MQTT Broker
  char mqtt_state_topic[128]; // MQTT Topic for publishing the state
  char mqtt_command_topic[128]; // MQTT Topic for receiving commands
  char mqtt_lwt_topic[128]; // MQTT Topic for publising Last Will and Testament
  bool gamma;               // Gamma Correction enabled or not
  bool mqtt_ha_use_discovery; // Home Assistant MQTT discovery enabled or not
  bool mqtt_ha_is_discovered; // Has this device already been discovered or not
  char mqtt_ha_disc_prefix[32]; // MQTT Discovery prefix for Home Assistant
  bool api;                     // REST API enabled or not
  char api_key[32];             // API Key

  bool holfuy_enabled;        // Is holfuy service enabled?
  char holfuy_url[128];       // URL - http://api.holfuy.com/live/
  char holfuy_pass[128];      // password
  uint16_t holfuy_id;         // Id of station - 101
  float holfuy_wind_min;      // Minimum wind speed, m/s
  float holfuy_wind_max;      // Maximum wind speed, m/s
  uint16_t holfuy_dir_from;   // The wind direction in degrees.
  uint16_t holfuy_dir_to;     // The wind direction in degrees.
};

struct config_t {
  struct holfuy_cfg_t {
    char pass[66];      // password
    uint16_t id;         // Id of station - 101
    float wind_min;      // Minimum wind speed, m/s
    float wind_max;      // Maximum wind speed, m/s
    uint16_t dir_from;   // The wind direction in degrees.
    uint16_t dir_to;     // The wind direction in degrees.
  };

  uint8_t ic;                 // Initialization check
  bool is_on;                 // Operational state (true == on)
  uint8_t brightness;         // Brightness level
  uint8_t color_temp;         // Colour temperature
  Color color;                // RGBW channel levels
  uint16_t mqtt_port;         // MQTT Broker port
  char hostname[128];         // Hostname/Identifier
  char wifi_ssid[32];         // WiFi SSID
  char wifi_psk[63];          // WiFi Passphrase Key
  char mqtt_server[128];      // Server/hostname of the MQTT Broker
  char mqtt_user[64];         // Username used for connecting to the MQTT Broker
  char mqtt_password[64];     // Password used for connecting to the MQTT Broker
  char mqtt_state_topic[128]; // MQTT Topic for publishing the state
  char mqtt_command_topic[128]; // MQTT Topic for receiving commands
  char mqtt_lwt_topic[128];   // MQTT Topic for publising Last Will and Testament
  bool gamma;                 // Gamma Correction enabled or not
  bool mqtt_ha_use_discovery; // Home Assistant MQTT discovery enabled or not
  bool mqtt_ha_is_discovered; // Has this device already been discovered or not
  char mqtt_ha_disc_prefix[32]; // MQTT Discovery prefix for Home Assistant
  bool api;                   // REST API enabled or not
  char api_key[32];           // API Key

  bool holfuy_enabled;        // Is holfuy service enabled?
  char holfuy_url[128];       // URL - http://api.holfuy.com/live/
  holfuy_cfg_t holfuy_stations[8];
  uint8_t holfuy_nr_stations; // Nr of stations that are used.
  char holfuy_wind_speed_unit[6]; // "knots"
};

union stored_config_t {
    config_42_t old;
    config_t current;
} stored_cfg;

config_t &cfg = stored_cfg.current;

// Globals for flash
bool flash = false;
bool startFlash = false;
uint16_t flashLength = 0;
uint32_t flashStartTime = 0;
Color flashColor;
uint8_t flashBrightness = 0;

// Globals for current state
Color currentColor;
uint8_t currentBrightness;
bool currentState;

// Globals for transition/fade
bool state = false;
uint16_t transitionTime = 0;
uint32_t startTransTime = 0;
int stepR, stepG, stepB, stepW, stepBrightness = 0;
uint16_t stepCount = 0;
Color transColor;
uint8_t transBrightness = 0;

#define SerialPrint(format, ...)                                               \
  StreamPrint_progmem(Serial, PSTR(format), ##__VA_ARGS__)
#define StreamPrint(stream, format, ...)                                       \
  StreamPrint_progmem(stream, PSTR(format), ##__VA_ARGS__)

#ifdef DEBUG
#define DEBUGLOG(...) SerialPrint(__VA_ARGS__)
#else
#define DEBUGLOG(...)
#endif

/**
 * @brief Template to allow any type of data to be written to the EEPROM
 *
 * Although this template allows any type of data, it will primarily be used to
 * save the config_t struct.
 *
 * @param  value the data to be written to the EEPROM
 *
 * @return void
 */
template <class T> void EEPROM_write(const T &value) {
  int ee = EEPROM_START_ADDRESS;
  const byte *p = (const byte *)(const void *)&value;
  for (uint16_t i = 0; i < sizeof(value); i++)
    EEPROM.write(ee++, *p++);

  EEPROM.commit();
}

/**
 * @brief Template to allow any type of data to be read from the EEPROM
 *
 * Although this template allows any type of data, it will primarily be used to
 * read the EEPROM contents into the config_t struct.
 *
 * @param  value the data to be loaded into from the EEPROM
 *
 * @return void
 */
template <class T> void EEPROM_read(T &value) {
  int ee = EEPROM_START_ADDRESS;
  byte *p = (byte *)(void *)&value;
  for (uint16_t i = 0; i < sizeof(value); i++)
    *p++ = EEPROM.read(ee++);
}
