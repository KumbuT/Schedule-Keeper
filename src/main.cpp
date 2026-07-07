#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

#include "config/Config.h"
#include "display/DisplayManager.h"
#include "network/WebServer.h"
#include "tasks/TaskScheduler.h"
#include "audio/AudioManager.h"
#include "sensors/BatteryMonitor.h"

// ─── Globals ─────────────────────────────────────────────────────────────────
DNSServer dnsServer;
bool      apMode = false;

uint32_t lastTick       = 0;
uint32_t lastWxFetch    = 0;
uint32_t lastWsBcast    = 0;
uint32_t lastBatUpdate  = 0;

// ─── Weather ─────────────────────────────────────────────────────────────────
void fetchWeather() {
  auto& cfg = Config::instance();
  auto& wx  = DisplayManager::instance().weather;

  if (cfg.data.owmApiKey.isEmpty() || cfg.data.city.isEmpty()) return;

  HTTPClient http;
  String units = cfg.data.metricUnits ? "metric" : "imperial";
  String url   = "http://api.openweathermap.org/data/2.5/weather?q=" +
                 cfg.data.city + "&units=" + units +
                 "&appid=" + cfg.data.owmApiKey;

  http.begin(url);
  http.setTimeout(8000);
  int code = http.GET();

  if (code == 200) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    if (!err) {
      wx.temp        = doc["main"]["temp"].as<float>();
      wx.feelsLike   = doc["main"]["feels_like"].as<float>();
      wx.humidity    = doc["main"]["humidity"].as<int>();
      wx.windSpeed   = doc["wind"]["speed"].as<float>();
      wx.description = doc["weather"][0]["description"].as<String>();
      wx.icon        = doc["weather"][0]["icon"].as<String>();
      wx.valid       = true;
      Serial.printf("[Weather] %.1f°  %s\n", wx.temp, wx.description.c_str());
    }
  } else {
    Serial.printf("[Weather] HTTP error: %d\n", code);
  }
  http.end();
}

// ─── WiFi AP mode (captive portal) ───────────────────────────────────────────
void startAP() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ScheduleTracker-Setup", "setup1234");
  dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
  Serial.println("[WiFi] AP mode: ScheduleTracker-Setup");
  Serial.println("[WiFi] Portal: http://192.168.4.1");
}

// ─── Task event handler ───────────────────────────────────────────────────────
void onTaskEvent(const Task& task, TaskEvent event) {
  if (Config::instance().data.muted) return;

  switch (event) {
    case TaskEvent::START:
      Serial.printf("[Task] START: %s\n", task.name.c_str());
      AudioManager::instance().play(
        task.audioStart.isEmpty() ? "/audio/starting.wav" : task.audioStart.c_str());
      break;
    case TaskEvent::MIDPOINT:
      Serial.printf("[Task] MIDPOINT: %s\n", task.name.c_str());
      AudioManager::instance().play(
        task.audioMid.isEmpty() ? "/audio/halfway.wav" : task.audioMid.c_str());
      break;
    case TaskEvent::ONE_MINUTE:
      Serial.printf("[Task] ONE_MINUTE: %s\n", task.name.c_str());
      AudioManager::instance().play("/audio/onemin.wav");
      break;
    case TaskEvent::COMPLETE:
      Serial.printf("[Task] COMPLETE: %s\n", task.name.c_str());
      AudioManager::instance().play(
        task.audioDone.isEmpty() ? "/audio/done.wav" : task.audioDone.c_str());
      break;
  }
}


// ─── WiFi STA mode ───────────────────────────────────────────────────────────
void startSTA() {
  auto& cfg = Config::instance();
  Serial.printf("[WiFi] Connecting to: %s\n", cfg.data.wifiSSID.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.data.wifiSSID.c_str(), cfg.data.wifiPassword.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Failed to connect — falling back to AP mode");
    startAP();  // Declare before use; see below
    return;
  }

  Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());

  // Timezone + NTP
  setenv("TZ", cfg.data.timezone.c_str(), 1);
  tzset();
  configTime(0, 0, "pool.ntp.org", "time.google.com");

  // Wait for valid time sync (up to 10s)
  struct tm ti;
  int attempts = 0;
  while (!getLocalTime(&ti) && attempts++ < 20) {
    delay(500);
  }
  if (attempts < 20) {
    Serial.println("[NTP] Time synced");
  } else {
    Serial.println("[NTP] Sync timeout — using RTC if available");
  }

  fetchWeather();
  lastWxFetch = millis();
}


// ─── setup() ─────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[Boot] Schedule Tracker starting...");

  // Init LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("[LittleFS] Mount failed — formatting...");
    LittleFS.format();
    LittleFS.begin();
  }

  // Load config
  Config::instance().load();

  // Init hardware
  BatteryMonitor::instance().begin(/*adcPin=*/1);
  DisplayManager::instance().begin();
  AudioManager::instance().begin();

  // Load schedule
  TaskScheduler::instance().loadFromFile();
  TaskScheduler::instance().onEvent(onTaskEvent);

  // Network
  if (Config::instance().data.wifiSSID.isEmpty()) {
    startAP();
  } else {
    startSTA();
  }

  // Web server (serves both AP portal and STA dashboard)
  AppWebServer::instance().begin();

  Serial.println("[Boot] Ready.");
}

// ─── loop() ──────────────────────────────────────────────────────────────────
void loop() {
  // DNS for captive portal
  if (apMode) dnsServer.processNextRequest();

  uint32_t now_ms = millis();

  // ── 1-second tick ──────────────────────────────────────────────────────────
  if (now_ms - lastTick >= 1000) {
    lastTick = now_ms;

    time_t   t   = time(nullptr);
    struct tm* tm_now = localtime(&t);

    TaskScheduler::instance().tick(tm_now);
    DisplayManager::instance().update(tm_now);
  }

  // ── WebSocket broadcast every 1s ───────────────────────────────────────────
  if (now_ms - lastWsBcast >= 1000) {
    lastWsBcast = now_ms;
    AppWebServer::instance().broadcastStatus();
  }

  // ── Battery update every 30s ───────────────────────────────────────────────
  if (now_ms - lastBatUpdate >= 30000) {
    lastBatUpdate = now_ms;
    BatteryMonitor::instance().update();
  }

  // ── Weather refresh every 20 minutes ───────────────────────────────────────
  if (!apMode && now_ms - lastWxFetch >= 20UL * 60 * 1000) {
    lastWxFetch = now_ms;
    fetchWeather();
  }

  // ── Touch input ────────────────────────────────────────────────────────────
  static uint32_t lastTouch = 0;
  if (now_ms - lastTouch > 300) {   // 300ms debounce
    int zone = DisplayManager::instance().pollTouch();
    if (zone >= 0) {
      lastTouch = now_ms;
      switch (zone) {
        case 1:  // All Tasks
          DisplayManager::instance().setScreen(Screen::TASK_LIST);
          break;
        case 2:  // Mute toggle
          {
            auto& cfg = Config::instance();
            cfg.data.muted = !cfg.data.muted;
            cfg.save();
            Serial.printf("[Touch] Mute: %s\n", cfg.data.muted ? "ON" : "OFF");
          }
          break;
        case 3:  // Back (from task list)
          DisplayManager::instance().setScreen(Screen::HOME);
          break;
      }
    }
  }

  // ── Audio ──────────────────────────────────────────────────────────────────
  AudioManager::instance().loop();
}