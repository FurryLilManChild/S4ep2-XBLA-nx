// S4ep1-nx: the port menu (s4_menu.cpp, 2026-11-15).
#pragma once

#include <filesystem>
#include <functional>

namespace rex::ui {
class ImGuiDrawer;
}

namespace s4 {
// From RenutApp::OnCreateDialogs: - and + together (Switch) or F8 (PC) opens and closes the menu. set_fps shows or
// hides the FPS counter (the app's FpsManager).
void RegisterMenu(rex::ui::ImGuiDrawer* drawer, const std::filesystem::path& config_path,
                  std::function<void(bool)> set_fps);
bool MenuOpen();  // the gamepad goes to the menu while it is open (ReXApp::AppOverlayOpen)
}  // namespace s4
