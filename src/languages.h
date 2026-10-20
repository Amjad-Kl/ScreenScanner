// ==============================================================================
// Automatisch generiert durch add_languages.ps1
// ==============================================================================
#pragma once
#include <string>
#include <vector>

namespace Config {
    struct LanguageEntry {
        std::wstring code;        // z. B. L"de", L"ar", L"en", L"es"
        std::wstring name;        // Anzeigename, z. B. L"Deutsch", L"Arabisch"
        std::wstring tag;         // BCP-47 Tag, z. B. L"de-DE", L"ar-SA"
        std::wstring capability;  // Windows FOD Name, z. B. L"Language.OCR~~~de-DE~0.0.1.0"
    };

    // Diese Liste wird automatisch vom Skript (add_languages.ps1) aktualisiert.
    inline const std::vector<LanguageEntry> CONFIGURED_LANGUAGES = {
        { L"de", L"Deutsch", L"de-DE", L"Language.OCR~~~de-DE~0.0.1.0" },
        { L"ar", L"Arabisch", L"ar-SA", L"Language.OCR~~~ar-SA~0.0.1.0" },
        { L"es", L"Spanisch", L"es-ES", L"Language.OCR~~~es-ES~0.0.1.0" }
    };
}
