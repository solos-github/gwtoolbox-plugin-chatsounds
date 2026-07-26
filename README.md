# ChatSoundsPlugin (Deutsch)

> Ein leichtgewichtiges Plugin für GWToolbox++, das benutzerdefinierte Benachrichtigungstöne für Whisper und frei definierbare Chat-Schlagwörter abspielt.

---

## Features

### Deutsch

- 🔔 Ton bei eingehenden Whispern
- 💬 Beliebig viele Schlagwort-Regeln
- 🎯 Filter nach Chatkanal
  - Alle
  - Lokal
  - Gilde
  - Allianz
  - Gruppe
  - Handel
  - Whisper
- 🎵 Eigene WAV-Datei für jede Regel
- 🧪 Test-Button für jeden Sound
- 🔠 Optionale Groß-/Kleinschreibung
- ⏱ Globaler Sound-Cooldown
- 💾 Automatisches Speichern aller Einstellungen
- ⚡ Asynchrone Audiowiedergabe ohne Guild Wars zu blockieren
- Gute Quelle für wav-files ohne sich registrieren zu müssen 'https://mixkit.co/'
---
# Screenshot

<img width="800" height="800" alt="image" src="https://github.com/user-attachments/assets/4e876a69-2861-46f0-8626-144362b80c4c" />

# Installation (Vorkompilierte DLL)

Wenn du eine bereits kompilierte `ChatSoundsPlugin_by_wolle.dll` heruntergeladen hast, benötigst du **weder Visual Studio noch den GWToolbox++-Quellcode**.
Download ist hier https://github.com/solos-github/gwtoolbox-plugin-chatsounds/releases/tag/dll

## 1. Plugin-Ordner öffnen

Öffne dein GWToolbox++-Verzeichnis und wechsle in den Ordner

```text
plugins/
```

Falls der Ordner nicht existiert, kannst du ihn einfach erstellen.

## 2. Plugin kopieren

Kopiere

```text
ChatSoundsPlugin_by_wolle.dll
```

nach

```text
GWToolboxpp/
└── plugins/
    └── ChatSoundsPlugin_by_wolle.dll
```

## 3. Guild Wars starten

Starte Guild Wars wie gewohnt zusammen mit GWToolbox++.

Das Plugin wird beim Start automatisch geladen.

## 4. Plugin konfigurieren

Öffne

```
GWToolbox
→ Settings
→ Plugins
→ Chat Sounds
```

Dort kannst du:

- Whisper-Benachrichtigungen aktivieren
- Schlagwort-Regeln erstellen
- Chatkanäle auswählen
- Eigene WAV-Dateien verwenden
- Sounds testen
- Den globalen Cooldown einstellen

### Aktualisieren

1. Guild Wars schließen.
2. Die vorhandene `ChatSoundsPlugin_by_wolle.dll` durch die neue Version ersetzen.
3. Guild Wars erneut starten.

Alle Einstellungen bleiben erhalten.

### Entfernen

Lösche

```text
plugins/ChatSoundsPlugin_by_wolle.dll
```

und starte Guild Wars erneut.

Das Plugin wird anschließend nicht mehr geladen.

---


# Entwickler-Voraussetzungen

- Windows oder Linux (Wine)
- Guild Wars
- GWToolbox++
- Visual Studio 2022
- CMake
- vcpkg

---


# Kompilieren (Quellcode)

Kopiere das Plugin in den GWToolbox++-Quellcode:

```text
GWToolboxpp/
└── plugins/
    └── ChatSoundsPlugin/
        ├── ChatSoundsPlugin.cpp
        └── ChatSoundsPlugin.h
```

Registriere das Plugin in

```text
cmake/gwtoolboxdll_plugins.cmake
```

```cmake
add_tb_plugin(ChatSoundsPlugin)

target_link_libraries(ChatSoundsPlugin PRIVATE
    winmm
    comdlg32
)
```

Konfigurieren:

```powershell
cmake --preset=vcpkg
```

Kompilieren:

```powershell
cmake --build build --config RelWithDebInfo --target ChatSoundsPlugin
```

Die fertige DLL befindet sich anschließend unter:

```text
bin/RelWithDebInfo/ChatSoundsPlugin_by_wolle.dll
```

---

# Verwendung

## Whisper-Benachrichtigung

1. Whisper-Sound aktivieren
2. WAV-Datei auswählen
3. Mit **Test** überprüfen

## Schlagwort-Regeln

Für jede Regel können folgende Optionen festgelegt werden:

- Chatkanal
- Suchbegriff
- WAV-Datei
- Optionale Groß-/Kleinschreibung

Beispiel:

| Kanal | Schlagwort | Sound |
|--------|------------|-------|
| Gilde | DoA | guild.wav |
| Handel | Obsidian | trade.wav |
| Alle | Ecto | ecto.wav |

Sobald eine passende Nachricht erscheint, wird der konfigurierte Sound abgespielt.

---


# Unterstützte Audioformate

Aktuell unterstützt:

- WAV

Empfohlen:

- PCM
- 16 Bit
- 44,1 kHz

---

# Technische Details

Das Plugin verwendet dieselben UI-Callbacks wie der integrierte ChatFilter von GWToolbox++:

- `kPrintChatMessage`
- `kPlayerChatMessage`

Dadurch bleibt das Plugin mit aktuellen Versionen von GWToolbox++ kompatibel und benötigt keine Paketmanipulation.

Die Audiowiedergabe erfolgt asynchron über die Windows Multimedia API (`PlaySoundW`).

---


# Kompatibilität

Entwickelt für aktuelle Versionen von GWToolbox++ mit:

- `ToolboxPlugin`
- `RegisterUIMessageCallback`
- `GW::HookEntry`

Ältere Versionen der Toolbox benötigen möglicherweise kleinere Anpassungen.

---


# Mitwirken

Pull Requests, Verbesserungsvorschläge und Fehlermeldungen sind jederzeit willkommen.

---


# Lizenz

MIT License

---

# Hinweis

Guild Wars® ist eine eingetragene Marke von ArenaNet, LLC.

Dieses Projekt ist ein unabhängiges Community-Plugin und steht in keiner Verbindung zu ArenaNet.
