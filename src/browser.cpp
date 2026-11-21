// ============================================================================
// browser.cpp – Browser-Öffnung für Google-Suche (Implementierung)
// ============================================================================
// URL-Kodierung (UTF-8 Percent-Encoding) und Öffnen im Standard-Browser
// per ShellExecuteW.
// ============================================================================

#include "browser.h"

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>

namespace Browser {

    // ── URL-Kodierung ───────────────────────────────────────────────────────

    std::wstring UrlEncode(const std::wstring& text) {
        // Schritt 1: Wide-String → UTF-8 konvertieren
        if (text.empty()) return {};

        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                          static_cast<int>(text.size()),
                                          nullptr, 0, nullptr, nullptr);
        if (utf8Len <= 0) return {};

        std::vector<unsigned char> utf8(utf8Len);
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                            static_cast<int>(text.size()),
                            reinterpret_cast<char*>(utf8.data()),
                            utf8Len, nullptr, nullptr);

        // Schritt 2: Percent-Encoding anwenden
        std::wostringstream encoded;
        encoded << std::hex << std::uppercase;

        for (unsigned char byte : utf8) {
            // Unreservierte Zeichen gemäß RFC 3986: A-Z, a-z, 0-9, -, _, ., ~
            if ((byte >= 'A' && byte <= 'Z') ||
                (byte >= 'a' && byte <= 'z') ||
                (byte >= '0' && byte <= '9') ||
                byte == '-' || byte == '_' ||
                byte == '.' || byte == '~') {
                encoded << static_cast<wchar_t>(byte);
            } else if (byte == ' ') {
                // Leerzeichen als '+' kodieren (üblich für Query-Parameter)
                encoded << L'+';
            } else {
                // Alle anderen Zeichen als %XX kodieren
                encoded << L'%'
                        << std::setw(2) << std::setfill(L'0')
                        << static_cast<unsigned int>(byte);
            }
        }

        return encoded.str();
    }

    // ── Google-Suche im Standard-Browser öffnen ─────────────────────────────

    bool OpenGoogleSearch(const std::wstring& query) {
        if (query.empty()) return false;

        // URL zusammenbauen
        std::wstring url = L"https://www.google.com/search?q=" + UrlEncode(query);

        // Im Standard-Browser öffnen
        HINSTANCE result = ShellExecuteW(
            nullptr,        // Kein übergeordnetes Fenster
            L"open",        // Aktion: Öffnen
            url.c_str(),    // URL
            nullptr,        // Keine zusätzlichen Parameter
            nullptr,        // Kein Arbeitsverzeichnis
            SW_SHOWNORMAL   // Normal anzeigen
        );

        // ShellExecuteW gibt einen Wert > 32 bei Erfolg zurück
        return reinterpret_cast<intptr_t>(result) > 32;
    }

} // namespace Browser
