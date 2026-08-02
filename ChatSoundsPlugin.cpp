#include "ChatSoundsPlugin.h"

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/ItemMgr.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Item.h>
#include <GWCA/Managers/UIMgr.h>


#include <Windows.h>
#include <commdlg.h>
#include <dshow.h>

#include <algorithm>
#include <cmath>
#include <array>
#include <cwctype>
#include <ranges>
#include <unordered_set>

#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace {
    ChatSoundsPlugin* plugin_instance = nullptr;

    void OnChatUiMessage(
        GW::HookStatus* status,
        GW::UI::UIMessage message_id,
        void* wparam,
        void*)
    {
        if (!plugin_instance || !wparam || status->blocked) {
            return;
        }

        switch (message_id) {
        case GW::UI::UIMessage::kPrintChatMessage: {
            const auto packet =
                static_cast<GW::UI::UIPacket::kPrintChatMessage*>(wparam);
            plugin_instance->HandleChatPacket(
                packet->channel,
                packet->message);
            break;
        }
        case GW::UI::UIMessage::kPlayerChatMessage: {
            const auto packet =
                static_cast<GW::UI::UIPacket::kPlayerChatMessage*>(wparam);
            plugin_instance->HandleChatPacket(
                packet->channel,
                packet->message);
            break;
        }
        case GW::UI::UIMessage::kRecvWhisper: {
            const auto packet =
                static_cast<GW::UI::UIPacket::kRecvWhisper*>(wparam);
            plugin_instance->HandleIncomingWhisper(packet->message);
            break;
        }
        default:
            break;
        }
    }

    std::wstring Lower(std::wstring value)
    {
        std::ranges::transform(value, value.begin(), [](const wchar_t c) {
            return static_cast<wchar_t>(std::towlower(c));
        });
        return value;
    }

    constexpr uint32_t LockedChestGadgetId = 8141;

    bool IsLockedChest(const GW::Agent* agent)
    {
        if (!agent || !agent->GetIsGadgetType()) {
            return false;
        }

        const GW::AgentGadget* gadget =
            agent->GetAsAgentGadget();

        return gadget &&
            gadget->gadget_id == LockedChestGadgetId;
    }

    bool IsOpenedLockedChest(const GW::Agent* agent)
    {
        return IsLockedChest(agent) &&
            !GW::Agents::GetAgentMatchesFlags(
                agent,
                GW::TargetFilter::Gadgets);
    }
}

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static ChatSoundsPlugin instance;
    return &instance;
}

void ChatSoundsPlugin::Initialize(
    ImGuiContext* ctx,
    const ImGuiAllocFns allocator_fns,
    const HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, allocator_fns, toolbox_dll);

    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    com_initialized_by_plugin_ = SUCCEEDED(com_result);

    plugin_instance = this;
    RegisterChatHooks();
}


void ChatSoundsPlugin::Update(float)
{
    ScanNearbyObjects();
}

void ChatSoundsPlugin::Draw(IDirect3DDevice9*)
{
    // Background logic intentionally runs in Update().
}

void ChatSoundsPlugin::ScanNearbyObjects()
{
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_nearby_scan_);

    if (last_nearby_scan_.time_since_epoch().count() != 0 &&
        elapsed.count() < nearby_scan_interval_ms_) {
        return;
    }
    last_nearby_scan_ = now;

    nearby_agent_debug_.clear();
    target_agent_id_ = 0;
    target_gadget_id_ = 0;
    target_extra_type_ = 0;
    target_type_ = 0;
    target_is_gadget_ = false;
    target_is_locked_chest_ = false;
    target_is_opened_locked_chest_ = false;
    scan_has_player_ = false;
    scan_has_agent_array_ = false;
    scanned_agent_count_ = 0;

    const GW::AgentLiving* player =
        GW::Agents::GetControlledCharacter();
    const GW::AgentArray* agents =
        GW::Agents::GetAgentArray();

    scan_has_player_ = player != nullptr;
    scan_has_agent_array_ = agents != nullptr;

    if (!player || !agents) {
        announced_nearby_agents_.clear();
        return;
    }

    ScanNearbyDrops(player, agents);

    const GW::Agent* target = GW::Agents::GetTarget();

    // Evaluate the current target separately, regardless of
    // whether it appears later in the regular AgentArray scan.
    if (target) {
        target_agent_id_ = target->agent_id;
        target_type_ = target->type;
        target_is_gadget_ = target->GetIsGadgetType();

        if (const GW::AgentGadget* gadget =
                target->GetAsAgentGadget()) {
            target_gadget_id_ = gadget->gadget_id;
            target_extra_type_ = gadget->extra_type;
            target_is_locked_chest_ =
                gadget->gadget_id == LockedChestGadgetId;
        }

        target_is_opened_locked_chest_ =
            target_is_locked_chest_ &&
            !GW::Agents::GetAgentMatchesFlags(
                target,
                GW::TargetFilter::Gadgets);
    }

    std::unordered_set<uint32_t> currently_nearby;

    for (GW::Agent* agent : *agents) {
        if (!agent) {
            continue;
        }

        ++scanned_agent_count_;

        const float dx = agent->x - player->x;
        const float dy = agent->y - player->y;
        const float distance =
            std::sqrt(dx * dx + dy * dy);

        if (distance > nearby_gadget_range_) {
            continue;
        }

        const bool is_gadget = agent->GetIsGadgetType();
        const bool is_item = agent->GetIsItemType();
        const bool is_living = agent->GetIsLivingType();
        const bool is_target =
            target && target->agent_id == agent->agent_id;

        uint32_t gadget_id = 0;
        uint32_t extra_type = 0;
        bool matches_locked_chest_id = false;
        bool targetable_as_gadget = false;

        // No blind reinterpret_cast: gadget fields are only read
        // when GWCA exposes the agent as a gadget.
        if (const GW::AgentGadget* gadget =
                agent->GetAsAgentGadget()) {
            gadget_id = gadget->gadget_id;
            extra_type = gadget->extra_type;
            matches_locked_chest_id =
                gadget_id == LockedChestGadgetId;
            targetable_as_gadget =
                GW::Agents::GetAgentMatchesFlags(
                    agent,
                    GW::TargetFilter::Gadgets);
        }

        nearby_agent_debug_.push_back({
            agent->agent_id,
            gadget_id,
            extra_type,
            agent->type,
            distance,
            is_target,
            is_gadget,
            is_item,
            is_living,
            matches_locked_chest_id,
            targetable_as_gadget
        });

        if (!matches_locked_chest_id) {
            continue;
        }

        // Chests that are no longer targetable are considered opened.
        if (!targetable_as_gadget) {
            continue;
        }

        currently_nearby.insert(agent->agent_id);

        if (enabled_ &&
            nearby_gadget_alert_enabled_ &&
            !announced_nearby_agents_.contains(agent->agent_id)) {
            PlayWav(nearby_gadget_wav_);
        }
    }

    // If the targeted chest does not appear in the AgentArray,
    // it is still detected separately and can trigger the alert.
    if (target &&
        target_is_locked_chest_ &&
        !target_is_opened_locked_chest_) {
        const float dx = target->x - player->x;
        const float dy = target->y - player->y;
        const float distance =
            std::sqrt(dx * dx + dy * dy);

        if (distance <= nearby_gadget_range_) {
            currently_nearby.insert(target->agent_id);

            if (enabled_ &&
                nearby_gadget_alert_enabled_ &&
                !announced_nearby_agents_.contains(target->agent_id)) {
                PlayWav(nearby_gadget_wav_);
            }
        }
    }

    std::ranges::sort(
        nearby_agent_debug_,
        [](const NearbyAgentDebug& a,
           const NearbyAgentDebug& b) {
            if (a.is_target != b.is_target) {
                return a.is_target > b.is_target;
            }
            return a.distance < b.distance;
        });

    announced_nearby_agents_ =
        std::move(currently_nearby);
}


std::wstring ChatSoundsPlugin::GetDropName(const GW::Item* item)
{
    if (!item) {
        return {};
    }

    // single_item_name is preferred because it excludes stack quantity.
    // The returned text follows the Guild Wars client language.
    if (item->single_item_name && *item->single_item_name) {
        return item->single_item_name;
    }

    if (item->name_enc && *item->name_enc) {
        return item->name_enc;
    }

    return {};
}

bool ChatSoundsPlugin::DropRuleMatches(
    const DropRule& rule,
    const GW::Item* item,
    const GW::AgentItem* item_agent,
    const uint32_t player_agent_id) const
{
    if (!rule.enabled || !item || !item_agent) {
        return false;
    }

    const uint32_t owner_agent_id = item_agent->owner;
    const bool is_my_drop =
        owner_agent_id == player_agent_id;
    const bool is_other_players_drop =
        owner_agent_id != player_agent_id;

    bool ownership_allowed = false;

    if (rule.only_my_drops && is_my_drop) {
        ownership_allowed = true;
    }

    if (rule.include_other_players_drops &&
        is_other_players_drop) {
        ownership_allowed = true;
    }

    if (!ownership_allowed) {
        return false;
    }

    if (rule.match_mode == DropMatchMode::ModelId) {
        return rule.model_id != 0 &&
            item->model_id == rule.model_id;
    }

    if (rule.name.empty()) {
        return false;
    }

    const std::wstring item_name = GetDropName(item);
    if (item_name.empty()) {
        return false;
    }

    const std::wstring configured_name =
        Utf8ToWide(rule.name);

    if (configured_name.empty()) {
        return false;
    }

    if (rule.match_mode == DropMatchMode::ExactName) {
        if (rule.case_sensitive) {
            return item_name == configured_name;
        }

        return Lower(item_name) ==
            Lower(configured_name);
    }

    return ContainsKeyword(
        item_name,
        rule.name,
        rule.case_sensitive);
}

void ChatSoundsPlugin::ScanNearbyDrops(
    const GW::AgentLiving* player,
    const GW::AgentArray* agents)
{
    if (!player || !agents) {
        announced_drop_agents_.clear();
        return;
    }

    std::unordered_set<uint32_t> currently_nearby_drops;

    for (GW::Agent* agent : *agents) {
        if (!agent) {
            continue;
        }

        const GW::AgentItem* item_agent =
            agent->GetAsAgentItem();

        if (!item_agent) {
            continue;
        }

        const float dx = agent->x - player->x;
        const float dy = agent->y - player->y;
        const float distance =
            std::sqrt(dx * dx + dy * dy);

        if (distance > drop_detection_range_) {
            continue;
        }

        const GW::Item* item =
            GW::Items::GetItemById(item_agent->item_id);

        if (!item) {
            continue;
        }

        currently_nearby_drops.insert(agent->agent_id);

        if (!enabled_ ||
            !drop_alerts_enabled_ ||
            announced_drop_agents_.contains(agent->agent_id)) {
            continue;
        }

        for (const DropRule& rule : drop_rules_) {
            if (!DropRuleMatches(
                    rule,
                    item,
                    item_agent,
                    player->agent_id)) {
                continue;
            }

            PlayWav(rule.wav_path);
            break;
        }
    }

    announced_drop_agents_ =
        std::move(currently_nearby_drops);
}


void ChatSoundsPlugin::SignalTerminate()
{
    RemoveChatHooks();
    StopSound();

    if (com_initialized_by_plugin_) {
        CoUninitialize();
        com_initialized_by_plugin_ = false;
    }

    plugin_instance = nullptr;
    ToolboxPlugin::SignalTerminate();
}

void ChatSoundsPlugin::RegisterChatHooks()
{
    // kPrintChatMessage captures incoming system, whisper and chat messages.
    RegisterUIMessageCallback(
        &chat_hook_,
        GW::UI::UIMessage::kPrintChatMessage,
        OnChatUiMessage);

    // kPlayerChatMessage captures normal player chat channels.
    RegisterUIMessageCallback(
        &chat_hook_,
        GW::UI::UIMessage::kPlayerChatMessage,
        OnChatUiMessage);

    // This event is emitted only for a real incoming player whisper.
    RegisterUIMessageCallback(
        &chat_hook_,
        GW::UI::UIMessage::kRecvWhisper,
        OnChatUiMessage);
}

void ChatSoundsPlugin::RemoveChatHooks()
{
    GW::UI::RemoveUIMessageCallback(&chat_hook_);
}

void ChatSoundsPlugin::HandleChatPacket(
    const uint32_t channel,
    const wchar_t* encoded_message)
{
    if (!enabled_ || !encoded_message || !*encoded_message) {
        return;
    }

    // Whisper-channel print packets are not reliable: map and system
    // messages can use the same channel value. Real incoming whispers are
    // handled exclusively through kRecvWhisper.
    if (channel == static_cast<uint32_t>(
            GW::Chat::Channel::CHANNEL_WHISPER)) {
        return;
    }

    const std::wstring readable = ExtractReadableText(encoded_message);
    if (readable.empty()) {
        return;
    }

    for (const Rule& rule : rules_) {
        if (!rule.enabled ||
            rule.keyword.empty() ||
            !ChannelMatches(rule.channel, channel)) {
            continue;
        }

        if (ContainsKeyword(
                readable,
                rule.keyword,
                rule.case_sensitive)) {
            PlayWav(rule.wav_path);
            return;
        }
    }
}

void ChatSoundsPlugin::HandleIncomingWhisper(
    const wchar_t* encoded_message)
{
    if (!enabled_) {
        return;
    }

    if (whisper_enabled_) {
        PlayWav(whisper_wav_);
        return;
    }

    // Whisper keyword rules remain available even when the general
    // whisper notification is disabled.
    if (!encoded_message || !*encoded_message) {
        return;
    }

    const std::wstring readable =
        ExtractReadableText(encoded_message);

    if (readable.empty()) {
        return;
    }

    for (const Rule& rule : rules_) {
        if (!rule.enabled ||
            rule.keyword.empty() ||
            (rule.channel != ChannelFilter::Any &&
             rule.channel != ChannelFilter::Whisper)) {
            continue;
        }

        if (ContainsKeyword(
                readable,
                rule.keyword,
                rule.case_sensitive)) {
            PlayWav(rule.wav_path);
            return;
        }
    }
}

std::wstring ChatSoundsPlugin::ExtractReadableText(
    const wchar_t* encoded_message)
{
    if (!encoded_message || !*encoded_message) {
        return {};
    }

    // Viele normale Spielermeldungen enthalten den sichtbaren Text in einem
    // 0x107-Segment und enden am Token 0x1. Dieses Muster wird auch vom
    // eingebauten ChatFilter verwendet.
    const wchar_t* start = wcschr(encoded_message, 0x107);
    if (start) {
        ++start;
        const wchar_t* end = wcschr(start, 0x1);
        if (!end) {
            end = start + wcslen(start);
        }
        if (end > start) {
            return std::wstring(start, end);
        }
    }

    // Manche Pakete liefern bereits direkt lesbaren Text.
    bool readable_ascii = true;
    for (const wchar_t* p = encoded_message; *p; ++p) {
        if (*p < 0x20 && *p != L'\t' && *p != L'\n') {
            readable_ascii = false;
            break;
        }
    }

    return readable_ascii ? std::wstring(encoded_message) : std::wstring{};
}

bool ChatSoundsPlugin::ContainsKeyword(
    const std::wstring& message,
    const std::string& keyword,
    const bool case_sensitive)
{
    if (keyword.empty()) {
        return false;
    }

    std::wstring haystack = message;
    std::wstring needle = Utf8ToWide(keyword);

    if (!case_sensitive) {
        haystack = Lower(std::move(haystack));
        needle = Lower(std::move(needle));
    }

    return haystack.find(needle) != std::wstring::npos;
}

bool ChatSoundsPlugin::ChannelMatches(
    const ChannelFilter filter,
    const uint32_t channel)
{
    const auto gw_channel =
        static_cast<GW::Chat::Channel>(channel);

    switch (filter) {
    case ChannelFilter::Any:
        return true;
    case ChannelFilter::Local:
        return gw_channel == GW::Chat::Channel::CHANNEL_ALL;
    case ChannelFilter::Guild:
        return gw_channel == GW::Chat::Channel::CHANNEL_GUILD;
    case ChannelFilter::Alliance:
        return gw_channel == GW::Chat::Channel::CHANNEL_ALLIANCE;
    case ChannelFilter::Party:
        return gw_channel == GW::Chat::Channel::CHANNEL_GROUP;
    case ChannelFilter::Trade:
        return gw_channel == GW::Chat::Channel::CHANNEL_TRADE;
    case ChannelFilter::Whisper:
        return gw_channel == GW::Chat::Channel::CHANNEL_WHISPER;
    }

    return false;
}

bool ChatSoundsPlugin::CooldownElapsed() const
{
    if (last_sound_.time_since_epoch().count() == 0) {
        return true;
    }

    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - last_sound_);

    return elapsed.count() >= cooldown_ms_;
}

void ChatSoundsPlugin::StopSound()
{
    if (audio_control_) {
        audio_control_->Stop();
    }

    if (basic_audio_) {
        basic_audio_->Release();
        basic_audio_ = nullptr;
    }

    if (audio_control_) {
        audio_control_->Release();
        audio_control_ = nullptr;
    }

    if (audio_graph_) {
        audio_graph_->Release();
        audio_graph_ = nullptr;
    }
}

void ChatSoundsPlugin::PlayWav(
    const std::filesystem::path& path)
{
    if (path.empty() || !CooldownElapsed()) {
        return;
    }

    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
        return;
    }

    StopSound();

    HRESULT result = CoCreateInstance(
        CLSID_FilterGraph,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IGraphBuilder,
        reinterpret_cast<void**>(&audio_graph_));

    if (FAILED(result) || !audio_graph_) {
        StopSound();
        return;
    }

    result = audio_graph_->QueryInterface(
        IID_IMediaControl,
        reinterpret_cast<void**>(&audio_control_));

    if (FAILED(result) || !audio_control_) {
        StopSound();
        return;
    }

    result = audio_graph_->QueryInterface(
        IID_IBasicAudio,
        reinterpret_cast<void**>(&basic_audio_));

    if (FAILED(result) || !basic_audio_) {
        StopSound();
        return;
    }

    result = audio_graph_->RenderFile(path.c_str(), nullptr);

    if (FAILED(result)) {
        StopSound();
        return;
    }

    const int percent =
        std::clamp(sound_volume_percent_, 0, 100);

    // DirectShow uses hundredths of a decibel:
    // 0 = full volume, -10000 = silence.
    long direct_show_volume = -10000;

    if (percent > 0) {
        const double linear_gain =
            static_cast<double>(percent) / 100.0;

        direct_show_volume = static_cast<long>(
            std::lround(2000.0 * std::log10(linear_gain)));

        direct_show_volume =
            std::clamp(direct_show_volume, -10000L, 0L);
    }

    basic_audio_->put_Volume(direct_show_volume);

    result = audio_control_->Run();

    if (SUCCEEDED(result)) {
        last_sound_ = std::chrono::steady_clock::now();
    } else {
        StopSound();
    }
}

bool ChatSoundsPlugin::SelectWav(
    std::filesystem::path& target)
{
    std::array<wchar_t, 4096> file{};

    if (!target.empty()) {
        wcsncpy_s(
            file.data(),
            file.size(),
            target.c_str(),
            _TRUNCATE);
    }

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFile = file.data();
    dialog.nMaxFile = static_cast<DWORD>(file.size());
    dialog.lpstrFilter =
        L"WAV Files (*.wav)\0*.wav\0All Files (*.*)\0*.*\0";
    dialog.nFilterIndex = 1;
    dialog.lpstrDefExt = L"wav";
    dialog.Flags =
        OFN_FILEMUSTEXIST |
        OFN_PATHMUSTEXIST |
        OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&dialog)) {
        return false;
    }

    target = file.data();
    return true;
}

void ChatSoundsPlugin::DrawSettings()
{
    if (!toolbox_handle) {
        return;
    }

    const ImVec4 gold(0.86f, 0.68f, 0.28f, 1.0f);
    const ImVec4 green(0.30f, 0.80f, 0.42f, 1.0f);
    const ImVec4 red(0.92f, 0.34f, 0.34f, 1.0f);
    const ImVec4 muted(0.62f, 0.65f, 0.70f, 1.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 7.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.28f, 0.38f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.40f, 0.55f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.35f, 0.48f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.30f, 0.62f, 0.92f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.30f, 0.62f, 0.92f, 1.0f));

    ImGui::TextColored(gold, "CHAT SOUNDS");
    ImGui::SameLine();
    ImGui::TextColored(muted, "for GWToolbox++");
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Checkbox("Enable Plugin", &enabled_);

    if (ImGui::BeginTabBar("ChatSoundsTabs")) {
        if (ImGui::BeginTabItem("Whisper")) {
            ImGui::TextColored(gold, "Whisper Alerts");
            ImGui::Separator();
            ImGui::Checkbox("Enable Whisper Sound", &whisper_enabled_);

            const std::string whisper_path = WideToUtf8(whisper_wav_.wstring());
            ImGui::TextColored(muted, "Selected sound");
            ImGui::TextWrapped("%s", whisper_path.empty() ? "No WAV file selected." : whisper_path.c_str());
            if (ImGui::Button("Browse...", ImVec2(120.0f, 0.0f))) SelectWav(whisper_wav_);
            ImGui::SameLine();
            if (ImGui::Button("Test Sound", ImVec2(120.0f, 0.0f))) PlayWav(whisper_wav_);

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Keyword Alerts")) {
            ImGui::TextColored(gold, "Keyword Rules");
            ImGui::Separator();
            ImGui::TextWrapped("Play a WAV file when a chat message contains a configured keyword.");
            if (ImGui::Button("Add Keyword Rule", ImVec2(170.0f, 0.0f))) rules_.push_back({});
            if (rules_.empty()) ImGui::TextColored(muted, "No keyword rules configured.");

            int remove_index = -1;
            for (size_t i = 0; i < rules_.size(); ++i) {
                Rule& rule = rules_[i];
                ImGui::PushID(static_cast<int>(i));
                const char* channels[] = {
                    "All Channels",
                    "Local Chat",
                    "Guild Chat",
                    "Alliance Chat",
                    "Party Chat",
                    "Trade Chat",
                    "Whisper"
                };

                const int selected_channel = std::clamp(
                    static_cast<int>(rule.channel),
                    0,
                    static_cast<int>(IM_ARRAYSIZE(channels)) - 1);

                const std::string visible_header =
                    "Rule " + std::to_string(i + 1) +
                    " - " +
                    (rule.keyword.empty() ? std::string("<New Rule>") : rule.keyword) +
                    " [" + channels[selected_channel] + "]";

                // The hidden ID remains stable while keyword or channel text changes.
                const std::string header = visible_header +
                    "###KeywordRule" + std::to_string(i);

                if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Indent();
                    ImGui::Checkbox("Enabled", &rule.enabled);
                    int channel = selected_channel;
                    ImGui::SetNextItemWidth(300.0f);
                    if (ImGui::Combo("Channel", &channel, channels, IM_ARRAYSIZE(channels)))
                        rule.channel = static_cast<ChannelFilter>(channel);

                    std::array<char, 256> keyword{};
                    strncpy_s(keyword.data(), keyword.size(), rule.keyword.c_str(), _TRUNCATE);
                    ImGui::SetNextItemWidth(300.0f);
                    if (ImGui::InputText("Keyword", keyword.data(), keyword.size())) rule.keyword = keyword.data();
                    ImGui::Checkbox("Case Sensitive", &rule.case_sensitive);

                    const std::string wav_path = WideToUtf8(rule.wav_path.wstring());
                    ImGui::TextColored(muted, "Selected sound");
                    ImGui::TextWrapped("%s", wav_path.empty() ? "No WAV file selected." : wav_path.c_str());
                    if (ImGui::Button("Browse...", ImVec2(120.0f, 0.0f))) SelectWav(rule.wav_path);
                    ImGui::SameLine();
                    if (ImGui::Button("Test Sound", ImVec2(120.0f, 0.0f))) PlayWav(rule.wav_path);
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.48f, 0.19f, 0.19f, 1.0f));
                    if (ImGui::Button("Remove", ImVec2(100.0f, 0.0f))) remove_index = static_cast<int>(i);
                    ImGui::PopStyleColor();
                    ImGui::Unindent();
                }
                ImGui::PopID();
            }
            if (remove_index >= 0 && remove_index < static_cast<int>(rules_.size()))
                rules_.erase(rules_.begin() + remove_index);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Item Drops")) {
            ImGui::TextColored(gold, "Item Drop Alerts");
            ImGui::Separator();

            ImGui::Checkbox(
                "Enable Item Drop Alerts",
                &drop_alerts_enabled_);

            ImGui::TextWrapped(
                "Play a sound when a matching dropped item appears within range. "
                "Name rules use the current Guild Wars client language; Model ID rules are language independent. "
                "Each rule can also filter between your drops and all other drops.");

            ImGui::SetNextItemWidth(300.0f);
            ImGui::SliderFloat(
                "Drop Detection Range",
                &drop_detection_range_,
                100.0f,
                10000.0f,
                "%.0f units");
            drop_detection_range_ =
                std::clamp(
                    drop_detection_range_,
                    100.0f,
                    10000.0f);

            if (ImGui::Button(
                    "Add Drop Rule",
                    ImVec2(170.0f, 0.0f))) {
                drop_rules_.push_back({});
            }

            if (drop_rules_.empty()) {
                ImGui::TextColored(
                    muted,
                    "No item drop rules configured.");
            }

            int remove_drop_index = -1;

            for (size_t i = 0; i < drop_rules_.size(); ++i) {
                DropRule& rule = drop_rules_[i];
                ImGui::PushID(
                    50000 + static_cast<int>(i));

                const char* mode_names[] = {
                    "Name Contains",
                    "Exact Name",
                    "Model ID"
                };

                const int selected_mode =
                    std::clamp(
                        static_cast<int>(rule.match_mode),
                        0,
                        static_cast<int>(
                            IM_ARRAYSIZE(mode_names)) - 1);

                std::string value_text;
                if (rule.match_mode ==
                    DropMatchMode::ModelId) {
                    value_text = rule.model_id == 0
                        ? "<No Model ID>"
                        : std::to_string(rule.model_id);
                }
                else {
                    value_text = rule.name.empty()
                        ? "<New Drop Rule>"
                        : rule.name;
                }

                const std::string visible_header =
                    "Rule " + std::to_string(i + 1) +
                    " - " +
                    (rule.rule_name.empty()
                        ? value_text
                        : rule.rule_name) +
                    " [" + mode_names[selected_mode] + "]";

                const std::string header =
                    visible_header +
                    "###DropRule" +
                    std::to_string(i);

                if (ImGui::CollapsingHeader(
                        header.c_str(),
                        ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::Indent();

                    ImGui::Checkbox(
                        "Enabled",
                        &rule.enabled);

                    std::array<char, 256> rule_name{};
                    strncpy_s(
                        rule_name.data(),
                        rule_name.size(),
                        rule.rule_name.c_str(),
                        _TRUNCATE);

                    ImGui::SetNextItemWidth(300.0f);
                    if (ImGui::InputText(
                            "Rule Name",
                            rule_name.data(),
                            rule_name.size())) {
                        rule.rule_name = rule_name.data();
                    }

                    int mode = selected_mode;
                    ImGui::SetNextItemWidth(300.0f);
                    if (ImGui::Combo(
                            "Match Mode",
                            &mode,
                            mode_names,
                            IM_ARRAYSIZE(mode_names))) {
                        rule.match_mode =
                            static_cast<DropMatchMode>(mode);
                    }

                    if (rule.match_mode ==
                        DropMatchMode::ModelId) {
                        int model_id =
                            static_cast<int>(rule.model_id);

                        ImGui::SetNextItemWidth(300.0f);
                        if (ImGui::InputInt(
                                "Model ID",
                                &model_id)) {
                            rule.model_id =
                                model_id > 0
                                    ? static_cast<uint32_t>(
                                          model_id)
                                    : 0;
                        }

                        ImGui::TextColored(
                            muted,
                            "Model ID matching is independent of the game language.");
                    }
                    else {
                        std::array<char, 256> name{};
                        strncpy_s(
                            name.data(),
                            name.size(),
                            rule.name.c_str(),
                            _TRUNCATE);

                        ImGui::SetNextItemWidth(300.0f);
                        if (ImGui::InputText(
                                "Item Name",
                                name.data(),
                                name.size())) {
                            rule.name = name.data();
                        }

                        ImGui::Checkbox(
                            "Case Sensitive",
                            &rule.case_sensitive);

                        ImGui::TextColored(
                            muted,
                            "Enter the item name as shown by your Guild Wars client.");
                    }

                    ImGui::Spacing();
                    ImGui::TextColored(
                        gold,
                        "Drop Ownership");
                    ImGui::Checkbox(
                        "My Drops",
                        &rule.only_my_drops);
                    ImGui::Checkbox(
                        "Other Players' Drops",
                        &rule.include_other_players_drops);

                    ImGui::TextColored(
                        muted,
                        "Unassigned drops (owner ID 0) are treated as Other Players' Drops.");

                    if (!rule.only_my_drops &&
                        !rule.include_other_players_drops) {
                        ImGui::TextColored(
                            ImVec4(0.92f, 0.34f, 0.34f, 1.0f),
                            "This rule cannot trigger because no ownership type is selected.");
                    }

                    const std::string wav_path =
                        WideToUtf8(
                            rule.wav_path.wstring());

                    ImGui::TextColored(
                        muted,
                        "Selected sound");
                    ImGui::TextWrapped(
                        "%s",
                        wav_path.empty()
                            ? "No WAV file selected."
                            : wav_path.c_str());

                    if (ImGui::Button(
                            "Browse...",
                            ImVec2(120.0f, 0.0f))) {
                        SelectWav(rule.wav_path);
                    }

                    ImGui::SameLine();

                    if (ImGui::Button(
                            "Test Sound",
                            ImVec2(120.0f, 0.0f))) {
                        PlayWav(rule.wav_path);
                    }

                    ImGui::SameLine();
                    ImGui::PushStyleColor(
                        ImGuiCol_Button,
                        ImVec4(
                            0.48f,
                            0.19f,
                            0.19f,
                            1.0f));

                    if (ImGui::Button(
                            "Remove",
                            ImVec2(100.0f, 0.0f))) {
                        remove_drop_index =
                            static_cast<int>(i);
                    }

                    ImGui::PopStyleColor();
                    ImGui::Unindent();
                }

                ImGui::PopID();
            }

            if (remove_drop_index >= 0 &&
                remove_drop_index <
                    static_cast<int>(
                        drop_rules_.size())) {
                drop_rules_.erase(
                    drop_rules_.begin() +
                    remove_drop_index);
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Locked Chests")) {
            ImGui::TextColored(gold, "Locked Chest Detection");
            ImGui::Separator();
            ImGui::Checkbox("Enable Locked Chest Sound", &nearby_gadget_alert_enabled_);
            ImGui::TextWrapped("Detects unopened locked chests with Gadget ID 8141 and plays the selected sound once while a chest remains nearby.");

            const std::string nearby_path = WideToUtf8(nearby_gadget_wav_.wstring());
            ImGui::TextColored(muted, "Selected sound");
            ImGui::TextWrapped("%s", nearby_path.empty() ? "No chest sound selected." : nearby_path.c_str());
            if (ImGui::Button("Browse...", ImVec2(120.0f, 0.0f))) SelectWav(nearby_gadget_wav_);
            ImGui::SameLine();
            if (ImGui::Button("Test Sound", ImVec2(120.0f, 0.0f))) PlayWav(nearby_gadget_wav_);

            ImGui::Spacing();
            ImGui::TextColored(gold, "Detection Settings");
            ImGui::Separator();
            ImGui::SetNextItemWidth(300.0f);
            ImGui::SliderFloat("Detection Range", &nearby_gadget_range_, 100.0f, 10000.0f, "%.0f units");
            nearby_gadget_range_ = std::clamp(nearby_gadget_range_, 100.0f, 10000.0f);
            ImGui::SetNextItemWidth(300.0f);
            ImGui::SliderInt("Scan Interval", &nearby_scan_interval_ms_, 100, 5000, "%d ms");
            nearby_scan_interval_ms_ = std::clamp(nearby_scan_interval_ms_, 100, 5000);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Settings")) {
            ImGui::TextColored(gold, "Global Audio");
            ImGui::Separator();

            ImGui::SetNextItemWidth(300.0f);
            ImGui::SliderInt(
                "Sound Volume",
                &sound_volume_percent_,
                0,
                100,
                "%d%%");
            sound_volume_percent_ =
                std::clamp(sound_volume_percent_, 0, 100);
            ImGui::TextColored(
                muted,
                "Controls the volume of this plugin only. Uses DirectShow playback rather than PlaySoundW.");

            ImGui::TextColored(
                muted,
                "For a clear test, compare 100%%, 25%% and 0%% using a Test Sound button.");

            ImGui::Spacing();
            ImGui::SetNextItemWidth(300.0f);
            ImGui::SliderInt(
                "Global Sound Cooldown",
                &cooldown_ms_,
                0,
                10000,
                "%d ms");
            cooldown_ms_ = std::clamp(cooldown_ms_, 0, 60000);
            ImGui::TextColored(
                muted,
                "Minimum delay between two plugin sounds.");

            ImGui::Spacing();
            if (ImGui::Button(
                    "Stop Current Sound",
                    ImVec2(180.0f, 0.0f))) {
                StopSound();
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Debug")) {
            ImGui::TextColored(gold, "Runtime Status");
            ImGui::Separator();
            ImGui::TextColored(scan_has_player_ ? green : red, "%s Player available", scan_has_player_ ? "[OK]" : "[!] ");
            ImGui::TextColored(scan_has_agent_array_ ? green : red, "%s Agent array available", scan_has_agent_array_ ? "[OK]" : "[!] ");
            ImGui::Text("Agents scanned: %zu", scanned_agent_count_);
            ImGui::Text("Agents within range: %zu", nearby_agent_debug_.size());

            ImGui::Spacing();
            ImGui::TextColored(gold, "Current Target");
            ImGui::Separator();
            ImGui::Text("Expected Gadget ID: %u", LockedChestGadgetId);
            ImGui::Text("Agent ID: %u", target_agent_id_);
            ImGui::Text("Type: 0x%X", target_type_);
            ImGui::Text("Gadget ID: %u", target_gadget_id_);
            ImGui::Text("Extra Type: %u", target_extra_type_);
            ImGui::TextColored(target_is_locked_chest_ ? green : muted, "Locked chest detected: %s", target_is_locked_chest_ ? "yes" : "no");

            ImGui::Spacing();
            ImGui::TextColored(gold, "Nearby Agents");
            ImGui::Separator();
            if (ImGui::BeginTable("NearbyAgentDebugTable", 10,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                    ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable, ImVec2(0.0f, 330.0f))) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Target");
                ImGui::TableSetupColumn("Agent ID");
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Gadget");
                ImGui::TableSetupColumn("Item");
                ImGui::TableSetupColumn("Living");
                ImGui::TableSetupColumn("Gadget ID");
                ImGui::TableSetupColumn("Extra Type");
                ImGui::TableSetupColumn("Distance");
                ImGui::TableSetupColumn("8141");
                ImGui::TableHeadersRow();

                for (const NearbyAgentDebug& entry : nearby_agent_debug_) {
                    ImGui::TableNextRow();
                    if (entry.matches_locked_chest_id)
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImVec4(0.18f, 0.42f, 0.22f, 0.55f)));
                    else if (entry.is_target)
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImVec4(0.20f, 0.35f, 0.55f, 0.50f)));

                    ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(entry.is_target ? "yes" : "");
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%u", entry.agent_id);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("0x%X", entry.type);
                    ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(entry.is_gadget ? "yes" : "no");
                    ImGui::TableSetColumnIndex(4); ImGui::TextUnformatted(entry.is_item ? "yes" : "no");
                    ImGui::TableSetColumnIndex(5); ImGui::TextUnformatted(entry.is_living ? "yes" : "no");
                    ImGui::TableSetColumnIndex(6); ImGui::Text("%u", entry.gadget_id);
                    ImGui::TableSetColumnIndex(7); ImGui::Text("%u", entry.extra_type);
                    ImGui::TableSetColumnIndex(8); ImGui::Text("%.0f", entry.distance);
                    ImGui::TableSetColumnIndex(9);
                    if (entry.matches_locked_chest_id) ImGui::TextColored(green, "yes");
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(3);
}

void ChatSoundsPlugin::LoadSettings(
    const wchar_t* folder)
{
    ToolboxPlugin::LoadSettings(folder);

    LoadSetting("enabled", enabled_);
    LoadSetting("whisper_enabled", whisper_enabled_);
    LoadSetting("cooldown_ms", cooldown_ms_);
    LoadSetting("sound_volume_percent", sound_volume_percent_);
    sound_volume_percent_ = std::clamp(sound_volume_percent_, 0, 100);
    LoadSetting("drop_alerts_enabled", drop_alerts_enabled_);
    LoadSetting("drop_detection_range", drop_detection_range_);
    drop_detection_range_ =
        std::clamp(drop_detection_range_, 100.0f, 10000.0f);

    LoadSetting("nearby_gadget_alert_enabled", nearby_gadget_alert_enabled_);
    LoadSetting("nearby_gadget_range", nearby_gadget_range_);
    LoadSetting("nearby_scan_interval_ms", nearby_scan_interval_ms_);

    std::string nearby_wav;
    LoadSetting("nearby_gadget_wav", nearby_wav);
    nearby_gadget_wav_ = Utf8ToWide(nearby_wav);


    std::string whisper;
    LoadSetting("whisper_wav", whisper);
    whisper_wav_ = Utf8ToWide(whisper);

    int drop_rule_count = 0;
    LoadSetting(
        "drop_rule_count",
        drop_rule_count);
    drop_rule_count =
        std::clamp(drop_rule_count, 0, 100);

    drop_rules_.clear();

    for (int i = 0; i < drop_rule_count; ++i) {
        DropRule rule;
        const std::string prefix =
            "drop_rule_" +
            std::to_string(i) +
            "_";

        LoadSetting(
            (prefix + "enabled").c_str(),
            rule.enabled);

        LoadSetting(
            (prefix + "rule_name").c_str(),
            rule.rule_name);

        int match_mode =
            static_cast<int>(rule.match_mode);
        LoadSetting(
            (prefix + "match_mode").c_str(),
            match_mode);
        rule.match_mode =
            static_cast<DropMatchMode>(
                std::clamp(match_mode, 0, 2));

        LoadSetting(
            (prefix + "name").c_str(),
            rule.name);

        int model_id = 0;
        LoadSetting(
            (prefix + "model_id").c_str(),
            model_id);
        rule.model_id =
            model_id > 0
                ? static_cast<uint32_t>(model_id)
                : 0;

        LoadSetting(
            (prefix + "case_sensitive").c_str(),
            rule.case_sensitive);

        LoadSetting(
            (prefix + "only_my_drops").c_str(),
            rule.only_my_drops);
        LoadSetting(
            (prefix + "include_other_players_drops").c_str(),
            rule.include_other_players_drops);

        std::string wav;
        LoadSetting(
            (prefix + "wav").c_str(),
            wav);
        rule.wav_path = Utf8ToWide(wav);

        drop_rules_.push_back(
            std::move(rule));
    }

    int count = 0;
    LoadSetting("rule_count", count);
    count = std::clamp(count, 0, 100);

    rules_.clear();

    for (int i = 0; i < count; ++i) {
        Rule rule;
        const std::string prefix =
            "rule_" + std::to_string(i) + "_";

        LoadSetting(
            (prefix + "enabled").c_str(),
            rule.enabled);

        int channel = static_cast<int>(rule.channel);
        LoadSetting(
            (prefix + "channel").c_str(),
            channel);
        rule.channel =
            static_cast<ChannelFilter>(
                std::clamp(channel, 0, 6));

        LoadSetting(
            (prefix + "keyword").c_str(),
            rule.keyword);

        LoadSetting(
            (prefix + "case_sensitive").c_str(),
            rule.case_sensitive);

        std::string wav;
        LoadSetting(
            (prefix + "wav").c_str(),
            wav);
        rule.wav_path = Utf8ToWide(wav);

        rules_.push_back(std::move(rule));
    }
}

void ChatSoundsPlugin::SaveSettings(
    const wchar_t* folder)
{
    SaveSetting("enabled", enabled_);
    SaveSetting(
        "whisper_enabled",
        whisper_enabled_);
    SaveSetting("cooldown_ms", cooldown_ms_);
    SaveSetting("sound_volume_percent", sound_volume_percent_);
    SaveSetting("drop_alerts_enabled", drop_alerts_enabled_);
    SaveSetting("drop_detection_range", drop_detection_range_);
    SaveSetting(
        "drop_rule_count",
        static_cast<int>(drop_rules_.size()));

    for (size_t i = 0; i < drop_rules_.size(); ++i) {
        const DropRule& rule =
            drop_rules_[i];
        const std::string prefix =
            "drop_rule_" +
            std::to_string(i) +
            "_";

        SaveSetting(
            (prefix + "enabled").c_str(),
            rule.enabled);
        SaveSetting(
            (prefix + "rule_name").c_str(),
            rule.rule_name);
        SaveSetting(
            (prefix + "match_mode").c_str(),
            static_cast<int>(
                rule.match_mode));
        SaveSetting(
            (prefix + "name").c_str(),
            rule.name);
        SaveSetting(
            (prefix + "model_id").c_str(),
            static_cast<int>(
                rule.model_id));
        SaveSetting(
            (prefix + "case_sensitive").c_str(),
            rule.case_sensitive);
        SaveSetting(
            (prefix + "only_my_drops").c_str(),
            rule.only_my_drops);
        SaveSetting(
            (prefix + "include_other_players_drops").c_str(),
            rule.include_other_players_drops);
        SaveSetting(
            (prefix + "wav").c_str(),
            WideToUtf8(
                rule.wav_path.wstring()));
    }

    SaveSetting("nearby_gadget_alert_enabled", nearby_gadget_alert_enabled_);
    SaveSetting("nearby_gadget_range", nearby_gadget_range_);
    SaveSetting("nearby_scan_interval_ms", nearby_scan_interval_ms_);
    SaveSetting(
        "nearby_gadget_wav",
        WideToUtf8(nearby_gadget_wav_.wstring()));
    SaveSetting(
        "whisper_wav",
        WideToUtf8(whisper_wav_.wstring()));
    SaveSetting(
        "rule_count",
        static_cast<int>(rules_.size()));

    for (size_t i = 0; i < rules_.size(); ++i) {
        const Rule& rule = rules_[i];
        const std::string prefix =
            "rule_" + std::to_string(i) + "_";

        SaveSetting(
            (prefix + "enabled").c_str(),
            rule.enabled);
        SaveSetting(
            (prefix + "channel").c_str(),
            static_cast<int>(rule.channel));
        SaveSetting(
            (prefix + "keyword").c_str(),
            rule.keyword);
        SaveSetting(
            (prefix + "case_sensitive").c_str(),
            rule.case_sensitive);
        SaveSetting(
            (prefix + "wav").c_str(),
            WideToUtf8(rule.wav_path.wstring()));
    }

    ToolboxPlugin::SaveSettings(folder);
}

std::string ChatSoundsPlugin::WideToUtf8(
    const std::wstring& value)
{
    if (value.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (size <= 0) {
        return {};
    }

    std::string result(
        static_cast<size_t>(size),
        '\0');

    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        result.data(),
        size,
        nullptr,
        nullptr);

    return result;
}

std::wstring ChatSoundsPlugin::Utf8ToWide(
    const std::string& value)
{
    if (value.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0);

    if (size <= 0) {
        return {};
    }

    std::wstring result(
        static_cast<size_t>(size),
        L'\0');

    MultiByteToWideChar(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        result.data(),
        size);

    return result;
}
