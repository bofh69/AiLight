/**
 * Ai-Thinker RGBW Light Firmware - Holfuy Module
 *
 * A module to connect to a holfuy weather station and then set
 * the light colour from the wind speed & direction or the temperature.
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
 * Put the body-parser in its own class.
 * Add support for temperature reporting.
 * Unit tests requires subscription to PlatformIO. :-(
 */

typedef enum {
    HS_IDLE,
    HS_CONNECTING,
    HS_SEND_HEADER,
    HS_RECV_HEADER,
    HS_RECV_HEADER_CR1,
    HS_RECV_HEADER_LF1,
    HS_RECV_HEADER_CR2,
    HS_RECV_BODY,
} holfuy_state_t;
static holfuy_state_t hs_state;

static int nr_commas;
static long tmp_nr;
static int n_decimals;

template <typename Sample_t>
class EvaluateSamples {
public:
    virtual void inspect(Sample_t sample);
};

class Samples {
public:
    Samples(int n_samples) : n_samples(n_samples) {
        samples = new Sample[n_samples];
    }
    ~Samples() {
        delete[] samples;
    }

    struct Sample {
        float speed, dir, temperature;
    };

    void add_sample(float speed, float dir, float temperature) {
        samples[sample_idx].speed = speed;
        samples[sample_idx].dir = dir;
        samples[sample_idx].temperature = temperature;
        ++total_samples;
        if(++sample_idx >= n_samples)
            sample_idx = 0;
    }
    const Sample &latest_sample() {
        int l = sample_idx-1;
        if(l < 0) sample_idx = n_samples - 1;
        return samples[l];
    }

    void const evaluate_samples(EvaluateSamples<const Sample &> &evaluator) {
        int max = total_samples;
        if(max > n_samples)
            max = n_samples;

        for(int i = 0; i < max; i++) {
            const Sample &s = samples[i];
            evaluator.inspect(s);
        }
    }

private:
    Sample *samples;
    int n_samples;
    int total_samples, sample_idx;
};

static Samples samples(HOLFUY_SAMPLES_TO_KEEP);

class SamplesToColour : public EvaluateSamples<const Samples::Sample &> {
public:
    enum colour_t {
        YELLOW,
        GREEN,
        RED,
        BLUE,
    };

    virtual void inspect(const Samples::Sample &s) {
        enum colour_t tmp = sample_within_direction(s) ? GREEN : RED;
        if(s.speed < cfg.holfuy_wind_min) tmp = RED;
        if(s.speed > cfg.holfuy_wind_max)
            tmp = (tmp != RED) ? BLUE : RED;
        switch(tmp) {
        case GREEN:
            if(colour == YELLOW)
                colour = GREEN;
            break;
        case RED:
            colour = RED;
            break;
        case BLUE:
            if(colour != RED)
               colour = BLUE;
            break;
        case YELLOW:
            break; /* Can't happen. */
        }
    }

    colour_t get_result() {
        return colour;
    }
private:
    colour_t colour = YELLOW;

    bool sample_within_direction(const Samples::Sample &s) {
        if(cfg.holfuy_dir_from < cfg.holfuy_dir_to) {
            return (s.dir >= cfg.holfuy_dir_from) &&
                   (s.dir <= cfg.holfuy_dir_to);
        } else {
            return (s.dir >= cfg.holfuy_dir_from) ||
                   (s.dir <= cfg.holfuy_dir_to);
        }
    }
};

static AsyncClient holfuy;

static void update_light_colour()
{
    SamplesToColour stc;
    samples.evaluate_samples(stc);
    auto colour = stc.get_result();
    switch(colour) {
    case SamplesToColour::YELLOW: AiLight.setColor(255, 255, 0); break;
    case SamplesToColour::GREEN: AiLight.setColor(0, 255, 0); break;
    case SamplesToColour::RED: AiLight.setColor(255, 0, 0); break;
    case SamplesToColour::BLUE: AiLight.setColor(0, 0, 255); break;
    }
}

float get_number() {
    float ret = tmp_nr;
    while(n_decimals-- > 0) {
       ret /= 10.0;
    }
    return ret;
}

static void onData(void*, AsyncClient*, void *indata, size_t len)
{
    static float speed, dir, temperature;

    const char *data = static_cast<const char*>(indata);
    while(len-- > 0) {
        switch(hs_state) {
        case HS_IDLE:
        case HS_CONNECTING:
        case HS_SEND_HEADER:
            /* Should not be able to happen. */
            holfuy.close();
            break;
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
                case 4: // Windspeed
                    speed = get_number();
                    break;
                case 5: // Windspeed gust
                    break;
                case 7: // Direction
                    dir = get_number();
                    break;
                case 8: // Temp
                    temperature = get_number();
                    break;
                case 9: { // Humidity
                    samples.add_sample(speed, dir, temperature);
                    update_light_colour();
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

void get_host_and_port(unsigned int host_len, char *host, int *port, const char ** path)
{
    const char *_path;
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
    if((lastMillis + HOLFUY_TIME_BETWEEN_SAMPLES) < current) {
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
    case HS_IDLE:
    case HS_RECV_HEADER:
    case HS_RECV_HEADER_CR1:
    case HS_RECV_HEADER_LF1:
    case HS_RECV_HEADER_CR2:
    case HS_RECV_BODY:
        /* Nothing to do here */
        break;
    case HS_CONNECTING:
        if(holfuy.connected()) {
            hs_state = HS_SEND_HEADER;
        }
        break;
    case HS_SEND_HEADER:
        if(holfuy.canSend()) {
            char host[128];
            const char *path;
            get_host_and_port(sizeof(host), host, NULL, &path);
            char request[256];
            snprintf(request,
                     sizeof(request),
                     "GET %s?pw=%s&s=%d&m=CSV&su=m/s HTTP/1.1\r\n"
                     "User-Agent: WindLight/0.0.1\r\n"
                     "Host: %s\r\n"
                     "X-Request-Nr: %u\r\n"
                     "Connection: Disconnect\r\n\r\n",
                     path, cfg.holfuy_pass, cfg.holfuy_id, host, connection);
            holfuy.write(request);
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
    if(cfg.holfuy_enabled)
        update_light_colour();
}
