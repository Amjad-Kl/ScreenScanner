#pragma once
#include "capture.h"
#include <string>
#include <vector>

namespace OCR {
    // Initialisiert alle konfigurierten Windows WinRT OCR Engines
    bool Initialize();

    // Gibt die Namen aller erfolgreich geladenen Sprachen zurück
    std::vector<std::wstring> GetLoadedLanguages();

    // Gibt die Namen aller konfigurierten Sprachen zurück, die auf Windows fehlen
    std::vector<std::wstring> GetMissingLanguages();

    // Gibt eine lesbare Zusammenfassung der aktiven Sprachen zurück (z. B. "Deutsch + Arabisch")
    std::wstring GetActiveLanguagesSummary();

    // Rückwärtskompatibilität: Gibt true zurück, falls Arabisch aktiv geladen ist
    bool IsArabicSupported();

    // Erkennt Text in einem CapturedImage (Multi-Language Auto-Detect)
    std::wstring RecognizeText(const CapturedImage& image);
}
