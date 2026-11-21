#include "capture.h"

namespace Capture {
    CapturedImage CaptureRegion(const RECT& rect) {
        CapturedImage result;
        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        if (width <= 0 || height <= 0) {
            return result;
        }

        // =======================================================================
        // DATENSCHUTZ-HINWEIS:
        // Die Pixeldaten existieren NUR im RAM (std::vector<uint8_t>).
        // Es wird NICHTS auf die Festplatte, in eine Datei oder ins Netzwerk geschrieben.
        // Nach der OCR-Verarbeitung wird der Vektor sofort geleert und freigegeben.
        // =======================================================================

        HDC hScreenDC = GetDC(nullptr);
        HDC hMemDC = CreateCompatibleDC(hScreenDC);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height; // Negativ = Top-Down Bitmap (Zeile 0 oben)
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;     // 32 Bit = BGRA
        bmi.bmiHeader.biCompression = BI_RGB;

        void* pBits = nullptr;
        HBITMAP hBitmap = CreateDIBSection(hMemDC, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);

        if (hBitmap && pBits) {
            HGDIOBJ hOldBitmap = SelectObject(hMemDC, hBitmap);

            // Bildschirmbereich in den Speicher-DC kopieren
            // NUR SRCCOPY verwenden (CAPTUREBLT kann auf manchen Systemen Probleme verursachen)
            BitBlt(hMemDC, 0, 0, width, height, hScreenDC, rect.left, rect.top, SRCCOPY);

            result.width = width;
            result.height = height;
            size_t dataSize = static_cast<size_t>(width) * height * 4;
            result.bgraPixels.resize(dataSize);
            memcpy(result.bgraPixels.data(), pBits, dataSize);

            SelectObject(hMemDC, hOldBitmap);
            DeleteObject(hBitmap);
        }

        DeleteDC(hMemDC);
        ReleaseDC(nullptr, hScreenDC);

        return result;
    }
}
