// S4ep2-XBLA-nx (2026-11-19): the "Preparing shaders" screen (s4_shader_prep.cpp).
#pragma once

namespace rex::ui {
class ImGuiDrawer;
}

namespace s4 {
// From RenutApp::OnCreateDialogs: a full-screen progress screen, drawn only while the shaders are being prepared.
void RegisterShaderPrepScreen(rex::ui::ImGuiDrawer* drawer);
}  // namespace s4
