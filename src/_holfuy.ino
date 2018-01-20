/**
 * Ai-Thinker RGBW Light Firmware - Holfuy Module
 *
 * A module to connect to a holfuy weather station and then set
 * the light colour from the wind speed & direction or the temperature.
 *
 * This file is part of the Ai-Thinker RGBW Light Firmware.
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.

 * Created by Sebastian Andersson <sebastian at bittr dot nu>
 * Copyright (c) 2017 Sebastian Andersson
 */

#include <ESPAsyncTCP.h>

 namespace holfuy {
     static AsyncClient sock;

     enum colour_t {
         NONE,
         YELLOW,
         GREEN,
         RED,
         BLUE,
     };

     template <typename Sample_t>
     class EvaluateSamples {
     public:
         virtual void inspect(Sample_t sample);
     };

     class holfuy_t {
     public:

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

         Samples samples;
         uint32_t lastMillis = 0;

         holfuy_t() : samples(Samples(HOLFUY_SAMPLES_TO_KEEP))
         {
         }

         class Parser {
         public:
             virtual bool parse(const char c) = 0;
             virtual void done() = 0;
             virtual ~Parser() {};
         };

         template <typename B>
         class HttpResponseParser : Parser {
             enum parse_state {
                 RECV_HEADER,
                 RECV_HEADER_CR1,
                 RECV_HEADER_LF1,
                 RECV_HEADER_CR2,
                 RECV_BODY,
             };

             enum parse_state state;
             B *bodyParser;
         public:
             HttpResponseParser(B *bodyParser) : state(RECV_HEADER), bodyParser(bodyParser) {}
             virtual ~HttpResponseParser() {
                 delete bodyParser;
             };

             bool parse(const char c) {
                 switch(state) {
                 case RECV_HEADER:
                     /* Ignore the headers, search for its end */
                     if(c == '\r')
                     state = RECV_HEADER_CR1;
                     break;
                 case RECV_HEADER_CR1:
                     if(c == '\n')
                     state = RECV_HEADER_LF1;
                     else if(c != '\r') {
                         state = RECV_HEADER;
                         return parse(c);
                     }
                     break;
                 case RECV_HEADER_LF1:
                     if(c == '\r')
                     state = RECV_HEADER_CR2;
                     else {
                         state = RECV_HEADER;
                         return parse(c);
                     }
                     break;
                 case RECV_HEADER_CR2:
                     if(c == '\n') {
                         state = RECV_BODY;
                     } else if(c == '\r')
                     state = RECV_HEADER_CR1;
                     else {
                         state = RECV_HEADER;
                         return parse(c);
                     }
                     break;
                 case RECV_BODY:
                     return bodyParser->parse(c);
                     break;
                 }
                 return true;
             }

             void done() {
                 if(state == RECV_BODY)
                     bodyParser->done();
             }
         };

         class SamplesToColour : public EvaluateSamples<const Samples::Sample &> {
             const config_t::holfuy_cfg_t &station_cfg;
             colour_t colour = YELLOW;

             bool sample_within_direction(const Samples::Sample &s) {
                 if(station_cfg.dir_from < station_cfg.dir_to) {
                     return (s.dir >= station_cfg.dir_from) &&
                            (s.dir <= station_cfg.dir_to);
                 } else {
                     return (s.dir >= station_cfg.dir_from) ||
                            (s.dir <= station_cfg.dir_to);
                 }
             }
         public:

             SamplesToColour(const config_t::holfuy_cfg_t &station_cfg) : station_cfg(station_cfg) {}

             virtual void inspect(const Samples::Sample &s) {
                 enum colour_t tmp = sample_within_direction(s) ? GREEN : RED;
                 if(s.speed < station_cfg.wind_min) tmp = RED;
                 if(s.speed > station_cfg.wind_max)
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
                 case NONE:
                 case YELLOW:
                     break; /* Can't happen. */
                 }
             }

             colour_t get_result() {
                 return colour;
             }
         };

         class ReduceColours {
             colour_t colour = YELLOW;
         public:
             ReduceColours() {}

             void reduce(const colour_t newColour) {
                 switch(colour) {
                 case NONE:
                 case YELLOW:
                     colour = newColour;
                     break;
                 case GREEN:
                     break;
                 case RED:
                     if((newColour == BLUE) ||
                        (newColour == GREEN))
                         colour = newColour;
                     break;
                 case BLUE:
                     if(colour == GREEN)
                         colour = newColour;
                     break;
                 }
             }

             colour_t get_result() {
                 return colour;
             }
         };

         class HolfuyCsvResponseParser : Parser {
             int nr_commas;
             long tmp_nr;
             int n_decimals;
             float speed, dir, temperature;
             Samples & samples;

             float get_number() {
                 float ret = tmp_nr;
                 while(n_decimals-- > 0) {
                     ret /= 10.0;
                 }
                 return ret;
             }
             holfuy_t & data;

         public:
             HolfuyCsvResponseParser(Samples & samples, holfuy_t & data) : nr_commas(0), tmp_nr(0), n_decimals(-100), samples(samples), data(data) { }

             bool parse(const char c) {
                 if(c == ',') {
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
                     case 9: // Humidity
                         break;
                     case 10: // preassure
                         return false;
                     }
                     tmp_nr = 0;
                     n_decimals = -100;
                 } else if(c == '.') {
                     n_decimals = 0;
                 } else if((c >= '0') &&
                 (c <= '9')) {
                     // This does count leading zeros as well, but since we
                     // only care about decimals after the dot, it does not matter.
                     n_decimals++;
                     tmp_nr = tmp_nr * 10 + (c - '0');
                 }
                 return true;
             }

             void done() {
                 // Add sample.
                 // TODO: Check for incorrect password...
                 samples.add_sample(speed, dir, temperature);
             }
         };

         typedef enum {
             HS_IDLE,
             HS_CONNECTING,
             HS_SEND_HEADER,
             HS_RECV_RESPONSE,
         } holfuy_state_t;

         holfuy_state_t hs_state;

         HttpResponseParser<HolfuyCsvResponseParser> *recvParser;

         static void onDisconnect(void *that, AsyncClient *client) {
             static_cast<holfuy_t*>(that)->onDisconnect2(that, client);
         }

         void onDisconnect2(void*, AsyncClient*) {
             if(hs_state == HS_RECV_RESPONSE) {
                 recvParser->done();
             }
             hs_state = HS_IDLE;
         }

         static void onData(void *that, AsyncClient *client, void *indata, size_t len) {
             static_cast<holfuy_t*>(that)->onData2(that, client, indata, len);
         }

         void onData2(void*, AsyncClient*, void *indata, size_t len) {
             const char *data = static_cast<const char*>(indata);
             while(len-- > 0) {
                 switch(hs_state) {
                 case HS_IDLE:
                 case HS_CONNECTING:
                 case HS_SEND_HEADER:
                     /* Should not be able to happen. */
                     sock.close();
                     break;
                 case HS_RECV_RESPONSE:
                     if(!recvParser->parse(*data)) {
                         sock.close();
                     }
                     break;
                 }
                 data++;
             }
         }

         void get_host_and_port(unsigned int host_len, char *host, int *port, const char ** path) {
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

         bool loop(const config_t::holfuy_cfg_t &config)
         {
             static int connection = 0;
             if (WiFi.getMode() != WIFI_STA) {
                 return false;
             }

             uint32_t current = millis();
             if((lastMillis + HOLFUY_TIME_BETWEEN_SAMPLES) < current) {
                 lastMillis = current;

                 if(sock.connected()) {
                     sock.close(true);
                 }
                 if(recvParser) delete recvParser;
                 recvParser = new HttpResponseParser<HolfuyCsvResponseParser>(new HolfuyCsvResponseParser(samples, *this));
                 sock.onData(onData, this);
                 sock.onDisconnect(onDisconnect, this);
                 connection++;
                 char host[128];
                 int port;
                 get_host_and_port(sizeof(host), host, &port, NULL);
                 sock.connect(host, port);
                 hs_state = HS_CONNECTING;
             }

             switch(hs_state) {
             case HS_IDLE:
                 return true;
             case HS_RECV_RESPONSE:
                 /* Nothing to do here */
                 break;
             case HS_CONNECTING:
                 if(sock.connected()) {
                     hs_state = HS_SEND_HEADER;
                 }
                 break;
             case HS_SEND_HEADER:
                 if(sock.canSend()) {
                     char host[128];
                     const char *path;
                     get_host_and_port(sizeof(host), host, NULL, &path);
                     char request[256];
                     // TODO: URL-encode password & id.
                     snprintf(request,
                     sizeof(request),
                     "GET %s?pw=%s&s=%d&m=CSV&su=m/s HTTP/1.1\r\n"
                     "User-Agent: " APP_NAME "/" APP_VERSION "\r\n"
                     "Host: %s\r\n"
                     "X-Request-Nr: %u\r\n"
                     "Connection: Disconnect\r\n\r\n",
                     path, config.pass, config.id, host, connection);
                     sock.write(request);
                     hs_state = HS_RECV_RESPONSE;
                 }
                 break;
             }
             return false;
         }
     };

     static int current_holfuy;
     static holfuy_t data[8];
     static colour_t lastColour = NONE;

     static void update_light_colour() {
         holfuy_t::ReduceColours rc;
         for(int holfuy_nr = 0; holfuy_nr < cfg.holfuy_nr_stations; ++holfuy_nr) {
             holfuy_t::SamplesToColour stc(cfg.holfuy_stations[holfuy_nr]);
             data[holfuy_nr].samples.evaluate_samples(stc);
             auto newColour = stc.get_result();
             rc.reduce(newColour);
         }
         auto colour = rc.get_result();
         if(lastColour != colour) {
             switch(colour) {
             case colour_t::NONE:
             case colour_t::YELLOW:
                 DEBUGLOG("[holfuy] Change colour to YELLOW\n");
                 AiLight.setColor(255, 255, 0);
                 break;
             case colour_t::GREEN:
                 DEBUGLOG("[holfuy] Change colour to GREEN\n");
                 AiLight.setColor(0, 255, 0);
                 break;
             case colour_t::RED:
                 DEBUGLOG("[holfuy] Change colour to RED\n");
                 AiLight.setColor(255, 0, 0);
                 break;
             case colour_t::BLUE:
                 DEBUGLOG("[holfuy] Change colour to BLUE\n");
                 AiLight.setColor(0, 0, 255);
                 break;
             }
             lastColour = colour;
         }
     }
 };

void loopHolfuy()
{
    if(holfuy::current_holfuy >= cfg.holfuy_nr_stations) {
        holfuy::current_holfuy = 0;
    } else if(holfuy::data[holfuy::current_holfuy].loop(cfg.holfuy_stations[holfuy::current_holfuy])) {
        holfuy::update_light_colour();
        ++holfuy::current_holfuy;
    }
}

/**
 * @brief Bootstrap function for the holfuy connection
 */
void setupHolfuy()
{
    if(cfg.holfuy_enabled) {
        holfuy::update_light_colour();
    }
}
