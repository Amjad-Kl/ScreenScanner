# ==============================================================================
# ScreenScanner - Sprach-Manager & Auto-Compiler Skript
# ==============================================================================
# Dieses Skript ermoeglicht das interaktive Auswaehlen und Hinzufuegen weiterer
# OCR-Sprachen, installiert die benoetigten Windows-Sprachpakete, aktualisiert
# den C++ Quellcode und kompiliert die ScreenScanner.exe automatisch neu.
# ==============================================================================

[CmdletBinding()]
param(
    [Parameter(Mandatory=$false)]
    [string]$SelectedTags,

    [Parameter(Mandatory=$false)]
    [switch]$AutoCompile,

    [Parameter(Mandatory=$false)]
    [switch]$StartApp
)

$Host.UI.RawUI.WindowTitle = "ScreenScanner - Sprachverwaltung"
$ErrorActionPreference = "Stop"

# Masterliste aller von Windows-OCR unterstuetzten Sprachen
$AllLanguages = @(
    [PSCustomObject]@{ Id = 1;  Code = "de";    Tag = "de-DE";      Name = "Deutsch";                   Label = "[DE] Deutsch" },
    [PSCustomObject]@{ Id = 2;  Code = "ar";    Tag = "ar-SA";      Name = "Arabisch";                  Label = "[AR] Arabisch (Saudi-Arabien)" },
    [PSCustomObject]@{ Id = 3;  Code = "en";    Tag = "en-US";      Name = "Englisch (US)";             Label = "[EN] Englisch (USA)" },
    [PSCustomObject]@{ Id = 4;  Code = "en-gb"; Tag = "en-GB";      Name = "Englisch (UK)";             Label = "[GB] Englisch (Grossbritannien)" },
    [PSCustomObject]@{ Id = 5;  Code = "es";    Tag = "es-ES";      Name = "Spanisch";                  Label = "[ES] Spanisch" },
    [PSCustomObject]@{ Id = 6;  Code = "fr";    Tag = "fr-FR";      Name = "Franzoesisch";              Label = "[FR] Franzoesisch" },
    [PSCustomObject]@{ Id = 7;  Code = "it";    Tag = "it-IT";      Name = "Italienisch";               Label = "[IT] Italienisch" },
    [PSCustomObject]@{ Id = 8;  Code = "ru";    Tag = "ru-RU";      Name = "Russisch";                  Label = "[RU] Russisch" },
    [PSCustomObject]@{ Id = 9;  Code = "tr";    Tag = "tr-TR";      Name = "Tuerkisch";                 Label = "[TR] Tuerkisch" },
    [PSCustomObject]@{ Id = 10; Code = "pt";    Tag = "pt-PT";      Name = "Portugiesisch";             Label = "[PT] Portugiesisch" },
    [PSCustomObject]@{ Id = 11; Code = "pt-br"; Tag = "pt-BR";      Name = "Portugiesisch (Brasilien)"; Label = "[BR] Portugiesisch (Brasilien)" },
    [PSCustomObject]@{ Id = 12; Code = "pl";    Tag = "pl-PL";      Name = "Polnisch";                  Label = "[PL] Polnisch" },
    [PSCustomObject]@{ Id = 13; Code = "nl";    Tag = "nl-NL";      Name = "Niederlaendisch";           Label = "[NL] Niederlaendisch" },
    [PSCustomObject]@{ Id = 14; Code = "zh";    Tag = "zh-CN";      Name = "Chinesisch (Vereinfacht)";  Label = "[ZH] Chinesisch (Vereinfacht)" },
    [PSCustomObject]@{ Id = 15; Code = "zh-tw"; Tag = "zh-TW";      Name = "Chinesisch (Traditionell)"; Label = "[TW] Chinesisch (Traditionell)" },
    [PSCustomObject]@{ Id = 16; Code = "ja";    Tag = "ja-JP";      Name = "Japanisch";                 Label = "[JA] Japanisch" },
    [PSCustomObject]@{ Id = 17; Code = "ko";    Tag = "ko-KR";      Name = "Koreanisch";                Label = "[KO] Koreanisch" },
    [PSCustomObject]@{ Id = 18; Code = "uk";    Tag = "uk-UA";      Name = "Ukrainisch";                Label = "[UK] Ukrainisch" },
    [PSCustomObject]@{ Id = 19; Code = "el";    Tag = "el-GR";      Name = "Griechisch";                Label = "[EL] Griechisch" },
    [PSCustomObject]@{ Id = 20; Code = "sv";    Tag = "sv-SE";      Name = "Schwedisch";                Label = "[SV] Schwedisch" },
    [PSCustomObject]@{ Id = 21; Code = "cs";    Tag = "cs-CZ";      Name = "Tschechisch";               Label = "[CS] Tschechisch" },
    [PSCustomObject]@{ Id = 22; Code = "da";    Tag = "da-DK";      Name = "Daenisch";                  Label = "[DA] Daenisch" },
    [PSCustomObject]@{ Id = 23; Code = "fi";    Tag = "fi-FI";      Name = "Finnisch";                  Label = "[FI] Finnisch" },
    [PSCustomObject]@{ Id = 24; Code = "hu";    Tag = "hu-HU";      Name = "Ungarisch";                 Label = "[HU] Ungarisch" },
    [PSCustomObject]@{ Id = 25; Code = "nb";    Tag = "nb-NO";      Name = "Norwegisch";                Label = "[NO] Norwegisch" },
    [PSCustomObject]@{ Id = 26; Code = "ro";    Tag = "ro-RO";      Name = "Rumaenisch";                Label = "[RO] Rumaenisch" },
    [PSCustomObject]@{ Id = 27; Code = "sk";    Tag = "sk-SK";      Name = "Slowakisch";                Label = "[SK] Slowakisch" },
    [PSCustomObject]@{ Id = 28; Code = "bg";    Tag = "bg-BG";      Name = "Bulgarisch";                Label = "[BG] Bulgarisch" }
)

# Pruefen, ob als Administrator ausgefuehrt wird
function Test-IsAdmin {
    $currentIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]$currentIdentity
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# WinRT-Abfrage: Welche OCR-Sprachen sind aktuell auf dem System einsatzbereit?
function Get-InstalledOcrTags {
    try {
        [Windows.Media.Ocr.OcrEngine, Windows.Foundation, ContentType = WindowsRuntime] | Out-Null
        $tags = [Windows.Media.Ocr.OcrEngine]::AvailableRecognizerLanguages | Select-Object -ExpandProperty LanguageTag
        if ($null -eq $tags) { return @() }
        return @($tags)
    } catch {
        return @()
    }
}

# Aktuell im C++ Header (src/languages.h) konfigurierte Sprachen auslesen
function Get-ConfiguredTags {
    $headerPath = Join-Path $PSScriptRoot "src\languages.h"
    if (-not (Test-Path $headerPath)) {
        return @("de-DE", "ar-SA")
    }

    $content = Get-Content -Path $headerPath -Raw -Encoding UTF8
    $matches = [regex]::Matches($content, 'L"([^"]+)",\s*L"([^"]+)",\s*L"([^"]+)"')
    $tags = @()
    foreach ($m in $matches) {
        $tags += $m.Groups[3].Value
    }
    if ($tags.Count -eq 0) {
        return @("de-DE", "ar-SA")
    }
    return $tags
}

# C++ Header src/languages.h mit neuen Sprachen generieren
function Set-ConfiguredLanguages {
    param([array]$Languages)

    $headerPath = Join-Path $PSScriptRoot "src\languages.h"
    $sb = [System.Text.StringBuilder]::new()
    [void]$sb.AppendLine("// ==============================================================================")
    [void]$sb.AppendLine("// Automatisch generiert durch add_languages.ps1")
    [void]$sb.AppendLine("// ==============================================================================")
    [void]$sb.AppendLine("#pragma once")
    [void]$sb.AppendLine("#include <string>")
    [void]$sb.AppendLine("#include <vector>")
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("namespace Config {")
    [void]$sb.AppendLine("    struct LanguageEntry {")
    [void]$sb.AppendLine("        std::wstring code;        // z. B. L`"de`", L`"ar`", L`"en`", L`"es`"")
    [void]$sb.AppendLine("        std::wstring name;        // Anzeigename, z. B. L`"Deutsch`", L`"Arabisch`"")
    [void]$sb.AppendLine("        std::wstring tag;         // BCP-47 Tag, z. B. L`"de-DE`", L`"ar-SA`"")
    [void]$sb.AppendLine("        std::wstring capability;  // Windows FOD Name, z. B. L`"Language.OCR~~~de-DE~0.0.1.0`"")
    [void]$sb.AppendLine("    };")
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("    // Diese Liste wird automatisch vom Skript (add_languages.ps1) aktualisiert.")
    [void]$sb.AppendLine("    inline const std::vector<LanguageEntry> CONFIGURED_LANGUAGES = {")
    
    for ($i = 0; $i -lt $Languages.Count; $i++) {
        $lang = $Languages[$i]
        $comma = if ($i -lt ($Languages.Count - 1)) { "," } else { "" }
        $line = "        { L`"$($lang.Code)`", L`"$($lang.Name)`", L`"$($lang.Tag)`", L`"Language.OCR~~~$($lang.Tag)~0.0.1.0`" }$comma"
        [void]$sb.AppendLine($line)
    }

    [void]$sb.AppendLine("    };")
    [void]$sb.AppendLine("}")
    
    [System.IO.File]::WriteAllText($headerPath, $sb.ToString(), [System.Text.Encoding]::UTF8)
}

# Windows-OCR Sprachpaket via Windows Capability installieren
function Install-LanguagePackage {
    param($Lang)

    Write-Host "[*] Installiere Windows-OCR fuer: $($Lang.Name) ($($Lang.Tag))..." -ForegroundColor Cyan
    
    # 1. Basic Component (Voraussetzung fuer OCR)
    $basicCap = "Language.Basic~~~$($Lang.Tag)~0.0.1.0"
    try {
        Add-WindowsCapability -Online -Name $basicCap -ErrorAction SilentlyContinue | Out-Null
    } catch {}

    # 2. OCR Component
    $ocrCap = "Language.OCR~~~$($Lang.Tag)~0.0.1.0"
    try {
        Add-WindowsCapability -Online -Name $ocrCap -ErrorAction Stop | Out-Null
        Write-Host "    [OK] Paket erfolgreich installiert!" -ForegroundColor Green
        return $true
    }
    catch {
        Write-Host "    [FEHLER] Installation fehlgeschlagen: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host "    Tipp: Stelle sicher, dass Windows Update aktiv ist und das System online ist." -ForegroundColor Yellow
        return $false
    }
}

# Kompilieren mit build.bat
function Invoke-CompileBuild {
    Write-Host ""
    Write-Host "========================================================" -ForegroundColor Cyan
    Write-Host "  Starte Kompilierung von ScreenScanner.exe..." -ForegroundColor Cyan
    Write-Host "========================================================" -ForegroundColor Cyan
    
    # Eventuell laufende ScreenScanner.exe stoppen
    Get-Process -Name ScreenScanner -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 300

    $buildBat = Join-Path $PSScriptRoot "build.bat"
    $process = Start-Process -FilePath "cmd.exe" -ArgumentList "/c `"$buildBat`"" -NoNewWindow -Wait -PassThru

    if ($process.ExitCode -eq 0) {
        Write-Host ""
        Write-Host "========================================================" -ForegroundColor Green
        Write-Host "  [OK] ERFOLG! ScreenScanner.exe wurde erfolgreich gebaut." -ForegroundColor Green
        Write-Host "  Pfad: bin\ScreenScanner.exe" -ForegroundColor Green
        Write-Host "========================================================" -ForegroundColor Green
        return $true
    } else {
        Write-Host ""
        Write-Host "[!] FEHLER: Kompilierung fehlgeschlagen mit Exit-Code $($process.ExitCode)!" -ForegroundColor Red
        return $false
    }
}

# ==============================================================================
# HAUPTPROGRAMM
# ==============================================================================

Clear-Host
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "   ScreenScanner - Sprach-Manager & Auto-Compiler       " -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""

$installedTags = Get-InstalledOcrTags
$configuredTags = Get-ConfiguredTags
$isAdmin = Test-IsAdmin

# Falls Parameter uebergeben wurden (z. B. nach Admin-Neustart oder Skriptaufruf)
if ($SelectedTags) {
    $targetTagList = $SelectedTags -split "[,; ]+" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" }
    $chosenLanguages = @()
    foreach ($tag in $targetTagList) {
        $found = $AllLanguages | Where-Object { $_.Tag.ToLower() -eq $tag.ToLower() -or $_.Code.ToLower() -eq $tag.ToLower() }
        if ($found) { $chosenLanguages += $found }
    }

    if ($chosenLanguages.Count -eq 0) {
        Write-Host "[!] Keine gueltigen Sprachen in Parametern gefunden." -ForegroundColor Red
        exit 1
    }

    # Fehlende Pakete installieren
    $missingToInstall = $chosenLanguages | Where-Object { $installedTags -notcontains $_.Tag }
    if ($missingToInstall.Count -gt 0) {
        if (-not $isAdmin) {
            Write-Host "[!] Fuer die Installation von Windows-OCR-Paketen sind Administrator-Rechte erforderlich." -ForegroundColor Yellow
            Write-Host "[*] Starte Skript mit Administrator-Rechten neu..." -ForegroundColor Cyan
            Start-Process powershell.exe -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -SelectedTags `"$SelectedTags`" -AutoCompile"
            exit 0
        }

        foreach ($l in $missingToInstall) {
            Install-LanguagePackage -Lang $l
        }
    }

    # Code anpassen
    Write-Host "[*] Aktualisiere Quellcode in src/languages.h..." -ForegroundColor Cyan
    Set-ConfiguredLanguages -Languages $chosenLanguages
    Write-Host "    [OK] src/languages.h erfolgreich aktualisiert!" -ForegroundColor Green

    # Kompilieren
    $buildOk = Invoke-CompileBuild
    if ($buildOk) {
        Write-Host ""
        if (-not $AutoCompile) {
            $answer = Read-Host "Moechtest du ScreenScanner jetzt starten? (J/N)"
            if ($answer -match "^[jJyY]") {
                $exePath = Join-Path $PSScriptRoot "bin\ScreenScanner.exe"
                Start-Process -FilePath $exePath
                Write-Host "[OK] ScreenScanner laeuft jetzt im System-Tray." -ForegroundColor Green
            }
        }
    }
    exit 0
}

# Interaktiver Modus
Write-Host "Aktuell im C++ Projekt aktive Sprachen:" -ForegroundColor Yellow
foreach ($tag in $configuredTags) {
    $lang = $AllLanguages | Where-Object { $_.Tag -eq $tag }
    $name = if ($lang) { $lang.Name } else { $tag }
    $isInstalled = $installedTags -contains $tag
    $status = if ($isInstalled) { "[In Windows bereit OK]" } else { "[In Windows noch NICHT installiert !]" }
    $color = if ($isInstalled) { "Green" } else { "Yellow" }
    Write-Host "  * $name ($tag) $status" -ForegroundColor $color
}

Write-Host ""
Write-Host "--------------------------------------------------------" -ForegroundColor Gray
Write-Host "Verfuegbare Sprachen zum Hinzufuegen / Verwalten:" -ForegroundColor Cyan
Write-Host "--------------------------------------------------------" -ForegroundColor Gray

for ($i = 0; $i -lt $AllLanguages.Count; $i++) {
    $l = $AllLanguages[$i]
    $isConfigured = $configuredTags -contains $l.Tag
    $isInstalled = $installedTags -contains $l.Tag

    $statusText = ""
    $fgColor = "White"

    if ($isConfigured -and $isInstalled) {
        $statusText = "[Aktiv und Bereit OK]"
        $fgColor = "Green"
    } elseif ($isConfigured -and -not $isInstalled) {
        $statusText = "[Aktiv im Code, aber Paket fehlt !]"
        $fgColor = "Yellow"
    } elseif ($isInstalled) {
        $statusText = "[In Windows installiert (noch nicht im Code)]"
        $fgColor = "Cyan"
    } else {
        $statusText = "[Download erforderlich]"
        $fgColor = "DarkGray"
    }

    $numStr = ("{0,2}" -f $l.Id)
    Write-Host "  [$numStr] $($l.Label.PadRight(35)) $statusText" -ForegroundColor $fgColor
}

Write-Host ""
Write-Host "Optionen:" -ForegroundColor Gray
Write-Host "  * Gib die Nummern der gewuenschten Sprachen getrennt durch Komma ein (z. B.: 1, 2, 5)"
Write-Host "  * 'alle'   = Alle Sprachen aktivieren"
Write-Host "  * 'reset'  = Standard wiederherstellen (Deutsch + Arabisch)"
Write-Host "  * 'q'      = Abbrechen und Beenden"
Write-Host ""

$inputStr = Read-Host "Deine Auswahl"
if (-not $inputStr -or $inputStr.Trim().ToLower() -eq "q") {
    Write-Host "Abgebrochen." -ForegroundColor Gray
    exit 0
}

$chosenLanguages = @()

if ($inputStr.Trim().ToLower() -eq "reset") {
    $chosenLanguages = @(
        ($AllLanguages | Where-Object { $_.Tag -eq "de-DE" }),
        ($AllLanguages | Where-Object { $_.Tag -eq "ar-SA" })
    )
} elseif ($inputStr.Trim().ToLower() -eq "alle") {
    $chosenLanguages = $AllLanguages
} else {
    $tokens = $inputStr -split "[,; ]+" | Where-Object { $_ -ne "" }
    foreach ($token in $tokens) {
        if ($token -match "^\d+$") {
            $id = [int]$token
            $matchLang = $AllLanguages | Where-Object { $_.Id -eq $id }
            if ($matchLang -and ($chosenLanguages | Where-Object { $_.Tag -eq $matchLang.Tag }).Count -eq 0) {
                $chosenLanguages += $matchLang
            }
        } else {
            $matchLang = $AllLanguages | Where-Object { $_.Code -eq $token.ToLower() -or $_.Tag.ToLower() -eq $token.ToLower() }
            if ($matchLang -and ($chosenLanguages | Where-Object { $_.Tag -eq $matchLang.Tag }).Count -eq 0) {
                $chosenLanguages += $matchLang
            }
        }
    }
}

if ($chosenLanguages.Count -eq 0) {
    Write-Host "[!] Keine gueltigen Sprachen ausgewaehlt." -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Ausgewaehlte Sprachen:" -ForegroundColor Green
foreach ($l in $chosenLanguages) {
    Write-Host "  + $($l.Name) ($($l.Tag))" -ForegroundColor Green
}
Write-Host ""

# Pruefen, welche Pakete noch in Windows installiert werden muessen
$missingPackages = $chosenLanguages | Where-Object { $installedTags -notcontains $_.Tag }

if ($missingPackages.Count -gt 0) {
    Write-Host "Folgende Sprachen muessen erst in Windows installiert werden:" -ForegroundColor Yellow
    foreach ($m in $missingPackages) {
        Write-Host "  * $($m.Name) ($($m.Tag))" -ForegroundColor Yellow
    }
    Write-Host ""

    if (-not $isAdmin) {
        Write-Host "[!] Fuer die Installation von Windows-Sprachpaketen werden Administrator-Rechte benoetigt." -ForegroundColor Yellow
        $runElevated = Read-Host "Moechtest du das Skript jetzt als Administrator ausfuehren? (J/N)"
        if ($runElevated -match "^[jJyY]") {
            $tagStr = ($chosenLanguages | Select-Object -ExpandProperty Tag) -join ","
            Start-Process powershell.exe -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -SelectedTags `"$tagStr`" -AutoCompile"
            exit 0
        } else {
            Write-Host "[!] Installation uebersprungen. Fahre mit Code-Aktualisierung und Kompilierung fort..." -ForegroundColor Yellow
        }
    } else {
        foreach ($m in $missingPackages) {
            Install-LanguagePackage -Lang $m
        }
    }
}

# 1. Quellcode anpassen (src/languages.h)
Write-Host ""
Write-Host "[*] Aktualisiere C++ Quellcode (src/languages.h)..." -ForegroundColor Cyan
Set-ConfiguredLanguages -Languages $chosenLanguages
Write-Host "[OK] src/languages.h wurde erfolgreich aktualisiert!" -ForegroundColor Green

# 2. Re-Kompilieren
$compileResult = Invoke-CompileBuild

# 3. Programm starten falls gewuenscht
if ($compileResult) {
    Write-Host ""
    $runApp = Read-Host "Moechtest du ScreenScanner jetzt im Hintergrund starten? (J/N)"
    if ($runApp -match "^[jJyY]") {
        $exePath = Join-Path $PSScriptRoot "bin\ScreenScanner.exe"
        Start-Process -FilePath $exePath
        Write-Host "[OK] ScreenScanner laeuft jetzt im System-Tray!" -ForegroundColor Green
    }
}
