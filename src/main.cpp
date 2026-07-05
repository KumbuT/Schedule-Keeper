#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#define BUZZER_PIN 6      // Labeled D4
#define TOUCH_CS_PIN 7    // Labeled D5 (GPIO 7)
#define BATTERY_ADC_PIN 2 // Labeled D0 / A0
#define IDLE_SLEEP_TIMEOUT_MS 300000

// Fine-tuned configuration architecture optimized for the red AliExpress panel
class LGFX_XIAO_ILI9341 : public lgfx::LGFX_Device
{
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Touch_XPT2046 _touch_instance;

public:
  LGFX_XIAO_ILI9341()
  {
    { // Configure Primary Shared SPI Bus Lines
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000; // Stable fast write speed for ILI9341
      cfg.freq_read = 16000000;
      cfg.pin_sclk = 8;  // SCK / D8
      cfg.pin_mosi = 10; // SDI / D10
      cfg.pin_miso = 9;  // SDO / D9
      cfg.pin_dc = 4;    // DC / D2
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    { // Sync LCD Display Panel Options
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 3;  // CS / D1
      cfg.pin_rst = 5; // RESET / D3
      cfg.panel_width = 240;
      cfg.panel_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.invert = false; // The red AliExpress panel does NOT require color inversion
      _panel_instance.config(cfg);
    }
    { // Sync Resistive XPT2046 Touch Layer Parameters
      auto cfg = _touch_instance.config();
      cfg.x_min = 300; // Standard resistance range tracking coordinates
      cfg.x_max = 3900;
      cfg.y_min = 200;
      cfg.y_max = 3700;
      cfg.pin_cs = TOUCH_CS_PIN; // T_CS / D5 (GPIO 7)
      cfg.freq = 2500000;        // Hardware constraint: XPT2046 requires safe 2.5MHz clock limits
      cfg.bus_shared = true;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }
    setPanel(&_panel_instance);
  }
};

LGFX_XIAO_ILI9341 tft;

struct Task
{
  String name;
  int durationSeconds;
};

std::vector<Task> taskList;
int currentTaskIndex = 0;
long timeRemaining = 0;
bool isPaused = false;
unsigned long lastTickTime = 0;
unsigned long lastBatteryCheckTime = 0;
unsigned long lastActivityTime = 0;
int cachedBatteryPercentage = 100;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

int calculateBatteryPercentage()
{
  uint32_t rawSum = 0;
  for (int i = 0; i < 10; i++)
  {
    rawSum += analogReadMilliVolts(BATTERY_ADC_PIN);
    delay(2);
  }
  float cellVoltage = ((rawSum / 10.0f) * 2.0f) / 1000.0f;
  if (cellVoltage >= 4.20f)
    return 100;
  if (cellVoltage <= 3.50f)
    return 0;
  return (int)((cellVoltage - 3.50f) / (4.20f - 3.50f) * 100.0f);
}

String formatTime(long totalSeconds) {
  int minutes = totalSeconds / 60;
  int seconds = totalSeconds % 60;
  char buffer[16]; // FIX: Changed from 'char buffer' to a char array buffer
  snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, seconds);
  return String(buffer);
}


void broadcastStateViaWebSocket()
{
  if (ws.count() == 0)
    return;
  JsonDocument doc;
  if (!taskList.empty() && !isPaused && timeRemaining > 0)
  {
    doc["task"] = taskList[currentTaskIndex].name;
    doc["time"] = formatTime(timeRemaining);
  }
  else if (isPaused && !taskList.empty())
  {
    doc["task"] = taskList[currentTaskIndex].name + " (Paused)";
    doc["time"] = formatTime(timeRemaining);
  }
  else
  {
    doc["task"] = "Routine Stopped";
    doc["time"] = "00:00";
  }
  doc["battery"] = cachedBatteryPercentage;
  doc["isPaused"] = isPaused;
  String response;
  serializeJson(doc, response);
  ws.textAll(response);
}

void enterDeepSleep()
{
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(textdatum_t::middle_center);
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString("Deep Sleeping...", 120, 140, &fonts::Font4);
  tft.drawString("Touch Screen to Wake", 120, 180, &fonts::Font2);
  delay(1000);

  tft.writeCommand(0x10); // Display Hardware Sleep Command

  // Maps Touch Chip Select pin (GPIO 7) to act as hardware wakeup trigger pin
  esp_deep_sleep_enable_gpio_wakeup(1ULL << TOUCH_CS_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_start();
}

void drawButton(int x, int y, int w, int h, String label, uint32_t color)
{
  tft.fillRoundRect(x, y, w, h, 8, color);
  tft.drawRoundRect(x, y, w, h, 8, TFT_WHITE);
  tft.setTextDatum(textdatum_t::middle_center);
  tft.setTextColor(TFT_WHITE);
  tft.drawString(label, x + (w / 2), y + (h / 2), &fonts::Font2);
}

void updateDisplay()
{
  tft.fillScreen(TFT_BLACK);

  tft.setTextDatum(textdatum_t::top_right);
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString(String(cachedBatteryPercentage) + "% BAT", 230, 10, &fonts::Font2);

  if (taskList.empty())
  {
    tft.setTextDatum(textdatum_t::middle_center);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("No Tasks Loaded", 120, 160, &fonts::Font4);
    return;
  }

  tft.setTextDatum(textdatum_t::top_center);
  tft.setTextColor(TFT_CYAN);
  tft.drawString(taskList[currentTaskIndex].name, 120, 45, &fonts::Font4);

  tft.setTextDatum(textdatum_t::middle_center);
  tft.setTextColor(isPaused ? TFT_YELLOW : TFT_GREEN, TFT_BLACK);
  tft.drawString(formatTime(timeRemaining), 120, 125, &fonts::Font7);

  // Graphical Target Box Visual Anchors
  drawButton(15, 220, 95, 45, isPaused ? "RESUME" : "PAUSE", isPaused ? 0x03E0 : 0xD3A0);
  drawButton(130, 220, 95, 45, "SKIP NEXT", 0x318F);

  tft.setTextDatum(textdatum_t::bottom_center);
  tft.setTextColor(TFT_DARKGREY);
  if (currentTaskIndex + 1 < (int)taskList.size())
  {
    tft.drawString("Next: " + taskList[currentTaskIndex + 1].name, 120, 305, &fonts::Font2);
  }
  else
  {
    tft.drawString("Final Task Sequence", 120, 305, &fonts::Font2);
  }
}

void loadTasks()
{
  if (!LittleFS.exists("/tasks.json"))
  {
    taskList.push_back({"Drink Coffee", 600});
    taskList.push_back({"Restroom Break", 1200});
    return;
  }
  File file = LittleFS.open("/tasks.json", "r");
  JsonDocument doc;
  deserializeJson(doc, file);
  file.close();

  taskList.clear();
  JsonArray array = doc.as<JsonArray>();
  for (JsonVariant v : array)
  {
    taskList.push_back({v["name"].as<String>(), v["duration"].as<int>()});
  }
}

void saveTasks(String jsonString)
{
  File file = LittleFS.open("/tasks.json", "w");
  if (file)
  {
    file.print(jsonString);
    file.close();
  }
  loadTasks();
  currentTaskIndex = 0;
  if (!taskList.empty())
    timeRemaining = taskList[currentTaskIndex].durationSeconds;
  isPaused = false;
  lastActivityTime = millis();
  updateDisplay();
}

void triggerAlarm()
{
  tft.fillScreen(TFT_RED);
  tft.setTextDatum(textdatum_t::middle_center);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("TIME'S UP!", 120, 160, &fonts::Font4);
  for (int i = 0; i < 5; i++)
  {
    tone(BUZZER_PIN, 1000, 300);
    delay(400);
  }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    lastActivityTime = millis();
    broadcastStateViaWebSocket();
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  analogReadResolution(12);

  tft.init();
  tft.setRotation(2); // Match orientation
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Booting Lovyan Touch...", 10, 10, &fonts::Font2);

  if (!LittleFS.begin(true))
  {
    Serial.println("LittleFS Error");
  }

  WiFiManager wm;
  tft.drawString("AP: TaskTimer-Setup", 10, 40, &fonts::Font2);
  if (!wm.autoConnect("TaskTimer-Setup"))
  {
    ESP.restart();
  }

  loadTasks();
  if (!taskList.empty())
  {
    timeRemaining = taskList[currentTaskIndex].durationSeconds;
  }

  cachedBatteryPercentage = calculateBatteryPercentage();
  lastActivityTime = millis();

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/index.html", "text/html", false, [](const String &var) -> String
                            {
      if (var == "TASK_JSON") {
        File file = LittleFS.open("/tasks.json", "r");
        if (!file) return "[]";
        String content = file.readString();
        file.close();
        return content;
      }
      return String(); }); });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    if (request->hasParam("json", true)) {
      String json = request->getParam("json", true)->value();
      saveTasks(json);
      request->send(200, "text/html", "<h3>Synced Successfully!</h3><a href='/'>Back</a>");
    } });

  server.begin();
  updateDisplay();
}

void loop()
{
  ws.cleanupClients();

  // Dynamic Touch Vector Detection Matrix Processor
  int32_t touchX = 0, touchY = 0;
  if (tft.getTouch(&touchX, &touchY))
  {
    lastActivityTime = millis(); // Reset idle sleep matrix timer

    // Bounds Check across Button Y-Axis Segment
    if (touchY >= 220 && touchY <= 265)
    {

      // Target Check A: Left Button (Pause/Resume Match)
      if (touchX >= 15 && touchX <= 110)
      {
        isPaused = !isPaused;
        updateDisplay();
        broadcastStateViaWebSocket();
        delay(300); // Prevent multi-trigger bouncing
      }

      // Target Check B: Right Button (Skip Next Routine Item)
      else if (touchX >= 130 && touchX <= 225)
      {
        if (!taskList.empty())
        {
          currentTaskIndex = (currentTaskIndex + 1) % taskList.size();
          timeRemaining = taskList[currentTaskIndex].durationSeconds;
          isPaused = false;
          updateDisplay();
          broadcastStateViaWebSocket();
          delay(300);
        }
      }
    }
  }

  if (millis() - lastBatteryCheckTime >= 30000)
  {
    lastBatteryCheckTime = millis();
    cachedBatteryPercentage = calculateBatteryPercentage();
  }
  if (!isPaused && !taskList.empty())
  {
    if (millis() - lastTickTime >= 1000)
    {
      lastTickTime = millis();
      lastActivityTime = millis();
      if (timeRemaining > 0)
      {
        timeRemaining--;
        updateDisplay();
      }
      else
      {
        triggerAlarm();
        currentTaskIndex++;
        if (currentTaskIndex < (int)taskList.size())
        {
          timeRemaining = taskList[currentTaskIndex].durationSeconds;
          updateDisplay();
        }
        else
        {
          currentTaskIndex = 0;
          timeRemaining = taskList[currentTaskIndex].durationSeconds;
          isPaused = true;
          updateDisplay();
        }
      }
      broadcastStateViaWebSocket();
    }
  }
  if (isPaused || taskList.empty())
  {
    if (millis() - lastActivityTime >= IDLE_SLEEP_TIMEOUT_MS)
    {
      enterDeepSleep();
    }
  }
}