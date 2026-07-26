#pragma once

#include <ToolboxUIPlugin.h>

#include <GWCA/Utilities/Hook.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_set>

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

    void Update(float delta) override;
    void Draw(IDirect3DDevice9*) override;
    void DrawSettings() override;
    void LoadSettings(const wchar_t* folder) override;
    void SaveSettings(const wchar_t* folder) override;

    // Called by the free GWCA callbacks.
    void HandleChatPacket(uint32_t channel, const wchar_t* encoded_message);

private:
    void ScanNearbyObjects();
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
    bool nearby_gadget_alert_enabled_ = false;
    std::filesystem::path nearby_gadget_wav_;
    float nearby_gadget_range_ = 5000.0f;
    int nearby_scan_interval_ms_ = 400;
    std::unordered_set<uint32_t> announced_nearby_agents_;
    std::chrono::steady_clock::time_point last_nearby_scan_{};

    int cooldown_ms_ = 1500;
    std::chrono::steady_clock::time_point last_sound_{};
    struct NearbyAgentDebug {
        uint32_t agent_id = 0;
        uint32_t gadget_id = 0;
        uint32_t extra_type = 0;
        uint32_t type = 0;
        float distance = 0.0f;
        bool is_target = false;
        bool is_gadget = false;
        bool is_item = false;
        bool is_living = false;
        bool matches_locked_chest_id = false;
        bool targetable_as_gadget = false;
    };

    std::vector<NearbyAgentDebug> nearby_agent_debug_;
    uint32_t target_agent_id_ = 0;
    uint32_t target_gadget_id_ = 0;
    uint32_t target_extra_type_ = 0;
    uint32_t target_type_ = 0;
    bool target_is_gadget_ = false;
    bool target_is_locked_chest_ = false;
    bool target_is_opened_locked_chest_ = false;
    bool scan_has_player_ = false;
    bool scan_has_agent_array_ = false;
    size_t scanned_agent_count_ = 0;

};
