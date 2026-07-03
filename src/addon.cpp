#include "shared.hpp"
#include "pact_supply_data.hpp"
#include "resource.h"

#include <Windows.h>
#include <imgui.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <ctime>
#include <random>
#include <string>

namespace addon
{
    AddonAPI_t* Api = nullptr;

    void Log(ELogLevel level, const char* message)
    {
        if (Api == nullptr || Api->Log == nullptr)
        {
            return;
        }

        Api->Log(level, LogChannel, message);
    }
}

namespace
{
    constexpr uint32_t AddonSignature = static_cast<uint32_t>(-17030703);
    constexpr float NotificationSeconds = 2.0f;

    bool IsVisible = true;
    ImVec2 ButtonPosition(300.0f, 300.0f);
    bool IsDraggingBarrel = false;

    float NotificationTimer = 0.0f;
    std::string NotificationText;

    float DelayRemaining = 0.0f;
    AddonDefinition_t AddonDef{};
    Texture_t* BarrelTexture = nullptr;
    Texture_t* BarrelFilledTexture = nullptr;
    float TextureRetryTimer = 0.0f;

    constexpr const char* BarrelTextureId = "PactSupply.Barrel";
    constexpr const char* BarrelFilledTextureId = "PactSupply.BarrelFilled";
    constexpr const char* SettingsDirectoryName = "PactSupplyTracker";
    constexpr const char* SettingsFileName = "settings.ini";

    void ConfigureImGui(AddonAPI_t* api);

    HMODULE GetAddonModule()
    {
        HMODULE module = nullptr;
        if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&GetAddonModule),
            &module))
        {
            return nullptr;
        }

        return module;
    }

    std::filesystem::path GetSettingsPath()
    {
        if (addon::Api == nullptr || addon::Api->Paths_GetAddonDirectory == nullptr)
        {
            return {};
        }

        const char* directory = addon::Api->Paths_GetAddonDirectory(SettingsDirectoryName);
        if (directory == nullptr || directory[0] == '\0')
        {
            return {};
        }

        return std::filesystem::path(directory) / SettingsFileName;
    }

    void LoadSettings()
    {
        const std::filesystem::path path = GetSettingsPath();
        if (path.empty())
        {
            addon::Log(LOGL_WARNING, "settings path unavailable");
            return;
        }

        std::ifstream file(path);
        if (!file.is_open())
        {
            return;
        }

        float x = ButtonPosition.x;
        float y = ButtonPosition.y;
        file >> x >> y;

        if (file.good() || file.eof())
        {
            ButtonPosition = ImVec2(x, y);
        }
    }

    void SaveSettings()
    {
        const std::filesystem::path path = GetSettingsPath();
        if (path.empty())
        {
            addon::Log(LOGL_WARNING, "settings path unavailable");
            return;
        }

        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
        {
            addon::Log(LOGL_WARNING, "failed to create settings directory");
            return;
        }

        std::ofstream file(path, std::ios::trunc);
        if (!file.is_open())
        {
            addon::Log(LOGL_WARNING, "failed to open settings file");
            return;
        }

        file << ButtonPosition.x << ' ' << ButtonPosition.y << '\n';
    }
    void LoadTextures(bool logFailures = true)
    {
        if (addon::Api == nullptr || addon::Api->Textures_GetOrCreateFromResource == nullptr)
        {
            addon::Log(LOGL_WARNING, "texture api unavailable");
            return;
        }

        HMODULE module = GetAddonModule();
        if (module == nullptr)
        {
            addon::Log(LOGL_WARNING, "addon module unavailable for texture resources");
            return;
        }

        BarrelTexture = addon::Api->Textures_GetOrCreateFromResource(BarrelTextureId, IDB_BARREL, module);
        BarrelFilledTexture = addon::Api->Textures_GetOrCreateFromResource(BarrelFilledTextureId, IDB_BARREL_HOVER, module);

        if (logFailures && BarrelTexture == nullptr)
        {
            addon::Log(LOGL_WARNING, "failed to load embedded barrel texture");
        }

        if (logFailures && BarrelFilledTexture == nullptr)
        {
            addon::Log(LOGL_WARNING, "failed to load embedded barrel hover texture");
        }
    }

    void ShowNotification(const std::string& text, float seconds = NotificationSeconds)
    {
        NotificationText = text;
        NotificationTimer = seconds;
    }

    bool CopyToClipboard(const std::string& text)
    {
        if (!OpenClipboard(nullptr))
        {
            addon::Log(LOGL_WARNING, "failed to open clipboard");
            return false;
        }

        EmptyClipboard();

        HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (handle == nullptr)
        {
            CloseClipboard();
            addon::Log(LOGL_WARNING, "failed to allocate clipboard memory");
            return false;
        }

        char* buffer = static_cast<char*>(GlobalLock(handle));
        if (buffer == nullptr)
        {
            GlobalFree(handle);
            CloseClipboard();
            addon::Log(LOGL_WARNING, "failed to lock clipboard memory");
            return false;
        }

        std::memcpy(buffer, text.c_str(), text.size() + 1);
        GlobalUnlock(handle);

        if (SetClipboardData(CF_TEXT, handle) == nullptr)
        {
            GlobalFree(handle);
            CloseClipboard();
            addon::Log(LOGL_WARNING, "failed to set clipboard data");
            return false;
        }

        CloseClipboard();
        return true;
    }

    int RandomDelaySeconds()
    {
        static std::random_device device;
        static std::mt19937 generator(device());
        static std::uniform_int_distribution<int> distribution(20, 25);
        return distribution(generator);
    }

    void StartRandomDelay()
    {
        const int seconds = RandomDelaySeconds();
        DelayRemaining = static_cast<float>(seconds);

        std::string message = "locked for " + std::to_string(seconds) + "s";
        addon::Log(LOGL_INFO, message.c_str());
    }

    void CopyCurrentDayWaypoints()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t currentTime = std::chrono::system_clock::to_time_t(now);

        // Pact Supply reset: 05:00 Brasilia time == 08:00 UTC.
        currentTime -= 8 * 60 * 60;

        std::tm utcTime{};
        gmtime_s(&utcTime, &currentTime);

        const std::string* message = nullptr;
        const std::string* notification = nullptr;

        switch (utcTime.tm_wday)
        {
        case 0:
            message = &MESSAGE::SUNDAY;
            notification = &DAY::SUNDAY;
            break;
        case 1:
            message = &MESSAGE::MONDAY;
            notification = &DAY::MONDAY;
            break;
        case 2:
            message = &MESSAGE::TUESDAY;
            notification = &DAY::TUESDAY;
            break;
        case 3:
            message = &MESSAGE::WEDNESDAY;
            notification = &DAY::WEDNESDAY;
            break;
        case 4:
            message = &MESSAGE::THURSDAY;
            notification = &DAY::THURSDAY;
            break;
        case 5:
            message = &MESSAGE::FRIDAY;
            notification = &DAY::FRIDAY;
            break;
        case 6:
            message = &MESSAGE::SATURDAY;
            notification = &DAY::SATURDAY;
            break;
        default:
            break;
        }

        if (message == nullptr || notification == nullptr)
        {
            ShowNotification("Pact Supply day not found");
            addon::Log(LOGL_WARNING, "weekday lookup failed");
            return;
        }

        if (CopyToClipboard(*message))
        {
            ShowNotification(*notification);
            StartRandomDelay();
        }
        else
        {
            ShowNotification("Could not copy Pact Supply waypoints");
        }
    }

    void DrawBarrel(ImDrawList* drawList, ImVec2 topLeft, ImVec2 size, ImU32 accentColor)
    {
        const float width = size.x;
        const float height = size.y;
        const float x = topLeft.x;
        const float y = topLeft.y;

        const ImU32 shadow = IM_COL32(0, 0, 0, 95);
        const ImU32 woodDark = IM_COL32(82, 45, 20, 255);
        const ImU32 woodMid = IM_COL32(132, 76, 32, 255);
        const ImU32 woodLight = IM_COL32(174, 104, 43, 255);
        const ImU32 bandDark = IM_COL32(68, 58, 70, 255);
        const ImU32 bandLight = IM_COL32(142, 126, 150, 255);
        const ImU32 plankLine = IM_COL32(63, 34, 15, 185);

        const ImVec2 bodyMin(x + width * 0.16f, y + height * 0.08f);
        const ImVec2 bodyMax(x + width * 0.84f, y + height * 0.92f);
        const float rounding = width * 0.20f;

        drawList->AddRectFilled(ImVec2(bodyMin.x + 1.5f, bodyMin.y + 2.0f), ImVec2(bodyMax.x + 1.5f, bodyMax.y + 2.0f), shadow, rounding);

        drawList->AddRectFilled(bodyMin, bodyMax, woodMid, rounding);
        drawList->AddRectFilled(ImVec2(bodyMin.x + 2.0f, bodyMin.y + 2.0f), ImVec2(bodyMax.x - 2.0f, bodyMax.y - 2.0f), woodLight, rounding * 0.65f);

        drawList->AddQuadFilled(
            ImVec2(x + width * 0.28f, y + height * 0.01f),
            ImVec2(x + width * 0.72f, y + height * 0.01f),
            ImVec2(x + width * 0.88f, y + height * 0.14f),
            ImVec2(x + width * 0.12f, y + height * 0.14f),
            woodDark);
        drawList->AddQuadFilled(
            ImVec2(x + width * 0.12f, y + height * 0.86f),
            ImVec2(x + width * 0.88f, y + height * 0.86f),
            ImVec2(x + width * 0.72f, y + height * 0.99f),
            ImVec2(x + width * 0.28f, y + height * 0.99f),
            woodDark);

        const float bandHeight = height * 0.095f;
        const float bandInset = width * 0.07f;
        const float bandTop = y + height * 0.24f;
        const float bandMid = y + height * 0.47f;
        const float bandBottom = y + height * 0.70f;

        auto drawBand = [&](float bandY)
        {
            const ImVec2 min(x + bandInset, bandY);
            const ImVec2 max(x + width - bandInset, bandY + bandHeight);
            drawList->AddRectFilled(min, max, bandDark, 1.5f);
            drawList->AddLine(ImVec2(min.x + 1.0f, min.y + 1.0f), ImVec2(max.x - 1.0f, min.y + 1.0f), bandLight, 1.0f);
            drawList->AddLine(ImVec2(min.x + 1.0f, max.y - 1.0f), ImVec2(max.x - 1.0f, max.y - 1.0f), IM_COL32(34, 28, 38, 180), 1.0f);
        };

        drawBand(bandTop);
        drawBand(bandMid);
        drawBand(bandBottom);

        drawList->AddLine(ImVec2(x + width * 0.41f, y + height * 0.13f), ImVec2(x + width * 0.36f, y + height * 0.87f), plankLine, 1.0f);
        drawList->AddLine(ImVec2(x + width * 0.59f, y + height * 0.13f), ImVec2(x + width * 0.64f, y + height * 0.87f), plankLine, 1.0f);

        const ImVec2 topLeftBarrel(x + width * 0.25f, y + height * 0.02f);
        const ImVec2 topRightBarrel(x + width * 0.75f, y + height * 0.02f);
        const ImVec2 middleLeft(x + width * 0.08f, y + height * 0.50f);
        const ImVec2 middleRight(x + width * 0.92f, y + height * 0.50f);
        const ImVec2 bottomLeft(x + width * 0.25f, y + height * 0.98f);
        const ImVec2 bottomRight(x + width * 0.75f, y + height * 0.98f);

        drawList->AddBezierCubic(topLeftBarrel, ImVec2(x + width * 0.04f, y + height * 0.20f), ImVec2(x + width * 0.02f, y + height * 0.34f), middleLeft, accentColor, 1.8f);
        drawList->AddBezierCubic(middleLeft, ImVec2(x + width * 0.02f, y + height * 0.66f), ImVec2(x + width * 0.04f, y + height * 0.80f), bottomLeft, accentColor, 1.8f);
        drawList->AddBezierCubic(topRightBarrel, ImVec2(x + width * 0.96f, y + height * 0.20f), ImVec2(x + width * 0.98f, y + height * 0.34f), middleRight, accentColor, 1.8f);
        drawList->AddBezierCubic(middleRight, ImVec2(x + width * 0.98f, y + height * 0.66f), ImVec2(x + width * 0.96f, y + height * 0.80f), bottomRight, accentColor, 1.8f);
        drawList->AddLine(topLeftBarrel, topRightBarrel, accentColor, 1.8f);
        drawList->AddLine(bottomLeft, bottomRight, accentColor, 1.8f);
    }
    void RenderNotification()
    {
        if (NotificationTimer <= 0.0f || NotificationText.empty())
        {
            return;
        }

        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(displaySize);

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoBackground;

        if (ImGui::Begin("##PactSupplyNotification", nullptr, flags))
        {
            ImGui::SetWindowFontScale(1.6f);

            const char* text = NotificationText.c_str();
            const ImVec2 textSize = ImGui::CalcTextSize(text);
            const float x = (displaySize.x * 0.5f) - (textSize.x * 0.5f);
            const float y = (displaySize.y * 0.25f) - (textSize.y * 0.5f);

            ImGui::SetCursorPos(ImVec2(x, y));
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", text);

            ImGui::SetWindowFontScale(1.0f);
        }

        ImGui::End();
    }

    void RenderBarrelButton()
    {
        if (!IsVisible)
        {
            return;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBackground;

        ImGui::SetNextWindowPos(ButtonPosition);

        if (ImGui::Begin("##PactSupplyBarrel", &IsVisible, flags))
        {
            const ImVec2 barrelSize(34.0f, 42.0f);
            const ImVec2 screenPosition = ImGui::GetCursorScreenPos();
            const bool filled = DelayRemaining > 0.0f || IsDraggingBarrel;
            Texture_t* texture = filled && BarrelFilledTexture != nullptr ? BarrelFilledTexture : BarrelTexture;

            ImGui::InvisibleButton("##PactSupplyBarrelButton", barrelSize);
            const bool hovered = ImGui::IsItemHovered();

            if (texture != nullptr && texture->Resource != nullptr)
            {
                ImGui::GetWindowDrawList()->AddImage(
                    texture->Resource,
                    screenPosition,
                    ImVec2(screenPosition.x + barrelSize.x, screenPosition.y + barrelSize.y),
                    ImVec2(0.0f, 0.0f),
                    ImVec2(1.0f, 1.0f),
                    IM_COL32_WHITE);
            }
            else
            {
                DrawBarrel(ImGui::GetWindowDrawList(), screenPosition, barrelSize, IM_COL32(202, 77, 213, 255));
            }
            const bool altDown = ImGui::GetIO().KeyAlt;

            if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                if (altDown)
                {
                    IsDraggingBarrel = true;
                }
                else if (DelayRemaining > 0.0f)
                {
                    ShowNotification("Locked. Wait delay", NotificationSeconds);
                }
                else
                {
                    CopyCurrentDayWaypoints();
                }
            }

            if (IsDraggingBarrel)
            {
                if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                {
                    const ImVec2 delta = ImGui::GetIO().MouseDelta;
                    ButtonPosition.x += delta.x;
                    ButtonPosition.y += delta.y;
                }
                else
                {
                    IsDraggingBarrel = false;

                    SaveSettings();
                }
            }


        }

        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void RenderInterface()
    {
        ConfigureImGui(addon::Api);
        const float deltaTime = ImGui::GetIO().DeltaTime;

        if (NotificationTimer > 0.0f)
        {
            NotificationTimer -= deltaTime;
        }

        if (DelayRemaining > 0.0f)
        {
            DelayRemaining -= deltaTime;
            if (DelayRemaining < 0.0f)
            {
                DelayRemaining = 0.0f;
            }
        }

        if (BarrelTexture == nullptr || BarrelFilledTexture == nullptr)
        {
            TextureRetryTimer -= deltaTime;
            if (TextureRetryTimer <= 0.0f)
            {
                LoadTextures(false);
                TextureRetryTimer = 0.5f;
            }
        }

        RenderBarrelButton();
        RenderNotification();
    }

    void ConfigureImGui(AddonAPI_t* api)
    {
        if (api == nullptr || api->ImguiContext == nullptr)
        {
            addon::Log(LOGL_WARNING, "ImguiContext unavailable");
            return;
        }

        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(api->ImguiContext));
    }

    void AddonLoad(AddonAPI_t* api)
    {
        addon::Api = api;
        addon::Log(LOGL_INFO, "load begin");
        ConfigureImGui(api);
        addon::Log(LOGL_INFO, "imgui configured");
        LoadSettings();
        LoadTextures();

        if (addon::Api != nullptr && addon::Api->GUI_Register != nullptr)
        {
            addon::Api->GUI_Register(RT_Render, RenderInterface);
            addon::Log(LOGL_INFO, "render registered");
        }

        addon::Log(LOGL_INFO, "loaded");
    }

    void AddonUnload()
    {
        if (addon::Api != nullptr && addon::Api->GUI_Deregister != nullptr)
        {
            addon::Api->GUI_Deregister(RenderInterface);
        }

        NotificationTimer = 0.0f;
        DelayRemaining = 0.0f;
        IsDraggingBarrel = false;
        BarrelTexture = nullptr;
        BarrelFilledTexture = nullptr;

        addon::Log(LOGL_INFO, "unloaded");
        addon::Api = nullptr;
    }
}

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef()
{
    AddonDef.Signature = AddonSignature;
    AddonDef.APIVersion = NEXUS_API_VERSION;
    AddonDef.Name = addon::Name;
    AddonDef.Version = AddonVersion_t{1, 2, 2, 0};
    AddonDef.Author = "Nahar.5349";
    AddonDef.Description = "Track the Pact Supply waypoints";
    AddonDef.Load = AddonLoad;
    AddonDef.Unload = AddonUnload;
    AddonDef.Flags = AF_None;
    AddonDef.Provider = UP_GitHub;
    AddonDef.UpdateLink = "https://github.com/gustavotoigo-max/pact-supply-tracker";

    return &AddonDef;
}

