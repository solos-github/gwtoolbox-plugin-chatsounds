# ChatSoundsPlugin

> A lightweight Guild Wars Toolbox++ plugin that plays custom notification sounds for whispers and keyword matches.

---

# ChatSoundsPlugin (Deutsch)

> Ein leichtgewichtiges Plugin für GWToolbox++, das benutzerdefinierte Benachrichtigungstöne für Whisper und frei definierbare Chat-Schlagwörter abspielt.

---

## Features

### English

- 🔔 Play a sound when receiving a whisper
- 💬 Create unlimited keyword-based sound notifications
- 🎯 Filter by chat channel
  - Any
  - Local
  - Guild
  - Alliance
  - Party
  - Trade
  - Whisper
- 🎵 Individual WAV file for every rule
- 🧪 Built-in sound test button
- 🔠 Optional case-sensitive matching
- ⏱ Global sound cooldown
- 💾 Automatic settings persistence
- ⚡ Lightweight and asynchronous playback

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

---

# Installation (Prebuilt DLL)

If you downloaded a precompiled `ChatSoundsPlugin.dll`, you do **not** need Visual Studio or the GWToolbox++ source code.

## 1. Locate your plugins folder

Open your GWToolbox++ installation directory and navigate to:

```text
plugins/
```

If the folder does not exist, simply create it.

## 2. Copy the plugin

Copy

```text
ChatSoundsPlugin.dll
```

into

```text
GWToolboxpp/
└── plugins/
    └── ChatSoundsPlugin.dll
```

## 3. Start Guild Wars

Launch Guild Wars normally with GWToolbox++.

The plugin will automatically be loaded during startup.

## 4. Configure the plugin

Open

```
GWToolbox
→ Settings
→ Plugins
→ Chat Sounds
```

From there you can:

- Enable whisper notifications
- Create keyword rules
- Select chat channels
- Choose WAV files
- Test sounds
- Configure the cooldown

### Updating

1. Close Guild Wars.
2. Replace `ChatSoundsPlugin.dll` with the new version.
3. Start Guild Wars again.

Your settings will be preserved.

### Removing

Delete

```text
plugins/ChatSoundsPlugin.dll
```

and restart Guild Wars.

The plugin will no longer be loaded.

---

# Installation (Vorkompilierte DLL)

Wenn du eine bereits kompilierte `ChatSoundsPlugin.dll` heruntergeladen hast, benötigst du **weder Visual Studio noch den GWToolbox++-Quellcode**.

## 1. Plugin-Ordner öffnen

Öffne dein GWToolbox++-Verzeichnis und wechsle in den Ordner

```text
plugins/
```

Falls der Ordner nicht existiert, kannst du ihn einfach erstellen.

## 2. Plugin kopieren

Kopiere

```text
ChatSoundsPlugin.dll
```

nach

```text
GWToolboxpp/
└── plugins/
    └── ChatSoundsPlugin.dll
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
2. Die vorhandene `ChatSoundsPlugin.dll` durch die neue Version ersetzen.
3. Guild Wars erneut starten.

Alle Einstellungen bleiben erhalten.

### Entfernen

Lösche

```text
plugins/ChatSoundsPlugin.dll
```

und starte Guild Wars erneut.

Das Plugin wird anschließend nicht mehr geladen.

---

# Developer Requirements

- Windows or Linux (Wine)
- Guild Wars
- GWToolbox++
- Visual Studio 2022
- CMake
- vcpkg

---

# Entwickler-Voraussetzungen

- Windows oder Linux (Wine)
- Guild Wars
- GWToolbox++
- Visual Studio 2022
- CMake
- vcpkg

---

# Building from Source

Copy the plugin into the GWToolbox++ source tree:

```text
GWToolboxpp/
└── plugins/
    └── ChatSoundsPlugin/
        ├── ChatSoundsPlugin.cpp
        └── ChatSoundsPlugin.h
```

Register the plugin in

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

Configure:

```powershell
cmake --preset=vcpkg
```

Build:

```powershell
cmake --build build --config RelWithDebInfo --target ChatSoundsPlugin
```

The compiled DLL will be located in:

```text
bin/RelWithDebInfo/ChatSoundsPlugin.dll
```

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
bin/RelWithDebInfo/ChatSoundsPlugin.dll
```

---

# Usage

## Whisper notification

1. Enable **Whisper Sound**
2. Select a WAV file
3. Press **Test**

## Keyword rules

Create a rule by specifying:

- Chat channel
- Keyword
- WAV file
- Optional case-sensitive matching

Example:

| Channel | Keyword | Sound |
|----------|----------|-------|
| Guild | DoA | guild.wav |
| Trade | Obsidian | trade.wav |
| Any | Ecto | ecto.wav |

Whenever a matching message is received, the configured sound will be played.

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

# Supported Audio

Currently supported:

- WAV

Recommended:

- PCM
- 16 Bit
- 44.1 kHz

---

# Unterstützte Audioformate

Aktuell unterstützt:

- WAV

Empfohlen:

- PCM
- 16 Bit
- 44,1 kHz

---

# Technical Details

The plugin uses the same UI message callbacks as the built-in GWToolbox++ ChatFilter:

- `kPrintChatMessage`
- `kPlayerChatMessage`

This keeps the plugin compatible with modern GWToolbox++ versions while avoiding packet injection.

Audio playback uses the Windows Multimedia API (`PlaySoundW`) asynchronously.

---

# Technische Details

Das Plugin verwendet dieselben UI-Callbacks wie der integrierte ChatFilter von GWToolbox++:

- `kPrintChatMessage`
- `kPlayerChatMessage`

Dadurch bleibt das Plugin mit aktuellen Versionen von GWToolbox++ kompatibel und benötigt keine Paketmanipulation.

Die Audiowiedergabe erfolgt asynchron über die Windows Multimedia API (`PlaySoundW`).

---

# Compatibility

Designed for modern GWToolbox++ versions using:

- `ToolboxPlugin`
- `RegisterUIMessageCallback`
- `GW::HookEntry`

Older versions may require small adjustments.

---

# Kompatibilität

Entwickelt für aktuelle Versionen von GWToolbox++ mit:

- `ToolboxPlugin`
- `RegisterUIMessageCallback`
- `GW::HookEntry`

Ältere Versionen der Toolbox benötigen möglicherweise kleinere Anpassungen.

---

# Contributing

Pull requests, feature requests and bug reports are always welcome.

---

# Mitwirken

Pull Requests, Verbesserungsvorschläge und Fehlermeldungen sind jederzeit willkommen.

---

# License

MIT License

---

# Lizenz

MIT License

---

# Disclaimer

Guild Wars® is a registered trademark of ArenaNet, LLC.

This project is an independent community plugin and is not affiliated with or endorsed by ArenaNet.

---

# Hinweis

Guild Wars® ist eine eingetragene Marke von ArenaNet, LLC.

Dieses Projekt ist ein unabhängiges Community-Plugin und steht in keiner Verbindung zu ArenaNet.
