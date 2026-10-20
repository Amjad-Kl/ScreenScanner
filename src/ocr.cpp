#include "ocr.h"
#include "languages.h"

#include <windows.h>
#include <unknwn.h>
#include <restrictederrorinfo.h>
#include <hstring.h>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>

#include <algorithm>
#include <cwctype>

#pragma comment(lib, "windowsapp.lib")

namespace {
    bool g_apartmentInitialized = false;

    struct LoadedEngine {
        Config::LanguageEntry info;
        winrt::Windows::Media::Ocr::OcrEngine engine{ nullptr };
    };

    std::vector<LoadedEngine> g_loadedEngines;
    std::vector<Config::LanguageEntry> g_missingLanguages;

    std::wstring ToLower(std::wstring str) {
        for (wchar_t& ch : str) {
            ch = static_cast<wchar_t>(towlower(ch));
        }
        return str;
    }

    bool TagMatches(const std::wstring& tag, const std::wstring& targetTag, const std::wstring& targetCode) {
        std::wstring ltag = ToLower(tag);
        std::wstring ltarget = ToLower(targetTag);
        std::wstring lcode = ToLower(targetCode);

        if (ltag == ltarget) return true;
        if (ltag.rfind(ltarget, 0) == 0) return true;
        if (ltag.rfind(lcode + L"-", 0) == 0) return true;
        if (ltag == lcode) return true;
        return false;
    }

    // Zählt arabische Unicode-Zeichen
    int CountArabicCharacters(const std::wstring& text) {
        int count = 0;
        for (wchar_t ch : text) {
            if ((ch >= 0x0600 && ch <= 0x06FF) ||
                (ch >= 0x0750 && ch <= 0x077F) ||
                (ch >= 0x08A0 && ch <= 0x08FF) ||
                (ch >= 0xFB50 && ch <= 0xFDFF) ||
                (ch >= 0xFE70 && ch <= 0xFEFF)) {
                count++;
            }
        }
        return count;
    }

    // Zählt kyrillische Buchstaben (Russisch, Ukrainisch, Bulgarisch etc.)
    int CountCyrillicCharacters(const std::wstring& text) {
        int count = 0;
        for (wchar_t ch : text) {
            if ((ch >= 0x0400 && ch <= 0x04FF) ||
                (ch >= 0x0500 && ch <= 0x052F)) {
                count++;
            }
        }
        return count;
    }

    // Zählt CJK-Zeichen (Chinesisch, Japanisch Kanji, Koreanisch)
    int CountCJKCharacters(const std::wstring& text) {
        int count = 0;
        for (wchar_t ch : text) {
            if ((ch >= 0x4E00 && ch <= 0x9FFF) ||
                (ch >= 0x3400 && ch <= 0x4DBF) ||
                (ch >= 0x3040 && ch <= 0x309F) || // Hiragana
                (ch >= 0x30A0 && ch <= 0x30FF) || // Katakana
                (ch >= 0xAC00 && ch <= 0xD7AF)) { // Hangul
                count++;
            }
        }
        return count;
    }

    // Zählt griechische Buchstaben
    int CountGreekCharacters(const std::wstring& text) {
        int count = 0;
        for (wchar_t ch : text) {
            if ((ch >= 0x0370 && ch <= 0x03FF) ||
                (ch >= 0x1F00 && ch <= 0x1FFF)) {
                count++;
            }
        }
        return count;
    }

    // Zählt hebräische Buchstaben
    int CountHebrewCharacters(const std::wstring& text) {
        int count = 0;
        for (wchar_t ch : text) {
            if (ch >= 0x0590 && ch <= 0x05FF) {
                count++;
            }
        }
        return count;
    }

    // Zählt lateinische Buchstaben (Deutsch, Englisch, Spanisch, Französisch etc.)
    int CountLatinLetters(const std::wstring& text) {
        int count = 0;
        for (wchar_t ch : text) {
            if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
                (ch >= 0x00C0 && ch <= 0x024F)) {
                count++;
            }
        }
        return count;
    }

    // Berechnet einen Konfidenz-Score für ein ergebnisbasiertes Matching
    double ScoreEngineResult(const std::wstring& text, const std::wstring& langCode) {
        if (text.empty()) return 0.0;

        int arabic = CountArabicCharacters(text);
        int cyrillic = CountCyrillicCharacters(text);
        int cjk = CountCJKCharacters(text);
        int greek = CountGreekCharacters(text);
        int hebrew = CountHebrewCharacters(text);
        int latin = CountLatinLetters(text);

        int digits = 0;
        int noise = 0;
        for (wchar_t ch : text) {
            if (ch >= L'0' && ch <= L'9') {
                digits++;
            } else if (ch == L'~' || ch == L'^' || ch == L'|' || ch == L'\\' || ch == L'_' || ch == 0x00A7) {
                noise++;
            }
        }

        double score = 0.0;

        if (langCode == L"ar") {
            score = (arabic * 5.0) + (digits * 0.5) - (latin * 0.5) - (noise * 2.0);
        }
        else if (langCode == L"ru" || langCode == L"uk" || langCode == L"bg") {
            score = (cyrillic * 5.0) + (digits * 0.5) - (noise * 2.0);
        }
        else if (langCode == L"zh" || langCode == L"ja" || langCode == L"ko") {
            score = (cjk * 5.0) + (digits * 0.5) - (noise * 2.0);
        }
        else if (langCode == L"el") {
            score = (greek * 5.0) + (digits * 0.5) - (noise * 2.0);
        }
        else if (langCode == L"he") {
            score = (hebrew * 5.0) + (digits * 0.5) - (noise * 2.0);
        }
        else {
            // Lateinische Sprachen (de, en, es, fr, it, pt, tr, etc.)
            score = (latin * 2.0) + (digits * 0.5) - (noise * 2.0);

            // Sprachspezifische Bonuspunkte für Sonderzeichen
            if (langCode == L"de") {
                for (wchar_t ch : text) {
                    if (ch == 0x00E4 || ch == 0x00F6 || ch == 0x00FC ||
                        ch == 0x00C4 || ch == 0x00D6 || ch == 0x00DC || ch == 0x00DF) {
                        score += 4.0;
                    }
                }
            } else if (langCode == L"es") {
                for (wchar_t ch : text) {
                    if (ch == 0x00F1 || ch == 0x00D1 || ch == 0x00BF || ch == 0x00A1 ||
                        ch == 0x00E1 || ch == 0x00E9 || ch == 0x00ED || ch == 0x00F3 || ch == 0x00FA) {
                        score += 4.0;
                    }
                }
            } else if (langCode == L"fr") {
                for (wchar_t ch : text) {
                    if (ch == 0x00E9 || ch == 0x00E8 || ch == 0x00EA || ch == 0x00EB ||
                        ch == 0x00E0 || ch == 0x00E2 || ch == 0x00EE || ch == 0x00EF ||
                        ch == 0x00F4 || ch == 0x00F9 || ch == 0x00FB || ch == 0x00E7 || ch == 0x0153) {
                        score += 4.0;
                    }
                }
            } else if (langCode == L"tr") {
                for (wchar_t ch : text) {
                    if (ch == 0x00E7 || ch == 0x011F || ch == 0x0131 || ch == 0x00F6 ||
                        ch == 0x015F || ch == 0x00FC || ch == 0x00C7 || ch == 0x011E ||
                        ch == 0x0130 || ch == 0x00D6 || ch == 0x015E || ch == 0x00DC) {
                        score += 4.0;
                    }
                }
            }
        }

        return (score > 0.0) ? score : 0.0;
    }

    std::wstring ExtractTextFromEngine(
        winrt::Windows::Media::Ocr::OcrEngine const& engine,
        winrt::Windows::Graphics::Imaging::SoftwareBitmap const& bitmap
    ) {
        if (!engine) return L"";
        try {
            auto ocrResult = engine.RecognizeAsync(bitmap).get();
            if (!ocrResult) return L"";
            std::wstring recognizedText;
            for (auto const& line : ocrResult.Lines()) {
                if (!recognizedText.empty()) {
                    recognizedText += L"\r\n";
                }
                recognizedText += line.Text().c_str();
            }
            return recognizedText;
        }
        catch (...) {
            return L"";
        }
    }
}

namespace OCR {
    bool Initialize() {
        if (!g_apartmentInitialized) {
            try {
                winrt::init_apartment(winrt::apartment_type::single_threaded);
                g_apartmentInitialized = true;
            }
            catch (winrt::hresult_error const&) {
                g_apartmentInitialized = true;
            }
        }

        if (!g_loadedEngines.empty()) return true;

        try {
            g_loadedEngines.clear();
            g_missingLanguages.clear();

            auto availableLanguages = winrt::Windows::Media::Ocr::OcrEngine::AvailableRecognizerLanguages();

            for (const auto& target : Config::CONFIGURED_LANGUAGES) {
                winrt::Windows::Media::Ocr::OcrEngine createdEngine{ nullptr };

                // 1. In der Liste der auf dem System verfügbaren Sprachen suchen
                for (uint32_t i = 0; i < availableLanguages.Size(); i++) {
                    auto lang = availableLanguages.GetAt(i);
                    std::wstring tag = lang.LanguageTag().c_str();
                    if (TagMatches(tag, target.tag, target.code)) {
                        try {
                            createdEngine = winrt::Windows::Media::Ocr::OcrEngine::TryCreateFromLanguage(lang);
                            if (createdEngine) break;
                        } catch (...) {}
                    }
                }

                // 2. Fallback: Direkt über den BCP-47 Tag instanziieren
                if (!createdEngine) {
                    try {
                        winrt::Windows::Globalization::Language directLang(target.tag);
                        if (winrt::Windows::Media::Ocr::OcrEngine::IsLanguageSupported(directLang)) {
                            createdEngine = winrt::Windows::Media::Ocr::OcrEngine::TryCreateFromLanguage(directLang);
                        }
                    } catch (...) {}
                }

                if (createdEngine) {
                    g_loadedEngines.push_back({ target, createdEngine });
                } else {
                    g_missingLanguages.push_back(target);
                }
            }

            // Fallback: Falls keine einzige konfigurierte Sprache geladen werden konnte
            if (g_loadedEngines.empty()) {
                try {
                    auto userEngine = winrt::Windows::Media::Ocr::OcrEngine::TryCreateFromUserProfileLanguages();
                    if (userEngine) {
                        g_loadedEngines.push_back({ { L"default", L"System-Standard", L"", L"" }, userEngine });
                    }
                } catch (...) {}
            }

            return !g_loadedEngines.empty();
        }
        catch (...) {
            return false;
        }
    }

    std::vector<std::wstring> GetLoadedLanguages() {
        if (g_loadedEngines.empty()) {
            Initialize();
        }
        std::vector<std::wstring> names;
        for (const auto& item : g_loadedEngines) {
            names.push_back(item.info.name);
        }
        return names;
    }

    std::vector<std::wstring> GetMissingLanguages() {
        if (g_loadedEngines.empty() && g_missingLanguages.empty()) {
            Initialize();
        }
        std::vector<std::wstring> names;
        for (const auto& item : g_missingLanguages) {
            names.push_back(item.name);
        }
        return names;
    }

    std::wstring GetActiveLanguagesSummary() {
        auto loaded = GetLoadedLanguages();
        if (loaded.empty()) {
            return L"Keine Sprache geladen";
        }
        if (loaded.size() == 1) {
            return loaded[0];
        }
        if (loaded.size() == 2) {
            return loaded[0] + L" + " + loaded[1];
        }

        std::wstring summary;
        for (size_t i = 0; i < loaded.size(); ++i) {
            if (i > 0) summary += L", ";
            summary += loaded[i];
        }
        return summary;
    }

    bool IsArabicSupported() {
        if (g_loadedEngines.empty()) {
            Initialize();
        }
        for (const auto& item : g_loadedEngines) {
            if (item.info.code == L"ar") return true;
        }
        return false;
    }

    std::wstring RecognizeText(const CapturedImage& image) {
        if (!image.isValid()) return L"";

        try {
            if (g_loadedEngines.empty()) {
                if (!Initialize()) {
                    return L"";
                }
            }

            // Pixeldaten in WinRT IBuffer übertragen
            winrt::Windows::Storage::Streams::DataWriter writer;
            writer.WriteBytes(
                winrt::array_view<const uint8_t>(
                    image.bgraPixels.data(),
                    image.bgraPixels.data() + image.bgraPixels.size()
                )
            );
            winrt::Windows::Storage::Streams::IBuffer pixelBuffer = writer.DetachBuffer();

            // SoftwareBitmap BGRA8 erstellen
            auto bitmap = winrt::Windows::Graphics::Imaging::SoftwareBitmap::CreateCopyFromBuffer(
                pixelBuffer,
                winrt::Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8,
                image.width,
                image.height,
                winrt::Windows::Graphics::Imaging::BitmapAlphaMode::Premultiplied
            );

            if (g_loadedEngines.empty()) return L"";

            // Wenn genau eine Engine aktiv ist: Direkt ausführen
            if (g_loadedEngines.size() == 1) {
                return ExtractTextFromEngine(g_loadedEngines[0].engine, bitmap);
            }

            // Wenn mehrere Engines aktiv sind: Jede Engine ausführen und bewerten
            std::wstring bestText;
            double bestScore = -1.0;
            std::wstring longestFallback;

            for (const auto& item : g_loadedEngines) {
                std::wstring text = ExtractTextFromEngine(item.engine, bitmap);
                if (text.empty()) continue;

                if (text.length() > longestFallback.length()) {
                    longestFallback = text;
                }

                double score = ScoreEngineResult(text, item.info.code);
                if (score > bestScore) {
                    bestScore = score;
                    bestText = text;
                }
            }

            if (bestScore > 0.0 && !bestText.empty()) {
                return bestText;
            }

            return longestFallback;
        }
        catch (winrt::hresult_error const&) {
            return L"";
        }
        catch (...) {
            return L"";
        }
    }
}
