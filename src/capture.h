#pragma once
#include <windows.h>
#include <vector>
#include <cstdint>

struct CapturedImage {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> bgraPixels; 
    bool isValid() const { return width > 0 && height > 0 && !bgraPixels.empty(); }
};

namespace Capture {
    // Nimmt den gewählten Bildschirmausschnitt auf (virtuelle Screen-Koordinaten + Multi-Monitor)
    CapturedImage CaptureRegion(const RECT& rect);
}
