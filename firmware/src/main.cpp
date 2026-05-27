// ESP32 + MAX485 → Promass 63 (Rackbus SL) reader, with WiFi + HTTP server.
//
// Wiring:
//   ESP32 GPIO 26 (UART2 TX) ──── MAX485 DI
//   ESP32 GPIO 25 (UART2 RX) ──── MAX485 RO
//   ESP32 GPIO 27            ──── MAX485 DE + RE  (one wire to both)
//   ESP32 5V                 ──── MAX485 VCC
//   ESP32 GND                ──── MAX485 GND
//
//   MAX485 A   ──── Promass term 20 (Data A)
//   MAX485 B   ──── Promass term 21 (Data B)
//   MAX485 GND ──── Promass term 28 (shield)
//
// After flashing:
//   - Watch the Serial monitor (115200) for the IP address.
//   - Open http://<ip>/ from the same WiFi network.
//   - JSON endpoint: http://<ip>/api/values

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "rackbus_client.h"
#include "secrets.h"

// ===== Config =====
// ESP32-S2 Mini (Wemos LOLIN S2 Mini) — pins 33/35/37 from the silkscreen.
// On ESP32-S2 there's only one HW UART besides the native-USB CDC, so we use Serial1.
constexpr int PIN_RS485_RX = 33;       // → MAX485 RO
constexpr int PIN_RS485_TX = 35;       // → MAX485 DI
constexpr int PIN_RS485_DE = 37;       // → MAX485 DE+RE

// On S2 there is no UART_PORT; rename UART_PORT → Serial1 below.
#define UART_PORT Serial1

constexpr uint8_t PROMASS_ADDR = 3;
constexpr uint32_t POLL_INTERVAL_MS = 2000;

// All credentials and endpoints live in secrets.h (gitignored).
// Copy include/secrets.h.example to include/secrets.h and edit it.
constexpr uint32_t PUBLISH_INTERVAL_MS = 5000;

// ===== Globals =====
RackbusClient rb(&UART_PORT, PIN_RS485_DE, PROMASS_ADDR);
WebServer server(80);

struct Param { uint8_t v, h; const char *key; const char *unit; };
const Param PARAMS[] = {
    {0, 0, "mass_flow",       "t/h"},
    {0, 1, "totalizer_1",     "t"},
    {1, 0, "density",         "g/cm3"},
    {1, 1, "temperature",     "C"},
    {1, 2, "calc_density",    "g/cm3"},
    {0, 5, "volume_flow",     "L/h"},        // Promass reports in L/h
    {2, 2, "diagnostic_code", ""},           // V2H2 — current diagnostic
};
// Indices into PARAMS[] for the telemetry publisher
constexpr int IDX_MASS_FLOW    = 0;
constexpr int IDX_TOTALIZER    = 1;
constexpr int IDX_DENSITY      = 2;
constexpr int IDX_TEMP         = 3;
constexpr int IDX_CALC_DENSITY = 4;
constexpr int IDX_VOLUME_FLOW  = 5;
constexpr int IDX_DIAGNOSTIC   = 6;
constexpr size_t NUM_PARAMS = sizeof(PARAMS) / sizeof(PARAMS[0]);

struct LastValue {
    char  kind;            // 'i'/'f'/'s'/'n'/'?' (?=no data yet)
    double value_f;
    long   value_i;
    char   text[24];
    char   decoded[24];
    uint32_t ts_ms;        // millis() of last successful read
    uint32_t fails;        // running fail count for this param
    uint32_t reads;        // running success count for this param
};
LastValue values[NUM_PARAMS];

uint32_t total_reads = 0, total_fails = 0;
uint32_t last_cycle_ms = 0;
bool polling_enabled = true;     // toggled via /api/polling/{on,off}
bool publish_enabled = true;     // toggled via /api/publish/{on,off}

// Telemetry publisher status
uint32_t publish_total = 0;
uint32_t publish_ok = 0;
uint32_t publish_fail = 0;
int      last_publish_code = 0;
char     last_publish_body[160] = {0};
uint32_t last_publish_ms = 0;
char     last_publish_payload[512] = {0};

// ===== HTML page =====
const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Promass 63 — Live</title>
<style>
  body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Arial,sans-serif;
       background:#0d1117;color:#e6edf3;margin:0;padding:16px;}
  h1{font-size:20px;margin:0 0 14px;color:#79c0ff}
  .row{display:flex;justify-content:space-between;padding:10px 0;
       border-bottom:1px solid #21262d}
  .key{color:#8b949e;font-size:13px}
  .val{font-size:22px;font-weight:600;color:#7ee787;font-variant-numeric:tabular-nums}
  .val.bad{color:#f85149}
  .unit{font-size:13px;color:#8b949e;margin-left:6px}
  .meta{margin-top:18px;color:#8b949e;font-size:12px;line-height:1.6}
  .raw{font-family:'SF Mono','Cascadia Code',Consolas,monospace;color:#a5d6ff;font-size:11px}
</style></head>
<body>
<h1>Promass 63 @ Rackbus addr=3</h1>
<div id="vals"></div>
<div class="meta">
  <div>Last cycle: <span id="ts">—</span></div>
  <div>Total reads: <span id="reads">0</span>  ·  Failures: <span id="fails">0</span></div>
  <div>WiFi RSSI: <span id="rssi">—</span></div>
  <div>Uptime: <span id="uptime">—</span></div>
</div>
<script>
async function tick(){
  try{
    let r = await fetch('/api/values'); let j = await r.json();
    let html = '';
    for(const [k,v] of Object.entries(j.params)){
      let cls = v.ok ? '' : 'bad';
      let val = v.ok ? v.value : (v.kind==='invalid'?'@':'no response');
      let raw = v.raw ? ` <span class="raw">[${v.raw}]</span>` : '';
      html += `<div class="row"><span class="key">${k}</span>`+
              `<span class="val ${cls}">${val}<span class="unit">${v.unit}</span>${raw}</span></div>`;
    }
    document.getElementById('vals').innerHTML = html;
    document.getElementById('ts').textContent = (Date.now() - j.last_cycle_ago_ms)
        ? new Date(Date.now() - j.last_cycle_ago_ms).toLocaleTimeString() : '—';
    document.getElementById('reads').textContent = j.total_reads;
    document.getElementById('fails').textContent = j.total_fails;
    document.getElementById('rssi').textContent = j.rssi + ' dBm';
    document.getElementById('uptime').textContent = (j.uptime_ms/1000|0) + ' s';
  }catch(e){console.log(e)}
}
setInterval(tick, 1500); tick();
</script>
</body></html>
)HTML";

// ===== Helpers =====
String json_escape(const char *s) {
    String r; r.reserve(strlen(s) + 2);
    for (const char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\') { r += '\\'; r += *p; }
        else if (*p == '\n') r += "\\n";
        else if ((uint8_t)*p < 0x20) { char b[8]; snprintf(b,8,"\\u%04x",(uint8_t)*p); r += b; }
        else r += *p;
    }
    return r;
}

String value_json(size_t i) {
    const LastValue &lv = values[i];
    const Param &p = PARAMS[i];
    String r = "{\"unit\":\"" + String(p.unit) + "\"";
    r += ",\"reads\":" + String(lv.reads);
    r += ",\"fails\":" + String(lv.fails);
    if (lv.kind == '?') {
        r += ",\"ok\":false,\"kind\":\"no_data\"}";
        return r;
    }
    if (lv.kind == 'n') {
        r += ",\"ok\":false,\"kind\":\"invalid\",\"raw\":\"" + json_escape(lv.decoded) + "\"}";
        return r;
    }
    r += ",\"ok\":true,\"raw\":\"" + json_escape(lv.decoded) + "\"";
    if (lv.kind == 'f') {
        char b[32]; snprintf(b, 32, "%.6f", lv.value_f);
        r += ",\"value\":" + String(b) + ",\"kind\":\"float\"";
    } else if (lv.kind == 'i') {
        r += ",\"value\":" + String(lv.value_i) + ",\"kind\":\"int\"";
    } else if (lv.kind == 's') {
        r += ",\"value\":\"" + json_escape(lv.text) + "\",\"kind\":\"string\"";
    }
    r += ",\"age_ms\":" + String(millis() - lv.ts_ms);
    r += "}";
    return r;
}

// ===== Telemetry publisher =====
//
// Builds JSON from latest values[] and POSTs to TELEMETRY_URL. Skips
// parameters that don't have a recent successful reading. Non-blocking
// failure: HTTP error just bumps publish_fail.

static bool fmt_float(char *out, size_t outsz, const char *key, double v, int decimals) {
    char b[24];
    int n = snprintf(b, sizeof(b), "%.*f", decimals, v);
    if (n < 0) return false;
    int w = snprintf(out, outsz, "\"%s\":%s,", key, b);
    return w > 0 && (size_t)w < outsz;
}

static bool build_payload(char *out, size_t outsz) {
    // Collect freshness — only include parameters that have a successful
    // reading no older than 30 seconds.
    uint32_t now = millis();
    auto fresh_ok = [&](size_t idx) -> bool {
        const LastValue &lv = values[idx];
        return lv.reads > 0 && (lv.kind == 'f' || lv.kind == 'i')
               && (now - lv.ts_ms) < 30000;
    };

    char *p = out; size_t left = outsz;
    int written = snprintf(p, left, "{\"device_id\":\"%s\"", DEVICE_ID);
    if (written < 0 || (size_t)written >= left) return false;
    p += written; left -= written;

    auto add = [&](const char *json_key, double val, int decimals) {
        char buf[48];
        int n = snprintf(buf, sizeof(buf), ",\"%s\":%.*f", json_key, decimals, val);
        if (n > 0 && (size_t)n < left) {
            memcpy(p, buf, n); p += n; left -= n;
        }
    };

    if (fresh_ok(IDX_MASS_FLOW)) {
        add("mass_flow_t_h", values[IDX_MASS_FLOW].value_f, 4);
    }
    if (fresh_ok(IDX_DENSITY)) {
        add("density_g_cm3", values[IDX_DENSITY].value_f, 5);
    }
    if (fresh_ok(IDX_TEMP)) {
        add("temp_c", values[IDX_TEMP].value_f, 2);
    }
    if (fresh_ok(IDX_TOTALIZER)) {
        // Promass reports totalizer in tonnes — endpoint wants kg
        double kg = values[IDX_TOTALIZER].value_f * 1000.0;
        add("totalizer_kg", kg, 3);
    }
    if (fresh_ok(IDX_VOLUME_FLOW)) {
        // Promass reports volume flow in L/h — endpoint wants L/min
        double lpm = values[IDX_VOLUME_FLOW].value_f / 60.0;
        add("volume_flow_l_min", lpm, 3);
    }
    if (fresh_ok(IDX_DIAGNOSTIC)) {
        char buf[48];
        long code = (values[IDX_DIAGNOSTIC].kind == 'i')
                    ? values[IDX_DIAGNOSTIC].value_i
                    : (long)values[IDX_DIAGNOSTIC].value_f;
        int n = snprintf(buf, sizeof(buf), ",\"diagnostic_code\":%ld", code);
        if (n > 0 && (size_t)n < left) {
            memcpy(p, buf, n); p += n; left -= n;
        }
    }

    if (left < 2) return false;
    *p++ = '}'; *p = '\0';
    return true;
}

void telemetry_publish() {
    if (!publish_enabled) return;
    if (WiFi.status() != WL_CONNECTED) return;

    if (!build_payload(last_publish_payload, sizeof(last_publish_payload))) {
        return;
    }
    // Don't waste a POST if there's nothing besides device_id
    if (!strchr(last_publish_payload, ',')) return;

    WiFiClientSecure client;
    client.setInsecure();              // no cert pinning for prototype
    HTTPClient http;
    http.setTimeout(4000);             // 4s per request
    if (!http.begin(client, TELEMETRY_URL)) {
        publish_fail++;
        last_publish_code = -1;
        snprintf(last_publish_body, sizeof(last_publish_body), "http.begin failed");
        return;
    }
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-Api-Key", TELEMETRY_API_KEY);

    publish_total++;
    int code = http.POST((uint8_t *)last_publish_payload, strlen(last_publish_payload));
    last_publish_code = code;
    last_publish_ms = millis();

    if (code > 0) {
        String body = http.getString();
        strncpy(last_publish_body, body.c_str(), sizeof(last_publish_body) - 1);
        last_publish_body[sizeof(last_publish_body) - 1] = '\0';
        if (code >= 200 && code < 300) publish_ok++;
        else publish_fail++;
    } else {
        publish_fail++;
        snprintf(last_publish_body, sizeof(last_publish_body),
                 "HTTP error: %s", http.errorToString(code).c_str());
    }
    http.end();
}

// ===== HTTP handlers =====
void handle_root() {
    server.send_P(200, "text/html", INDEX_HTML);
}

void handle_publish_on()  { publish_enabled = true;  server.send(200, "application/json", "{\"publish\":true}"); }
void handle_publish_off() { publish_enabled = false; server.send(200, "application/json", "{\"publish\":false}"); }

void handle_api_last_publish() {
    String body = "{\"publish_enabled\":";
    body += publish_enabled ? "true" : "false";
    body += ",\"total\":" + String(publish_total);
    body += ",\"ok\":" + String(publish_ok);
    body += ",\"fail\":" + String(publish_fail);
    body += ",\"last_code\":" + String(last_publish_code);
    body += ",\"last_age_ms\":" + String(last_publish_ms ? millis() - last_publish_ms : 0);
    body += ",\"last_body\":" + String("\"") + json_escape(last_publish_body) + "\"";
    body += ",\"last_payload\":" + String("\"") + json_escape(last_publish_payload) + "\"";
    body += "}";
    server.send(200, "application/json", body);
}

void handle_api_values() {
    String body = "{\"params\":{";
    for (size_t i = 0; i < NUM_PARAMS; i++) {
        if (i) body += ",";
        body += "\"" + String(PARAMS[i].key) + "\":" + value_json(i);
    }
    body += "}";
    body += ",\"total_reads\":" + String(total_reads);
    body += ",\"total_fails\":" + String(total_fails);
    body += ",\"uptime_ms\":" + String(millis());
    body += ",\"rssi\":" + String(WiFi.RSSI());
    body += ",\"last_cycle_ago_ms\":" + String(millis() - last_cycle_ms);
    body += ",\"slave_addr\":" + String(PROMASS_ADDR);
    body += "}";
    server.send(200, "application/json", body);
}

void handle_api_raw() {
    if (!server.hasArg("v") || !server.hasArg("h")) {
        server.send(400, "application/json",
                    "{\"error\":\"missing v or h argument\"}");
        return;
    }
    int v = server.arg("v").toInt();
    int h = server.arg("h").toInt();
    if (v < 0 || v > 15 || h < 0 || h > 15) {
        server.send(400, "application/json", "{\"error\":\"v/h out of range\"}");
        return;
    }
    RackbusResult r;
    bool ok = rb.read_variable((uint8_t)v, (uint8_t)h, r);
    String body = "{\"v\":" + String(v) + ",\"h\":" + String(h);
    if (!ok) {
        body += ",\"ok\":false,\"error\":\"no_response\"}";
    } else {
        body += ",\"ok\":true,\"raw\":\"" + json_escape(r.decoded) + "\"";
        if (r.kind == 'f') {
            char b[32]; snprintf(b, 32, "%.6f", r.value_f);
            body += ",\"value\":" + String(b) + ",\"kind\":\"float\"";
        } else if (r.kind == 'i') {
            body += ",\"value\":" + String(r.value_i) + ",\"kind\":\"int\"";
        } else if (r.kind == 's') {
            body += ",\"value\":\"" + json_escape(r.text) + "\",\"kind\":\"string\"";
        } else if (r.kind == 'n') {
            body += ",\"value\":null,\"kind\":\"invalid\"";
        }
        body += "}";
    }
    server.send(200, "application/json", body);
}

void handle_health() {
    String body = "{\"status\":\"ok\",\"polling\":";
    body += polling_enabled ? "true" : "false";
    body += "}";
    server.send(200, "application/json", body);
}

void handle_polling_on() {
    polling_enabled = true;
    server.send(200, "application/json", "{\"polling\":true}");
}
void handle_polling_off() {
    polling_enabled = false;
    // Make sure MAX485 driver is OFF so we don't load the bus
    digitalWrite(PIN_RS485_DE, LOW);
    server.send(200, "application/json", "{\"polling\":false}");
}

// /api/sniff?ms=3000 — passive listen with per-byte microsecond timestamps.
// Frames are split by inter-byte gap > 3 ms. Returns JSON with timed frames.
void handle_api_sniff() {
    uint32_t ms = 3000;
    if (server.hasArg("ms")) {
        long v = server.arg("ms").toInt();
        if (v > 0 && v < 30000) ms = (uint32_t)v;
    }
    digitalWrite(PIN_RS485_DE, LOW);
    while (UART_PORT.available()) UART_PORT.read();

    struct FrameRec { uint32_t start_us; uint8_t data[16]; uint8_t len; };
    static FrameRec frames[80];
    size_t nframes = 0;

    uint8_t cur[64]; uint8_t curlen = 0;
    uint32_t last_byte_us = 0;
    uint32_t cur_start_us = 0;
    uint32_t t0 = micros();
    uint32_t end_us = t0 + ms * 1000UL;

    while (micros() < end_us && nframes < 80) {
        if (UART_PORT.available()) {
            uint32_t now = micros();
            if (curlen == 0) cur_start_us = now;
            else if (now - last_byte_us > 3000) {
                // commit current frame
                FrameRec &f = frames[nframes++];
                f.start_us = cur_start_us;
                f.len = curlen > 16 ? 16 : curlen;
                memcpy(f.data, cur, f.len);
                curlen = 0;
                cur_start_us = now;
            }
            if (curlen < sizeof(cur)) cur[curlen++] = (uint8_t)UART_PORT.read();
            else UART_PORT.read();
            last_byte_us = now;
        } else {
            if (curlen && micros() - last_byte_us > 3000 && nframes < 80) {
                FrameRec &f = frames[nframes++];
                f.start_us = cur_start_us;
                f.len = curlen > 16 ? 16 : curlen;
                memcpy(f.data, cur, f.len);
                curlen = 0;
            }
            delay(1);
        }
    }
    if (curlen && nframes < 80) {
        FrameRec &f = frames[nframes++];
        f.start_us = cur_start_us;
        f.len = curlen > 16 ? 16 : curlen;
        memcpy(f.data, cur, f.len);
    }

    String body = "{\"listen_ms\":" + String(ms) + ",\"frames\":[";
    for (size_t i = 0; i < nframes; i++) {
        if (i) body += ",";
        body += "{\"t_us\":" + String(frames[i].start_us - t0);
        body += ",\"hex\":\"";
        for (uint8_t j = 0; j < frames[i].len; j++) {
            char b[4]; snprintf(b, 4, "%02X", frames[i].data[j]);
            if (j) body += " ";
            body += b;
        }
        body += "\"}";
    }
    body += "]}";
    server.send(200, "application/json", body);
}

// ===== WiFi =====
void connect_wifi() {
    Serial.printf("\nConnecting to WiFi '%s'", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
        delay(500);
        Serial.print('.');
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.printf("WiFi connected. IP: %s   RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());

        if (MDNS.begin(MDNS_NAME)) {
            MDNS.addService("http", "tcp", 80);
            Serial.printf("mDNS started: http://%s.local/\n", MDNS_NAME);
        }

        ArduinoOTA.setHostname(OTA_HOSTNAME);
        ArduinoOTA.setPassword(OTA_PASSWORD);
        ArduinoOTA
            .onStart([]() { Serial.println("OTA: starting"); })
            .onEnd([]() { Serial.println("\nOTA: done"); })
            .onProgress([](unsigned p, unsigned t) {
                Serial.printf("OTA: %u%%\r", (p * 100) / t);
            })
            .onError([](ota_error_t e) { Serial.printf("OTA error %u\n", e); });
        ArduinoOTA.begin();
        Serial.printf("OTA ready (host=%s, password set)\n", OTA_HOSTNAME);
    } else {
        Serial.printf("\nWiFi FAILED — continuing without it (will retry in background)\n");
    }
}

// ===== Setup / loop =====
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("=== ESP32 Rackbus SL client + HTTP ===");
    Serial.printf("UART2  RX=%d TX=%d   DE/RE=%d\n",
                  PIN_RS485_RX, PIN_RS485_TX, PIN_RS485_DE);
    Serial.printf("Slave address: %u   Baud: 19200 8N1\n", PROMASS_ADDR);

    rb.begin(PIN_RS485_RX, PIN_RS485_TX, 19200);

    // init last-value table
    for (size_t i = 0; i < NUM_PARAMS; i++) {
        values[i].kind = '?';
        values[i].ts_ms = 0;
        values[i].fails = 0;
        values[i].reads = 0;
        strcpy(values[i].decoded, "");
        strcpy(values[i].text, "");
    }

    connect_wifi();

    server.on("/",            handle_root);
    server.on("/api/values",  handle_api_values);
    server.on("/api/raw",     handle_api_raw);
    server.on("/api/sniff",   handle_api_sniff);
    server.on("/api/polling/on",  handle_polling_on);
    server.on("/api/polling/off", handle_polling_off);
    server.on("/api/publish/on",  handle_publish_on);
    server.on("/api/publish/off", handle_publish_off);
    server.on("/api/last_publish", handle_api_last_publish);
    server.on("/health",      handle_health);
    server.begin();
    Serial.println("HTTP server on port 80");
    Serial.println("Open http://<ip>/ from a device on the same WiFi.");
}

void loop() {
    server.handleClient();
    ArduinoOTA.handle();

    static uint32_t next_publish = 0;
    if (millis() >= next_publish) {
        next_publish = millis() + PUBLISH_INTERVAL_MS;
        telemetry_publish();
    }

    static uint32_t next_poll = 0;
    if (polling_enabled && millis() >= next_poll) {
        next_poll = millis() + POLL_INTERVAL_MS;
        last_cycle_ms = millis();

        for (size_t i = 0; i < NUM_PARAMS; i++) {
            const Param &p = PARAMS[i];
            // keep the web server / OTA responsive between params
            server.handleClient();
            ArduinoOTA.handle();

            RackbusResult r;
            bool ok = rb.read_variable(p.v, p.h, r);
            total_reads++;
            if (!ok) {
                total_fails++;
                values[i].fails++;
                Serial.printf("V%dH%d %-14s : <no response>\n",
                              p.v, p.h, p.key);
                continue;
            }
            values[i].kind = r.kind;
            values[i].value_f = r.value_f;
            values[i].value_i = r.value_i;
            strncpy(values[i].text, r.text, sizeof(values[i].text) - 1);
            strncpy(values[i].decoded, r.decoded, sizeof(values[i].decoded) - 1);
            values[i].text[sizeof(values[i].text) - 1] = '\0';
            values[i].decoded[sizeof(values[i].decoded) - 1] = '\0';
            values[i].ts_ms = millis();
            values[i].reads++;

            if (r.kind == 'f') {
                Serial.printf("V%dH%d %-14s : %.6f %s\n",
                              p.v, p.h, p.key, r.value_f, p.unit);
            } else if (r.kind == 'i') {
                Serial.printf("V%dH%d %-14s : %ld %s\n",
                              p.v, p.h, p.key, r.value_i, p.unit);
            } else if (r.kind == 's') {
                Serial.printf("V%dH%d %-14s : \"%s\"\n",
                              p.v, p.h, p.key, r.text);
            } else {
                Serial.printf("V%dH%d %-14s : invalid '@'\n",
                              p.v, p.h, p.key);
            }
        }

        // WiFi reconnect if dropped
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi dropped — reconnecting...");
            WiFi.reconnect();
        }
    }
}
