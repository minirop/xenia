/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_APP_EMULATOR_WINDOW_H_
#define XENIA_APP_EMULATOR_WINDOW_H_

#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>

#define SOL_EXCEPTIONS_CATCH_ALL 0
#include <sol/sol.hpp>

#include "xenia/emulator.h"
#include "xenia/gpu/command_processor.h"
#include "xenia/ui/imgui_dialog.h"
#include "xenia/ui/imgui_drawer.h"
#include "xenia/ui/immediate_drawer.h"
#include "xenia/ui/menu_item.h"
#include "xenia/ui/presenter.h"
#include "xenia/ui/window.h"
#include "xenia/ui/window_listener.h"
#include "xenia/ui/windowed_app_context.h"
#include "xenia/xbox.h"

static const uint32_t BASE_ADDRESS = 0x82450000;
static const uint32_t BYTES_PER_CHUNK = 65536;

namespace xe {
namespace app {

class EmulatorWindow {
 public:
  enum : size_t {
    // The UI is on top of the game and is open in special cases, so
    // lowest-priority.
    kZOrderHidInput,
    kZOrderImGui,
    kZOrderProfiler,
    // Emulator window controls are expected to be always accessible by the
    // user, so highest-priority.
    kZOrderEmulatorWindowInput,
  };

  virtual ~EmulatorWindow();

  static std::unique_ptr<EmulatorWindow> Create(
      Emulator* emulator, ui::WindowedAppContext& app_context);

  Emulator* emulator() const { return emulator_; }
  ui::WindowedAppContext& app_context() const { return app_context_; }
  ui::Window* window() const { return window_.get(); }
  ui::ImGuiDrawer* imgui_drawer() const { return imgui_drawer_.get(); }

  ui::Presenter* GetGraphicsSystemPresenter() const;
  void SetupGraphicsSystemPresenterPainting();
  void ShutdownGraphicsSystemPresenterPainting();

  void OnEmulatorInitialized();

  void UpdateTitle();
  void SetFullscreen(bool fullscreen);
  void ToggleFullscreen();
  void SetInitializingShaderStorage(bool initializing);
  void ToggleScript(const std::filesystem::path & path);
  void SendBroadcast(uint32_t ID, bool data);

  template <typename search_t>
  void ToggleMemorySearch() {
	  if (memory_search_dialogs_.find(typeid(search_t)) == memory_search_dialogs_.end()) {
		memory_search_dialogs_[typeid(search_t)].reset(new MemorySearchDialog<search_t>(imgui_drawer_.get(), *this));
	  } else {
		memory_search_dialogs_[typeid(search_t)].reset();
	  }
	}

 private:
  class EmulatorWindowListener final : public ui::WindowListener,
                                       public ui::WindowInputListener {
   public:
    explicit EmulatorWindowListener(EmulatorWindow& emulator_window)
        : emulator_window_(emulator_window) {}

    void OnClosing(ui::UIEvent& e) override;
    void OnFileDrop(ui::FileDropEvent& e) override;

    void OnKeyDown(ui::KeyEvent& e) override;

   private:
    EmulatorWindow& emulator_window_;
  };

  class DisplayConfigGameConfigLoadCallback
      : public Emulator::GameConfigLoadCallback {
   public:
    DisplayConfigGameConfigLoadCallback(Emulator& emulator,
                                        EmulatorWindow& emulator_window)
        : Emulator::GameConfigLoadCallback(emulator),
          emulator_window_(emulator_window) {}

    void PostGameConfigLoad() override;

   private:
    EmulatorWindow& emulator_window_;
  };

  class DisplayConfigDialog final : public ui::ImGuiDialog {
   public:
    DisplayConfigDialog(ui::ImGuiDrawer* imgui_drawer,
                        EmulatorWindow& emulator_window)
        : ui::ImGuiDialog(imgui_drawer), emulator_window_(emulator_window) {}

   protected:
    void OnDraw(ImGuiIO& io) override;

   private:
    EmulatorWindow& emulator_window_;
  };

  template <typename search_t>
  class MemorySearchDialog final : public ui::ImGuiDialog {
   public:
    MemorySearchDialog(ui::ImGuiDrawer* imgui_drawer,
                        EmulatorWindow& emulator_window)
        : ui::ImGuiDialog(imgui_drawer), emulator_window_(emulator_window) {
		}

   protected:
    void OnDraw(ImGuiIO& io) override {
	  ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
	  ImGui::SetNextWindowSize(ImVec2(20, 20), ImGuiCond_FirstUseEver);
	  ImGui::SetNextWindowBgAlpha(0.6f);
	  
	  static std::unordered_map<std::type_index, const char*> window_titles = {
		  {typeid(uint8_t), "Memory search - 8 bits"},
		  {typeid(uint16_t), "Memory search - 16 bits"},
		  {typeid(uint32_t), "Memory search - 32 bits"},
		  {typeid(float), "Memory search - float"},
	  };
	  
	  bool dialog_open = true;
	  if (!ImGui::Begin(window_titles[typeid(search_t)], &dialog_open,
						ImGuiWindowFlags_NoCollapse |
						ImGuiWindowFlags_AlwaysAutoResize |
						ImGuiWindowFlags_HorizontalScrollbar)) {
		ImGui::End();
		return;
	  }
	  
		auto memory = emulator_window_.emulator_->memory();
		ImGui::TextUnformatted(fmt::format("{} cells", memory_cells.size()).data());

		if (typeid(search_t) == typeid(float))
		{
			static char min_value[64] = "0";
			static char max_value[64] = "0";
			ImGui::InputText("minimum", min_value, 32, ImGuiInputTextFlags_CharsDecimal);
			ImGui::InputText("maximum", max_value, 32, ImGuiInputTextFlags_CharsDecimal);
			auto min_val = std::stof(min_value);
			auto max_val = std::stof(max_value);
			
			if (ImGui::Button("new search"))
			{
				memory_cells.clear();
				for (uint32_t i = 0; i < BYTES_PER_CHUNK*15; i += sizeof(search_t))
				{
					auto value = xe::load_and_swap<search_t>(memory->TranslateVirtual(BASE_ADDRESS + i));
					if (value >= min_val && value < max_val)
					{
						memory_cells.push_back(BASE_ADDRESS + i);
					}
				}
			}
			
			ImGui::SameLine();
			if (ImGui::Button("continue"))
			{
				for (auto it = memory_cells.begin(); it != memory_cells.end();)
				{
					auto value = xe::load_and_swap<search_t>(memory->TranslateVirtual(*it));
					if (value >= min_val && value < max_val)
					{
						++it;
					}
					else
					{
						it = memory_cells.erase(it);
					}
				}
			}
			
			if (memory_cells.size() < 100)
			{
				for (auto cell : memory_cells)
				{
					auto value = xe::load_and_swap<search_t>(memory->TranslateVirtual(cell));
					
					ImGui::Spacing();
					ImGui::TextUnformatted(fmt::format("0x{:x}: {}", cell, value).data());
				}
			}
		}
		else
		{
			static char value[64] = "0";
			ImGui::InputText("value", value, 32, ImGuiInputTextFlags_CharsDecimal);
			
			auto int_val = std::stoi(value);
			auto real_val = static_cast<search_t>(int_val);
			
			if (ImGui::Button("New"))
			{
				memory_cells.clear();
				for (uint32_t i = 0; i < BYTES_PER_CHUNK*15; i += sizeof(search_t))
				{
					auto value = xe::load_and_swap<search_t>(memory->TranslateVirtual(BASE_ADDRESS + i));
					if (value == real_val)
					{
						memory_cells.push_back(BASE_ADDRESS + i);
					}
				}
			}
			
			ImGui::SameLine();
			if (ImGui::Button("=="))
			{
				for (auto it = memory_cells.begin(); it != memory_cells.end();)
				{
					auto value = xe::load_and_swap<search_t>(memory->TranslateVirtual(*it));
					if (value == real_val)
					{
						++it;
					}
					else
					{
						it = memory_cells.erase(it);
					}
				}
			}
			
			ImGui::SameLine();
			if (ImGui::Button("!="))
			{
				for (auto it = memory_cells.begin(); it != memory_cells.end();)
				{
					auto value = xe::load_and_swap<search_t>(memory->TranslateVirtual(*it));
					if (value != real_val)
					{
						++it;
					}
					else
					{
						it = memory_cells.erase(it);
					}
				}
			}
			
			if (memory_cells.size() < 100)
			{
				for (auto cell : memory_cells)
				{
					auto value = xe::load_and_swap<search_t>(memory->TranslateVirtual(cell));
					
					ImGui::Spacing();
					ImGui::TextUnformatted(fmt::format("0x{:x}: 0x{:x}", cell, value).data());
				}
			}
		}
	  
	  ImGui::End();
	  
	  if (!dialog_open) {
		emulator_window_.ToggleMemorySearch<search_t>();
		return;
	  }
	}

   private:
    EmulatorWindow& emulator_window_;
    std::vector<uint32_t> memory_cells; 
  };

  class LuaScriptDialog final : public ui::ImGuiDialog {
   public:
    LuaScriptDialog(ui::ImGuiDrawer* imgui_drawer,
                    EmulatorWindow& emulator_window,
                    const std::filesystem::path & path);

   protected:
    void OnDraw(ImGuiIO& io) override;

   private:
    EmulatorWindow& emulator_window_;
    std::filesystem::path path_;
    sol::state lua;
    std::string title;
    sol::function draw;
  };

  explicit EmulatorWindow(Emulator* emulator,
                          ui::WindowedAppContext& app_context);

  bool Initialize();

  // For comparisons, use GetSwapPostEffectForCvarValue instead as the default
  // fallback may be used for multiple values.
  static const char* GetCvarValueForSwapPostEffect(
      gpu::CommandProcessor::SwapPostEffect effect);
  static gpu::CommandProcessor::SwapPostEffect GetSwapPostEffectForCvarValue(
      const std::string& cvar_value);
  // For comparisons, use GetGuestOutputPaintEffectForCvarValue instead as the
  // default fallback may be used for multiple values.
  static const char* GetCvarValueForGuestOutputPaintEffect(
      ui::Presenter::GuestOutputPaintConfig::Effect effect);
  static ui::Presenter::GuestOutputPaintConfig::Effect
  GetGuestOutputPaintEffectForCvarValue(const std::string& cvar_value);
  static ui::Presenter::GuestOutputPaintConfig
  GetGuestOutputPaintConfigForCvars();
  void ApplyDisplayConfigForCvars();

  void OnKeyDown(ui::KeyEvent& e);
  void FileDrop(const std::filesystem::path& filename);
  void FileOpen();
  void FileClose();
  void ShowContentDirectory();
  void CpuTimeScalarReset();
  void CpuTimeScalarSetHalf();
  void CpuTimeScalarSetDouble();
  void CpuBreakIntoDebugger();
  void CpuBreakIntoHostDebugger();
  void GpuTraceFrame();
  void GpuClearCaches();
  void ToggleDisplayConfigDialog();
  void ShowCompatibility();
  void ShowFAQ();
  void ShowBuildCommit();

  Emulator* emulator_;
  ui::WindowedAppContext& app_context_;
  EmulatorWindowListener window_listener_;
  std::unique_ptr<ui::Window> window_;
  std::unique_ptr<ui::ImGuiDrawer> imgui_drawer_;
  std::unique_ptr<DisplayConfigGameConfigLoadCallback>
      display_config_game_config_load_callback_;
  // Creation may fail, in this case immediate drawer UI must not be drawn.
  std::unique_ptr<ui::ImmediateDrawer> immediate_drawer_;

  bool emulator_initialized_ = false;

  std::string base_title_;
  bool initializing_shader_storage_ = false;

  std::unique_ptr<DisplayConfigDialog> display_config_dialog_;
  std::unordered_map<std::type_index, std::unique_ptr<ui::ImGuiDialog>> memory_search_dialogs_;
  std::unordered_map<std::string, std::unique_ptr<LuaScriptDialog>> lua_script_dialogs_;
};

}  // namespace app
}  // namespace xe

#endif  // XENIA_APP_EMULATOR_WINDOW_H_
