#pragma once

#include <ToolboxUIPlugin.h>

#include <GWCA/Utilities/Hook.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Item.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_set>

struct IGraphBuilder;
struct IMediaControl;
struct IBasicAudio;

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

    enum class DropMatchMode : int {
        NameContains = 0,
        ExactName,
        ModelId
    };

    struct DropRule {
        bool enabled = true;
        std::string rule_name;
        DropMatchMode match_mode = DropMatchMode::NameContains;
        std::string name;
        uint32_t model_id = 0;
        std::filesystem::path wav_path;
        bool case_sensitive = false;

        // AgentItem::owner contains the runtime agent ID that currently owns
        // the ground drop. owner == player_agent_id is treated as my drop.
        // Every other owner value, including 0, is treated as another
        // player's/unassigned drop.
        bool only_my_drops = true;
        bool include_other_players_drops = false;
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
    void HandleChatPacket(
        uint32_t channel,
        const wchar_t* encoded_message);
    void HandleIncomingWhisper(
        const wchar_t* encoded_message);

private:
    void ScanNearbyObjects();
    void ScanNearbyDrops(
        const GW::AgentLiving* player,
        const GW::AgentArray* agents);
    bool DropRuleMatches(
        const DropRule& rule,
        const GW::Item* item,
        const GW::AgentItem* item_agent,
        uint32_t player_agent_id) const;
    static std::wstring GetDropName(const GW::Item* item);
    void RegisterChatHooks();
    void RemoveChatHooks();

    void PlayWav(const std::filesystem::path& path);
    void StopSound();
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
    bool drop_alerts_enabled_ = true;
    float drop_detection_range_ = 5000.0f;
    std::vector<DropRule> drop_rules_;
    std::unordered_set<uint32_t> announced_drop_agents_;
    bool nearby_gadget_alert_enabled_ = false;
    std::filesystem::path nearby_gadget_wav_;
    float nearby_gadget_range_ = 5000.0f;
    int nearby_scan_interval_ms_ = 400;
    std::unordered_set<uint32_t> announced_nearby_agents_;
    std::chrono::steady_clock::time_point last_nearby_scan_{};

    int cooldown_ms_ = 1500;
    int sound_volume_percent_ = 100;
    IGraphBuilder* audio_graph_ = nullptr;
    IMediaControl* audio_control_ = nullptr;
    IBasicAudio* basic_audio_ = nullptr;
    bool com_initialized_by_plugin_ = false;
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
