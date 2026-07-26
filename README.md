# ChatSoundsPlugin

> A lightweight Guild Wars Toolbox++ plugin that plays custom notification sounds for chat events and keyword matches.

## Features

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

---

## Requirements

- Windows
- Guild Wars
- GWToolbox++
- Visual Studio 2022
- CMake
- vcpkg

---

## Installation

Clone or copy the plugin into the GWToolbox++ source tree:

```text
GWToolboxpp/
└── plugins/
    └── ChatSoundsPlugin/
        ├── ChatSoundsPlugin.cpp
        └── ChatSoundsPlugin.h
```

Register the plugin inside:

```cmake
cmake/gwtoolboxdll_plugins.cmake
```

```cmake
add_tb_plugin(ChatSoundsPlugin)

target_link_libraries(ChatSoundsPlugin PRIVATE
    winmm
    comdlg32
)
```

---

## Build

Configure:

```powershell
cmake --preset=vcpkg
```

Compile:

```powershell
cmake --build build --config RelWithDebInfo --target ChatSoundsPlugin
```

The compiled DLL will be located in:

```text
bin/RelWithDebInfo/ChatSoundsPlugin.dll
```

---

## Usage

Open the Toolbox settings and navigate to:

```
Plugins → Chat Sounds
```

### Whisper notification

1. Enable **Whisper Sound**
2. Select a WAV file
3. Press **Test**

---

### Keyword notification

Create a rule by specifying:

- Chat channel
- Keyword
- WAV file
- Case-sensitive (optional)

Example:

| Channel | Keyword | Sound |
|---------|----------|-------|
| Guild | DoA | guild.wav |
| Trade | Obsidian | trade.wav |
| Any | Ecto | ecto.wav |

Whenever a matching message is received, the configured sound is played.

---

## Supported audio

Currently supported:

- WAV

Recommended:

- PCM 16-bit
- 44.1 kHz

---

## Planned Features

- Multiple sounds per rule (random playback)
- Regular Expression support
- Per-rule cooldown
- Volume control
- Folder monitoring
- Import / Export rules
- JSON configuration
- Drag & Drop rule ordering
- Sound packs
- UI improvements

---

## Technical Details

The plugin uses the same UI message callbacks as the built-in chat filter:

- `kPrintChatMessage`
- `kPlayerChatMessage`

This avoids packet injection and keeps compatibility with current GWToolbox++ versions.

Sound playback uses the Windows Multimedia API (`PlaySoundW`) asynchronously.

---

## Compatibility

Designed for modern GWToolbox++ builds using:

- `ToolboxPlugin`
- `RegisterUIMessageCallback`
- `GW::HookEntry`

Older Toolbox versions may require adjustments.

---

## Contributing

Pull requests and feature suggestions are welcome.

If you discover a compatibility issue with a newer GWToolbox++ version, please open an issue.

---

## License

MIT License

---

## Disclaimer

Guild Wars® is a registered trademark of ArenaNet, LLC.

This project is an independent community plugin and is not affiliated with or endorsed by ArenaNet.
