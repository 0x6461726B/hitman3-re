#pragma once
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx12.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_internal.h"
#include "Vector.h"

#include <string>

class Drawings {
  public:
    void initFonts();
    void Begin(UINT frameIndex);
    void Draw();
    void End();
    void drawText(const std::string &text, const Vector &pos, float size, const Vector &color, bool center);
    void drawTextShadow(const std::string &text, const Vector &pos, float fontSize, const Vector &color);
    void drawLine(const Vector &from, const Vector &to, Vector color, float thickness);
    void drawCircle(const Vector &position, float radius, Vector color, int numSegments);
    void DrawCornerESP(Vector position, float width, float height, Vector color);

  private:
    ImFont *m_pFont;
};