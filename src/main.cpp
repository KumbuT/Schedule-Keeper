#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>

// Core Display Engine Framework
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// Hardware Pin Defs for Seeed Studio XIAO ESP32-C3
#define BUZZER_PIN 6      // Labeled D4
#define BTN_NEXT_PIN 7    // Labeled D5
#define BTN_PAUSE_PIN 21  // Labeled D6 (GPIO 21)
#define BATTERY_ADC_PIN 2 // Labeled D0 / A0

#define IDLE_SLEEP_TIMEOUT_MS 300000

// Dynamic LovyanGFX Hardware Bus Mapping Class Configuration
class LGFX_XIAO_ILI9341 : public lgfx::LGFX_Device
{
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX_XIAO_ILI9341()
  {
    { // Configure standard high-speed SPI bus communication pipeline paths
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000; // Standard stable 40MHz write frequency
      cfg.pin_sclk = 8;          // SCK / D8
      cfg.pin_mosi = 10;         // MOSI / D10
      cfg.pin_miso = 9;          // MISO / D9
      cfg.pin_dc = 4;            // DC / D2
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    { // Sync physical screen alignment profiles
      auto cfg = _panel_instance.config();
      cfg.pin_cs = 3;  // CS / D1
      cfg.pin_rst = 5; // RST / D3
      cfg.panel_width = 240;
      cfg.panel_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.invert = true; // FIX: Changed from 'inverted' to 'invert' to match API bounds
      _panel_instance.config(cfg);
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

volatile bool nextButtonPressed = false;
volatile bool pauseButtonPressed = false;

void IRAM_ATTR handleNextButton()
{
  nextButtonPressed = true;
}

void IRAM_ATTR handlePauseButton()
{
  pauseButtonPressed = true;
}

int calculateBatteryPercentage()
{
  uint32_t rawSum = 0;
  for (int i = 0; i < 10; i++)
  {
    rawSum += analogReadMilliVolts(BATTERY_ADC_PIN);
    delay(2);
  }
  // Safe 200k/200k external resistor tracking math evaluation calculation logic
  float cellVoltage = ((rawSum / 10.0f) * 2.0f) / 1000.0f;

  if (cellVoltage >= 4.20f)
    return 100;
  if (cellVoltage <= 3.50f)
    return 0;
  return (int)((cellVoltage - 3.50f) / (4.20f - 3.50f) * 100.0f);
}

String formatTime(long totalSeconds)
{
  int minutes = totalSeconds / 60;
  int seconds = totalSeconds % 60;
  char buffer[16];
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
  tft.drawString("Press Next to Wake", 120, 180, &fonts::Font2);
  delay(1000);

  tft.writeCommand(0x10); // Native Sleep execution register command shift

  esp_deep_sleep_enable_gpio_wakeup(1ULL << BTN_NEXT_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_start();
}

void updateDisplay()
{
  tft.fillScreen(TFT_BLACK);

  // Render battery tracking text to the top-right corner
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

  // Draw current primary running target names
  tft.setTextDatum(textdatum_t::top_center);
  tft.setTextColor(TFT_CYAN);
  tft.drawString(taskList[currentTaskIndex].name, 120, 50, &fonts::Font4);

  // Print current relative clock position elements
  tft.setTextDatum(textdatum_t::middle_center);
  tft.setTextColor(isPaused ? TFT_YELLOW : TFT_GREEN, TFT_BLACK);
  tft.drawString(formatTime(timeRemaining), 120, 150, &fonts::Font7);

  // Compute alternative pipeline context visibility flags
  tft.setTextDatum(textdatum_t::bottom_center);
  tft.setTextColor(TFT_DARKGREY);
  if (currentTaskIndex + 1 < (int)taskList.size())
  {
    tft.drawString("Next: " + taskList[currentTaskIndex + 1].name, 120, 290, &fonts::Font2);
  }
  else
  {
    tft.drawString("Final Task Sequence", 120, 290, &fonts::Font2);
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
  pinMode(BTN_NEXT_PIN, INPUT_PULLUP);
  pinMode(BTN_PAUSE_PIN, INPUT_PULLUP);
  analogReadResolution(12);

  // FIX: Passed raw pin numbers straight into ISR attachment loops to bypass missing macro references
  attachInterrupt(BTN_NEXT_PIN, handleNextButton, FALLING);
  attachInterrupt(BTN_PAUSE_PIN, handlePauseButton, FALLING);

  tft.init();
  tft.setRotation(2); // Inverted axis adjustment
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Booting LovyanGFX...", 10, 10, &fonts::Font2);

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

  if (nextButtonPressed)
  {
    delay(50);
    if (digitalRead(BTN_NEXT_PIN) == LOW)
    {
      lastActivityTime = millis();
      if (!taskList.empty())
      {
        currentTaskIndex = (currentTaskIndex + 1) % taskList.size();
        timeRemaining = taskList[currentTaskIndex].durationSeconds;
        isPaused = false;
        updateDisplay();
        broadcastStateViaWebSocket();
      }
    }
    nextButtonPressed = false;
  }

  if (pauseButtonPressed)
  {
    delay(50);
    if (digitalRead(BTN_PAUSE_PIN) == LOW)
    {
      lastActivityTime = millis();
      isPaused = !isPaused;
      updateDisplay();
      broadcastStateViaWebSocket();
    }
    pauseButtonPressed = false;
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