#pragma once
#include <windows.h>
#include <functional>

namespace Overlay {
    using SelectionCallback = std::function<void(const RECT& selectedRect)>;

    // Startet die interaktive Bereichsauswahl auf dem Bildschirm
    void StartSelection(HINSTANCE hInstance, SelectionCallback onSelected);
}
