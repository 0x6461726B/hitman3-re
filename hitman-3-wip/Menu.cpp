#include "Menu.h"
#include "Logger.h"
#include "Settings.h"
#include "Vector.h"
#include "ZActorManager.h"
#include "sdk_keys.h"
#include "sdk_types.h"
#include "utils.h"

#include <string>


void DisplayHexWithCopy(const char *label, uintptr_t value) {
    ImGui::Text("%s: 0x%llx", label, static_cast<unsigned long long>(value));

    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        char buffer[32];
        int written = std::snprintf(buffer, sizeof(buffer), "0x%llx", static_cast<unsigned long long>(value));
        if (written > 0 && written < static_cast<int>(sizeof(buffer))) {
            ImGui::SetClipboardText(buffer);
        } else {
            Logger::instance().error("Failed to format hex string for %s", label);
        }
    }
}
Menu::Menu() : show(false), isHover(false) {}

Menu::~Menu() {}

auto infiniteAmmo = false;
int ammo = 99999;

struct mat4 {
    float m[4][4];
};

void Menu::Render() {
    handleKeyInputs();
    if (!show)
        return;

    ImGui::SetNextWindowSize(ImVec2(600, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Base")) {
        ImGui::End();
        return;
    }

    auto ZHM5MainCamera_ = callFunction<uintptr_t>(MAKE_RVA(0x13F5170)); // E8 ? ? ? ? 4C 8B F0 48 85 C0 0F 84 ? ? ? ? 48 8B 4B
    DisplayHexWithCopy("ZHM5MainCamera_", ZHM5MainCamera_);

    auto ZNetPlayerControllerPtr = reinterpret_cast<uintptr_t *>(MAKE_RVA(0x30C5C10)); // 48 8B 0D ? ? ? ? 48 8B 35 ? ? ? ? 48 89 BC 24 ? ? ? ? 48 8B D9
    uintptr_t ZNetPlayerController = ZNetPlayerControllerPtr ? *ZNetPlayerControllerPtr : 0;
    DisplayHexWithCopy("ZNetPlayerController", ZNetPlayerController);

    uintptr_t idkYet = 0;
    if (ZNetPlayerController)
        idkYet = *reinterpret_cast<uintptr_t *>(ZNetPlayerController + 0xA8);
    DisplayHexWithCopy("idkYet", idkYet);

    uintptr_t ZHitman5_ = idkYet ? getComponent(idkYet, (RegistryKeys::ZHitman5)) : 0;
    DisplayHexWithCopy("ZHitman5_", ZHitman5_);

    auto ICharacterTransformStateHitman5 = ZHitman5_ ? (uintptr_t)(ZHitman5_ + 0x20) : 0;
    DisplayHexWithCopy("Maybe Position Info", ICharacterTransformStateHitman5);

    auto ZActorManagerAddr = MAKE_RVA(0x3036AA0); // 4C 8D 15 ? ? ? ? 4C 8B 0D ? ? ? ? 49 8B C0 48 C1 E8 ? A8 ? 74 ? 41 0F B6 C0 49 8D 8A
    auto ZActorManager_ = reinterpret_cast<ZActorManager *>(ZActorManagerAddr);

   ImGui::Checkbox("Draw", &g_settings.draw);
    
    if (ImGui::BeginTabBar("##Tabs")) {
        if (ImGui::BeginTabItem("general")) {
            ImGui::Text("is hovering window: %i", isHover);

            if (ICharacterTransformStateHitman5) {
                auto pos = *(Vector *)(ICharacterTransformStateHitman5 + 0x1A0);
                ImGui::Text("Position: (%f, %f, %f)", pos.X, pos.Y, pos.Z);

                if (ZHM5MainCamera_) {
                    auto first = *(Vector *)(ZHM5MainCamera_ + 0x20);
                    auto second = *(Vector *)(ZHM5MainCamera_ + 0x2C);
                    auto third = *(Vector *)(ZHM5MainCamera_ + 0x38);
                    auto cameraPos = *(Vector *)(ZHM5MainCamera_ + 0x44);


                    mat4 trans_matrix{};
                    trans_matrix.m[0][0] = first.X;
                    trans_matrix.m[0][1] = second.X;
                    trans_matrix.m[0][2] = third.X;
                    trans_matrix.m[0][3] = 0.0f;

                    trans_matrix.m[1][0] = first.Y;
                    trans_matrix.m[1][1] = second.Y;
                    trans_matrix.m[1][2] = third.Y;
                    trans_matrix.m[1][3] = 0.0f;

                    trans_matrix.m[2][0] = first.Z;
                    trans_matrix.m[2][1] = second.Z;
                    trans_matrix.m[2][2] = third.Z;
                    trans_matrix.m[2][3] = 0.0f;

                    trans_matrix.m[3][0] = -cameraPos.dot(first);
                    trans_matrix.m[3][1] = -cameraPos.dot(second);
                    trans_matrix.m[3][2] = -cameraPos.dot(third);
                    trans_matrix.m[3][3] = 1.0f;

                    for (int i = 0; i < 4; i++) {
                        ImGui::Text("Matrix Row %i: %f, %f, %f, %f", i, trans_matrix.m[i][0], trans_matrix.m[i][1],
                                    trans_matrix.m[i][2], trans_matrix.m[i][3]);
                    }

                    Vector out{0, 0, 0, 0};
                    bool isOffScreen = false;
                    float fadeStuff = 0.0f;
                    auto worldToScreen = MAKE_RVA(0x33D170); // E8 ? ? ? ? 0F 10 00 0F 11 43 ? 48 8B 3D

                    callFunction<void *>(worldToScreen, nullptr, &out, ZHM5MainCamera_, trans_matrix, pos, &isOffScreen,
                                         &fadeStuff, false);

                    ImGui::Text("Is off screen: %i", isOffScreen);
                    ImGui::Text("Fade Stuff: %f", fadeStuff);
                    ImGui::Text("WorldToScreen: (%f, %f, %f, %f)", out.X + 1980 / 2.0f, out.Y + 1080 / 2.0f, out.Z,
                                out.W);
                }
            }

            if (ZHitman5_) {
                auto HPStuff = *(uintptr_t *)(ZHitman5_ + 0xE70);
                auto testDecryptHP = (HPStuff + 0x228);
                auto testDecryptMaxHP = (HPStuff + 0x238);

                auto hpField = EncryptedField<float>(testDecryptHP);

                ImGui::Text("HP Decrypted: %f", (float)hpField);


                auto maxHpField = EncryptedField<float>(testDecryptMaxHP);
                ImGui::Text("HP2 (Max): %f", (float)maxHpField);

                // Calculations using casts
                float hpVal = (float)hpField;
                float maxHpVal = (float)maxHpField;

                if (maxHpVal != 0.0f) {
                    ImGui::Text("HP / HP2: %f", hpVal / maxHpVal);
                    ImGui::Text("HP 3: %f", 1.0f / maxHpVal);
                    ImGui::Text("HP 4: %f", hpVal * (1.0f / maxHpVal));
                }
            }

            if (ZActorManager_) {
                auto actorCount = ZActorManager_->getSize();
                ImGui::Text("Actor Count: %i", actorCount);

                for (int i = 0; i < actorCount; i++) {
                    ImGui::Separator();
                    auto actor = ZActorManager_->getActor(i);
                    DisplayHexWithCopy("Actor", actor);
                    if (!actor)
                        continue;

                    uintptr_t transformState = actor + 0x20;
                    auto actorPos = *(Vector *)(transformState + 0x1A0);
                    ImGui::Text("Position: (%f, %f, %f)", actorPos.X, actorPos.Y, actorPos.Z);

                    auto nameLength = *reinterpret_cast<int *>(actor + 0x488);
                    if (nameLength > 0) {
                        char *name = *reinterpret_cast<char **>(actor + 0x490);
                        ImGui::Text("Name: %s", name);
                    }

                    auto isTarget = *reinterpret_cast<bool *>(actor + 0x115A);
                    ImGui::Text("Is Target: %i", isTarget);

                    auto moveControllerPtr = *(uintptr_t *)(actor + 0x10A8);
                    if (moveControllerPtr) {
                        auto something = *(uintptr_t *)(moveControllerPtr + 0x50);
                        auto isMoving = callFunction<bool>(MAKE_RVA(0x3B4BC0), something); // E8 ? ? ? ? 84 C0 75 ? 80 4E
                        ImGui::Text("Is Moving: %i", isMoving);
                    }
                }
                ImGui::Separator();
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("weapons")) {
            if (ZHitman5_) {
                auto InventoryMaybe = *reinterpret_cast<uintptr_t *>(*(uintptr_t *)(ZHitman5_ + 0xF18) + 0xB8);
                uintptr_t ZCharacterSubcontrollerInventory =
                    getComponent(InventoryMaybe, RegistryKeys::ZCharacterSubcontrollerInventory);

                DisplayHexWithCopy("ZCharacterSubcontrollerInventory", ZCharacterSubcontrollerInventory);

                if (ZCharacterSubcontrollerInventory) {
                    auto currentItem = *(int64_t*)(ZCharacterSubcontrollerInventory + 0xA8);
                    DisplayHexWithCopy("Current Item", currentItem);

                    auto IItem = getComponent(currentItem, RegistryKeys::IItem);
                    if (IItem)
                        DisplayHexWithCopy("IItem", IItem);

                    auto IItemAmmo = getComponent(IItem, RegistryKeys::IItemAmmo);
                    if (IItemAmmo)
                        DisplayHexWithCopy("IItemAmmo", IItemAmmo);

                    auto idkForItem = *(int64_t*)(ZCharacterSubcontrollerInventory + 0xB0);

                    if (idkForItem) {
                        auto iFireArm = getComponent(currentItem, RegistryKeys::IFirearm);
                        DisplayHexWithCopy("IFireArm", iFireArm);

                        auto ZHM5Item = getComponent(currentItem, RegistryKeys::ZHM5Item);
                        DisplayHexWithCopy("ZHM5Item", ZHM5Item);

                        auto ZHM5ItemWeapon = getComponent(currentItem, RegistryKeys::ZHM5ItemWeapon);
                        if (ZHM5ItemWeapon)
                            DisplayHexWithCopy("ZHM5ItemWeapon", ZHM5ItemWeapon);

                        ImGui::Checkbox("Infinite Ammo", &infiniteAmmo);

                        if (iFireArm) {
                            auto slot = callFunction<int>(MAKE_RVA(0x48CC50), iFireArm); // 48 8D 91 ? ? ? ? 48 8B 42 ? 48 C1 E8 ? A8 ? 75 ? 48 8B 12 8B 89 ? ? ? ? 48 8D 04 C9 48 03 C0 8B 44 C2 ? C3 CC CC CC CC CC CC CC CC 48 89 5C 24
                            ImGui::Text("Slot %i", slot);

                            auto encryptedData = (uintptr_t)(ZCharacterSubcontrollerInventory + (0x10 * slot) + 0x188) + 8;

                            auto ammoField = EncryptedField<int>(encryptedData);
                            ImGui::Text("Ammo in inventory (not clip) %i", (int)ammoField);

                            ImGui::InputInt("Ammo", &ammo);
                            if (infiniteAmmo) {
                                ammoField.set(ammo); 
                            }
                        }
                    } else {
                        ImGui::Text("No ammo for current item.");
                    }

                    auto test1 = *(uintptr_t *)(ZCharacterSubcontrollerInventory + 0x280);
                    DisplayHexWithCopy("test1", test1);
                } else {
                    ImGui::Text("Inventory not available.");
                }
            } else {
                ImGui::Text("ZHitman5 not available.");
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void Menu::initStyles() {
    ImGuiStyle &style = ImGui::GetStyle();
    ImVec4 *colors = style.Colors;
    ImGuiIO &io = ImGui::GetIO();
    (void)io;

    io.FontDefault = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 14);

    // Window background
    colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);

    // Headers
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

    // Buttons
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

    // Frame Background
    colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_TabDimmed] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);

    // Text
    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

    // Window shadows
    style.WindowRounding = 0.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameRounding = 0.0f;

    // Frame shadows
    style.ChildRounding = 0.0f;
    style.FrameBorderSize = 0.0f;
}

void Menu::beginNewFrame() {

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void Menu::endFrame() { ImGui::EndFrame(); }

void Menu::handleKeyInputs() {
    if (GetAsyncKeyState(VK_HOME) & 1)
        Menu::show = !Menu::show;


    isHover = ImGui::GetIO().WantCaptureMouse;
}

bool Menu::isShowing() const { return show; }

bool Menu::isHovering() const { return isHover; }