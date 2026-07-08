#include "ClothingAdvisor.h"
#include "DisplayManager.h"  // for WeatherData definition

bool ClothingAdvisor::_isRainy(const String& desc) {
  String d = desc; d.toLowerCase();
  return d.indexOf("rain") >= 0 || d.indexOf("drizzle") >= 0 ||
         d.indexOf("shower") >= 0;
}

bool ClothingAdvisor::_isSnowy(const String& desc) {
  String d = desc; d.toLowerCase();
  return d.indexOf("snow") >= 0 || d.indexOf("sleet") >= 0 ||
         d.indexOf("blizzard") >= 0;
}

bool ClothingAdvisor::_isStormy(const String& desc) {
  String d = desc; d.toLowerCase();
  return d.indexOf("storm") >= 0 || d.indexOf("thunder") >= 0;
}

std::vector<ClothingItem> ClothingAdvisor::recommend(const WeatherData& wx) {
  std::vector<ClothingItem> items;
  float temp  = wx.temp;       // Metric °C; caller ensures correct units
  float wind  = wx.windSpeed;  // km/h
  bool  rainy = _isRainy(wx.description);
  bool  snowy = _isSnowy(wx.description);
  bool  stormy= _isStormy(wx.description);

  // ── Top layer ────────────────────────────────────────────────────────────
  if (snowy || temp < 0)   items.push_back({"🧥", "Heavy coat"});
  else if (temp < 8)       items.push_back({"🧥", "Winter coat"});
  else if (temp < 14)      items.push_back({"🧣", "Jacket + scarf"});
  else if (temp < 18)      items.push_back({"👕", "Light jacket"});
  else if (temp < 24)      items.push_back({"👕", "T-shirt / shirt"});
  else                     items.push_back({"👕", "Light top"});

  // ── Bottom layer ─────────────────────────────────────────────────────────
  if (temp < 8 || snowy)   items.push_back({"👖", "Thermal trousers"});
  else if (temp < 18)      items.push_back({"👖", "Trousers / jeans"});
  else                     items.push_back({"🩳", "Shorts / light trousers"});

  // ── Footwear ─────────────────────────────────────────────────────────────
  if (snowy || temp < 2)       items.push_back({"🥾", "Winter boots"});
  else if (rainy || stormy)    items.push_back({"🥾", "Waterproof shoes"});
  else if (temp > 22)          items.push_back({"👟", "Trainers / sandals"});
  else                         items.push_back({"👟", "Trainers"});

  // ── Rain gear ────────────────────────────────────────────────────────────
  if (stormy)                  items.push_back({"⛈",  "Stay indoors if possible"});
  else if (rainy)              items.push_back({"☂️", "Umbrella"});

  // ── Wind chill ───────────────────────────────────────────────────────────
  if (wind > 30 && temp < 15)  items.push_back({"🧣", "Scarf / windbreaker"});
  if (wind > 20 && temp < 10)  items.push_back({"🧤", "Gloves"});

  // ── Sun ──────────────────────────────────────────────────────────────────
  if (temp > 24)               items.push_back({"🕶️", "Sunglasses"});
  if (temp > 28)               items.push_back({"🧴", "Sunscreen"});

  // ── Humidity ─────────────────────────────────────────────────────────────
  if (wx.humidity > 80 && temp > 22)
                               items.push_back({"💧", "Stay hydrated"});

  return items;
}
