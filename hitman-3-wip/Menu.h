#pragma once
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx12.h"
#include "ImGui/imgui_impl_win32.h"
#include "Windows.h"

class Menu {
  public:
    Menu();
    ~Menu();
    void Render();
    void beginNewFrame();
    void endFrame();
    void initStyles();

    bool isShowing() const;
    bool isHovering() const;

  private:
    void handleKeyInputs();

    bool show;
    bool isHover;
};