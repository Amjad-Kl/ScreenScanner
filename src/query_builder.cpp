// ============================================================================
// query_builder.cpp – LLM-basierte Suchanfragen-Generierung (Implementierung)
// ============================================================================
// Verwendet WinHTTP für den HTTP-Request an eine OpenAI-kompatible API
// (speziell Google Gemini OpenAI-Endpunkt).
// JSON-Parsing erfolgt minimal per String-Suche (keine externe Library).
// ============================================================================

#include "query_builder.h"
#include "config.h"
#include "sanitizer.h"

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cstdlib>

#pragma comment(lib, "winhttp.lib")

namespace QueryBuilder {

    // ── Interne Konfigurationsdaten (modul-lokal) ───────────────────────────

    namespace {
        std::wstring g_apiEndpoint;   // Vollständige URL des API-Endpunkts
        std::wstring g_apiKey;        // API-Schlüssel (NIEMALS loggen!)
        std::wstring g_apiModel;      // Modellname
        std::wstring g_lastError;     // Letzter Fehlertext (für Diagnose)
        bool         g_initialized = false;

        // Timeout für HTTP-Requests in Millisekunden
        constexpr DWORD kTimeoutMs = 10000; // 10 Sekunden

        // ── Hilfsfunktion: Umgebungsvariable lesen ──────────────────────────

        std::wstring GetEnvVar(const wchar_t* name) {
            // 1. Zuerst aus aktuellem Prozess-Environment abfragen
            DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
            if (size > 0) {
                std::wstring buffer(size, L'\0');
                GetEnvironmentVariableW(name, buffer.data(), size);
                buffer.resize(size - 1);
                return buffer;
            }

            // 2. Fallback: Direkt aus HKCU\Environment lesen (falls frisch in Windows gesetzt)
            HKEY hKey = nullptr;
            if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                DWORD type = 0;
                DWORD dataSize = 0;
                if (RegQueryValueExW(hKey, name, nullptr, &type, nullptr, &dataSize) == ERROR_SUCCESS && dataSize > 0) {
                    std::vector<wchar_t> regBuffer(dataSize / sizeof(wchar_t) + 1, L'\0');
                    if (RegQueryValueExW(hKey, name, nullptr, &type, reinterpret_cast<LPBYTE>(regBuffer.data()), &dataSize) == ERROR_SUCCESS) {
                        RegCloseKey(hKey);
                        return std::wstring(regBuffer.data());
                    }
                }
                RegCloseKey(hKey);
            }

            return {};
        }

        // ── Hilfsfunktion: Wide-String → UTF-8 ─────────────────────────────

        std::string WideToUtf8(const std::wstring& wide) {
            if (wide.empty()) return {};
            int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                          static_cast<int>(wide.size()),
                                          nullptr, 0, nullptr, nullptr);
            if (len <= 0) return {};

            std::string utf8(len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
                                static_cast<int>(wide.size()),
                                utf8.data(), len, nullptr, nullptr);
            return utf8;
        }

        // ── Hilfsfunktion: UTF-8 → Wide-String ─────────────────────────────

        std::wstring Utf8ToWide(const std::string& utf8) {
            if (utf8.empty()) return {};
            int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                          static_cast<int>(utf8.size()),
                                          nullptr, 0);
            if (len <= 0) return {};

            std::wstring wide(len, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                static_cast<int>(utf8.size()),
                                wide.data(), len);
            return wide;
        }

        // ── Hilfsfunktion: JSON-String escapen ──────────────────────────────

        std::string EscapeJsonString(const std::string& input) {
            std::string result;
            result.reserve(input.size() + 16);
            for (char c : input) {
                switch (c) {
                    case '"':  result += "\\\""; break;
                    case '\\': result += "\\\\"; break;
                    case '\b': result += "\\b";  break;
                    case '\f': result += "\\f";  break;
                    case '\n': result += "\\n";  break;
                    case '\r': result += "\\r";  break;
                    case '\t': result += "\\t";  break;
                    default:
                        if (static_cast<unsigned char>(c) < 0x20) {
                            char buf[8];
                            snprintf(buf, sizeof(buf), "\\u%04x",
                                     static_cast<unsigned int>(c));
                            result += buf;
                        } else {
                            result += c;
                        }
                        break;
                }
            }
            return result;
        }

        // ── Hilfsfunktion: URL-Komponenten extrahieren ──────────────────────

        struct UrlComponents {
            std::wstring host;
            INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
            std::wstring path;
            bool useHttps = true;
        };

        UrlComponents ParseUrl(const std::wstring& url) {
            UrlComponents result;

            std::wstring lower = url;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);

            size_t schemaEnd = 0;
            if (lower.find(L"https://") == 0) {
                schemaEnd = 8;
                result.useHttps = true;
                result.port = INTERNET_DEFAULT_HTTPS_PORT;
            } else if (lower.find(L"http://") == 0) {
                schemaEnd = 7;
                result.useHttps = false;
                result.port = INTERNET_DEFAULT_HTTP_PORT;
            }

            std::wstring rest = url.substr(schemaEnd);
            size_t pathStart = rest.find(L'/');
            if (pathStart != std::wstring::npos) {
                result.host = rest.substr(0, pathStart);
                result.path = rest.substr(pathStart);
            } else {
                result.host = rest;
                result.path = L"/";
            }

            size_t colonPos = result.host.find(L':');
            if (colonPos != std::wstring::npos) {
                std::wstring portStr = result.host.substr(colonPos + 1);
                result.host = result.host.substr(0, colonPos);
                try {
                    result.port = static_cast<INTERNET_PORT>(std::stoi(portStr));
                } catch (...) {
                }
            }

            return result;
        }

        // ── Hilfsfunktion: Minimales JSON-Feld-Parsing ──────────────────────

        /// Extrahiert den String-Wert eines JSON-Feldes ("key": "value").
        std::string ExtractJsonField(const std::string& json, const std::string& key) {
            const std::string patterns[] = {
                "\"" + key + "\":\"",
                "\"" + key + "\": \"",
                "\"" + key + "\" : \"",
                "\"" + key + "\" :\"",
            };

            for (const auto& pattern : patterns) {
                size_t pos = json.find(pattern);
                if (pos == std::string::npos) continue;

                size_t valueStart = pos + pattern.size();
                std::string value;
                bool escaped = false;
                for (size_t i = valueStart; i < json.size(); ++i) {
                    char c = json[i];
                    if (escaped) {
                        switch (c) {
                            case '"':  value += '"';  break;
                            case '\\': value += '\\'; break;
                            case 'n':  value += ' ';  break;
                            case 'r':  break;
                            case 't':  value += ' ';  break;
                            case '/':  value += '/';  break;
                            default:   value += c;    break;
                        }
                        escaped = false;
                    } else if (c == '\\') {
                        escaped = true;
                    } else if (c == '"') {
                        break;
                    } else {
                        value += c;
                    }
                }
                return value;
            }
            return {};
        }

        // ── Hilfsfunktion: HTTP POST per WinHTTP ────────────────────────────

        struct HttpResponse {
            DWORD statusCode = 0;
            std::string body;
            std::wstring networkError;
        };

        HttpResponse HttpPost(const UrlComponents& urlParts,
                              const std::string& jsonBody,
                              const std::wstring& apiKey) {
            HttpResponse response;

            HINTERNET hSession = WinHttpOpen(
                L"ScreenScanner/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0);
            if (!hSession) {
                response.networkError = L"WinHttpOpen fehlgeschlagen (Code: " + std::to_wstring(::GetLastError()) + L")";
                return response;
            }

            WinHttpSetTimeouts(hSession, kTimeoutMs, kTimeoutMs, kTimeoutMs, kTimeoutMs);

            HINTERNET hConnect = WinHttpConnect(
                hSession,
                urlParts.host.c_str(),
                urlParts.port,
                0);
            if (!hConnect) {
                response.networkError = L"Verbindung zum Server fehlgeschlagen (Code: " + std::to_wstring(::GetLastError()) + L")";
                WinHttpCloseHandle(hSession);
                return response;
            }

            DWORD flags = urlParts.useHttps ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET hRequest = WinHttpOpenRequest(
                hConnect,
                L"POST",
                urlParts.path.c_str(),
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                flags);
            if (!hRequest) {
                response.networkError = L"WinHttpOpenRequest fehlgeschlagen (Code: " + std::to_wstring(::GetLastError()) + L")";
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return response;
            }

            std::wstring headers = L"Content-Type: application/json\r\n";
            headers += L"Authorization: Bearer " + apiKey + L"\r\n";

            WinHttpAddRequestHeaders(hRequest, headers.c_str(),
                                     static_cast<DWORD>(headers.size()),
                                     WINHTTP_ADDREQ_FLAG_ADD);

            BOOL sent = WinHttpSendRequest(
                hRequest,
                WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                const_cast<char*>(jsonBody.c_str()),
                static_cast<DWORD>(jsonBody.size()),
                static_cast<DWORD>(jsonBody.size()),
                0);
            if (!sent) {
                response.networkError = L"HTTP-Request konnte nicht gesendet werden (Code: " + std::to_wstring(::GetLastError()) + L")";
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return response;
            }

            if (!WinHttpReceiveResponse(hRequest, nullptr)) {
                response.networkError = L"Keine Antwort vom Server erhalten (Code: " + std::to_wstring(::GetLastError()) + L")";
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return response;
            }

            // Statuscode abfragen
            DWORD statusCode = 0;
            DWORD statusCodeSize = sizeof(statusCode);
            WinHttpQueryHeaders(hRequest,
                                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX,
                                &statusCode, &statusCodeSize,
                                WINHTTP_NO_HEADER_INDEX);
            response.statusCode = statusCode;

            // Response-Body lesen (auch bei != 200, um Google-Fehlermeldungen zu parsen)
            DWORD bytesAvailable = 0;
            while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
                std::vector<char> buffer(bytesAvailable);
                DWORD bytesRead = 0;
                if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
                    response.body.append(buffer.data(), bytesRead);
                }
            }

            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);

            return response;
        }

    } // anonymer Namespace

    // ── Öffentliche API ─────────────────────────────────────────────────────

    bool Initialize() {
        g_apiEndpoint = GetEnvVar(Config::ENV_API_ENDPOINT);
        g_apiKey      = GetEnvVar(Config::ENV_API_KEY);
        g_apiModel    = GetEnvVar(Config::ENV_API_MODEL);

        // Standard-Endpunkt setzen falls nicht gesetzt
        if (g_apiEndpoint.empty()) {
            g_apiEndpoint = Config::DEFAULT_API_ENDPOINT;
        }

        // Standardmodell setzen falls nicht konfiguriert
        if (g_apiModel.empty()) {
            g_apiModel = Config::DEFAULT_API_MODEL;
        }
        // Schutz vor veralteten Modellen, die von Google mit 404 beantwortet werden
        else if (g_apiModel == L"gemini-2.5-flash-lite" || g_apiModel == L"gemini-2.5-flash") {
            g_apiModel = Config::DEFAULT_API_MODEL;
        }

        g_initialized = !g_apiKey.empty();
        return g_initialized;
    }

    bool IsAvailable() {
        return g_initialized && !g_apiKey.empty();
    }

    std::wstring GetLastError() {
        return g_lastError;
    }

    std::wstring GenerateQuery(const std::wstring& sanitizedText, SearchMode mode) {
        if (!IsAvailable()) {
            g_lastError = L"API nicht konfiguriert (SCREENSCANNER_API_KEY fehlt)";
            return sanitizedText;
        }

        const char* systemPrompt = nullptr;
        if (mode == SearchMode::Search) {
            systemPrompt =
                "Du bist ein Suchassistent. Forme den folgenden Text in eine "
                "pr\\u00e4zise, allgemeine Google-Suchanfrage um. Entferne alle "
                "Platzhalter wie [EMAIL], [IP], [TOKEN] etc. Gib NUR die "
                "Suchanfrage zur\\u00fcck, ohne Anf\\u00fchrungszeichen, ohne Erkl\\u00e4rung.";
        } else {
            systemPrompt =
                "Du bist ein Suchassistent. Forme den folgenden Text in eine "
                "Google-Suchanfrage um, die auf Erkl\\u00e4rung, Definition oder "
                "Ursache fokussiert ist. Entferne alle Platzhalter wie [EMAIL], "
                "[IP], [TOKEN] etc. Gib NUR die Suchanfrage zur\\u00fcck, ohne "
                "Anf\\u00fchrungszeichen, ohne Erkl\\u00e4rung.";
        }

        std::string userContent = EscapeJsonString(WideToUtf8(sanitizedText));
        std::string modelUtf8   = EscapeJsonString(WideToUtf8(g_apiModel));

        // JSON-Body (OpenAI Chat-Completions-Format) mit max_tokens=512
        std::ostringstream json;
        json << "{"
             << "\"model\":\"" << modelUtf8 << "\","
             << "\"messages\":["
             <<   "{\"role\":\"system\",\"content\":\"" << systemPrompt << "\"},"
             <<   "{\"role\":\"user\",\"content\":\"" << userContent << "\"}"
             << "],"
             << "\"max_tokens\":512,"
             << "\"temperature\":0.3"
             << "}";

        UrlComponents urlParts = ParseUrl(g_apiEndpoint);

        HttpResponse httpResp = HttpPost(urlParts, json.str(), g_apiKey);

        if (!httpResp.networkError.empty()) {
            g_lastError = L"[Netzwerkfehler]: " + httpResp.networkError;
            return g_lastError;
        }

        if (httpResp.statusCode != 200) {
            std::string errMsg = ExtractJsonField(httpResp.body, "message");
            if (errMsg.empty()) {
                errMsg = ExtractJsonField(httpResp.body, "error");
            }
            if (errMsg.empty() && !httpResp.body.empty()) {
                errMsg = httpResp.body.substr(0, 180);
            }
            std::wstring wideErr = Utf8ToWide(errMsg);
            if (wideErr.empty()) {
                wideErr = L"HTTP " + std::to_wstring(httpResp.statusCode);
            }
            g_lastError = L"[API-Fehler " + std::to_wstring(httpResp.statusCode) + L"]: " + wideErr;
            return g_lastError;
        }

        std::string content = ExtractJsonField(httpResp.body, "content");
        if (content.empty()) {
            g_lastError = L"[Parse-Fehler]: Keine Antwort in choices[0].message.content erhalten";
            return sanitizedText;
        }

        g_lastError.clear();
        std::wstring result = Utf8ToWide(content);

        // Führende/abschließende Leerzeichen und Anführungszeichen trimmen
        while (!result.empty() &&
               (result.front() == L' ' || result.front() == L'"' ||
                result.front() == L'\n' || result.front() == L'\r')) {
            result.erase(result.begin());
        }
        while (!result.empty() &&
               (result.back() == L' ' || result.back() == L'"' ||
                result.back() == L'\n' || result.back() == L'\r')) {
            result.pop_back();
        }

        // Post-Check: falls verdächtige Inhalte in der API-Antwort sind
        if (Sanitizer::ContainsSuspiciousContent(result)) {
            return sanitizedText;
        }

        return result;
    }

} // namespace QueryBuilder
