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

    // Umgebungsvariablen-Namen für LLM-API-Konfiguration
    constexpr const wchar_t* ENV_API_ENDPOINT = L"SCREENSCANNER_API_ENDPOINT";
    constexpr const wchar_t* ENV_API_KEY = L"SCREENSCANNER_API_KEY";
    constexpr const wchar_t* ENV_API_MODEL = L"SCREENSCANNER_API_MODEL";

    // Standard Gemini-OpenAI Endpunkt und Modell
    constexpr const wchar_t* DEFAULT_API_ENDPOINT = L"https://generativelanguage.googleapis.com/v1beta/openai/chat/completions";
    constexpr const wchar_t* DEFAULT_API_MODEL = L"gemini-3.5-flash-lite";

    // Modus-Pop-up Menü-IDs
    constexpr int ID_MODE_SEARCH = 3001;
    constexpr int ID_MODE_EXPLAIN = 3002;
    constexpr int ID_MODE_COPY = 3003;
}
