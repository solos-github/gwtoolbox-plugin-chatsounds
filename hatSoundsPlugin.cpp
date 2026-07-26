#include "ChatSoundsPlugin.h"

#include <GWCA/Managers/ChatMgr.h>#include <GWCA/Managers/UIMgr.h>

#include <Windows.h>#include <commdlg.h>#include <mmsystem.h>

#include <algorithm>#include <array>#include <cwctype>#include <ranges>

#pragma comment(lib, "winmm.lib")#pragma comment(lib, "comdlg32.lib")

namespace {ChatSoundsPlugin* plugin_instance = nullptr;

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
        plugin_instance->HandleChatPacket(packet->channel, packet->message);
        break;
    }
    case GW::UI::UIMessage::kPlayerChatMessage: {
        const auto packet =
            static_cast<GW::UI::UIPacket::kPlayerChatMessage*>(wparam);
        plugin_instance->HandleChatPacket(packet->channel, packet->message);
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

}

DLLAPI ToolboxPlugin* ToolboxPluginInstance(){static ChatSoundsPlugin instance;return &instance;}

void ChatSoundsPlugin::Initialize(ImGuiContext* ctx,const ImGuiAllocFns allocator_fns,const HMODULE toolbox_dll){ToolboxPlugin::Initialize(ctx, allocator_fns, toolbox_dll);plugin_instance = this;RegisterChatHooks();}

void ChatSoundsPlugin::SignalTerminate(){RemoveChatHooks();PlaySoundW(nullptr, nullptr, 0);plugin_instance = nullptr;ToolboxPlugin::SignalTerminate();}

void ChatSoundsPlugin::RegisterChatHooks(){// kPrintChatMessage erfasst eingehende System-/Whisper-/Chat-Ausgaben.RegisterUIMessageCallback(&chat_hook_,GW::UI::UIMessage::kPrintChatMessage,OnChatUiMessage);

// kPlayerChatMessage erfasst normale Spielerkanaele.
RegisterUIMessageCallback(
    &chat_hook_,
    GW::UI::UIMessage::kPlayerChatMessage,
    OnChatUiMessage);

}

void ChatSoundsPlugin::RemoveChatHooks(){GW::UI::RemoveUIMessageCallback(&chat_hook_);}

void ChatSoundsPlugin::HandleChatPacket(const uint32_t channel,const wchar_t* encoded_message){if (!enabled_ || !encoded_message || !*encoded_message) {return;}

const std::wstring readable = ExtractReadableText(encoded_message);
if (readable.empty()) {
    return;
}

if (whisper_enabled_ &&
    channel == static_cast<uint32_t>(
        GW::Chat::Channel::CHANNEL_WHISPER)) {
    PlayWav(whisper_wav_);
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

std::wstring ChatSoundsPlugin::ExtractReadableText(const wchar_t* encoded_message){if (!encoded_message || !*encoded_message) {return {};}

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

bool ChatSoundsPlugin::ContainsKeyword(const std::wstring& message,const std::string& keyword,const bool case_sensitive){if (keyword.empty()) {return false;}

std::wstring haystack = message;
std::wstring needle = Utf8ToWide(keyword);

if (!case_sensitive) {
    haystack = Lower(std::move(haystack));
    needle = Lower(std::move(needle));
}

return haystack.find(needle) != std::wstring::npos;

}

bool ChatSoundsPlugin::ChannelMatches(const ChannelFilter filter,const uint32_t channel){const auto gw_channel =static_castGW::Chat::Channel(channel);

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

bool ChatSoundsPlugin::CooldownElapsed() const{if (last_sound_.time_since_epoch().count() == 0) {return true;}

const auto elapsed =
    std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - last_sound_);

return elapsed.count() >= cooldown_ms_;

}

void ChatSoundsPlugin::PlayWav(const std::filesystem::path& path){if (path.empty() || !CooldownElapsed()) {return;}

std::error_code error;
if (!std::filesystem::is_regular_file(path, error)) {
    return;
}

if (PlaySoundW(
        path.c_str(),
        nullptr,
        SND_FILENAME | SND_ASYNC | SND_NODEFAULT) != FALSE) {
    last_sound_ = std::chrono::steady_clock::now();
}

}

bool ChatSoundsPlugin::SelectWav(std::filesystem::path& target){std::array<wchar_t, 4096> file{};

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
    L"WAV-Dateien (*.wav)\0*.wav\0Alle Dateien (*.*)\0*.*\0";
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

void ChatSoundsPlugin::DrawSettings(){if (!toolbox_handle) {return;}

ImGui::Checkbox("Plugin aktivieren", &enabled_);
ImGui::Separator();

ImGui::TextUnformatted("Whisper-Alarm");
ImGui::Checkbox(
    "Sound bei eingehendem Whisper",
    &whisper_enabled_);

const std::string whisper_path =
    WideToUtf8(whisper_wav_.wstring());

ImGui::TextWrapped(
    "%s",
    whisper_path.empty()
        ? "Keine WAV-Datei ausgewaehlt."
        : whisper_path.c_str());

if (ImGui::Button("Whisper-WAV auswaehlen...")) {
    SelectWav(whisper_wav_);
}

ImGui::SameLine();

if (ImGui::Button("Whisper testen")) {
    PlayWav(whisper_wav_);
}

ImGui::Separator();
ImGui::TextUnformatted("Schlagwortalarme");

if (ImGui::Button("+ Schlagwortalarm hinzufuegen")) {
    rules_.push_back({});
}

if (rules_.empty()) {
    ImGui::TextDisabled(
        "Noch keine Schlagwortalarme angelegt.");
}

int remove_index = -1;

for (size_t i = 0; i < rules_.size(); ++i) {
    Rule& rule = rules_[i];

    ImGui::PushID(static_cast<int>(i));
    ImGui::Separator();

    ImGui::Checkbox("Aktiv", &rule.enabled);

    int channel = static_cast<int>(rule.channel);
    const char* channels[] = {
        "Alle Kanaele",
        "Lokaler Chat",
        "Gildenchat",
        "Allianzchat",
        "Gruppenchat",
        "Handelschat",
        "Whisper"
    };

    ImGui::SetNextItemWidth(300.0f);
    if (ImGui::Combo(
            "Kanal",
            &channel,
            channels,
            IM_ARRAYSIZE(channels))) {
        rule.channel =
            static_cast<ChannelFilter>(channel);
    }

    std::array<char, 256> keyword{};
    strncpy_s(
        keyword.data(),
        keyword.size(),
        rule.keyword.c_str(),
        _TRUNCATE);

    ImGui::SetNextItemWidth(300.0f);
    if (ImGui::InputText(
            "Schlagwort",
            keyword.data(),
            keyword.size())) {
        rule.keyword = keyword.data();
    }

    ImGui::Checkbox(
        "Gross-/Kleinschreibung beachten",
        &rule.case_sensitive);

    const std::string wav_path =
        WideToUtf8(rule.wav_path.wstring());

    ImGui::TextWrapped(
        "%s",
        wav_path.empty()
            ? "Keine WAV-Datei ausgewaehlt."
            : wav_path.c_str());

    if (ImGui::Button("WAV auswaehlen...")) {
        SelectWav(rule.wav_path);
    }

    ImGui::SameLine();

    if (ImGui::Button("Testen")) {
        PlayWav(rule.wav_path);
    }

    ImGui::SameLine();

    if (ImGui::Button("Regel entfernen")) {
        remove_index = static_cast<int>(i);
    }

    ImGui::PopID();
}

if (remove_index >= 0 &&
    remove_index < static_cast<int>(rules_.size())) {
    rules_.erase(rules_.begin() + remove_index);
}

ImGui::Separator();

ImGui::SetNextItemWidth(160.0f);
ImGui::InputInt(
    "Globaler Sound-Cooldown (ms)",
    &cooldown_ms_);

cooldown_ms_ =
    std::clamp(cooldown_ms_, 0, 60000);

if (ImGui::Button("Aktuellen Sound stoppen")) {
    PlaySoundW(nullptr, nullptr, 0);
}

}

void ChatSoundsPlugin::LoadSettings(const wchar_t* folder){ToolboxPlugin::LoadSettings(folder);

LoadSetting("enabled", enabled_);
LoadSetting("whisper_enabled", whisper_enabled_);
LoadSetting("cooldown_ms", cooldown_ms_);

std::string whisper;
LoadSetting("whisper_wav", whisper);
whisper_wav_ = Utf8ToWide(whisper);

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

void ChatSoundsPlugin::SaveSettings(const wchar_t* folder){SaveSetting("enabled", enabled_);SaveSetting("whisper_enabled",whisper_enabled_);SaveSetting("cooldown_ms", cooldown_ms_);SaveSetting("whisper_wav",WideToUtf8(whisper_wav_.wstring()));SaveSetting("rule_count",static_cast<int>(rules_.size()));

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

std::string ChatSoundsPlugin::WideToUtf8(const std::wstring& value){if (value.empty()) {return {};}

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

std::wstring ChatSoundsPlugin::Utf8ToWide(const std::string& value){if (value.empty()) {return {};}

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
