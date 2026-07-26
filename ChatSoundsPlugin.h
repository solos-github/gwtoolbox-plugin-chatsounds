#pragma once

#include <ToolboxUIPlugin.h>

#include <GWCA/Utilities/Hook.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

class ChatSoundsPlugin final : public ToolboxPlugin {
public:
    enum class ChannelFilter : int {
        Any = 0,
        Local,
        Guild,
        Alliance,
        Party,
        Trade,
        Whisper
    };

    struct Rule {
        bool enabled = true;
        ChannelFilter channel = ChannelFilter::Any;
        std::string keyword;
        std::filesystem::path wav_path;
        bool case_sensitive = false;
    };

    ChatSoundsPlugin() = default;
    ~ChatSoundsPlugin() override = default;

    const char* Name() const override { return "Chat Sounds"; }
    [[nodiscard]] bool HasSettings() const override { return true; }

    void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    void SignalTerminate() override;
    bool CanTerminate() override { return true; }

    void Draw(IDirect3DDevice9*) override {}
    void DrawSettings() override;
    void LoadSettings(const wchar_t* folder) override;
    void SaveSettings(const wchar_t* folder) override;

    // Wird von den freien GWCA-Callbacks aufgerufen.
    void HandleChatPacket(uint32_t channel, const wchar_t* encoded_message);

private:
    void RegisterChatHooks();
    void RemoveChatHooks();

    void PlayWav(const std::filesystem::path& path);
    bool SelectWav(std::filesystem::path& target);
    bool CooldownElapsed() const;

    static std::wstring ExtractReadableText(const wchar_t* encoded_message);
    static bool ContainsKeyword(const std::wstring& message, const std::string& keyword, bool case_sensitive);
    static bool ChannelMatches(ChannelFilter filter, uint32_t channel);
    static std::string WideToUtf8(const std::wstring& value);
    static std::wstring Utf8ToWide(const std::string& value);

    GW::HookEntry chat_hook_;

    bool enabled_ = true;
    bool whisper_enabled_ = true;
    std::filesystem::path whisper_wav_;
    std::vector<Rule> rules_;
    int cooldown_ms_ = 1500;
    std::chrono::steady_clock::time_point last_sound_{};
};
