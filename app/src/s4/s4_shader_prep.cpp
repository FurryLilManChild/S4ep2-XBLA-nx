// S4ep2-XBLA-nx (2026-11-19): the "Preparing shaders" screen (Mark: shaders compiled up front, before the player is in
// game, as recomp-nx's FFXIII Switch port does). The work is banjo::renut_gpu::deko3d::PrepareListedVariants
// (banjo_renut_deko3d.cpp), run on the game's thread from Direct3D's CreateDevice hook before the game draws; while it
// runs the game has not presented, so Renut's UI pump presents the SDK's dialogs, this one among them.
#include <algorithm>
#include <cstdio>

#include <imgui.h>

#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>

#include "renut_engine/gpu/banjo_renut_deko3d.h"
#include "s4/s4_shader_prep.h"

namespace s4 {
#if defined(__SWITCH__)  // banjo_renut_deko3d.cpp (ShaderPrep) is in the Switch build only
namespace {

class ShaderPrepDialog final : public rex::ui::ImGuiDialog {
 public:
  explicit ShaderPrepDialog(rex::ui::ImGuiDrawer* drawer) : ImGuiDialog(drawer) {}

 protected:
  void OnDraw(ImGuiIO& io) override {
    auto& p = banjo::renut_gpu::deko3d::ShaderPrep();
    if (!p.active.load(std::memory_order_relaxed)) return;
    const unsigned done = p.done.load(), total = std::max(1u, p.total.load()), compiled = p.compiled.load();
    const float s = 2.0f;
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * s);
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);
    if (ImGui::Begin("##s4shaderprep", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoInputs)) {
      ImGui::SetCursorPos(ImVec2(io.DisplaySize.x * 0.1f, io.DisplaySize.y * 0.4f));
      ImGui::BeginGroup();
      ImGui::Text("Preparing shaders: %u of %u", done, total);
      char label[64];
      std::snprintf(label, sizeof(label), "%u%%", done * 100 / total);
      ImGui::ProgressBar(float(done) / float(total), ImVec2(io.DisplaySize.x * 0.8f, 0), label);
      ImGui::TextDisabled("Compiled on this console so far: %u. Later starts skip this.", compiled);
      ImGui::EndGroup();
    }
    ImGui::End();
    ImGui::PopFont();
  }
};

}  // namespace

void RegisterShaderPrepScreen(rex::ui::ImGuiDrawer* drawer) {
  drawer->AddDialog(new ShaderPrepDialog(drawer));
}
#else
void RegisterShaderPrepScreen(rex::ui::ImGuiDrawer*) {}
#endif

}  // namespace s4
