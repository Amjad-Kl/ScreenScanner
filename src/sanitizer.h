#pragma once
// ============================================================================
// sanitizer.h – Lokaler Sicherheits-Filter für OCR-Text
// ============================================================================
// Maskiert sensible Daten (E-Mails, IPs, Tokens, Pfade, …) bevor der Text
// an eine externe LLM-API gesendet wird.  100 % RAM-only, keine Festplatten-
// Protokollierung sensibler Daten.
//
// C++17 / MSVC / Wide-Strings (std::wstring) durchgängig.
// ============================================================================

#include <string>

namespace Sanitizer {

    // ── Haupt-API ───────────────────────────────────────────────────────────

    /// Bereinigt den rohen OCR-Text und gibt die maskierte Fassung zurück.
    /// Reihenfolge: Regex-Ersetzungen → Shannon-Entropie-Check.
    std::wstring Sanitize(const std::wstring& rawText);

    /// Gibt eine gekürzte Vorschau (max. 200 Zeichen) des bereits
    /// bereinigten Textes zurück.  Wird am Ende ggf. mit „…" abgeschnitten.
    std::wstring GetPreview(const std::wstring& sanitized);

    /// Post-Check: prüft, ob im Text noch Platzhalter ([EMAIL], [TOKEN] …)
    /// oder anderweitig verdächtige Muster enthalten sind.
    bool ContainsSuspiciousContent(const std::wstring& text);

    // ── Hilfsfunktion (öffentlich für Tests) ────────────────────────────────

    /// Berechnet die Shannon-Entropie eines Tokens (in Bit pro Zeichen).
    double CalculateEntropy(const std::wstring& token);

} // namespace Sanitizer
