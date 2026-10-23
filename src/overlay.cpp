#include "overlay.h"
#include "config.h"
#include <windows.h>
#include <windowsx.h>
#include <algorithm>
#include <string>

namespace {
    HWND g_hOverlayWnd = nullptr;
    Overlay::SelectionCallback g_callback = nullptr;
    bool g_isSelecting = false;
    POINT g_startPoint = { 0, 0 };
    POINT g_currentPoint = { 0, 0 };
    int g_vLeft = 0;
    int g_vTop = 0;
    int g_vWidth = 0;
    int g_vHeight = 0;

    RECT GetNormalizedSelection() {
        RECT r;
        r.left = (std::min)(g_startPoint.x, g_currentPoint.x);
        r.top = (std::min)(g_startPoint.y, g_currentPoint.y);
        r.right = (std::max)(g_startPoint.x, g_currentPoint.x);
        r.bottom = (std::max)(g_startPoint.y, g_currentPoint.y);
        return r;
    }

    LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                // Mit ESC abbrechen
                g_isSelecting = false;
                DestroyWindow(hWnd);
                return 0;
            }
            break;

        case WM_LBUTTONDOWN:
            g_isSelecting = true;
            GetCursorPos(&g_startPoint);
            g_currentPoint = g_startPoint;
            SetCapture(hWnd);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;

        case WM_MOUSEMOVE:
            if (g_isSelecting) {
                GetCursorPos(&g_currentPoint);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;

        case WM_LBUTTONUP:
            if (g_isSelecting) {
                g_isSelecting = false;
                ReleaseCapture();
                GetCursorPos(&g_currentPoint);

                RECT sel = GetNormalizedSelection();

                
                ShowWindow(hWnd, SW_HIDE);

                // Kurz warten bis Windows Desktop Compositor (DWM) das Overlay vollständig vom Bildschirm entfernt hat
                Sleep(150);

                // Fenster endgültig zerstören
                DestroyWindow(hWnd);

                // Wenn mindestens 10x10 Pixel markiert wurden
                if ((sel.right - sel.left > 10) && (sel.bottom - sel.top > 10)) {
                    if (g_callback) {
                        g_callback(sel);
                    }
                }
                return 0;
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            HDC hMemDC = CreateCompatibleDC(hdc);
            HBITMAP hMemBmp = CreateCompatibleBitmap(hdc, g_vWidth, g_vHeight);
            HGDIOBJ hOldBmp = SelectObject(hMemDC, hMemBmp);

            // 1. Hintergrund leicht abdunkeln (halbtransparentes Schwarz)
            HBRUSH hDimBrush = CreateSolidBrush(RGB(0, 0, 0));
            RECT fullRect = { 0, 0, g_vWidth, g_vHeight };
            FillRect(hMemDC, &fullRect, hDimBrush);
            DeleteObject(hDimBrush);

            // 2. Falls gezogen wird, den Auswahlbereich "ausschneiden" (transparent machen)
            if (g_isSelecting) {
                RECT sel = GetNormalizedSelection();
                // Bildschirmkoordinaten zu Fensterkoordinaten umrechnen
                sel.left -= g_vLeft;
                sel.top -= g_vTop;
                sel.right -= g_vLeft;
                sel.bottom -= g_vTop;

                // Magenta als Farb-Schlüssel → wird komplett durchsichtig (ColorKey)
                HBRUSH hKeyBrush = CreateSolidBrush(RGB(255, 0, 255));
                FillRect(hMemDC, &sel, hKeyBrush);
                DeleteObject(hKeyBrush);

                // Rahmen um den ausgewählten Bereich zeichnen
                HPEN hPen = CreatePen(PS_SOLID, Config::OVERLAY_BORDER_WIDTH, Config::OVERLAY_BORDER_COLOR);
                HGDIOBJ hOldPen = SelectObject(hMemDC, hPen);
                HGDIOBJ hOldBrush2 = SelectObject(hMemDC, GetStockObject(NULL_BRUSH));

                Rectangle(hMemDC, sel.left, sel.top, sel.right, sel.bottom);

                SelectObject(hMemDC, hOldPen);
                SelectObject(hMemDC, hOldBrush2);
                DeleteObject(hPen);

                // Größenanzeige im Rahmen
                std::wstring sizeText = std::to_wstring(sel.right - sel.left) + L" x " + std::to_wstring(sel.bottom - sel.top);
                SetBkMode(hMemDC, TRANSPARENT);
                SetTextColor(hMemDC, RGB(255, 255, 255));
                TextOutW(hMemDC, sel.left + 4, sel.top - 20, sizeText.c_str(), (int)sizeText.length());
            }
            else {
                // Hinweistext anzeigen bevor der Benutzer zieht
                SetBkMode(hMemDC, TRANSPARENT);
                SetTextColor(hMemDC, RGB(200, 200, 200));
                const wchar_t* hint = L"Ziehe mit der Maus einen Bereich  |  ESC = Abbrechen";
                TextOutW(hMemDC, g_vWidth / 2 - 200, g_vHeight / 2, hint, (int)wcslen(hint));
            }

            BitBlt(hdc, 0, 0, g_vWidth, g_vHeight, hMemDC, 0, 0, SRCCOPY);

            SelectObject(hMemDC, hOldBmp);
            DeleteObject(hMemBmp);
            DeleteDC(hMemDC);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            g_hOverlayWnd = nullptr;
            return 0;
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

namespace Overlay {
    void StartSelection(HINSTANCE hInstance, SelectionCallback onSelected) {
        if (g_hOverlayWnd) return; // Bereits geöffnet

        g_callback = onSelected;
        g_isSelecting = false;

        // Virtuelle Desktop-Größe ermitteln (unterstützt alle Monitore)
        g_vLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
        g_vTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
        g_vWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        g_vHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        static const wchar_t CLASS_NAME[] = L"ScreenScannerOverlayClass";
        static bool isRegistered = false;

        if (!isRegistered) {
            WNDCLASSEX wc = {};
            wc.cbSize = sizeof(WNDCLASSEX);
            wc.lpfnWndProc = OverlayWndProc;
            wc.hInstance = hInstance;
            wc.lpszClassName = CLASS_NAME;
            wc.hCursor = LoadCursor(nullptr, IDC_CROSS); // Cursor
            RegisterClassEx(&wc);
            isRegistered = true;
        }

        g_hOverlayWnd = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
            CLASS_NAME,
            L"ScreenScanner Overlay",
            WS_POPUP,
            g_vLeft, g_vTop, g_vWidth, g_vHeight,
            nullptr, nullptr, hInstance, nullptr
        );

        if (g_hOverlayWnd) {
            // RGB(255,0,255) = Magenta → wird komplett durchsichtig (für die Frei-Auswahl)
            SetLayeredWindowAttributes(g_hOverlayWnd, RGB(255, 0, 255), 100, LWA_COLORKEY | LWA_ALPHA);

            ShowWindow(g_hOverlayWnd, SW_SHOW);
            SetForegroundWindow(g_hOverlayWnd);
            SetFocus(g_hOverlayWnd);
            UpdateWindow(g_hOverlayWnd);
        }
    }
}
