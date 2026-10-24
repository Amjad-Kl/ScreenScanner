# 🖥️ ScreenScanner

> **100% Offline, privates & blitzschnelles "Circle to Search" für Windows in nativem C++.**  
> Bildschirmbereich per Hotkey markieren – Text wird via nativer Windows-OCR sofort in die Zwischenablage kopiert.  
> **Keine Cloud, keine Drittanbieter, kein Python-Overhead.**

---

## ⚡ Highlights

- 🚀 **Extrem schlank:** Die fertige `.exe` ist nur **~150 KB** groß und verbraucht im Hintergrund ca. **5–8 MB RAM** bei 0% CPU.
- 🌐 **Mehrsprachig (Multi-Language Auto-Detect):** Erkennt automatisch Texte in Deutsch, Arabisch sowie beliebigen weiteren Sprachen (z. B. Englisch, Spanisch, Französisch, Russisch, Chinesisch, Japanisch etc.) ohne manuelles Umschalten.
- 🔒 **100% Datenschutz & Privatsphäre:** Verwendet ausschließlich die native Windows-eigene OCR-Engine (`winrt::Windows::Media::Ocr`). Kein Byte verlässt deinen Rechner.
- ⌨️ **Globaler Hotkey:** Standardmäßig `Strg + Shift + S` (oder frei konfigurierbar z. B. auf `F9` in `src/config.h`).
- 🎯 **Flüssiges Overlay:** Abgedunkelter Bildschirm mit Fadenkreuz-Cursor und rechteckiger Bereichsauswahl (unterstützt auch Multi-Monitor-Setups).
- 📋 **Auto-Clipboard:** Sobald die Maustaste losgelassen wird, landet der erkannte Text sofort in deiner Zwischenablage (`Strg + V`).
- 🔔 **Tray-Notification:** Zeigt eine kurze Benachrichtigung unten rechts mit Textvorschau und Zeichenanzahl.

---

## 🏗️ Projektstruktur

```
ScreenScanner/
├── bin/
│   └── ScreenScanner.exe    # Fertige native Standalone-App (~150 KB)
├── src/
│   ├── main.cpp             # WinMain, System Tray & Hotkey Message Loop
│   ├── overlay.h / .cpp     # Transparentes Auswahlfenster mit Fadenkreuz
│   ├── capture.h / .cpp     # GDI Screen-Capture des markierten Bereichs
│   ├── ocr.h / .cpp         # C++/WinRT Native Windows OCR (Multi-Language)
│   ├── clipboard.h / .cpp   # Win32 Clipboard (Unicode Text)
│   ├── languages.h          # Konfigurierte OCR-Sprachen (automatisch verwaltet)
│   └── config.h             # Hotkey- & Farbeinstellungen
├── add_languages.bat        # Interaktiver 1-Klick-Sprachmanager (Launcher)
├── add_languages.ps1        # PowerShell-Skript: Sprachen wählen, installieren & kompilieren
├── build.bat                # 1-Klick-Build Skript (MSVC)
└── README.md                # Dokumentation
```

---

## 🚀 Schnellstart

### 1. Ausführen
Starte einfach die Datei:
```powershell
.\bin\ScreenScanner.exe
```
* Die App minimiert sich automatisch in deinen **System Tray** (unten rechts neben der Uhr).
* Du siehst ein Tray-Icon mit dem Hinweis *"ScreenScanner (Strg+Shift+S)"*.

### 2. Benutzen
1. Drücke zu einem beliebigen Zeitpunkt **`Strg + Shift + S`** (in jeder App, Browser, PDF, Video etc.).
2. Der Bildschirm wird leicht abgedunkelt. Ziehe mit der linken Maustaste einen Kasten um den gewünschten Text.
3. Lass die Maustaste los → Der Text wird sofort erkannt und kopiert!
4. Drücke an beliebiger Stelle **`Strg + V`**, um den extrahierten Text einzufügen.
5. Mit **`ESC`** kannst du die Auswahl jederzeit abbrechen.

---

## 🛠️ Selbst kompilieren

### Voraussetzungen
* Windows 10 oder Windows 11
* Visual Studio (C++ Desktop Development) oder Visual Studio Build Tools (MSVC)

### Build mit einem Klick
Führe einfach `build.bat` aus:
```cmd
build.bat
```
Das Skript findet automatisch deinen MSVC-Compiler (`cl.exe`), initialisiert die C++/WinRT-Umgebung und erzeugt die optimierte `bin\ScreenScanner.exe`.

---

## 🌐 Weitere Sprachen hinzufügen & verwalten

Du kannst jederzeit weitere Sprachen (z. B. Englisch, Spanisch, Französisch, Russisch, Chinesisch, Türkisch, Japanisch etc.) hinzufügen:

### Sprach-Manager im Terminal ausführen:
```cmd
add_languages.bat
```
*(oder im PowerShell: `.\add_languages.ps1`)*

**Was das Skript automatisch für dich tut:**
1. 📋 **Interaktive Auswahl:** Zeigt alle von Windows unterstützten Sprachen und deren aktuellen Status an.
2. 📥 **Automatische Installation:** Installiert fehlende Windows-OCR-Sprachpakete direkt über das Windows-Feature-System (FOD / DISM).
3. ✏️ **Code-Aktualisierung:** Passt `src/languages.h` automatisch an deine Auswahl an.
4. ⚙️ **Auto-Build:** Kompiliert das C++-Projekt sofort mit MSVC in eine neue `bin\ScreenScanner.exe`.
5. 🚀 **Direktstart:** Fragt, ob du die neue Version direkt im Hintergrund starten möchtest.

---

## ⚙️ Konfiguration

In [`src/config.h`](file:///c:/Users/Amjad/Desktop/ScreenScanner/src/config.h) kannst du den Hotkey oder die Farben ganz nach deinem Geschmack anpassen:

```cpp
namespace Config {
    // Beispiel: Einzelne Taste wie F9 statt Tastenkombination
    // constexpr UINT HOTKEY_MODIFIERS = 0;
    // constexpr UINT HOTKEY_KEY = VK_F9;

    // Standard: Strg + Shift + S
    constexpr UINT HOTKEY_MODIFIERS = MOD_CONTROL | MOD_SHIFT;
    constexpr UINT HOTKEY_KEY = 'S';

    // Rahmenfarbe beim Auswählen (Windows Akzentblau)
    constexpr COLORREF OVERLAY_BORDER_COLOR = RGB(0, 120, 215);
}
```
Nach einer Änderung einfach `build.bat` erneut ausführen.

---

## 🛣️ Roadmap / Phase 2: Lokale KI-Anbindung

Da ScreenScanner komplett modular aufgebaut ist, kann in Phase 2 eine **100% lokale KI** angebunden werden:
- [ ] Integration mit einem lokalen [Ollama](https://ollama.ai) oder `llama.cpp` Server.
- [ ] Rechtsklick-Menü nach dem Scannen: *"Zusammenfassen"*, *"Ins Deutsche übersetzen"* oder *"Frage an Modell stellen"*.
- [ ] Weiterhin 0% Cloud – volle Privatsphäre auf deiner lokalen Hardware!

---

## 📄 Lizenz
MIT License. Frei verwendbar und anpassbar.
