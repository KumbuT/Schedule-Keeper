#pragma once
#include <Arduino.h>
#include <vector>

// Forward declaration — avoid circular include with DisplayManager.h
struct WeatherData;

struct ClothingItem {
  String icon;   // Emoji rendered as text on TFT
  String label;
};

class ClothingAdvisor {
public:
  static std::vector<ClothingItem> recommend(const WeatherData& wx);

private:
  static bool _isRainy (const String& desc);
  static bool _isSnowy (const String& desc);
  static bool _isStormy(const String& desc);
};
