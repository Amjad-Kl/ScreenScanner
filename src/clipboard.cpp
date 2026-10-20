#include "clipboard.h"
#include <windows.h>

namespace Clipboard {
    bool SetText(const std::wstring& text) {
        if (text.empty()) return false;

        if (!OpenClipboard(nullptr)) {
            return false;
        }

        EmptyClipboard();

        size_t byteSize = (text.length() + 1) * sizeof(wchar_t);
        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, byteSize);
        if (!hGlob) {
            CloseClipboard();
            return false;
        }

        memcpy(GlobalLock(hGlob), text.c_str(), byteSize);
        GlobalUnlock(hGlob);

        SetClipboardData(CF_UNICODETEXT, hGlob);
        CloseClipboard();
        return true;
    }
}
