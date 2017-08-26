/**
 * Ai-Thinker RGBW Light Firmware - Holfuy Module
 *
 * TODO - description here.
 *
 * This file is part of the Ai-Thinker RGBW Light Firmware.
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.

 * Created by Sebastian Andersson <sebastian at bittr dot com>
 * Copyright (c) 2017 Sebastian Andersson
 */

#include <ESPAsyncTCP.h>
#include <ESPAsyncTCPbuffer.h>

typedef enum {
    HS_IDLE,
    HS_CONNECTING,
    HS_SEND_HEADER,
    HS_RECV_HEADER,
    HS_RECV_BODY,
} holfuy_state_t;

static AsyncClient holfuy;
// static AsyncTCPbuffer recv(&holfuy);
static holfuy_state_t hs_state;

static void onData(void*, AsyncClient*, void *data, size_t len)
{
    AiLight.setColor(0x00, 0xff, 0x00);
}

void loopHolfuy()
{
    static uint32_t lastMillis = 0;
    static int connection = 0;

    uint32_t current = millis();
    if((lastMillis + 60000) < current) {
        lastMillis = current;

        AiLight.setColor(0xff, 0x00, 0x00);

        if(holfuy.connected()) {
        AiLight.setColor(0x00, 0xff, 0x00);
            holfuy.close(true);
        AiLight.setColor(0xff, 0xff, 0x00);
        }
        holfuy.onData(onData);
        connection++;
        holfuy.connect("192.168.0.16", 80);
        hs_state = HS_CONNECTING;
    }

    switch(hs_state) {
    case HS_IDLE: break;
    case HS_CONNECTING:
        if(holfuy.connected()) {
            hs_state = HS_SEND_HEADER;
        AiLight.setColor(0x00, 0xff, 0xff);
        }
        break;
    case HS_SEND_HEADER:
        if(holfuy.canSend()) {
        AiLight.setColor(0x00, 0x00, 0xff);
            char tmp[100];
            snprintf(tmp, sizeof(tmp), "Will send (%d) request:\n", connection);
            holfuy.write(tmp);
            // http://api.holfuy.com/live/
            char *host_start = cfg.holfuy_url;
            host_start = strstr(cfg.holfuy_url, "://");
            if(host_start) {
                host_start += 3;
            } else {
                host_start = cfg.holfuy_url;
            }
            char *host_end = strchr(host_start, '/');
            char host[128];
            if(host_end) {
                unsigned int len = host_end - host_start;
                if(len > sizeof(host)) {
                    len = sizeof(host)-1;
                }
                strncpy(host, host_start, len);
                host[len] = 0;
                host_start = host;
            } else {
                host_end = host_start;
            }
            char request[256];
            snprintf(request,
                     sizeof(request),
                     "GET %s?pw=%s&s=%d&m=CSV&su=m/s HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "Connection: Disconnect\r\n\r\n",
                     host_end, cfg.holfuy_pass, cfg.holfuy_id, host_start);
            holfuy.write(request);
            hs_state = HS_RECV_HEADER;
        }
        break;
    case HS_RECV_HEADER:
        break;
    case HS_RECV_BODY:
        break;
    }

        // AiLight.setColor(red, 0, 0);
}

/**
 * @brief Bootstrap function for the holfuy connection
 */
void setupHolfuy()
{
}
