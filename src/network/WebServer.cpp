#include "WebServer.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>
#include "../config/Config.h"
#include "../tasks/TaskScheduler.h"
#include "../sensors/BatteryMonitor.h"
#include "../display/DisplayManager.h"

AppWebServer &AppWebServer::instance()
{
    static AppWebServer inst;
    return inst;
}

void AppWebServer::begin()
{
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
    _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // ── GET /api/status ────────────────────────────────────────────────────────
    _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *req)
               { req->send(200, "application/json", _buildStatusJson()); });

    // ── GET /api/schedule ──────────────────────────────────────────────────────
    _server.on("/api/schedule", HTTP_GET, [](AsyncWebServerRequest *req)
               { req->send(LittleFS, "/schedule.json", "application/json"); });

    // ── POST /api/schedule ─────────────────────────────────────────────────────
    _server.on("/api/schedule", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr, [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total)
               {
      static String buf;
      buf += String((char*)data).substring(0, len);
      if (index + len == total) {
        File f = LittleFS.open("/schedule.json", "w");
        f.print(buf);
        f.close();
        buf = "";
        TaskScheduler::instance().loadFromFile();
        req->send(200, "application/json", "{\"ok\":true}");
      } });

    // ── GET /api/config ────────────────────────────────────────────────────────
    _server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *req)
               {
    auto& cfg = Config::instance();
    JsonDocument doc;
    doc["timezone"] = cfg.data.timezone;
    doc["city"]     = cfg.data.city;
    doc["metric"]   = cfg.data.metricUnits;
    doc["muted"]    = cfg.data.muted;
    // Never expose owmApiKey over HTTP for basic security
    String out;
    serializeJson(doc, out);
    req->send(200, "application/json", out); });

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
      if (doc["city"].is<String>())   cfg.data.city        = doc["city"].as<String>();
      if (doc["owmkey"].is<String>()) cfg.data.owmApiKey   = doc["owmkey"].as<String>();
      if (doc["metric"].is<bool>())   cfg.data.metricUnits = doc["metric"].as<bool>();
      cfg.save();
      req->send(200, "application/json", "{\"ok\":true}"); });

    // ── POST /api/mute ─────────────────────────────────────────────────────────
    _server.on("/api/mute", HTTP_POST, [](AsyncWebServerRequest *req)
               {
    auto& cfg = Config::instance();
    cfg.data.muted = !cfg.data.muted;
    cfg.save();
    String resp = String("{\"muted\":") + (cfg.data.muted ? "true" : "false") + "}";
    req->send(200, "application/json", resp); });

    // ── POST /save-setup (captive portal) ─────────────────────────────────────
    _server.on("/save-setup", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr, [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t)
               {
      JsonDocument doc;
      deserializeJson(doc, data, len);
      auto& cfg = Config::instance();
      cfg.data.wifiSSID     = doc["ssid"]     | "";
      cfg.data.wifiPassword = doc["password"] | "";
      cfg.data.timezone     = doc["timezone"] | "UTC0";
      cfg.data.city         = doc["city"]     | "London";
      cfg.data.owmApiKey    = doc["owmkey"]   | "";
      cfg.data.metricUnits  = doc["metric"]   | true;
      cfg.save();
      req->send(200, "application/json", "{\"ok\":true,\"msg\":\"Saved. Rebooting...\"}");
      delay(1000);
      ESP.restart(); });

    // Captive portal catch-all redirect
    _server.onNotFound([](AsyncWebServerRequest *req)
                       { req->redirect("http://192.168.4.1/setup.html"); });
}

String AppWebServer::_buildStatusJson()
{
    auto &sched = TaskScheduler::instance();
    auto &bat = BatteryMonitor::instance();
    auto &cfg = Config::instance();
    auto &wx = DisplayManager::instance().weather;

    time_t now_t = time(nullptr);
    struct tm *now = localtime(&now_t);
    char timeBuf[9], dateBuf[24];
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", now);
    strftime(dateBuf, sizeof(dateBuf), "%A %d %b %Y", now);

    JsonDocument doc;
    doc["type"] = "status";
    doc["time"] = timeBuf;
    doc["date"] = dateBuf;
    doc["battery_pct"] = bat.percentage();
    doc["rssi"] = WiFi.isConnected() ? (int)WiFi.RSSI() : 0;
    doc["muted"] = cfg.data.muted;

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
    }

    String out;
    serializeJson(doc, out);
    return out;
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
        // Push current status immediately on connect
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
        // Guard: only process complete single-frame text messages
        if (info->final && info->index == 0 &&
            info->len == len && info->opcode == WS_TEXT)
        {
            data[len] = 0;
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
}
