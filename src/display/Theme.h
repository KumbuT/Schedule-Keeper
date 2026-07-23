#pragma once
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// Shared RGB565 color palette.
//
// These moved out of DisplayManager (where they were private static members)
// so that the Widget classes can reference the exact same constants. The names
// are unchanged and at global scope, so every existing `CLR_*` reference in
// DisplayManager.cpp keeps compiling untouched. constexpr => internal linkage,
// so including this header in multiple translation units is safe (no ODR
// clash).
// ─────────────────────────────────────────────────────────────────────────────
constexpr uint32_t CLR_BG = 0x1082;
constexpr uint32_t CLR_CARD = 0x2965;     // slightly saturated card fill
constexpr uint32_t CLR_ACCENT = 0x07FF;   // cyan
constexpr uint32_t CLR_TEXT = 0xFFFF;
constexpr uint32_t CLR_SUBTEXT = 0xAD75;  // gray
constexpr uint32_t CLR_GREEN = 0x07E0;
constexpr uint32_t CLR_YELLOW = 0xFFE0;
constexpr uint32_t CLR_RED = 0xF800;
constexpr uint32_t CLR_ORANGE = 0xFDA0;
constexpr uint32_t CLR_PINK = 0xFE19;
constexpr uint32_t CLR_BAR_BG = 0x39C7;
constexpr uint32_t CLR_STATUSBG = 0x0841;
constexpr uint32_t CLR_CLOUD = 0xC618;    // weather-icon cloud body
