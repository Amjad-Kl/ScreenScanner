#include <windows.h>
#include <shellapi.h>
#include <string>

#include "config.h"
#include "capture.h"
#include "ocr.h"
#include "clipboard.h"
#include "overlay.h"
#include "sanitizer.h"
#include "query_builder.h"
#include "browser.h"

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_SCAN 2001
#define ID_TRAY_EXIT 2002

namespace {
    HWND g_hWnd = nullptr;
    NOTIFYICONDATAW g_nid = {};
    HINSTANCE g_hInstance = nullptr;

    void ShowNotification(const std::wstring& title, const std::wstring& message) {
        g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;
        wcsncpy_s(g_nid.szInfoTitle, title.c_str(), _TRUNCATE);
        wcsncpy_s(g_nid.szInfo, message.c_str(), _TRUNCATE);
        g_nid.dwInfoFlags = NIIF_INFO;
        Shell_NotifyIconW(NIM_MODIFY, &g_nid);
    }

    // OCR & Safe Search Copilot Workflow:
    // Läuft synchron auf dem Hauptthread (WinRT STA-Apartment).
    void ProcessScan(const RECT& rect) {
        // 1. Bildschirmbereich aufnehmen (nur im RAM, NICHTS wird auf die Festplatte geschrieben!)
        CapturedImage img = Capture::CaptureRegion(rect);
        if (!img.isValid()) {
            ShowNotification(L"ScreenScanner", L"Fehler: Bildschirmbereich konnte nicht aufgenommen werden.");
            return;
        }

        // 2. OCR ausführen (Windows native WinRT Engine, 100% lokal)
        std::wstring recognized = OCR::RecognizeText(img);

        // Bild-Daten sofort aus dem RAM löschen (Sicherheit & Datenschutz)
        img.bgraPixels.clear();
        img.bgraPixels.shrink_to_fit();
        img.width = 0;
        img.height = 0;

        // Leerzeichen am Anfang/Ende trimmen
        while (!recognized.empty() && (recognized.back() == L'\r' || recognized.back() == L'\n' || recognized.back() == L' ')) {
            recognized.pop_back();
        }

        if (recognized.empty()) {
            auto missing = OCR::GetMissingLanguages();
            if (!missing.empty()) {
                std::wstring msg = L"Kein Text erkannt.\nHinweis: Folgende Windows-OCR-Pakete fehlen noch:\n";
                for (size_t i = 0; i < missing.size(); ++i) {
                    if (i > 0) msg += L", ";
                    msg += missing[i];
                }
                ShowNotification(L"ScreenScanner", msg);
            } else {
                ShowNotification(L"ScreenScanner", L"Kein Text im ausgewählten Bereich erkannt.\nVersuche einen größeren Bereich zu markieren.");
            }
            return;
        }

        // 3. Text sofort in die Zwischenablage kopieren (Standard-Verhalten bleibt immer erhalten)
        bool clipboardOk = Clipboard::SetText(recognized);

        // 4. Lokale Bereinigung im RAM (100% lokal, Maskierung sensibler Daten)
        std::wstring sanitized = Sanitizer::Sanitize(recognized);
        std::wstring preview = Sanitizer::GetPreview(sanitized);

        // 5. Schnellauswahl-Pop-up an aktueller Cursorposition anzeigen
        POINT pt;
        GetCursorPos(&pt);

        HMENU hMenu = CreatePopupMenu();

        // Vorschau-Header (gekürzter maskierter Text)
        std::wstring previewHeader = L"Vorschau: " + (preview.length() > 45 ? preview.substr(0, 42) + L"..." : preview);
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, previewHeader.c_str());
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

        bool apiReady = QueryBuilder::IsAvailable();
        UINT searchFlags = MF_STRING | (apiReady ? 0 : MF_GRAYED);

        AppendMenuW(hMenu, searchFlags, Config::ID_MODE_SEARCH, L"🔍 Im Web suchen (Google)");
        AppendMenuW(hMenu, searchFlags, Config::ID_MODE_EXPLAIN, L"💡 Erklären / Definieren (Google)");
        AppendMenuW(hMenu, MF_STRING, Config::ID_MODE_COPY, L"📋 Nur kopieren (in Zwischenablage)");

        if (!apiReady) {
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"Hinweis: Setze SCREENSCANNER_API_KEY für Suche");
        }

        SetForegroundWindow(g_hWnd);
        int choice = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_TOPALIGN | TPM_LEFTALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, g_hWnd, nullptr);
        PostMessageW(g_hWnd, WM_NULL, 0, 0);
        DestroyMenu(hMenu);

        // 6. Ausgewählte Aktion ausführen
        if (choice == Config::ID_MODE_SEARCH) {
            ShowNotification(L"ScreenScanner Copilot", L"Generiere präzise Suchanfrage...");
            std::wstring query = QueryBuilder::GenerateQuery(sanitized, QueryBuilder::SearchMode::Search);
            if (query.rfind(L"[API-Fehler", 0) == 0 || query.rfind(L"[Netzwerkfehler", 0) == 0 || query.rfind(L"[Parse-Fehler", 0) == 0) {
                ShowNotification(L"Copilot API-Fehler", query);
            } else {
                if (Browser::OpenGoogleSearch(query)) {
                    ShowNotification(L"Google-Suche geöffnet", L"Suchanfrage: " + query);
                } else {
                    ShowNotification(L"ScreenScanner", L"Fehler: Browser konnte nicht geöffnet werden.");
                }
            }
        }
        else if (choice == Config::ID_MODE_EXPLAIN) {
            ShowNotification(L"ScreenScanner Copilot", L"Generiere Erklärungssuche...");
            std::wstring query = QueryBuilder::GenerateQuery(sanitized, QueryBuilder::SearchMode::Explain);
            if (query.rfind(L"[API-Fehler", 0) == 0 || query.rfind(L"[Netzwerkfehler", 0) == 0 || query.rfind(L"[Parse-Fehler", 0) == 0) {
                ShowNotification(L"Copilot API-Fehler", query);
            } else {
                if (Browser::OpenGoogleSearch(query)) {
                    ShowNotification(L"Erklärungssuche geöffnet", L"Suchanfrage: " + query);
                } else {
                    ShowNotification(L"ScreenScanner", L"Fehler: Browser konnte nicht geöffnet werden.");
                }
            }
        }
        else {
            // ID_MODE_COPY oder Menü durch Klick außerhalb / ESC geschlossen
            if (clipboardOk) {
                std::wstring clipPreview = recognized.length() > 60 ? recognized.substr(0, 57) + L"..." : recognized;
                std::wstring msg = L"Kopiert (" + std::to_wstring(recognized.length()) + L" Zeichen):\n" + clipPreview;
                ShowNotification(L"Text kopiert!", msg);
            } else {
                ShowNotification(L"ScreenScanner", L"Fehler: Text konnte nicht in die Zwischenablage kopiert werden.");
            }
        }
    }

    void TriggerScan() {
        Overlay::StartSelection(g_hInstance, [](const RECT& selectedRect) {
            ProcessScan(selectedRect);
        });
    }

    void InitTray(HWND hWnd, HINSTANCE hInstance) {
        ZeroMemory(&g_nid, sizeof(g_nid));
        g_nid.cbSize = sizeof(NOTIFYICONDATAW);
        g_nid.hWnd = hWnd;
        g_nid.uID = 1;
        g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        g_nid.uCallbackMessage = WM_TRAYICON;
        g_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
        
        std::wstring tip = L"ScreenScanner (" + OCR::GetActiveLanguagesSummary() + L") - Strg+Shift+S";
        wcsncpy_s(g_nid.szTip, tip.c_str(), _TRUNCATE);

        Shell_NotifyIconW(NIM_ADD, &g_nid);
    }

    void ShowTrayMenu(HWND hWnd) {
        POINT pt;
        GetCursorPos(&pt);
        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, ID_TRAY_SCAN, L"Bereich scannen (Strg+Shift+S)");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

        std::wstring modeStr = L"Modus: " + OCR::GetActiveLanguagesSummary() + L" (Auto) ✓";
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, modeStr.c_str());

        // Copilot Status im Tray anzeigen
        if (QueryBuilder::IsAvailable()) {
            AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"Copilot: Safe Search aktiv ✓");
        } else {
            AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"Copilot: API nicht konfiguriert");
        }

        auto missing = OCR::GetMissingLanguages();
        if (!missing.empty()) {
            std::wstring missingStr = L"Fehlt in Windows: ";
            for (size_t i = 0; i < missing.size(); ++i) {
                if (i > 0) missingStr += L", ";
                missingStr += missing[i];
            }
            AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, missingStr.c_str());
        }
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Beenden");

        SetForegroundWindow(hWnd);
        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
        PostMessageW(hWnd, WM_NULL, 0, 0);
        DestroyMenu(hMenu);
    }

    LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_HOTKEY:
            if (wParam == Config::HOTKEY_ID) {
                TriggerScan();
                return 0;
            }
            break;

        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) {
                ShowTrayMenu(hWnd);
            }
            else if (lParam == WM_LBUTTONDBLCLK) {
                TriggerScan();
            }
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
            case ID_TRAY_SCAN:
                TriggerScan();
                break;
            case ID_TRAY_EXIT:
                DestroyWindow(hWnd);
                break;
            }
            return 0;

        case WM_DESTROY:
            UnregisterHotKey(hWnd, Config::HOTKEY_ID);
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    g_hInstance = hInstance;

    // ====================================================================
    // DPI-Awareness aktivieren (WICHTIG für Laptops mit 125%/150% Skalierung!)
    // Ohne das sind alle Bildschirmkoordinaten verfälscht.
    // ====================================================================
    SetProcessDPIAware();

    // Mutex für Single-Instance (nur eine Instanz gleichzeitig)
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"ScreenScannerSingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"ScreenScanner läuft bereits im Hintergrund (System Tray).", L"ScreenScanner", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // Windows OCR initialisieren
    if (!OCR::Initialize()) {
        MessageBoxW(nullptr, 
            L"Windows OCR konnte nicht initialisiert werden.\n\n"
            L"Lösung: Gehe zu Windows-Einstellungen → Zeit und Sprache → Sprache\n"
            L"→ Klicke auf deine Sprache → Optionen → und installiere das \"OCR\"-Sprachpaket.", 
            L"ScreenScanner Fehler", MB_OK | MB_ICONERROR);
        return 1;
    }

    // LLM-QueryBuilder initialisieren (lädt API-Config aus Umgebungsvariablen)
    QueryBuilder::Initialize();

    // Unsichtbares Fenster für Message-Handling, Tray-Icon und Pop-ups registrieren
    const wchar_t CLASS_NAME[] = L"ScreenScannerHiddenWnd";
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wc);

    g_hWnd = CreateWindowExW(
        0, CLASS_NAME, L"ScreenScanner",
        WS_POPUP, 0, 0, 0, 0,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!g_hWnd) {
        return 1;
    }

    // Globalen Hotkey registrieren (Strg + Shift + S)
    if (!RegisterHotKey(g_hWnd, Config::HOTKEY_ID, Config::HOTKEY_MODIFIERS, Config::HOTKEY_KEY)) {
        MessageBoxW(nullptr, 
            L"Der Hotkey (Strg + Shift + S) konnte nicht registriert werden.\nEventuell wird er bereits von einem anderen Programm belegt.", 
            L"ScreenScanner Warnung", MB_OK | MB_ICONWARNING);
    }

    // Tray Icon initialisieren
    InitTray(g_hWnd, hInstance);

    // Initialer Begrüßungs-Hinweis mit Copilot-Status
    if (QueryBuilder::IsAvailable()) {
        ShowNotification(L"ScreenScanner + Copilot aktiv!",
            L"Drücke Strg+Shift+S um Text zu scannen, sicher zu bereinigen und im Web zu suchen.\n100% lokaler Datenschutz-Filter aktiv.");
    } else {
        ShowNotification(L"ScreenScanner aktiv!",
            L"Drücke Strg+Shift+S um jederzeit Text vom Bildschirm zu scannen.\nHinweis: Setze SCREENSCANNER_API_KEY für den Safe Search Copilot.");
    }

    // Message Loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (hMutex) {
        CloseHandle(hMutex);
    }

    return 0;
}
