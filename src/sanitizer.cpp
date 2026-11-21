// ============================================================================
// sanitizer.cpp – Implementierung des lokalen Sicherheits-Filters
// ============================================================================
// Maskiert sensible Daten im OCR-Text, bevor dieser an eine externe LLM-API
// gesendet wird.  Arbeitet ausschließlich im RAM – keine Festplatten-
// Protokollierung sensibler Daten.
//
// C++17 / MSVC / Wide-Strings durchgängig.
// ============================================================================

#include "sanitizer.h"

#include <cmath>
#include <cwctype>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace Sanitizer {

// ════════════════════════════════════════════════════════════════════════════
// Interne Hilfsfunktionen (anonymer Namespace)
// ════════════════════════════════════════════════════════════════════════════
namespace {

    // ── OCR-Vorab-Normalisierung ───────────────────────────────────────────
    // Repariert typische OCR-Lesefehler (Gedankenstriche statt Bindestriche,
    // fehlende Backslashes nach Laufwerksbuchstaben, Leerzeichen nach Punkten),
    // damit die Regex-Regeln zuverlässig greifen.
    std::wstring NormalizeOcrText(const std::wstring& input) {
        std::wstring text = input;

        // 1. Unicode-Bindestriche (—, –, −) zu normalem '-' vereinheitlichen
        for (wchar_t& ch : text) {
            if (ch == L'\u2014' || ch == L'\u2013' || ch == L'\u2212') {
                ch = L'-';
            }
        }

        // 2. Verschluckten Backslash nach Laufwerksbuchstaben ergänzen
        //    z. B. "C: Users\..." oder "C: Wsers\..." -> "C:\Users\..."
        static const std::wregex brokenDriveRegex(
            LR"(\b([A-Za-z]):\s+(Users|Wsers|Windows|Program|AppData|Temp)\b)",
            std::regex_constants::icase);
        text = std::regex_replace(text, brokenDriveRegex, L"$1:\\$2");

        // 3. OCR-Leerzeichen nach Punkt in Domains/Hostnamen entfernen
        //    z. B. "srv-dbel. intern.local" -> "srv-dbel.intern.local"
        static const std::wregex brokenDomainDotRegex(
            LR"(([a-zA-Z0-9-])\.\s+(local|intern|internal|lan|corp|intranet|home|de|com|net|org|io|gov|edu)\b)",
            std::regex_constants::icase);
        text = std::regex_replace(text, brokenDomainDotRegex, L"$1.$2");

        return text;
    }

    // ── Regex-Muster + Ersetzungs-Paare ────────────────────────────────────
    // Reihenfolge ist entscheidend: spezifischere Muster zuerst.

    struct RegexRule {
        std::wregex  pattern;
        std::wstring replacement;
    };

    const std::vector<RegexRule>& GetRegexRules() {
        static const std::vector<RegexRule> rules = {

            // 1. E-Mail-Adressen
            {   std::wregex(LR"([a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,})"),
                L"[EMAIL]" },

            // 2. URLs (http / https / ftp)
            {   std::wregex(LR"((https?|ftp)://[^\s<>"']+)"),
                L"[URL]" },

            // 3. IPv4-Adressen (strikt VOR Hostnamen)
            {   std::wregex(LR"(\b(?:\d{1,3}\.){3}\d{1,3}\b)"),
                L"[IP]" },

            // 4. IPv6-Adressen (vereinfachtes Muster)
            {   std::wregex(LR"(\b(?:[0-9a-fA-F]{1,4}:){2,7}[0-9a-fA-F]{1,4}\b)"),
                L"[IP]" },

            // 5a. Windows-Dateipfade inkl. OCR-Leerzeichen vor Dateiendung
            //     (z. B. C:\Wsers\amjad. j son)
            {   std::wregex(LR"((?:[A-Za-z]:\\|\\\\)[^\s<>"']+\s+[a-zA-Z0-9_.-]+\.(?:json|xml|yaml|yml|ini|conf|cfg|env|log|txt|pem|key|crt|db|sqlite))",
                            std::regex_constants::icase),
                L"[PATH]" },

            // 5b. Standard-Windows-Dateipfade (z. B. C:\Users\... oder \\server\share)
            {   std::wregex(LR"((?:[A-Za-z]:\\|\\\\)[^\s<>"']+)"),
                L"[PATH]" },

            // 6. Unix-Dateipfade (z. B. /home/user/.ssh/id_rsa)
            {   std::wregex(LR"((?:/(?:home|etc|usr|var|tmp|opt|root|mnt|srv|dev|proc|sys))/[^\s<>"']+)"),
                L"[PATH]" },

            // 7. API-Token-Präfixe (VOR Hostnamen)
            {   std::wregex(LR"(\b(?:Bearer|Basic)\s+[A-Za-z0-9\-._~+/]+=*\b)"),
                L"[TOKEN]" },
            {   std::wregex(LR"(\b(?:sk-|ghp_|gho_|glpat-|xoxb-|xoxp-)[A-Za-z0-9\-._~+/]{8,}\b)"),
                L"[TOKEN]" },
            {   std::wregex(LR"(\bAKIA[A-Z0-9]{16}\b)"),
                L"[TOKEN]" },

            // 8. Session-IDs / GUIDs (UUID-Format: 8-4-4-4-12)
            {   std::wregex(LR"(\b[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\b)"),
                L"[SESSION_ID]" },

            // 9. Hex-Hashes (exakt 64, 40 oder 32 Hex-Zeichen)
            {   std::wregex(LR"(\b[0-9a-fA-F]{64}\b)"),
                L"[HASH]" },
            {   std::wregex(LR"(\b[0-9a-fA-F]{40}\b)"),
                L"[HASH]" },
            {   std::wregex(LR"(\b[0-9a-fA-F]{32}\b)"),
                L"[HASH]" },

            // 10a. Hostnamen mit Subdomain-Muster (3+ Segmente, z. B. srv-db01.intern.local)
            {   std::wregex(LR"(\b[a-zA-Z0-9](?:[a-zA-Z0-9\-]*[a-zA-Z0-9])?(?:\.[a-zA-Z0-9](?:[a-zA-Z0-9\-]*[a-zA-Z0-9])?){2,}\b)"),
                L"[HOST]" },

            // 10b. Interne 2-teilige Domains (z. B. db01.local, app.intern)
            {   std::wregex(LR"(\b[a-zA-Z0-9](?:[a-zA-Z0-9\-]*[a-zA-Z0-9])?\.(?:local|intern|internal|lan|corp|intranet|home)\b)",
                            std::regex_constants::icase),
                L"[HOST]" },

            // 10c. Interne Server-/Hostnamen ohne Domain-Endung (z. B. host-web02, srv-db01)
            {   std::wregex(LR"(\b(?:srv|host|web|db|app|api|auth|proxy|dc|fw|vpn|node|prod|dev|stage|staging|pc|ws)-[a-zA-Z0-9-]+\b)",
                            std::regex_constants::icase),
                L"[HOST]" },
        };
        return rules;
    }

    // ── Regex-basierte Ersetzungen anwenden ────────────────────────────────
    std::wstring ApplyRegexRules(const std::wstring& text) {
        std::wstring result = text;
        for (const auto& rule : GetRegexRules()) {
            result = std::regex_replace(result, rule.pattern, rule.replacement);
        }
        return result;
    }

    // ── Prüft, ob ein Token ein bereits maskierter Platzhalter ist ────────
    bool IsPlaceholder(const std::wstring& token) {
        if (token.size() >= 3 && token.front() == L'[' && token.back() == L']') {
            return true;
        }
        return false;
    }

    // ── Prüft, ob ein Token nur aus normalen Wort-Zeichen besteht ─────────
    bool IsNormalWord(const std::wstring& token) {
        for (wchar_t ch : token) {
            if (!std::iswalpha(ch) && ch != L'-' && ch != L'\'' && ch != L'\u00E4'
                && ch != L'\u00F6' && ch != L'\u00FC' && ch != L'\u00C4'
                && ch != L'\u00D6' && ch != L'\u00DC' && ch != L'\u00DF') {
                return false;
            }
        }
        return true;
    }

    // ── Entropie-basierte Token-Maskierung (zeilenweise, erhält Zeilenumbrüche)
    std::wstring ApplyEntropyCheck(const std::wstring& text) {
        std::wistringstream lineStream(text);
        std::wostringstream result;
        std::wstring line;
        bool firstLine = true;

        while (std::getline(lineStream, line)) {
            if (!line.empty() && line.back() == L'\r') {
                line.pop_back();
            }

            if (!firstLine) {
                result << L"\r\n";
            }
            firstLine = false;

            std::wistringstream wordStream(line);
            std::wstring token;
            bool firstWord = true;

            while (wordStream >> token) {
                if (!firstWord) {
                    result << L' ';
                }
                firstWord = false;

                if (IsPlaceholder(token)) {
                    result << token;
                    continue;
                }

                if (token.size() >= 16 && !IsNormalWord(token)) {
                    double entropy = CalculateEntropy(token);
                    if (entropy > 4.0) {
                        result << L"[TOKEN]";
                        continue;
                    }
                }

                result << token;
            }
        }

        return result.str();
    }

} // anonymer Namespace

// ════════════════════════════════════════════════════════════════════════════
// Öffentliche API
// ════════════════════════════════════════════════════════════════════════════

double CalculateEntropy(const std::wstring& token) {
    if (token.empty()) {
        return 0.0;
    }

    std::unordered_map<wchar_t, size_t> freq;
    for (wchar_t ch : token) {
        ++freq[ch];
    }

    double entropy = 0.0;
    const double len = static_cast<double>(token.size());

    for (const auto& [ch, count] : freq) {
        double p = static_cast<double>(count) / len;
        if (p > 0.0) {
            entropy -= p * std::log2(p);
        }
    }

    return entropy;
}

std::wstring Sanitize(const std::wstring& rawText) {
    // Schritt 0: OCR-Typo-Normalisierung
    std::wstring normalized = NormalizeOcrText(rawText);

    // Schritt 1: Regex-Ersetzungen
    std::wstring sanitized = ApplyRegexRules(normalized);

    // Schritt 2: Entropie-basierte Erkennung hochentropischer Tokens
    sanitized = ApplyEntropyCheck(sanitized);

    return sanitized;
}

std::wstring GetPreview(const std::wstring& sanitized) {
    constexpr size_t kMaxLen = 200;

    if (sanitized.size() <= kMaxLen) {
        return sanitized;
    }

    return sanitized.substr(0, kMaxLen - 1) + L"\u2026";
}

bool ContainsSuspiciousContent(const std::wstring& text) {
    static const std::wregex placeholderPattern(
        LR"(\[(EMAIL|URL|IP|PATH|HOST|TOKEN|HASH|SESSION_ID|USER)\])");

    if (std::regex_search(text, placeholderPattern)) {
        return true;
    }

    std::wistringstream stream(text);
    std::wstring token;
    while (stream >> token) {
        if (IsPlaceholder(token)) {
            continue;
        }
        if (token.size() >= 16 && !IsNormalWord(token)) {
            if (CalculateEntropy(token) > 4.0) {
                return true;
            }
        }
    }

    return false;
}

} // namespace Sanitizer