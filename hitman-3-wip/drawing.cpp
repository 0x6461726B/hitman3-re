#include "drawing.h"
#include "ZActorManager.h"
#include "hooks.h"
#include "utils.h"
#include "Settings.h"

#include <algorithm>
#include <sstream>
#include <vector>

namespace {
ImU32 ConvertColor(const Vector &color, float alpha = 255.0f) {
    return IM_COL32(
        static_cast<int>(std::clamp(color.X, 0.0f, 255.0f)), static_cast<int>(std::clamp(color.Y, 0.0f, 255.0f)),
        static_cast<int>(std::clamp(color.Z, 0.0f, 255.0f)), static_cast<int>(std::clamp(alpha, 0.0f, 255.0f)));
}
} // namespace

void Drawings::drawTextShadow(const std::string &text, const Vector &pos, float fontSize, const Vector &color) {
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImFont *font = ImGui::GetFont();
    // Convert Vector (0–1 floats) to ImU32
    ImU32 textCol = ConvertColor(color);

    // Four offset directions
    constexpr float offsets[4][2] = {{1.0f, 1.0f}, {-1.0f, -1.0f}, {1.0f, -1.0f}, {-1.0f, 1.0f}};

    for (auto &o : offsets) {
        dl->AddText(font, fontSize, ImVec2(pos.X + o[0], pos.Y + o[1]), textCol, text.c_str());
    }
}

void Drawings::initFonts() {

    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
    m_pFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f, NULL,
                                           io.Fonts->GetGlyphRangesCyrillic());

}

void Drawings::Begin(UINT frameIndex) {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::Begin("BackBuffer", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoMouseInputs |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavInputs |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);

    ImGui::SetWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y), ImGuiCond_Always);

    ImGui::PushFont(m_pFont);
}

bool once = false;

Vector green(0, 255, 0);
Vector white(255, 255, 255);
Vector red(255, 0, 0);

struct mat4 {
    float m[4][4];
};

bool WorldToScreenWrapper(const Vector &worldPos, Vector &screenPos, float &fade, int screenWidth, int screenHeight) {
    auto ZHM5MainCamera = callFunction<uintptr_t>(MAKE_RVA(0x13F5170)); // updated

    auto first = *(Vector *)(ZHM5MainCamera + 0x20);
    auto second = *(Vector *)(ZHM5MainCamera + 0x2C);
    auto third = *(Vector *)(ZHM5MainCamera + 0x38);
    auto cameraPos = *(Vector *)(ZHM5MainCamera + 0x44);

    // Build the transformation matrix from the camera vectors.
    mat4 trans_matrix;
    // Row 0
    trans_matrix.m[0][0] = first.X;
    trans_matrix.m[0][1] = second.X;
    trans_matrix.m[0][2] = third.X;
    trans_matrix.m[0][3] = 0.0f;
    // Row 1
    trans_matrix.m[1][0] = first.Y;
    trans_matrix.m[1][1] = second.Y;
    trans_matrix.m[1][2] = third.Y;
    trans_matrix.m[1][3] = 0.0f;
    // Row 2
    trans_matrix.m[2][0] = first.Z;
    trans_matrix.m[2][1] = second.Z;
    trans_matrix.m[2][2] = third.Z;
    trans_matrix.m[2][3] = 0.0f;
    // Row 3: translation part (using dot products to convert camera space)
    trans_matrix.m[3][0] = -cameraPos.dot(first);
    trans_matrix.m[3][1] = -cameraPos.dot(second);
    trans_matrix.m[3][2] = -cameraPos.dot(third);
    trans_matrix.m[3][3] = 1.0f;

    bool isOffScreen = false;
    fade = 0.0f;

    callFunction<void *>(MAKE_RVA(0x33D170), nullptr, &screenPos, ZHM5MainCamera, trans_matrix, worldPos, &isOffScreen,
                         &fade, true); //world2screen internal 


    

    // if (!isOffScreen) {
    //	return false;
    // }

    // Adjust the coordinates from a center-origin to a top-left origin.
    screenPos.X += screenWidth / 2.0f;
    screenPos.Y += screenHeight / 2.0f;

    isOffScreen = screenPos.X < 0 || screenPos.X > screenWidth || screenPos.Y < 0 || screenPos.Y > screenHeight;

    return !isOffScreen;
};

void Drawings::Draw() {

    
    if (DX12Hook::getInstance().isUnhooking()) {
        return;
    }

    if (!g_settings.draw) {
        return;
    }

    auto ZActorManager_ = reinterpret_cast<ZActorManager *>(MAKE_RVA(0x3036AA0)); // 4C 8D 15 ? ? ? ? 4C 8B 0D ? ? ? ? 49 8B C0 48 C1 E8 ? A8 ? 74 ? 41 0F B6 C0 49 8D 8A
    auto actorCount = ZActorManager_->getSize();

    for (int i = 0; i < actorCount; i++) {
        auto actor = ZActorManager_->getActor(i);

        uintptr_t transformState = actor + 0x20;
        auto pos = *(Vector *)(transformState + 0x1A0);

        Vector screenPos;
        float fade;

        if (WorldToScreenWrapper(pos, screenPos, fade, 1980, 1080)) {
            auto nameLength = *reinterpret_cast<int *>(actor + 0x488);
            if (nameLength > 0) {
                char *name = *reinterpret_cast<char **>(actor + 0x490);
                auto isTarget = *reinterpret_cast<bool *>(actor + 0x115A);

                
                // if (isTarget) {
                Drawings::drawText(name, screenPos, 18.0f, red, true);
                //}
            }
        }
    }
}


void Drawings::End() {

    ImGui::PopFont();
    ImGui::End();
    ImGui::PopStyleColor();
}

void Drawings::drawText(const std::string &text, const Vector &pos, float size, const Vector &color, bool center) {
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImFont *font = ImGui::GetFont();

    const ImU32 mainCol = ConvertColor(color);
    const ImU32 shadowCol = IM_COL32(0, 0, 0, 255);

    std::stringstream ss(text);
    std::string line;
    int lineNo = 0;

    while (std::getline(ss, line)) {
        ImVec2 txtSize = font->CalcTextSizeA(size, FLT_MAX, 0.f, line.c_str());

        float x = center ? pos.X - txtSize.x * 0.5f : pos.X;
        float y = pos.Y + txtSize.y * lineNo;

        dl->AddText(font, size, {x + 1.f, y + 1.f}, shadowCol, line.c_str());
        dl->AddText(font, size, {x, y}, mainCol, line.c_str());

        ++lineNo;
    }
}

void Drawings::drawLine(const Vector &from, const Vector &to, Vector color, float thickness) {

    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (!window)
        return;

    // Convert to ImGui format (ABGR)
    ImU32 imguiColor = IM_COL32(color.X, color.Y, color.Z, 255);

    // Get the draw list and add the line
    ImDrawList *draw_list = window->DrawList;
    draw_list->AddLine(ImVec2(from.X, from.Y), ImVec2(to.X, to.Y), imguiColor, thickness);
}

void Drawings::drawCircle(const Vector &position, float radius, Vector color, int numSegments) {
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext &g = *GImGui;
    ImU32 imguiColor = IM_COL32(color.X, color.Y, color.Z, 255);

    window->DrawList->AddCircle(ImVec2(position.X, position.Y), radius, imguiColor, numSegments, 2.0f);
}

void Drawings::DrawCornerESP(Vector position, float width, float height, Vector color) {

    position.X -= width / 2;

    float lineW = width / 5;
    float lineH = height / 6;
    Vector outlineColor(0, 0, 0); // Black for the outline

    // Outline
    // Top
    drawLine(Vector(position.X, position.Y, 0), Vector(position.X + lineW, position.Y, 0), outlineColor, 1);
    drawLine(Vector(position.X, position.Y, 0), Vector(position.X, position.Y + lineH, 0), outlineColor, 1);
    drawLine(Vector(position.X + width - lineW, position.Y, 0), Vector(position.X + width, position.Y, 0), outlineColor,
             1);
    drawLine(Vector(position.X + width, position.Y, 0), Vector(position.X + width, position.Y + lineH, 0), outlineColor,
             1);

    // Bottom
    drawLine(Vector(position.X, position.Y + height, 0), Vector(position.X + lineW, position.Y + height, 0),
             outlineColor, 1);
    drawLine(Vector(position.X, position.Y + height - lineH, 0), Vector(position.X, position.Y + height, 0),
             outlineColor, 1);
    drawLine(Vector(position.X + width - lineW, position.Y + height, 0),
             Vector(position.X + width, position.Y + height, 0), outlineColor, 1);
    drawLine(Vector(position.X + width, position.Y + height - lineH, 0),
             Vector(position.X + width, position.Y + height, 0), outlineColor, 1);

    // Inline
    // Top
    drawLine(Vector(position.X + 1, position.Y + 1, 0), Vector(position.X + lineW - 1, position.Y + 1, 0), color, 1);
    drawLine(Vector(position.X + 1, position.Y + 1, 0), Vector(position.X + 1, position.Y + lineH - 1, 0), color, 1);
    drawLine(Vector(position.X + width - lineW + 1, position.Y + 1, 0),
             Vector(position.X + width - 1, position.Y + 1, 0), color, 1);
    drawLine(Vector(position.X + width - 1, position.Y + 1, 0),
             Vector(position.X + width - 1, position.Y + lineH - 1, 0), color, 1);

    // Bottom
    drawLine(Vector(position.X + 1, position.Y + height - 1, 0),
             Vector(position.X + lineW - 1, position.Y + height - 1, 0), color, 1);
    drawLine(Vector(position.X + 1, position.Y + height - lineH + 1, 0),
             Vector(position.X + 1, position.Y + height - 1, 0), color, 1);
    drawLine(Vector(position.X + width - lineW + 1, position.Y + height - 1, 0),
             Vector(position.X + width - 1, position.Y + height - 1, 0), color, 1);
    drawLine(Vector(position.X + width - 1, position.Y + height - lineH + 1, 0),
             Vector(position.X + width - 1, position.Y + height - 1, 0), color, 1);
}