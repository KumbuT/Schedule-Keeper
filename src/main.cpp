#include <Arduino.h>
#include <ctime>
using std::difftime;
using std::gmtime;
using std::localtime;
using std::mktime;
using std::strftime;
using std::time;
using std::time_t;
using std::tm;
#include <ctime>
#include <WiFi.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

#include "config/Config.h"
#include "display/DisplayManager.h"
#include "display/BacklightManager.h"
#include "network/WebServer.h"
#include "tasks/TaskScheduler.h"
#include "audio/AudioManager.h"
#include "sensors/BatteryMonitor.h"

// Forward declarations
void startAP();
void startSTA();
void fetchWeather();
void saveWeatherCache();
void loadWeatherCache();

// ─── Globals ─────────────────────────────────────────────────────────────────
DNSServer dnsServer;
bool apMode = false;

uint32_t lastTick = 0;
uint32_t lastWxFetch = 0;
uint32_t lastWsBcast = 0;
uint32_t lastBatUpdate = 0;

// ─── Weather error state ──────────────────────────────────────────────────────
enum class WxState
{
  OK,
  BAD_KEY,
  BAD_CITY,
  RATE_LIMITED,
  SERVER_ERROR
};
WxState wxState = WxState::OK;
uint32_t wxBackoffUntil = 0;

// ─── Weather cache ────────────────────────────────────────────────────────────
// Persists the last successful fetch to LittleFS so the display shows real data
// immediately on boot, even before WiFi connects or the first fetch completes.
// Format: JSON with all WeatherData fields + a Unix timestamp.
static constexpr const char *WX_CACHE_PATH = "/weather_cache.json";

void saveWeatherCache()
{
  auto &wx = DisplayManager::instance().weather;
  if (!wx.valid)
    return;

  JsonDocument doc;
  doc["temp"] = wx.temp;
  doc["feelsLike"] = wx.feelsLike;
  doc["humidity"] = wx.humidity;
  doc["windSpeed"] = wx.windSpeed;
  doc["description"] = wx.description;
  doc["icon"] = wx.icon;
  doc["cachedAt"] = (uint32_t)time(nullptr); // Unix timestamp

  File f = LittleFS.open(WX_CACHE_PATH, "w");
  if (f)
  {
    serializeJson(doc, f);
    f.close();
    Serial.println("[Weather] Cache saved");
  }
}

void loadWeatherCache()
{
  File f = LittleFS.open(WX_CACHE_PATH, "r");
  if (!f)
  {
    Serial.println("[Weather] No cache file");
    return;
  }

  JsonDocument doc;
  if (deserializeJson(doc, f))
  {
    f.close();
    return;
  }
  f.close();

  auto &wx = DisplayManager::instance().weather;
  wx.temp = doc["temp"].as<float>();
  wx.feelsLike = doc["feelsLike"].as<float>();
  wx.humidity = doc["humidity"].as<float>();
  wx.windSpeed = doc["windSpeed"].as<float>();
  wx.description = doc["description"].is<const char *>() ? String(doc["description"].as<const char *>()) : String("Cached");
  wx.icon = doc["icon"].is<const char *>() ? String(doc["icon"].as<const char *>()) : String("01d");
  wx.valid = true;
  wx.errorMsg = "";

  // Calculate how stale the cache is
  uint32_t cachedAt = (uint32_t)doc["cachedAt"].as<int>();
  uint32_t nowUnix = (uint32_t)time(nullptr);
  uint32_t ageMin = (nowUnix > cachedAt) ? (nowUnix - cachedAt) / 60 : 0;

  Serial.printf("[Weather] Cache loaded — %u min old: %.1f° %s\n",
                ageMin, wx.temp, wx.description.c_str());

  // If cache is older than 4 hours, mark valid but flag it as stale so
  // the display shows a subtle "last updated Xh ago" note
  if (ageMin > 240)
  {
    wx.errorMsg = "Cached " + String(ageMin / 60) + "h ago";
  }
}

// ─── Weather ─────────────────────────────────────────────────────────────────
void fetchWeather()
{
  auto &cfg = Config::instance();
  auto &wx = DisplayManager::instance().weather;

  // Skip if we're in a backoff window
  if (millis() < wxBackoffUntil)
    return;

  // Skip if key or city is missing — not an error, just not configured yet
  if (cfg.data.owmApiKey.isEmpty() || cfg.data.city.isEmpty())
  {
    wx.valid = false;
    wx.errorMsg = "";
    return;
  }

  HTTPClient http;
  String units = cfg.data.metricUnits ? "metric" : "imperial";
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" +
               cfg.data.city + "&units=" + units +
               "&appid=" + cfg.data.owmApiKey;

  http.begin(url);
  http.setTimeout(8000);
  int code = http.GET();

  Serial.printf("[Weather] HTTP %d\n", code);

  switch (code)
  {
  case 200:
  {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    if (!err)
    {
      wx.temp = doc["main"]["temp"].as<float>();
      wx.feelsLike = doc["main"]["feels_like"].as<float>();
      wx.humidity = doc["main"]["humidity"].as<int>();
      wx.windSpeed = doc["wind"]["speed"].as<float>();
      wx.description = doc["weather"][0]["description"].as<String>();
      wx.icon = doc["weather"][0]["icon"].as<String>();
      wx.valid = true;
      wx.errorMsg = "";
      wxState = WxState::OK;
      wxBackoffUntil = 0;
      Serial.printf("[Weather] OK: %.1f° %s\n", wx.temp, wx.description.c_str());
      saveWeatherCache(); // Persist to flash for next boot
    }
    break;
  }

  case 401:
    // Key invalid or revoked — stop retrying until user fixes key in /config
    wx.valid = false;
    wx.errorMsg = "API key invalid";
    wxState = WxState::BAD_KEY;
    wxBackoffUntil = UINT32_MAX; // Never auto-retry
    Serial.println("[Weather] 401 — API key invalid or revoked. Fix in /config");
    break;

  case 404:
    // City not found — stop retrying until user fixes city in /config
    wx.valid = false;
    wx.errorMsg = "City not found";
    wxState = WxState::BAD_CITY;
    wxBackoffUntil = UINT32_MAX; // Never auto-retry
    Serial.println("[Weather] 404 — City not found. Fix in /config");
    break;

  case 429:
    // Free tier rate limit — back off 60 minutes (shouldn't happen at 20-min intervals)
    wx.errorMsg = "Rate limited";
    wxState = WxState::RATE_LIMITED;
    wxBackoffUntil = millis() + 60UL * 60 * 1000;
    Serial.println("[Weather] 429 — Rate limited. Backing off 60 min");
    break;

  default:
    if (code >= 500)
    {
      // OWM server error — back off 10 minutes
      wx.errorMsg = "Weather service error";
      wxState = WxState::SERVER_ERROR;
      wxBackoffUntil = millis() + 10UL * 60 * 1000;
      Serial.printf("[Weather] %d — Server error. Backing off 10 min\n", code);
    }
    else if (code < 0)
    {
      // Connection failed (WiFi down) — back off 2 minutes, don't clear valid data
      wx.errorMsg = "No connection";
      wxBackoffUntil = millis() + 2UL * 60 * 1000;
      Serial.println("[Weather] Connection failed. Backing off 2 min");
    }
    break;
  }

  http.end();
}

// ─── WiFi STA mode ───────────────────────────────────────────────────────────
void startSTA()
{
  auto &cfg = Config::instance();
  Serial.printf("[WiFi] Connecting to: %s\n", cfg.data.wifiSSID.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.data.wifiSSID.c_str(), cfg.data.wifiPassword.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000)
  {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("[WiFi] Failed to connect — falling back to AP mode");
    startAP(); // Declare before use; see below
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
  while (!getLocalTime(&ti) && attempts++ < 20)
  {
    delay(500);
  }
  if (attempts < 20)
  {
    Serial.println("[NTP] Time synced");
  }
  else
  {
    Serial.println("[NTP] Sync timeout — using RTC if available");
  }

  fetchWeather();
  lastWxFetch = millis();
}

// ─── WiFi AP mode (captive portal) ───────────────────────────────────────────
void startAP()
{
  apMode = true;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ScheduleTracker-Setup", "setup1234");
  dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));
  Serial.println("[WiFi] AP mode: ScheduleTracker-Setup");
  Serial.println("[WiFi] Portal: http://192.168.4.1");
}

// ─── Task event handler ───────────────────────────────────────────────────────
void onTaskEvent(const Task &task, TaskEvent event)
{
  if (Config::instance().data.muted)
    return;

  switch (event)
  {
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

// ─── setup() ─────────────────────────────────────────────────────────────────
void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[Boot] Schedule Tracker starting...");

  // Init LittleFS
  if (!LittleFS.begin(true))
  {
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
  BacklightManager::instance().begin(/*pin=*/7, /*fullBrightness=*/255,
                                     /*dimAfterMs=*/60000, /*dimBrightness=*/30);

  // Load weather cache before WiFi connects so display shows data immediately
  loadWeatherCache();

  // Load schedule
  TaskScheduler::instance().loadFromFile();
  TaskScheduler::instance().onEvent(onTaskEvent);

  // Network
  if (Config::instance().data.wifiSSID.isEmpty())
  {
    startAP();
  }
  else
  {
    startSTA();
  }

  // Web server (serves both AP portal and STA dashboard)
  AppWebServer::instance().begin();

  Serial.println("[Boot] Ready.");
}

// ─── loop() ──────────────────────────────────────────────────────────────────
void loop()
{
  // DNS for captive portal
  if (apMode)
    dnsServer.processNextRequest();

  uint32_t now_ms = millis();

  // ── 1-second tick ──────────────────────────────────────────────────────────
  // ── 1-second tick ──────────────────────────────────────────
  if (now_ms - lastTick >= 1000)
  {
    lastTick = now_ms;
    time_t t = time(nullptr);
    std::tm *tm_now = localtime(&t);
    TaskScheduler::instance().tick(tm_now);
    if (!DisplayManager::instance().overlayActive())
      DisplayManager::instance().update(tm_now);
  }
  // ── WebSocket broadcast every 1s ───────────────────────────────────────────
  if (now_ms - lastWsBcast >= 1000)
  {
    lastWsBcast = now_ms;
    AppWebServer::instance().broadcastStatus();
  }

  // ── Battery update every 30s ───────────────────────────────────────────────
  if (now_ms - lastBatUpdate >= 30000)
  {
    lastBatUpdate = now_ms;
    BatteryMonitor::instance().update();
  }

  // ── Weather refresh every 20 minutes ───────────────────────────────────────
  // fetchWeather() internally checks wxBackoffUntil — for permanent errors
  // (bad key, bad city) it will no-op until the user fixes config and the
  // POST /api/config handler resets wxBackoffUntil to 0.
  if (!apMode && now_ms - lastWxFetch >= 20UL * 60 * 1000)
  {
    lastWxFetch = now_ms;
    fetchWeather();
  }

  // ── Backlight auto-dim ─────────────────────────────────────────────────────
  BacklightManager::instance().tick();

  // ── Touch input ─────────────────────────────────────────────
  DisplayManager::instance().updateTaskListScroll(); // drag-scroll, every iteration
  if (DisplayManager::instance().consumeDirty())
  {
    time_t t = time(nullptr);
    std::tm *tm_now = localtime(&t);
    DisplayManager::instance().update(tm_now);
  }

  if (DisplayManager::instance().overlayActive())
  {
    DisplayManager::instance().tickOverlay();
  }
  else
  {
    static uint32_t lastTouch = 0;
    if (now_ms - lastTouch > 300)
    {
      int zone = DisplayManager::instance().pollTouch();
      if (zone >= 0)
      {
        lastTouch = now_ms;
        BacklightManager::instance().wake();
        if (!Config::instance().data.muted)
          AudioManager::instance().beep(1200, 18);

        switch (zone)
        {
        case 1:
          DisplayManager::instance().setScreen(Screen::TASK_LIST);
          break;
        case 2:
        {
          auto &cfg = Config::instance();
          cfg.data.muted = !cfg.data.muted;
          cfg.save();
          break;
        }
        case 3:
          DisplayManager::instance().setScreen(Screen::HOME);
          break;
        case 4:
          DisplayManager::instance().showClothingOverlay();
          break;
        case 5:
          DisplayManager::instance().setScreen(Screen::TIMER_SET);
          break;
        }
      }
    }
  }

  // ── Audio ──────────────────────────────────────────────────────────────────
  AudioManager::instance().loop();
}
