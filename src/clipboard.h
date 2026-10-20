#pragma once
#include <string>

namespace Clipboard {
    // Kopiert einen UTF-16 (Unicode) String in die Windows Zwischenablage
    bool SetText(const std::wstring& text);
}
