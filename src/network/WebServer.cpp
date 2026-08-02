#include "WebServer.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Update.h>   // OTA: flash a new firmware image received over HTTP
#include <esp_partition.h> // look up the filesystem partition size for U_SPIFFS
#include <time.h>
#include <vector>
#include <algorithm>
#include "../config/Config.h"
#include "../tasks/TaskScheduler.h"
#include "../sensors/BatteryMonitor.h"
#include "../display/DisplayManager.h"

// Weather state — defined in main.cpp
enum class WxState
{
  OK,
  BAD_KEY,
  BAD_CITY,
  RATE_LIMITED,
  SERVER_ERROR
};
extern WxState wxState;
extern uint32_t wxBackoffUntil;
extern uint32_t lastWxFetch;
extern bool apMode;
extern int localDayIndex(); // defined in main.cpp -- local day for reward logic

// OTA error flag, shared between the /update upload + completion callbacks.
// Set true on any begin/write/end failure across the (up to two) images in a
// single upload; the device reboots only when it's still false.
static bool s_otaError = false;

AppWebServer &AppWebServer::instance()
{
  static AppWebServer inst;
  return inst;
}

void AppWebServer::begin()
{
  _statusJson.reserve(384); // one-time grow; reused by every status broadcast

  // CORS headers for browser dev access
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");

  _ws.onEvent([this](AsyncWebSocket *s, AsyncWebSocketClient *c,
                     AwsEventType t, void *a, uint8_t *d, size_t l)
              { _onWsEvent(s, c, t, a, d, l); });

  _server.addHandler(&_ws);
  _setupRoutes();
  _server.begin();

  Serial.printf("[WebServer] Started at http://%s\n",
                WiFi.localIP().toString().c_str());
}

void AppWebServer::_setupRoutes()
{
  // Static files from LittleFS
  // Page routes — gated on AP vs STA mode
  _server.on("/", HTTP_GET, [](AsyncWebServerRequest *req)
             {
  if (apMode) { req->send(LittleFS, "/setup.html", "text/html"); return; }
  req->send(LittleFS, "/index.html", "text/html"); });

  _server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *req)
             {
  if (apMode) { req->redirect("/setup.html"); return; }
  req->send(LittleFS, "/index.html", "text/html"); });

  _server.on("/config.html", HTTP_GET, [](AsyncWebServerRequest *req)
             {
  if (apMode) { req->redirect("/setup.html"); return; }
  req->send(LittleFS, "/config.html", "text/html"); });

  _server.on("/setup.html", HTTP_GET, [](AsyncWebServerRequest *req)
             { req->send(LittleFS, "/setup.html", "text/html"); });

  // ── GET /api/wifi-scan ────────────────────────────────────────────────────────

  _server.on("/api/wifi-scan", HTTP_GET, [](AsyncWebServerRequest *req)
             {
  int n = WiFi.scanNetworks();  // blocking ~1-4s; runs in AsyncWebServer's own task,doesn't block loop()/audio/task-ticking

  struct Net { String ssid; int rssi; bool secure; };
  std::vector<Net> nets;

  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) continue;  // hidden networks broadcast blank SSID — nothing to show
    int  rssi   = WiFi.RSSI(i);
    bool secure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);

    // Same network often shows up more than once (multiple APs/bands) — keep strongest
    auto it = std::find_if(nets.begin(), nets.end(),
                            [&](const Net& x) { return x.ssid == ssid; });
    if (it != nets.end()) {
      if (rssi > it->rssi) { it->rssi = rssi; it->secure = secure; }
    } else {
      nets.push_back({ssid, rssi, secure});
    }
  }
  WiFi.scanDelete();

  std::sort(nets.begin(), nets.end(),
            [](const Net& a, const Net& b) { return a.rssi > b.rssi; });

  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (auto& net : nets) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"]   = net.ssid;
    o["rssi"]   = net.rssi;
    o["secure"] = net.secure;
  }

  String out;
  serializeJson(doc, out);
  req->send(200, "application/json", out); });

  // ── GET /api/status ────────────────────────────────────────────────────────
  _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *req)
             { req->send(200, "application/json", _buildStatusJson()); });

  // ── GET /api/schedule ──────────────────────────────────────────────────────
  _server.on("/api/schedule", HTTP_GET, [](AsyncWebServerRequest *req)
             {
    File sf = LittleFS.open("/schedule.json", "r");
    if (!sf) { req->send(404, "application/json", "{}"); return; }
    String body;
    uint8_t buf2[64]; size_t nr;
    while ((nr = sf.read(buf2, sizeof(buf2))) > 0) {
      for (size_t i=0; i<nr; i++) body += (char)buf2[i];
    }
    sf.close();
    req->send(200, "application/json", body.c_str()); });

  // ── POST /api/schedule ─────────────────────────────────────────────────────
  _server.on("/api/schedule", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr, [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total)
             {
      static String buf;
      // buf += String((char*)data).substring(0, len);
      buf.concat((const char*)data, len);
      if (index + len == total) {
        File f = LittleFS.open("/schedule.json", "w");
        f.print(buf.c_str());
        f.close();
        buf = "";
        TaskScheduler::instance().loadFromFile();
        req->send(200, "application/json", "{\"ok\":true}");
      } });

  // ── GET /api/config ────────────────────────────────────────────────────────
  _server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *req)
             {
    auto& cfg = Config::instance();
    auto& wx  = DisplayManager::instance().weather;
    JsonDocument doc;
    doc["timezone"] = cfg.data.timezone;
    doc["city"]     = cfg.data.city;
    doc["metric"]   = cfg.data.metricUnits;
    doc["muted"]    = cfg.data.muted;
    doc["tstyle"]   = cfg.data.timerStyle;
    doc["rgoal"]    = cfg.data.dailyGoal;

    // Expose whether a key exists and its last 6 chars so the UI can show
    // a masked hint (e.g. "…a3f92c") without putting the full key in the page.
    // The full key is never sent over HTTP.
    if (!cfg.data.owmApiKey.isEmpty()) {
      doc["owmkey_set"]  = true;
      doc["owmkey_hint"] = "…" + cfg.data.owmApiKey.substring(
                             cfg.data.owmApiKey.length() - 6);
    } else {
      doc["owmkey_set"]  = false;
      doc["owmkey_hint"] = "";
    }

    // Expose current weather error so the UI can highlight the key field
    // when the key has been revoked or the city name is wrong.
    doc["weather_error"] = wx.errorMsg;

    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out.c_str()); });

  // ── POST /api/config ───────────────────────────────────────────────────────
  _server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr, [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t)
             {
      JsonDocument doc;
      deserializeJson(doc, data, len);
      auto& cfg = Config::instance();

      if (doc["timezone"].is<String>()) {
        cfg.data.timezone = doc["timezone"].as<String>();
        setenv("TZ", cfg.data.timezone.c_str(), 1);
        tzset();
      }
      if (doc["city"].is<String>()) {
        String newCity = doc["city"].as<String>();
        newCity.trim();
        if (!newCity.isEmpty()) cfg.data.city = newCity;
      }
      // Track whether the unit system actually changed -- OWM returns the
      // temperature pre-converted for whichever units the request asked
      // for (see fetchWeather()'s "units=metric"/"units=imperial" query
      // param), so flipping this setting doesn't itself convert the
      // ALREADY-cached wx.temp value. Only the degree symbol in
      // _drawWeatherRow() re-reads cfg.data.metricUnits live on every draw,
      // which is why the number looked frozen while the C/F letter changed
      // instantly -- the fix is to force an immediate re-fetch below, same
      // as a city/key change already does.
      bool metricChanged = false;
      if (doc["metric"].is<bool>()) {
        bool newMetric = doc["metric"].as<bool>();
        metricChanged = (newMetric != cfg.data.metricUnits);
        cfg.data.metricUnits = newMetric;
      }

      if (doc["tstyle"].is<int>()) {
        int ts = doc["tstyle"].as<int>();
        cfg.data.timerStyle = (ts == 1) ? 1 : 0;  // clamp to known styles
        Serial.printf("[Config] timerStyle saved = %d\n", cfg.data.timerStyle);
      }

      if (doc["goal"].is<int>()) {
        int g = doc["goal"].as<int>();
        if (g < 1)  g = 1;
        if (g > 50) g = 50;
        cfg.data.dailyGoal = g;
      }

      // Only overwrite the OWM key if a non-empty value was submitted.
      // An absent or empty "owmkey" field means "keep the existing key" —
      // this prevents a blank config save from accidentally wiping a working key.
      bool keyChanged = false;
      if (doc["owmkey"].is<String>()) {
        String newKey = doc["owmkey"].as<String>();
        newKey.trim();
        if (!newKey.isEmpty()) {
          cfg.data.owmApiKey = newKey;
          keyChanged = true;
        }
      }

      cfg.save();

      // If city, key, or units changed, reset weather backoff so a fetch is
      // attempted immediately on the next loop iteration -- for a units
      // change this is what actually converts wx.temp, not just the symbol.
      if (keyChanged || doc["city"].is<String>() || metricChanged) {
        wxState        = WxState::OK;
        wxBackoffUntil = 0;
        lastWxFetch    = 0;

        // Clear any existing error message so the display stops showing it
        DisplayManager::instance().weather.errorMsg = "";
      }

      req->send(200, "application/json", "{\"ok\":true}"); });

  // ── POST /api/mute ─────────────────────────────────────────────────────────
  _server.on("/api/mute", HTTP_POST, [](AsyncWebServerRequest *req)
             {
    auto& cfg = Config::instance();
    cfg.data.muted = !cfg.data.muted;
    cfg.save();
    String resp = String("{\"muted\":") + (cfg.data.muted ? "true" : "false") + "}";
    req->send(200, "application/json", resp.c_str()); });

  // ── POST /save-setup (captive portal) ─────────────────────────────────────
  _server.on("/save-setup", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr, [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t)
             {
      JsonDocument doc;
      if (deserializeJson(doc, data, len)) {
        req->send(400, "application/json", "{\"error\":\"invalid_json\"}");
        return;
      }

      // ── SSID ──────────────────────────────────────────────────────────────
      String ssid = doc["ssid"] | "";
      ssid.trim();
      if (ssid.isEmpty()) {
        req->send(400, "application/json", "{\"error\":\"ssid_required\"}");
        return;
      }
      if (ssid.length() > 32) {
        req->send(400, "application/json", "{\"error\":\"ssid_too_long\"}");
        return;
      }

      // ── Password ──────────────────────────────────────────────────────────
      String password = doc["password"] | "";
      if (password.length() > 0 && password.length() < 8) {
        req->send(400, "application/json", "{\"error\":\"password_too_short\"}");
        return;
      }
      if (password.length() > 63) {
        req->send(400, "application/json", "{\"error\":\"password_too_long\"}");
        return;
      }

      // ── Timezone ──────────────────────────────────────────────────────────
      String tz = doc["timezone"] | "UTC0";
      if (tz.isEmpty() || tz.length() > 64) tz = "UTC0";

      // ── City ──────────────────────────────────────────────────────────────
      String city = doc["city"] | "London";
      city.trim();
      if (city.isEmpty()) city = "London";
      if (city.length() > 85) city = city.substring(0, 85);

      // ── OWM key (optional) ────────────────────────────────────────────────
      // Accept empty (user skips weather for now) or exactly 32 hex chars.
      // If malformed but non-empty, save it with a warning — user can fix
      // later in /config without having to redo the whole WiFi setup.
      String owmkey = doc["owmkey"] | "";
      owmkey.trim();
      bool owmSuspect = (!owmkey.isEmpty() && owmkey.length() != 32);

      // ── All valid — save and reboot ───────────────────────────────────────
      auto& cfg = Config::instance();
      cfg.data.wifiSSID     = ssid;
      cfg.data.wifiPassword = password;
      cfg.data.timezone     = tz;
      cfg.data.city         = city;
      cfg.data.owmApiKey    = owmkey;
      cfg.data.metricUnits  = doc["metric"] | true;
      cfg.save();

      String resp = owmSuspect
        ? "{\"ok\":true,\"warn\":\"owmkey_suspect\",\"msg\":\"Saved. Rebooting...\"}"
        : "{\"ok\":true,\"msg\":\"Saved. Rebooting...\"}";

      req->send(200, "application/json", resp.c_str());
      delay(800);
      ESP.restart(); });

  // ── POST /update ── combined OTA (firmware + filesystem) ───────────────────
  // Accepts firmware.bin and/or littlefs.bin in ONE multipart upload. Each file
  // is routed to the right flash region by name (contains "littlefs"/"spiffs" ->
  // filesystem partition via U_SPIFFS, otherwise the app slot via U_FLASH) and
  // flashed in its own Update pass. The device reboots only if EVERY image
  // flashed cleanly (s_otaError stays false). Firmware is A/B: a rejected or
  // failed image leaves the current firmware active, so a bad upload can't brick
  // the device -- worst case it stays on the old build. Unauthenticated: fine on
  // a trusted home network; gate behind the planned parent PIN later if needed.
  _server.on("/update", HTTP_POST,
             [](AsyncWebServerRequest *req)
             {
               bool ok = !s_otaError;
               AsyncWebServerResponse *resp = req->beginResponse(
                   200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
               resp->addHeader("Connection", "close");
               req->send(resp);
               if (ok)
               {
                 delay(300);
                 ESP.restart();
               }
             },
             [](AsyncWebServerRequest *req, const String &filename, size_t index,
                uint8_t *data, size_t len, bool final)
             {
               // Clear the error flag once at the start of each new request
               // (the request pointer changes between uploads).
               static AsyncWebServerRequest *otaReq = nullptr;
               if (req != otaReq)
               {
                 otaReq = req;
                 s_otaError = false;
               }

               if (index == 0)
               {
                 bool isFs = (filename.indexOf("littlefs") >= 0 ||
                              filename.indexOf("spiffs") >= 0);
                 int cmd = isFs ? U_SPIFFS : U_FLASH;
                 size_t sz = UPDATE_SIZE_UNKNOWN;
                 if (isFs)
                 {
                   // U_SPIFFS needs the real partition size; look it up.
                   const esp_partition_t *p = esp_partition_find_first(
                       ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
                   if (p) sz = p->size;
                 }
                 Serial.printf("[OTA] Start %s -> %s\n", filename.c_str(),
                               isFs ? "filesystem" : "firmware");
                 if (!Update.begin(sz, cmd))
                 {
                   s_otaError = true;
                   Update.printError(Serial);
                 }
               }
               if (!s_otaError && len && Update.write(data, len) != len)
               {
                 s_otaError = true;
                 Update.printError(Serial);
               }
               if (final)
               {
                 if (!Update.end(true))
                 {
                   s_otaError = true;
                   Update.printError(Serial);
                 }
                 else
                 {
                   Serial.printf("[OTA] %s done (%u bytes)\n", filename.c_str(),
                                 (unsigned)(index + len));
                 }
               }
             });

  // ── POST /api/star ── parent grants (+1) or removes (-1) a reward star ──────
  // Body: {"delta": 1} or {"delta": -1}. Stars are parent-controlled from the
  // dashboard, not auto-earned, so a child can't farm them.
  _server.on("/api/star", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr,
             [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t)
             {
               JsonDocument doc;
               deserializeJson(doc, data, len);
               int delta = doc["delta"] | 0;
               int today = localDayIndex();
               if (today < 0)
               {
                 req->send(409, "application/json", "{\"error\":\"clock_not_synced\"}");
                 return;
               }
               auto &cfg = Config::instance();
               if (delta > 0)
                 cfg.rewardStar(today);
               else if (delta < 0)
                 cfg.rewardRemove(today);
               AppWebServer::instance().broadcastStatus(); // update open dashboards now
               char buf[80];
               snprintf(buf, sizeof(buf), "{\"stars\":%d,\"goal\":%d,\"streak\":%d}",
                        cfg.data.rewardStars, cfg.data.dailyGoal, cfg.data.streak);
               req->send(200, "application/json", buf);
             });

  // Captive portal catch-all redirect
  _server.onNotFound([](AsyncWebServerRequest *req)
                     { req->redirect("http://192.168.4.1/setup.html"); });

  // Captive-portal catch-all: answers literally any unmatched request with the
  // setup page, but ONLY for clients connected via our AP (not normal STA browsing).
  // This is what makes phones auto-pop the "Sign in to network" prompt — the OS's
  // probe request (e.g. /generate_204, /hotspot-detect.html) gets a real 200 with
  // portal content instead of silently 404ing.
  class CaptivePortalHandler : public AsyncWebHandler
  {
  public:
    bool canHandle(AsyncWebServerRequest *request) const override { return true; }
    void handleRequest(AsyncWebServerRequest *request) override
    {
      request->send(LittleFS, "/setup.html", "text/html");
    }
  };
  _server.addHandler(new CaptivePortalHandler()).setFilter(ON_AP_FILTER);

  // STA-mode fallback for genuinely missing files
  _server.onNotFound([](AsyncWebServerRequest *req)
                     { req->send(404, "text/plain", "Not found"); });
}

const String &AppWebServer::_buildStatusJson()
{
  auto &sched = TaskScheduler::instance();
  auto &bat = BatteryMonitor::instance();
  auto &cfg = Config::instance();
  auto &wx = DisplayManager::instance().weather;

  time_t now_t = time(nullptr);
  std::tm *now = localtime(&now_t);
  char timeBuf[9], dateBuf[24];
  strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", now);
  strftime(dateBuf, sizeof(dateBuf), "%A %d %b %Y", now);

  // Reuse the member document (see WebServer.h) rather than allocating a new
  // one every second -- clear() resets its contents but keeps the grown pool.
  JsonDocument &doc = _statusDoc;
  doc.clear();
  doc["type"] = "status";
  doc["time"] = timeBuf;
  doc["date"] = dateBuf;
  doc["battery_pct"] = bat.percentage();
  doc["rssi"] = WiFi.isConnected() ? (int)WiFi.RSSI() : 0;
  doc["muted"] = cfg.data.muted;
  doc["stars"]  = cfg.data.rewardStars;
  doc["goal"]   = cfg.data.dailyGoal;
  doc["streak"] = cfg.data.streak;

  const Task *ct = sched.currentTask();
  if (ct)
  {
    doc["current_task"] = ct->name;
    doc["elapsed_sec"] = sched.elapsedSec();
    doc["remaining_sec"] = sched.remainingSec();
    doc["duration_sec"] = ct->durationMin * 60;
    doc["progress_pct"] = sched.progressPct();
    if (sched.currentGroup())
      doc["group"] = sched.currentGroup()->name;
  }
  if (sched.nextTask())
    doc["next_task"] = sched.nextTask()->name;

  if (wx.valid)
  {
    doc["weather"]["temp"] = wx.temp;
    doc["weather"]["desc"] = wx.description;
    doc["weather"]["icon"] = wx.icon;
    doc["weather"]["humid"] = wx.humidity;
    doc["weather"]["wind"] = wx.windSpeed;
    doc["weather"]["metric"] = cfg.data.metricUnits;
  }
  // Always emit weather_error — empty string means no error.
  // Web dashboard uses this to show a warning banner with a /config link.
  doc["weather_error"] = wx.errorMsg;

  _statusJson.clear(); // keeps the already-grown capacity
  serializeJson(doc, _statusJson);
  return _statusJson;
}

void AppWebServer::broadcastStatus()
{
  if (_ws.count() == 0)
    return;
  _ws.textAll(_buildStatusJson());

  // Periodic cleanup of closed connections
  static uint32_t lastCleanup = 0;
  if (millis() - lastCleanup > 5000)
  {
    lastCleanup = millis();
    _ws.cleanupClients();
  }
}

void AppWebServer::_onWsEvent(AsyncWebSocket *server,
                              AsyncWebSocketClient *client,
                              AwsEventType type,
                              void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    Serial.printf("[WS] Client #%u connected from %s\n",
                  client->id(), client->remoteIP().toString().c_str());
    client->text(_buildStatusJson());
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    Serial.printf("[WS] Client #%u disconnected\n", client->id());
  }
  else if (type == WS_EVT_ERROR)
  {
    Serial.printf("[WS] Error from client #%u\n", client->id());
  }
  else if (type == WS_EVT_DATA)
  {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;

    // Guard: only complete single-frame text messages, within a sane size cap
    static constexpr size_t MAX_WS_MSG = 256; // schedule/mute commands are tiny
    if (!(info->final && info->index == 0 &&
          info->len == len && info->opcode == WS_TEXT))
    {
      return;
    }
    if (len == 0 || len >= MAX_WS_MSG)
    {
      Serial.printf("[WS] Rejected message: len=%u\n", (unsigned)len);
      return;
    }

    data[len] = 0; // safe: within AsyncWebSocket's buffer padding

    JsonDocument doc;
    if (deserializeJson(doc, (char *)data))
      return;

    String cmd = doc["type"] | "";
    if (cmd == "mute_toggle")
    {
      auto &cfg = Config::instance();
      cfg.data.muted = !cfg.data.muted;
      cfg.save();
      server->textAll(_buildStatusJson());
    }
    else if (cmd == "reload_schedule")
    {
      TaskScheduler::instance().loadFromFile();
      server->textAll(_buildStatusJson());
    }
  }
}