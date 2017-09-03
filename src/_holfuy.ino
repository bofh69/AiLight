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

/* TODO:
 * Put the http-client in its own class.
 * Put the sample handling in its own class.
 * Refactor.
 * Unit tests requires subscription to PlatformIO. :-(
 */

#define N_SAMPLES (15)

typedef enum {
    HS_IDLE,
    HS_CONNECTING,
    HS_SEND_HEADER,
    HS_RECV_HEADER,
    HS_RECV_HEADER_CR1,
    HS_RECV_HEADER_LF1,
    HS_RECV_HEADER_CR2,
    HS_RECV_HEADER_LF2,
    HS_RECV_BODY,
} holfuy_state_t;
static holfuy_state_t hs_state;
static int nr_commas;
static long tmp_nr;
static int n_decimals;

typedef struct {
    long speed, dir;
    float temperature;
} sample_t;
static sample_t samples[N_SAMPLES];
static int sample_idx;
static int total_samples;

static AsyncClient holfuy;

static void update_light_colour()
{
    // TODO:
    // Check that the wind is inside the desired range.
    // Switch the lamp's colour to red or green depending on the result.
    // Support for a blue state as well, when the wind has the right direction,
    // but is too hard?
    sample_t *s = &samples[sample_idx];
    AiLight.setColor(s->speed & 255,
                     s->dir & 255,
                     int(s->temperature) & 255);
}

static void onData(void*, AsyncClient*, void *indata, size_t len)
{
    const char *data = static_cast<const char*>(indata);
    while(len-- > 0) {
        switch(hs_state) {
        case HS_RECV_HEADER:
            if(*data == '\r')
              hs_state = HS_RECV_HEADER_CR1;
            break;
        case HS_RECV_HEADER_CR1:
            if(*data == '\n')
              hs_state = HS_RECV_HEADER_LF1;
            else if(*data != '\r')
              hs_state = HS_RECV_HEADER;
            break;
        case HS_RECV_HEADER_LF1:
            if(*data == '\r')
              hs_state = HS_RECV_HEADER_CR2;
            else hs_state = HS_RECV_HEADER;
            break;
        case HS_RECV_HEADER_CR2:
            if(*data == '\n') {
              hs_state = HS_RECV_BODY;
              nr_commas = 0;
              tmp_nr = 0;
              n_decimals = -100;
            } else if(*data == '\r')
              hs_state = HS_RECV_HEADER_CR1;
            else hs_state = HS_RECV_HEADER;
            break;
        case HS_RECV_BODY:
            if(*data == ',') {
                switch(nr_commas++) {
                case 4: // Windspeed #1
                    samples[sample_idx].speed = tmp_nr;
                    break;
                case 5: // Windspeed #2
                    break;
                case 7: // Direction
                    samples[sample_idx].dir = tmp_nr;
                    break;
                case 8: // Temp
                    samples[sample_idx].temperature = tmp_nr;
                    while(n_decimals-- > 0) {
                        samples[sample_idx].temperature /= 10.0;
                    }
                    break;
                case 9: { // ???
                    update_light_colour();
                    sample_idx++;
                    total_samples++;
                    if(sample_idx >= N_SAMPLES) sample_idx = 0;
                } break;
                case 10: // preassure
                    holfuy.close();
                    return;
                }
                tmp_nr = 0;
                n_decimals = -100;
            } else if(*data == '.') {
                n_decimals = 0;
            } else if((*data >= '0') &&
                      (*data <= '9')) {
                // This does count leading zeros as well, but since we
                // only care about decimals after the dot, it does not matter.
                n_decimals++;
                tmp_nr = tmp_nr * 10 + (*data - '0');
            }
            break;
        }
        data++;
    }
}

void get_host_and_port(unsigned int host_len, char *host, int *port, char **path)
{
    char *_path;
    int _port;
    if(!port) port = &_port;
    if(!path) path = &_path;
    if(!host || !host_len) return;

    *port = 80;
    char *host_start = cfg.holfuy_url;
    host_start = strstr(cfg.holfuy_url, "://");
    if(host_start) {
        host_start += 3;
    } else {
        host_start = cfg.holfuy_url;
    }
    char *host_end = strchr(host_start, ':');
    if(!host_end)
        host_end = strchr(host_start, '/');
    if(host_end) {
        unsigned int len = host_end - host_start;
        if(len > host_len) {
            len = host_len-1;
        }
        strncpy(host, host_start, len);
        host[len] = 0;
        if(*host_end == ':') {
            *port = atoi(host_end+1);
            if(!*port) *port = 80;
            host_end = strchr(host_end, '/');
        }
        *path = host_end;
    } else {
        unsigned int len = strlen(host_start);
        if(len > host_len) {
            len = host_len-1;
        }
        strncpy(host, host_start, len);
        host[len] = 0;
        *path = "/";
    }
}

void loopHolfuy()
{
    static uint32_t lastMillis = 0;
    static int connection = 0;

    uint32_t current = millis();
    if((lastMillis + 30000) < current) {
        lastMillis = current;

        if(holfuy.connected()) {
            holfuy.close(true);
        }
        holfuy.onData(onData);
        connection++;
        char host[128];
        int port;
        get_host_and_port(sizeof(host), host, &port, NULL);
        holfuy.connect(host, port);
        hs_state = HS_CONNECTING;
    }

    switch(hs_state) {
    case HS_IDLE: break;
    case HS_CONNECTING:
        if(holfuy.connected()) {
            hs_state = HS_SEND_HEADER;
        }
        break;
    case HS_SEND_HEADER:
        if(holfuy.canSend()) {
            char host[128];
            char *path;
            get_host_and_port(sizeof(host), host, NULL, &path);
            char request[256];
            snprintf(request,
                     sizeof(request),
                     "GET %s?pw=%s&s=%d&m=CSV&su=m/s HTTP/1.1\r\n"
                     "User-Agent: WindLight/0.0.1\r\n"
                     "Host: %s\r\n"
                     "Request-Nr: %u\r\n"
                     "Connection: Disconnect\r\n\r\n",
                     path, cfg.holfuy_pass, cfg.holfuy_id, host, connection);
            holfuy.write(request);
            // holfuy.send();
            hs_state = HS_RECV_HEADER;
        }
        break;
    }
}

/**
 * @brief Bootstrap function for the holfuy connection
 */
void setupHolfuy()
{
}
