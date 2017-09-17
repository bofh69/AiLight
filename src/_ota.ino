/**
 * Ai-Thinker RGBW Light Firmware - OTA Module
 *
 * The OTA (Over The Air) module holds all the code to manage over the air
 * firmware updates.
 *
 * This file is part of the Ai-Thinker RGBW Light Firmware.
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.

 * Created by Sacha Telgenhof <stelgenhof at gmail dot com>
 * (https://www.sachatelgenhof.nl)
 * Copyright (c) 2016 - 2017 Sacha Telgenhof
 */

#include <ESP8266httpUpdate.h>

/**
 * @brief Bootstrap function for the OTA UDP service
 */
void setupOTA() {

  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setHostname(cfg.hostname);
  ArduinoOTA.setPassword(ADMIN_PASSWORD);

  DEBUGLOG("[OTA ] Server running at %s:%u\n", ArduinoOTA.getHostname().c_str(),
           OTA_PORT);

  ArduinoOTA.onStart([]() {
    DEBUGLOG("[OTA ] Start\n");
    events.send("start", "ota");

    ws.enable(false); // Disable WebSocket client connections
    ws.closeAll(); // Close WebSocket client connections
  });

  ArduinoOTA.onEnd([]() {
    DEBUGLOG("\n[OTA ] End\n");
    events.send("end", "ota");
  });

  ArduinoOTA.onProgress([](uint32_t progress, uint32_t total) {
    uint8_t pp = (progress / (total / 100));
    DEBUGLOG("Progress: %u%%\r", pp);

    char p[6];
    sprintf(p, "p-%u", pp);
    events.send(p, "ota");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    DEBUGLOG("\n[OTA ] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      DEBUGLOG("Authentication Failed\n");
    else if (error == OTA_BEGIN_ERROR)
      DEBUGLOG("Begin Failed\n");
    else if (error == OTA_CONNECT_ERROR)
      DEBUGLOG("Connection Failed\n");
    else if (error == OTA_RECEIVE_ERROR)
      DEBUGLOG("Receive Failed\n");
    else if (error == OTA_END_ERROR)
      DEBUGLOG("End Failed\n");
  });

  ArduinoOTA.begin();
}

bool try_upgrade = false;

static void upgrade_triggered()
{
    try_upgrade = false;
    ESPhttpUpdate.rebootOnUpdate(true);
    auto ret = ESPhttpUpdate.update(UPDATE_URL);
    if(ret == HTTP_UPDATE_NO_UPDATES) {
        ws.textAll("{\"upgrade_error\":\"No new firmware was found.\"}");
        return;
    }
    auto errMsg = "{\"upgrade_error\":\"Upgrade failed: " + ESPhttpUpdate.getLastErrorString() + "\"}";
    ws.textAll(errMsg);
}

/**
 * @brief Listen to OTA requests
 */
void loopOTA() {
  ArduinoOTA.handle();
  if(try_upgrade) upgrade_triggered();
}
