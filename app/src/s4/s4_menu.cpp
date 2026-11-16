// S4ep1-nx: the port menu (2026-11-15), made from LoS1-nx's (app/src/los_menu.cpp there).
//
// Opened with - and + together on the Switch (switch_input_driver.cpp sends F8), F8 on the PC. "Save and close" writes
// the menu's keys into the player's config (renut.toml next to the NRO / exe), keeping every other line.
//
// The settings:
//   FPS counter          banjo_show_fps: the frames shown on screen a second (the display thread's "Shown" counter).
//                        Live.
//   Handheld resolution  banjo_renut_render_height_handheld: 360 = 960x540 (the stock-clock default, Switch test 5:
//                        a steady 60), 486 = 1296x729, 540 = 1440x810 (for overclocked consoles). The heights are read
//                        once at start (surfaces.cpp), so from the next start.
//   Docked resolution    banjo_renut_render_height_docked: 486 = 1296x729 (default), 540 = 1440x810, 720 = 1920x1080
//                        (the game's own). From the next start.
//   Handheld GPU         banjo_gpu_boost: the official 460.8 MHz handheld GPU profile (default; Switch test 5 ran with
//                        it) or the system's 384 MHz. Not an overclock. From the next start.
//   Stick dead zone      input_stick_deadzone: none (default: the game's own only) / 10 % / 20 % for drifting sticks.
//                        Live.
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include <imgui.h>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/platform.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/keybinds.h>

#include "s4/s4_menu.h"

namespace s4 {
namespace {

std::filesystem::path g_config_path;
std::function<void(bool)> g_set_fps;
constexpr float kScale = 2.0f;  // LoS1-nx: 1.0 was too small in handheld
std::atomic<bool> g_open{false};

// The menu's choices. Read from the settings when the menu opens; the live ones are applied at once, the rest are
// written by Save and take effect at the next start.
struct Choices {
  bool fps = false;
  int handheld = 360;
  int docked = 486;
  bool gpu = true;
  int deadzone = 0;
} g_start, g_now;  // g_start: as the game started (what is in force for the start-only ones)
bool g_read = false;

int FlagInt(const char* name, int fallback) {
  try {
    return std::stoi(rex::cvar::GetFlagByName(name));
  } catch (...) {
    return fallback;
  }
}

Choices ReadChoices() {
  Choices c;
  c.fps = rex::cvar::GetFlagByName("banjo_show_fps") == "true";
  c.handheld = FlagInt("banjo_renut_render_height_handheld", 360);
  c.docked = FlagInt("banjo_renut_render_height_docked", 486);
  c.gpu = rex::cvar::GetFlagByName("banjo_gpu_boost") != "false";
  c.deadzone = FlagInt("input_stick_deadzone", 0);
  return c;
}

std::string Trim(const std::string& s) {
  const size_t a = s.find_first_not_of(" \t\r");
  if (a == std::string::npos) return "";
  return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
}

// The key a line sets ("name = value"), or "" for comments, blank lines and sections.
std::string KeyOf(const std::string& line) {
  const std::string t = Trim(line);
  if (t.empty() || t[0] == '#' || t[0] == '[') return "";
  const size_t eq = t.find('=');
  return eq == std::string::npos ? "" : Trim(t.substr(0, eq));
}

bool SaveToConfig(std::string* error) {
  if (g_config_path.empty()) {
    *error = "no config file path";
    return false;
  }
  std::vector<std::string> lines;
  {
    std::ifstream in(g_config_path);
    std::string line;
    while (std::getline(in, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      lines.push_back(line);
    }
  }
  struct Key {
    const char* name;
    std::string value;
    bool written = false;
  };
  Key keys[] = {
      {"banjo_show_fps", g_now.fps ? "true" : "false"},
      {"banjo_renut_render_height_handheld", std::to_string(g_now.handheld)},
      {"banjo_renut_render_height_docked", std::to_string(g_now.docked)},
      {"banjo_gpu_boost", g_now.gpu ? "true" : "false"},
#if REX_PLATFORM_SWITCH
      {"input_stick_deadzone", std::to_string(g_now.deadzone)},
#endif
  };
  for (std::string& line : lines) {
    const std::string key = KeyOf(line);
    for (Key& k : keys)
      if (key == k.name) {
        line = std::string(k.name) + " = " + k.value;
        k.written = true;
      }
  }
  bool header = false;
  for (Key& k : keys)
    if (!k.written) {
      if (!header) {
        lines.push_back("");
        lines.push_back("# The port menu's settings (- and + together).");
        header = true;
      }
      lines.push_back(std::string(k.name) + " = " + k.value);
    }
  std::ofstream out(g_config_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    *error = "could not write " + g_config_path.string();
    return false;
  }
  for (const std::string& line : lines) out << line << "\n";
  out.close();
  if (!out) {
    *error = "could not write " + g_config_path.string();
    return false;
  }
  REXLOG_INFO("[menu] saved to {}: fps counter {}, handheld height {}, docked height {}, handheld GPU 460.8 MHz {}, "
              "stick dead zone {} %",
              g_config_path.string(), keys[0].value, keys[1].value, keys[2].value, keys[3].value, g_now.deadzone);
  return true;
}

void Label(const char* text) {
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(text);
  ImGui::SameLine(190 * kScale);
}

bool Choice(const char* label, bool on) {
  return ImGui::RadioButton(label, on);
}

class S4MenuDialog final : public rex::ui::ImGuiDialog {
 public:
  explicit S4MenuDialog(rex::ui::ImGuiDrawer* drawer) : ImGuiDialog(drawer) { g_open = true; }
  void RequestClose() { Close(); }

 protected:
  void OnClose() override;

  void OnDraw(ImGuiIO& io) override {
    const float s = kScale;
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * s);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10 * s, 4 * s));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12 * s, 5 * s));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14 * s, 12 * s));
    const ImVec2 size(std::min(io.DisplaySize.x * 0.94f, 640 * s), 0);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    if (first_) ImGui::SetNextWindowFocus();
    if (ImGui::Begin("Sonic 4 Episode I: port menu##s4", nullptr,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize)) {
      if (first_) {
        ImGui::SetKeyboardFocusHere();
        first_ = false;
      }
      Choices& c = g_now;

      Label("FPS counter");
      if (Choice("On##fps", c.fps)) SetFps(true);
      ImGui::SameLine();
      if (Choice("Off##fps", !c.fps)) SetFps(false);

#if REX_PLATFORM_SWITCH  // the Switch input driver's (the PC's pads keep their own)
      Label("Stick dead zone");
      if (Choice("None##dz", c.deadzone == 0)) SetDeadzone(0);
      ImGui::SameLine();
      if (Choice("10%##dz", c.deadzone == 10)) SetDeadzone(10);
      ImGui::SameLine();
      if (Choice("20%##dz", c.deadzone == 20)) SetDeadzone(20);
      ImGui::TextDisabled("None: the game's own only. More for a drifting stick.");
#endif
      ImGui::Separator();

      Label("Handheld");
      if (Choice("960x540##hh", c.handheld == 360)) c.handheld = 360;
      ImGui::SameLine();
      if (Choice("1296x729##hh", c.handheld == 486)) c.handheld = 486;
      ImGui::SameLine();
      if (Choice("1440x810##hh", c.handheld == 540)) c.handheld = 540;
      ImGui::TextDisabled("960x540 holds 60 fps at stock clocks. Sharper needs an overclock.");

      Label("Docked");
      if (Choice("1296x729##dk", c.docked == 486)) c.docked = 486;
      ImGui::SameLine();
      if (Choice("1440x810##dk", c.docked == 540)) c.docked = 540;
      ImGui::SameLine();
      if (Choice("1920x1080##dk", c.docked == 720)) c.docked = 720;

      Label("Handheld GPU");
      if (Choice("460.8 MHz##gpu", c.gpu)) c.gpu = true;
      ImGui::SameLine();
      if (Choice("384 MHz##gpu", !c.gpu)) c.gpu = false;
      ImGui::TextDisabled("460.8 MHz is Nintendo's own handheld profile, not an overclock. 384 saves battery.");

      const bool restart = c.handheld != g_start.handheld || c.docked != g_start.docked || c.gpu != g_start.gpu;
      if (restart) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Resolution and GPU: Save, then restart.");
      ImGui::Separator();

      if (ImGui::Button("Save and close", ImVec2(280 * s, 0))) {
        std::string error;
        if (SaveToConfig(&error)) {
          Close();
        } else {
          message_ = "Not saved: " + error;
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Close", ImVec2(280 * s, 0))) Close();
      ImGui::TextDisabled("Close keeps the FPS and stick changes until the game is restarted.");
      if (!message_.empty()) ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "%s", message_.c_str());
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopFont();
  }

 private:
  static void SetFps(bool on) {
    g_now.fps = on;
    rex::cvar::SetFlagByName("banjo_show_fps", on ? "true" : "false");
    if (g_set_fps) g_set_fps(on);
  }
  static void SetDeadzone(int v) {
    g_now.deadzone = v;
    rex::cvar::SetFlagByName("input_stick_deadzone", std::to_string(v));
  }

  bool first_ = true;
  std::string message_;
};

S4MenuDialog* g_menu = nullptr;  // the UI thread's only

void S4MenuDialog::OnClose() {
  g_menu = nullptr;
  g_open = false;
}

}  // namespace

bool MenuOpen() {
  return g_open.load(std::memory_order_relaxed);
}

void RegisterMenu(rex::ui::ImGuiDrawer* drawer, const std::filesystem::path& config_path,
                  std::function<void(bool)> set_fps) {
  g_config_path = config_path;
  g_set_fps = std::move(set_fps);
  if (!g_read) {
    g_start = g_now = ReadChoices();
    g_read = true;
    REXLOG_INFO("[menu] in force: fps counter {}, handheld height {}, docked height {}, handheld GPU 460.8 MHz {}, "
                "stick dead zone {} %; config {}",
                g_start.fps, g_start.handheld, g_start.docked, g_start.gpu, g_start.deadzone, config_path.string());
  }
  rex::ui::RegisterBind("bind_s4_menu", "F8", "Toggle the port menu (- and + on the Switch)", [drawer] {
    if (g_menu) {
      g_menu->RequestClose();
    } else {
      g_now.fps = rex::cvar::GetFlagByName("banjo_show_fps") == "true";  // F1 may have changed it
      g_menu = new S4MenuDialog(drawer);
    }
  });
}

}  // namespace s4
