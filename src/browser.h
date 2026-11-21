#pragma once
// ============================================================================
// browser.h – Browser-Öffnung für Google-Suche
// ============================================================================
// URL-Kodierung und Öffnen einer Google-Suche im Standard-Browser.
//
// C++17 / MSVC / Wide-Strings (std::wstring) durchgängig.
// ============================================================================

#include <string>

namespace Browser {

    /// URL-kodiert einen Wide-String (UTF-8 Percent-Encoding).
    /// Reservierte und nicht-ASCII-Zeichen werden als %XX kodiert.
    std::wstring UrlEncode(const std::wstring& text);

    /// Öffnet eine Google-Suche mit der übergebenen Query im Standard-Browser.
    /// Gibt true zurück, wenn ShellExecuteW erfolgreich war.
    bool OpenGoogleSearch(const std::wstring& query);

} // namespace Browser
