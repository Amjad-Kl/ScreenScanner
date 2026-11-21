#pragma once
// ============================================================================
// query_builder.h – LLM-basierte Suchanfragen-Generierung
// ============================================================================
// Sendet bereinigten OCR-Text an eine OpenAI-kompatible Chat-Completions-API
// (standardmäßig Google Gemini OpenAI-Endpunkt) und liefert eine optimierte
// Google-Suchanfrage zurück.
//
// Konfiguration über Umgebungsvariablen:
//   SCREENSCANNER_API_ENDPOINT  –  z.B. https://generativelanguage.googleapis.com/v1beta/openai/chat/completions
//   SCREENSCANNER_API_KEY       –  API-Schlüssel (wird NIEMALS im Klartext geloggt)
//   SCREENSCANNER_API_MODEL     –  Modellname (Standard: gemini-3.5-flash-lite)
//
// C++17 / MSVC / Wide-Strings (std::wstring) durchgängig.
// ============================================================================

#include <string>

namespace QueryBuilder {

    // ── Such-Modus ──────────────────────────────────────────────────────────

    /// Bestimmt, welcher System-Prompt an die LLM-API gesendet wird.
    enum class SearchMode {
        Search,   // Allgemeine, präzise Google-Suchanfrage
        Explain   // Suchanfrage mit Fokus auf Erklärung / Definition / Ursache
    };

    // ── Initialisierung ─────────────────────────────────────────────────────

    /// Lädt API-Endpoint, API-Key und Modell aus den Umgebungsvariablen.
    /// Falls Endpoint oder Modell nicht gesetzt sind, werden die Gemini-Defaults geladen.
    /// Gibt true zurück, wenn ein API-Key vorhanden ist.
    bool Initialize();

    /// Prüft, ob ein API-Key konfiguriert ist (nach Initialize()).
    bool IsAvailable();

    // ── Kern-Funktionalität ─────────────────────────────────────────────────

    /// Sendet den bereinigten Text an die LLM-API und gibt die generierte
    /// Suchanfrage zurück.  Bei Fehler wird ein Diagnose-Hinweis zurückgegeben.
    std::wstring GenerateQuery(const std::wstring& sanitizedText, SearchMode mode);

    /// Gibt die letzte Fehlermeldung zurück (oder leer, wenn der Aufruf erfolgreich war).
    std::wstring GetLastError();

} // namespace QueryBuilder
