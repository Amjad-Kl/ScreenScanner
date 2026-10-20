#pragma once
#include <windows.h>

namespace Config {
    // Standard-Hotkey: Strg + Shift + S
    // Wenn du lieber F9 oder PrintScreen möchtest, kannst du das hier anpassen.
    constexpr UINT HOTKEY_MODIFIERS = MOD_CONTROL | MOD_SHIFT;
    constexpr UINT HOTKEY_KEY = 'S';
    constexpr int HOTKEY_ID = 1001;

    // Farben für das Overlay
    constexpr COLORREF OVERLAY_BORDER_COLOR = RGB(0, 120, 215); // Windows Akzentblau
    constexpr int OVERLAY_BORDER_WIDTH = 2;
}
