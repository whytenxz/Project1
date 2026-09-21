#include "Menu.h"
#include "../Renderer/Renderer.h"
#include "../../Backend/Utilities/Utilities.h"
#include "../../Backend/Config/Settings.h"
#include "../../Backend/Config/Config.h"
#include "../../Backend/Config/CloudConfig.h"
#include "../../Backend/Utilities/Spotify.h"
#include "SkinsTab.h"
#include "PlayersTab.h"
#include "LuaTab.h"
#include "../../cs2/weapons.h"

#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>
#include <fstream>
#include <sstream>
#include <filesystem>

using namespace IdaLovesMe;

/// <summary>
/// Initialize GTX, and preset some stuff.
/// Called only once at the beginning.
/// </summary>
void CMenu::Initialize() 
{
	if (this->m_bInitialized)
		return;

	ui::CreateContext();
	GuiContext* g = Globals::Gui_Ctx;
	g->ItemSpacing = Vec2(0, 6);
	g->MenuAlpha = 1;

	CConfig::get()->LoadDefaults();
	CConfig::get()->Refresh();

	char default_path[MAX_PATH]{};
	CConfig::get()->GetFilePath(default_path);
	if (std::filesystem::exists(default_path)) {
		CConfig::get()->Load();
	}

	Misc::Utilities->Game_Msg("Cheat Initialized!");

	this->m_bIsOpened = false;
	this->m_bInitialized = true;
}

/// <summary>
/// Drawing the user interface.
/// Please keep in mind that this function is called on each frame.
/// </summary>
void CMenu::Draw()
{
	if (!this->m_bInitialized)
		this->Initialize();

	if (!Globals::Gui_Ctx)
		return;

	CConfig* cfg = CConfig::get();

	static float alpha = 0;
	const float dt = Misc::Utilities->GetDeltaTime();
	constexpr float kMenuFadeSpeed = 14.f;
	if (!this->m_bIsOpened && alpha > 0.f)
		alpha = std::clamp(alpha - dt * kMenuFadeSpeed, 0.f, 1.f);

	if (this->m_bIsOpened && alpha < 1.f)
		alpha = std::clamp(alpha + dt * kMenuFadeSpeed, 0.f, 1.f);

	Globals::Gui_Ctx->MenuAlpha = static_cast<int>(floor(alpha * 255));

	if (!this->m_bIsOpened && alpha == 0.f)
		return;

	ui::GetInputFromWindow("Counter-Strike 2");

	static const float kDpiScales[] = { 1.f, 1.25f, 1.5f, 1.75f, 2.f };
	const int scaleIdx = std::clamp(cfg->i["menu_scale"], 0, 4);
	ui::ApplyDpiScale(kDpiScales[scaleIdx]);

	const Vec2 menuSize = ui::GetMenuDefaultSize();

	static int lastMenuScale = -1;
	static bool menuSizeInitialized = false;
	if (!menuSizeInitialized || lastMenuScale != scaleIdx) {
		ui::SetNextWindowSize(menuSize);
		menuSizeInitialized = true;
		lastMenuScale = scaleIdx;
	}

	static bool menuPositioned = false;
	if (!menuPositioned) {
		const float screenW = Render::Draw && Render::Draw->Screen.Width > 100.f ? (float)Render::Draw->Screen.Width : 1920.f;
		const float screenH = Render::Draw && Render::Draw->Screen.Height > 100.f ? (float)Render::Draw->Screen.Height : 1080.f;
		ui::SetNextWindowPos(Vec2(
			(std::max)(0.f, (screenW - menuSize.x) * 0.5f),
			(std::max)(0.f, (screenH - menuSize.y) * 0.5f)));
		menuPositioned = true;
	}

	ui::EnsureMainWindowMinimumSize();

	ui::Begin("Main", GuiFlags_None);

	if (this->m_bIsBanned) {
		Vec2 pos = ui::GetWindowPos();
		Vec2 size = ui::GetWindowSize();
		
		float center_x = pos.x + (size.x / 2.f);
		float center_y = pos.y + (size.y / 2.f);
		
		Render::Draw->Text("You are banned.", center_x, center_y - ui::Scale(12.f), CENTER, Render::Fonts::Tahombd, false, D3DCOLOR_RGBA(255, 0, 0, Globals::Gui_Ctx->MenuAlpha));
		Render::Draw->Text("You can no longer use gamesense.", center_x, center_y + ui::Scale(12.f), CENTER, Render::Fonts::Verdana, false, D3DCOLOR_RGBA(160, 160, 160, Globals::Gui_Ctx->MenuAlpha));
		
		ui::End();
		return;
	}

	const int animSpeed = cfg->i["menu_animation_speed"] > 0 ? cfg->i["menu_animation_speed"] : 100;
	static bool rageTabAddedMigrated = false;
	if (!rageTabAddedMigrated) {
		this->m_nCurrentTab += 1;
		rageTabAddedMigrated = true;
		Globals::Gui_Ctx->TabAnimation.initialized = false;
		ui::SetNextWindowSize(menuSize);
	}

	static bool childLayoutMigrated = false;
	if (!childLayoutMigrated) {
		ui::ResetChildLayouts(5);
		childLayoutMigrated = true;
	}
	this->m_nCurrentTab = std::clamp(this->m_nCurrentTab, 0, 7);

	ui::UpdateMainTabAnimation(&this->m_nCurrentTab, 8, (float)animSpeed, dt);

	ui::TabButton("A", &this->m_nCurrentTab, 0, 8);
	ui::TabButton("B", &this->m_nCurrentTab, 1, 8);
	ui::TabButton("C", &this->m_nCurrentTab, 2, 8);
	ui::TabButton("D", &this->m_nCurrentTab, 3, 8);
	ui::TabButton("E", &this->m_nCurrentTab, 4, 8);
	ui::TabButton("F", &this->m_nCurrentTab, 5, 8);
	ui::TabButton("H", &this->m_nCurrentTab, 6, 8);
	ui::TabButton("Lua", &this->m_nCurrentTab, 7, 8, GuiFlags_LuaTab);
	ui::DrawMainTabIndicator(8);

	switch (ui::GetMainTabDisplayIndex())
	{
		//
		// RAGE
		//
		case 0:
		{
			ui::BeginChild("Other#Rage", { Vec2(6, 0), Vec2(3, 10) });
			{
				ui::Checkbox("Remove recoil", &cfg->b["rage_remove_recoil"]);
				ui::SingleSelect("Accuracy boost", &cfg->i["rage_accuracy_boost"], { "Low", "Medium", "High", "Maximum" });
				ui::Checkbox("Delay shot", &cfg->b["rage_delay_shot"]);
				ui::Checkbox("Quick stop", &cfg->b["rage_quick_stop"]);
				ui::KeyBind("rage_quick_stop_bind", &cfg->i["rage_quick_stop_bind"], &cfg->i["rage_quick_stop_bind_style"]);

				if (cfg->b["rage_quick_stop"])
					ui::MultiSelect("Quick stop options", &cfg->m["rage_quick_stop_options"], { "Early", "Slow motion", "Duck", "Fake duck", "Move between shots", "Ignore molotov", "Taser" });

				ui::Checkbox("Quick peek assist", &cfg->b["rage_quick_peek_assist"]);
				ui::KeyBind("rage_quick_peek_bind", &cfg->i["rage_quick_peek_bind"], &cfg->i["rage_quick_peek_bind_style"]);

				if (cfg->b["rage_quick_peek_assist"]) {
					ui::MultiSelect("Quick peek assist mode", &cfg->m["rage_quick_peek_assist_mode"], { "Retreat on shot", "Retreat on key release" });
					ui::ColorPicker("quick_peek_assist_colorpicker", cfg->c["quick_peek_assist_colorpicker"]);
					ui::SliderInt("Quick peek assist distance", &cfg->i["rage_quick_peek_distance"], 16, 200, cfg->i["rage_quick_peek_distance"] == 200 ? "8" : "%din");
				}

				if (ui::Checkbox("Anti-aim correction", &cfg->b["rage_aa_correction"])) {
					ui::Label("Anti-aim correction override");
					ui::KeyBind("rage_correction_override_bind", &cfg->i["rage_correction_override_bind"], &cfg->i["rage_correction_override_bind_style"]);
				}

				if (cfg->m["rage_target_hitbox"][1] || cfg->m["rage_target_hitbox"][2])
					if (ui::Checkbox("Prefer body aim", &cfg->b["rage_prefer_body_aim"]))
						ui::MultiSelect("Prefer body aim disablers", &cfg->m["rage_other_baim_disablers"], { "Low inaccuray", "Target shot fired", "Target resolved", "Safe point headshot", "Low damage" });

				ui::Label("Force body aim");
				ui::KeyBind("rage_baim_bind", &cfg->i["rage_baim_bind"], &cfg->i["rage_baim_bind_style"]);
				ui::Checkbox("Force body aim on peek", &cfg->b["rage_force_baim"]);
				ui::Label("Duck peek assist");
				ui::KeyBind("rage_fakeduck_bind", &cfg->i["rage_fakeduck_bind"], &cfg->i["rage_fakeduck_bind_style"]);
				ui::Checkbox("Double tap", &cfg->b["rage_doubletap"], true);
				ui::KeyBind("rage_doubletap_bind", &cfg->i["rage_doubletap_bind"], &cfg->i["rage_doubletap_bind_style"]);
				if (cfg->b["rage_doubletap"]) {
					ui::SingleSelect("Double tap mode", &cfg->i["rage_dooubletap_mode"], { "Offensive", "Defensive" });
					ui::SliderInt("Double tap hit chance", &cfg->i["rage_dt_hitchance"], 0, 100, "%d%%");
					ui::SliderInt("Double tap fake lag limit", &cfg->i["rage_dt_fakelag_limit"], 0, 10, "%d");
					ui::MultiSelect("Double tap quick stop", &cfg->m["rage_dt_quick_stop"], { "Slow motion", "Duck", "Move between shots" });
				}
			}
			ui::EndChild();
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			ui::BeginChild("Aimbot", { Vec2(0, 0), Vec2(3, 10) });
			{
				ui::Checkbox("Enabled", &cfg->b["rage_enabled"]);
				ui::KeyBind("rage_enabled_bind_label", &cfg->i["rage_enabled_bind"], &cfg->i["rage_enabled_bind_style"]);
				ui::SingleSelect("Target selection", &cfg->i["rage_target_selection"], { "Highest damage", "Cycle", "Cycle (2x)", "Near crosshair", "Best hit chance" });
				ui::MultiSelect("Target hitbox", &cfg->m["rage_target_hitbox"], { "Head", "Chest", "Stomach", "Arms", "Legs", "Feet" });
				ui::MultiSelect("Multi-point", &cfg->m["rage_multi_point"], { "Head", "Chest", "Stomach", "Arms", "Legs", "Feet" });

				if (std::find_if(std::begin(cfg->m["rage_multi_point"]), std::end(cfg->m["rage_multi_point"]), [](auto p) { return p.second == true; }) != std::end(cfg->m["rage_multi_point"])) {
					ui::SingleSelect("", &cfg->i["rage_multi_point_amount"], {"Low", "Medium", "High"});
					ui::SliderInt("Multi-point scale", &cfg->i["rage_multi_point_scale"], 24, 100, cfg->i["rage_multi_point_scale"] == 24 ? "Auto" : "%d%%");
				}
				ui::KeyBind("rage_multi_point_label", &cfg->i["rage_multi_point_bind"], &cfg->i["rage_multi_point_bind_style"]);
				ui::Checkbox("Prefer safe point", &cfg->b["rage_prefer_safe_point"]);
				ui::Label("Force safe point");
				ui::KeyBind("rage_prefer_safe_point_label", &cfg->i["rage_prefer_safe_point_bind"], &cfg->i["rage_prefer_safe_point_bind_style"]);
				ui::MultiSelect("Avoid unsafe hitboxes", &cfg->m["rage_avoid_unsafe_hitboxes"], { "Head", "Chest", "Stomach", "Arms", "Legs", "Feet" });
				ui::Checkbox("Automatic fire", &cfg->b["rage_autofire"]);
				ui::Checkbox("Automatic penetration", &cfg->b["rage_auto_penetration"]);
				ui::Checkbox("Silent aim", &cfg->b["rage_silent_aim"]);
				ui::SliderInt("Minimum hit chance", &cfg->i["rage_hitchance"], 0, 100, cfg->i["rage_hitchance"] < 1 ? "Off" : "%d%%");
				ui::SliderInt("Minimum damage", &cfg->i["rage_mindmg"], 0, 126, cfg->i["rage_mindmg"] == 0 ? "Auto" : (cfg->i["rage_mindmg"] > 100 ? "HP+%d" : "%d"), (cfg->i["rage_mindmg"] > 100 ? 100 : 0));
				ui::Checkbox("Automatic scope", &cfg->b["rage_autoscope"]);
				ui::Checkbox("Reduce aim step", &cfg->b["rage_reduce_aimstep"]);
				ui::SliderInt("Maximum FOV", &cfg->i["rage_fov"], 0, 180, "%d°");
				ui::Checkbox("Log misses due to spread", &cfg->b["rage_log_misses"]);
				ui::MultiSelect("Low FPS mitigations", &cfg->m["rage_fps_mitigations"], { "Force low accuracy boost", "Disable multipoint: feet", "Disable multipoint: arms", "Disable multipoint: legs", "Disable hitbox: feet", "Force low multipoint", "Lower hit chance precision", "Limit targets per tick" });
			}
			ui::EndChild();

			break;
		}
	
		//
		// LEGIT
		//
		case 1:
		{
			const int wt = std::clamp(this->m_nCurrentLegitTab, 0, 5);
			const std::string prefix = "legit_w" + std::to_string(wt);
			const std::string globalizeKey = prefix + "_globalize";

			ui::BeginChild("Weapon type#Legit", { Vec2(0,0), Vec2(9, 0) }, GuiFlags_NoMove | GuiFlags_NoResize);
			ui::TabButton("G", &this->m_nCurrentLegitTab, 0, 6, GuiFlags_LegitTab);
			ui::TabButton("P", &this->m_nCurrentLegitTab, 1, 6, GuiFlags_LegitTab);
			ui::TabButton("W", &this->m_nCurrentLegitTab, 2, 6, GuiFlags_LegitTab);
			ui::TabButton("d", &this->m_nCurrentLegitTab, 3, 6, GuiFlags_LegitTab);
			ui::TabButton("f", &this->m_nCurrentLegitTab, 4, 6, GuiFlags_LegitTab);
			ui::TabButton("a", &this->m_nCurrentLegitTab, 5, 6, GuiFlags_LegitTab);
			ui::EndChild();
			ui::BeginChild("Aimbot#Legit", { Vec2(0, 2), Vec2(3, 8) });
			{
				ui::Checkbox("Enabled", &cfg->b["legit_aimbot"]);
				ui::KeyBind("legit_aimbot_key", &cfg->i["legit_aimbot_key"], &cfg->i["legit_aimbot_keystyle"]);
				ui::Checkbox("Always on", &cfg->b["legit_aimbot_always_on"]);

				ui::Label(weapons::GetLegitWeaponGroupName(wt));
				ui::Checkbox("Globalize this category", &cfg->b[globalizeKey]);
				if (cfg->b[globalizeKey]) {
					for (int i = 0; i <= 5; ++i) {
						if (i != wt)
							cfg->b["legit_w" + std::to_string(i) + "_globalize"] = false;
					}
				}

				ui::SingleSelect("Hitbox", &cfg->i[prefix + "_hitbox"], { "Head", "Neck", "Chest", "Stomach", "Pelvis", "Dynamic" });
				if (cfg->i[prefix + "_hitbox"] == 5)
					ui::SingleSelect("Dynamic mode", &cfg->i[prefix + "_dynamic_mode"], { "Nearest to crosshair", "Most damage", "Farthest distance" });
				ui::SliderFloat("FOV", &cfg->f[prefix + "_fov"], 1.f, 30.f, "%.1f", 0.1f);
				ui::SliderFloat("Smoothing", &cfg->f[prefix + "_smooth"], 1.f, 30.f, "%.1f", 0.1f);
				ui::Checkbox("Visible only", &cfg->b["legit_aimbot_visible_only"]);
				ui::Checkbox("Recoil control", &cfg->b["legit_aimbot_rcs"]);
				ui::Checkbox("Draw FOV circle", &cfg->b["legit_aimbot_fov_circle"]);
				ui::ColorPicker("legit_aimbot_fov_circle_color", cfg->c["legit_aimbot_fov_circle_color"]);
			}
			ui::EndChild();
			ui::BeginChild("Other#Legit", { Vec2(6, 2), Vec2(3, 8) });
			{
				static bool removeSpreadDisabled = false;
				ui::Checkbox("Remove spread", &removeSpreadDisabled, true, true);
				ui::Label("Disabled for protection", true);
				ui::Checkbox("Smoke check", &cfg->b["legit_smoke_check"]);
			}
			ui::EndChild();

			break;
		}
	
		//
		// VFX
		//
		case 2:
		{
			ui::BeginChild("Category#Visuals", { Vec2(0,0), Vec2(9, 0) }, GuiFlags_NoMove | GuiFlags_NoResize);
			ui::TabButton("\xEF\x88\xB5", &this->m_nCurrentVisualsTab, 0, 4, GuiFlags_VisualsTab);
			ui::TabButton("\xEF\x88\xB4", &this->m_nCurrentVisualsTab, 1, 4, GuiFlags_VisualsTab);
			ui::TabButton("\xEF\x82\xAC", &this->m_nCurrentVisualsTab, 2, 4, GuiFlags_VisualsTab);
			ui::TabButton("\xEF\x81\xAE", &this->m_nCurrentVisualsTab, 3, 4, GuiFlags_VisualsTab);
			ui::EndChild();

			switch (this->m_nCurrentVisualsTab)
			{
			case 0: // Enemy ESP
			{
				ui::BeginChild("Enemy ESP", { Vec2(0, 2), Vec2(3, 8) });
				{
					ui::Label("Activation type");
					ui::KeyBind("visuals_player_esp_activation_type", &cfg->i["visuals_player_esp_activation_type_key"], &cfg->i["visuals_player_esp_activation_type_keystyle"]);
					ui::Checkbox("Dormant", &cfg->b["visuals_player_esp_dormant"]);
					ui::Checkbox("Bounding box", &cfg->b["visuals_player_esp_bounding_box"]);
					ui::ColorPicker("visuals_player_esp_bounding_box_color", cfg->c["visuals_player_esp_bounding_box_color"]);
					ui::SingleSelect("Box style", &cfg->i["visuals_player_esp_box_style"], { "Full", "Corners" });
					ui::Checkbox("Filled box", &cfg->b["visuals_player_esp_filled_box"]);
					ui::ColorPicker("visuals_player_esp_filled_box_color", cfg->c["visuals_player_esp_filled_box_color"]);
					ui::Checkbox("Health bar", &cfg->b["visuals_player_esp_health_bar"]);
					ui::Checkbox("Health text", &cfg->b["visuals_player_esp_health_text"]);
					ui::Checkbox("Armor bar", &cfg->b["visuals_player_esp_armor_bar"]);
					ui::Checkbox("Name", &cfg->b["visuals_player_esp_name"]);
					ui::ColorPicker("visuals_player_esp_name_color", cfg->c["visuals_player_esp_name_color"]);
					ui::Checkbox("Snaplines", &cfg->b["visuals_player_esp_snaplines"]);
					ui::ColorPicker("visuals_player_esp_snaplines_color", cfg->c["visuals_player_esp_snaplines_color"]);
					ui::Checkbox("Head dot", &cfg->b["visuals_player_esp_head_dot"]);
					ui::ColorPicker("visuals_player_esp_head_dot_color", cfg->c["visuals_player_esp_head_dot_color"]);
					ui::ColorPicker("visuals_player_esp_skeleton_color", cfg->c["visuals_player_esp_skeleton_color"]);
					ui::Checkbox("Distance", &cfg->b["visuals_player_esp_distance"]);
					ui::Checkbox("Weapon", &cfg->b["visuals_player_esp_weapon"]);
					ui::ColorPicker("visuals_player_esp_weapon_color", cfg->c["visuals_player_esp_weapon_color"]);
					ui::Checkbox("Flags", &cfg->b["visuals_player_esp_flags"]);
					ui::Checkbox("Glow", &cfg->b["visuals_player_esp_glow"]);
					ui::ColorPicker("visuals_player_esp_glow_color", cfg->c["visuals_player_esp_glow_color"]);
					ui::SliderInt("Max distance", &cfg->i["visuals_player_esp_max_distance"], 100, 5000, "%d u");
				}
				ui::EndChild();

				ui::BeginChild("Enemy models", { Vec2(6, 2), Vec2(3, 8) });
				{
					ui::Checkbox("Chams", &cfg->b["visuals_chams"]);
					ui::Checkbox("Chams through walls", &cfg->b["visuals_chams_ignorez"]);
					ui::SingleSelect("Chams style", &cfg->i["visuals_chams_type"], { "Flat", "Solid", "Latex", "Chrome", "Glow" });
					ui::Label("Visible chams color");
					ui::ColorPicker("visuals_chams_visible_color", cfg->c["visuals_chams_visible_color"]);
					ui::Label("Behind wall color");
					ui::ColorPicker("visuals_chams_hidden_color", cfg->c["visuals_chams_hidden_color"]);
				}
				ui::EndChild();
				break;
			}
			case 1: // Teammate ESP
			{
				ui::BeginChild("Teammate ESP", { Vec2(0, 2), Vec2(3, 8) });
				{
					ui::Checkbox("Enabled", &cfg->b["visuals_player_esp_teammates"]);
					ui::Label("Activation type");
					ui::KeyBind("visuals_player_esp_activation_type", &cfg->i["visuals_player_esp_activation_type_key"], &cfg->i["visuals_player_esp_activation_type_keystyle"]);
					ui::Checkbox("Dormant", &cfg->b["visuals_player_esp_dormant"]);
					ui::Checkbox("Bounding box", &cfg->b["visuals_player_esp_bounding_box"]);
					ui::ColorPicker("visuals_player_esp_bounding_box_color", cfg->c["visuals_player_esp_bounding_box_color"]);
					ui::SingleSelect("Box style", &cfg->i["visuals_player_esp_box_style"], { "Full", "Corners" });
					ui::Checkbox("Filled box", &cfg->b["visuals_player_esp_filled_box"]);
					ui::ColorPicker("visuals_player_esp_filled_box_color", cfg->c["visuals_player_esp_filled_box_color"]);
					ui::Checkbox("Health bar", &cfg->b["visuals_player_esp_health_bar"]);
					ui::Checkbox("Health text", &cfg->b["visuals_player_esp_health_text"]);
					ui::Checkbox("Armor bar", &cfg->b["visuals_player_esp_armor_bar"]);
					ui::Checkbox("Name", &cfg->b["visuals_player_esp_name"]);
					ui::ColorPicker("visuals_player_esp_name_color", cfg->c["visuals_player_esp_name_color"]);
					ui::Checkbox("Snaplines", &cfg->b["visuals_player_esp_snaplines"]);
					ui::ColorPicker("visuals_player_esp_snaplines_color", cfg->c["visuals_player_esp_snaplines_color"]);
					ui::Checkbox("Head dot", &cfg->b["visuals_player_esp_head_dot"]);
					ui::ColorPicker("visuals_player_esp_head_dot_color", cfg->c["visuals_player_esp_head_dot_color"]);
					ui::ColorPicker("visuals_player_esp_skeleton_color", cfg->c["visuals_player_esp_skeleton_color"]);
					ui::Checkbox("Distance", &cfg->b["visuals_player_esp_distance"]);
					ui::Checkbox("Weapon", &cfg->b["visuals_player_esp_weapon"]);
					ui::ColorPicker("visuals_player_esp_weapon_color", cfg->c["visuals_player_esp_weapon_color"]);
					ui::Checkbox("Flags", &cfg->b["visuals_player_esp_flags"]);
					ui::Checkbox("Glow", &cfg->b["visuals_player_esp_glow"]);
					ui::ColorPicker("visuals_player_esp_glow_color", cfg->c["visuals_player_esp_glow_color"]);
					ui::SliderInt("Max distance", &cfg->i["visuals_player_esp_max_distance"], 100, 5000, "%d u");
				}
				ui::EndChild();

				ui::BeginChild("Teammate models", { Vec2(6, 2), Vec2(3, 8) });
				{
					ui::Checkbox("Chams", &cfg->b["visuals_chams_teammates"]);
					ui::Checkbox("Chams through walls", &cfg->b["visuals_chams_teammates_ignorez"]);
					ui::SingleSelect("Chams style", &cfg->i["visuals_chams_teammates_type"], { "Flat", "Solid", "Latex", "Chrome", "Glow" });
					ui::Label("Visible chams color");
					ui::ColorPicker("visuals_chams_teammates_visible_color", cfg->c["visuals_chams_teammates_visible_color"]);
					ui::Label("Behind wall color");
					ui::ColorPicker("visuals_chams_teammates_hidden_color", cfg->c["visuals_chams_teammates_hidden_color"]);
				}
				ui::EndChild();
				break;
			}
			case 2: // World ESP
			{
				ui::BeginChild("World ESP", { Vec2(0, 2), Vec2(3, 8) });
				{
					ui::Checkbox("Night mode", &cfg->b["visuals_night_mode"]);
					if (cfg->b["visuals_night_mode"]) {
						ui::SliderFloat("Exposure", &cfg->f["visuals_night_mode_exposure"], 0.05f, 2.f, "%.2f", 0.01f);
						ui::SliderFloat("Sky brightness", &cfg->f["visuals_night_mode_sky_brightness"], 0.1f, 3.f, "%.2f", 0.05f);
					}
					ui::Checkbox("Skybox tint", &cfg->b["visuals_skybox_tint"]);
					ui::ColorPicker("visuals_skybox_tint_color", cfg->c["visuals_skybox_tint_color"]);
					ui::Checkbox("Light modulation", &cfg->b["visuals_light_modulation"]);
					if (cfg->b["visuals_light_modulation"]) {
						ui::Checkbox("Disable lights", &cfg->b["visuals_light_disable"]);
						if (!cfg->b["visuals_light_disable"]) {
							ui::ColorPicker("visuals_light_color", cfg->c["visuals_light_color"]);
							ui::Checkbox("Light shadows", &cfg->b["visuals_light_shadows"]);
							ui::Checkbox("Baked shadows", &cfg->b["visuals_light_baked_shadows"]);
							ui::Checkbox("Change light rotation", &cfg->b["visuals_light_change_rotation"]);
							if (cfg->b["visuals_light_change_rotation"]) {
								ui::SliderFloat("Light rot X", &cfg->f["visuals_light_rot_x"], -180.f, 180.f, "%.0f", 1.f);
								ui::SliderFloat("Light rot Y", &cfg->f["visuals_light_rot_y"], -180.f, 180.f, "%.0f", 1.f);
							}
						}
					}
				}
				ui::EndChild();
				break;
			}
			case 3: // Other ESP
			{
				ui::BeginChild("Other ESP", { Vec2(0, 2), Vec2(3, 8) });
				{
					ui::Checkbox("OOF arrows", &cfg->b["visuals_oof_arrows"]);
					ui::ColorPicker("visuals_oof_arrows_color", cfg->c["visuals_oof_arrows_color"]);
					ui::SliderInt("OOF radius", &cfg->i["visuals_oof_arrows_radius"], 60, 300, "%d");
					ui::SliderInt("OOF size", &cfg->i["visuals_oof_arrows_size"], 6, 24, "%d");
					ui::Checkbox("Bomb timer", &cfg->b["visuals_bomb_timer"]);
					ui::Checkbox("Spectator list", &cfg->b["visuals_spectator_list"]);
					ui::Checkbox("Bullet tracers", &cfg->b["visuals_bullet_tracers"]);
				}
				ui::EndChild();
				break;
			}
			}

			break;
		}
	
		//
		// MISCELLANEOUS
		//
		case 3:
		{
			ui::BeginChild("Miscellaneous", { Vec2(0,0), Vec2(3, 10) });
			{
				ui::Checkbox("Watermark", &cfg->b["misc_watermark"]);
				ui::Checkbox("Show FPS", &cfg->b["misc_watermark_fps"]);
				ui::Checkbox("Show time", &cfg->b["misc_watermark_time"]);
				ui::Checkbox("Show username", &cfg->b["misc_watermark_username"]);
				ui::Checkbox("Show ping", &cfg->b["misc_watermark_ping"]);
				if (cfg->b["misc_watermark_ping"])
					ui::SliderInt("Ping value", &cfg->i["misc_watermark_ping_value"], 0, 200, "%dms");
				//ui::Checkbox("Hit logs", &cfg->b["misc_hit_logs"]);
				ui::Checkbox("Keybind list", &cfg->b["misc_keybind_list"]);
				ui::Checkbox("Bunny hop", &cfg->b["misc_bhop"]);
				ui::Checkbox("No flash", &cfg->b["misc_antiflash"]);
				ui::Checkbox("Media player", &cfg->b["misc_spotify_player"]);
				//ui::Checkbox("Walkbot", &cfg->b["misc_walkbot"]);
				//ui::KeyBind("misc_walkbot_key", &cfg->i["misc_walkbot_key"], &cfg->i["misc_walkbot_keystyle"]);
				//ui::Label("Nav folder path");
				//ui::InputText("walkbot_nav_path", cfg->s["misc_walkbot_nav_path"], GuiFlags_InputPath, sizeof(cfg->s["misc_walkbot_nav_path"]));
				//ui::Checkbox("Viewmodel FOV", &cfg->b["misc_viewmodel_fov"]);
				//if (cfg->b["misc_viewmodel_fov"])
				//	ui::SliderFloat("Viewmodel FOV", &cfg->f["misc_viewmodel_fov"], 54.f, 90.f, "%.0f", 1.f);
				ui::Checkbox("Third person", &cfg->b["misc_third_person"]);
				ui::KeyBind("misc_third_person_key", &cfg->i["misc_third_person_key"], &cfg->i["misc_third_person_keystyle"]);
				ui::Checkbox("View glitch", &cfg->b["misc_tp_glitch"]);
				ui::KeyBind("misc_tp_glitch_key", &cfg->i["misc_tp_glitch_key"], &cfg->i["misc_tp_glitch_keystyle"]);
				if (cfg->b["misc_tp_glitch"]) {
					ui::SliderFloat("Wall trace distance", &cfg->f["misc_tp_glitch_distance"], 32.f, 256.f, "%.0f", 1.f);
					ui::SliderFloat("View depth", &cfg->f["misc_tp_glitch_through"], 4.f, 64.f, "%.0f", 1.f);
				}
			}
			ui::EndChild();

			break;
		}
	
		//
		// SKINS & MODELS
		//
		case 4:
		{
			ui::BeginChild("Model options", { Vec2(0,0), Vec2(3, 10) }, GuiFlags_NoMove | GuiFlags_NoResize);
			SkinsTab::DrawModelOptions();
			ui::EndChild();
			ui::BeginChild("Weapon skin#Skins", { Vec2(6, 0), Vec2(3, 10) }, GuiFlags_NoMove | GuiFlags_NoResize);
			SkinsTab::DrawWeaponSkins();
			ui::EndChild();

			break;
		}
	
		//
		// PLAYERS
		//	btw, someone fix tab icon thx (if not fixed yet) -mtfy
		//
		case 5:
		{
			ui::BeginChild("Players", { Vec2(0,0), Vec2(3, 10) });
			PlayersTab::DrawPlayers();
			ui::EndChild();
			ui::BeginChild("Adjustments", { Vec2(6, 0), Vec2(3, 10) });
			PlayersTab::DrawAdjustments();
			ui::EndChild();

			break;
		}
		
		//
		//
		// CFG & LUA
		//
		case 6:
		{
			static char cloud_name[64] = "";
			static char cloud_desc[256] = "";
			static std::vector<cloud_config::CloudConfigEntry> cloud_entries;
			static std::mutex cloud_entries_mutex;
			static std::atomic<bool> operation_in_progress(false);
			static std::string operation_status = "Not initialized";
			static int selected_cloud_index = -1;
			static bool cloud_initialized = false;

			if (!cloud_initialized) {
				if (cloud_config::Initialize()) {
					operation_status = "Loaded session: " + cloud_config::GetUsername();
					operation_in_progress = true;
					std::thread([]() {
						std::vector<cloud_config::CloudConfigEntry> list;
						auto res = cloud_config::FetchList(list);
						std::lock_guard<std::mutex> lock(cloud_entries_mutex);
						if (res == cloud_config::CloudConfigResult::Success) {
							cloud_entries = list;
							operation_status = "Loaded " + std::to_string(list.size()) + " configs";
						} else if (res == cloud_config::CloudConfigResult::AuthError) {
							operation_status = "Error: Authentication failed";
						} else {
							operation_status = "Error: Failed to fetch list";
						}
						operation_in_progress = false;
					}).detach();
				} else {
					operation_status = "Error: No active session found";
				}
				cloud_initialized = true;
			}

			ui::BeginChild("Presets", { Vec2(0, 0), Vec2(3, 10) });

			if (ui::BeginListbox("ConfigsList")) 
			{
				/*ui::Selectable("KAKI", false);
				ui::Selectable("KAKIasdsda", false);*/ // hmm? /mtfy

				for (auto config : CConfig::get()->List) {
					if (ui::Selectable(config.c_str(), strcmp(cfg->s["config_name"], config.c_str()) == 0))
						strcpy_s(cfg->s["config_name"], config.c_str());
				}
			}
			ui::EndListbox();

			ui::InputText("cfg_input", cfg->s["config_name"], NULL, 64, "Config name");

			if (ui::Button("Load"))
				CConfig::get()->Load();

			if (ui::Button("Save"))
				CConfig::get()->Save();
		
			if (ui::Button("Delete"))
				CConfig::get()->Delete();

			if (ui::Button("Reset"))
				CConfig::get()->LoadDefaults();

			if (ui::Button("Import from clipboard"))
				CConfig::get()->ImportFromClipboard();

			if (ui::Button("Export to clipboard"))
				CConfig::get()->ExportToClipboard();

			ui::EndChild();

			// Cloud configs panel
			ui::BeginChild("Cloud configs", { Vec2(6, 6), Vec2(3, 4) });
			{
				if (ui::BeginListbox("CloudConfigsList")) {
					std::lock_guard<std::mutex> lock(cloud_entries_mutex);
					for (size_t idx = 0; idx < cloud_entries.size(); ++idx) {
						const auto& entry = cloud_entries[idx];
						std::string label = entry.name + " (by " + entry.author + ")";
						if (ui::Selectable(label.c_str(), selected_cloud_index == static_cast<int>(idx))) {
							selected_cloud_index = static_cast<int>(idx);
						}
					}
				}
				ui::EndListbox();

				ui::Label(operation_status.c_str());

				if (!operation_in_progress.load()) {
					if (ui::Button("Refresh list")) {
						operation_in_progress = true;
						operation_status = "Refreshing...";
						std::thread([]() {
							std::vector<cloud_config::CloudConfigEntry> list;
							auto res = cloud_config::FetchList(list);
							std::lock_guard<std::mutex> lock(cloud_entries_mutex);
							if (res == cloud_config::CloudConfigResult::Success) {
								cloud_entries = list;
								operation_status = "Loaded " + std::to_string(list.size()) + " configs";
							} else if (res == cloud_config::CloudConfigResult::AuthError) {
								operation_status = "Error: Authentication failed";
							} else {
								operation_status = "Error: Network error";
							}
							operation_in_progress = false;
						}).detach();
					}

					if (selected_cloud_index >= 0) {
						bool valid_index = false;
						int config_id = -1;
						{
							std::lock_guard<std::mutex> lock(cloud_entries_mutex);
							if (selected_cloud_index < static_cast<int>(cloud_entries.size())) {
								config_id = cloud_entries[selected_cloud_index].id;
								valid_index = true;
							}
						}

						if (valid_index) {
							if (ui::Button("Load selected")) {
								operation_in_progress = true;
								operation_status = "Loading config...";
								std::thread([config_id]() {
									std::string data;
									auto res = cloud_config::FetchConfig(config_id, data);
									if (res == cloud_config::CloudConfigResult::Success) {
										char tempPath[MAX_PATH]{};
										GetTempPathA(MAX_PATH, tempPath);
										strcat_s(tempPath, "gamesense-cs2-cloud.cfg");
										
										std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
										if (file) {
											file << data;
											file.close();
											CConfig::get()->LoadFromPath(tempPath);
											std::remove(tempPath);
											operation_status = "Loaded cloud config!";
										} else {
											operation_status = "Error: Local IO failed";
										}
									} else if (res == cloud_config::CloudConfigResult::AuthError) {
										operation_status = "Error: Session expired";
									} else if (res == cloud_config::CloudConfigResult::NotFound) {
										operation_status = "Error: Config not found";
									} else {
										operation_status = "Error: Network error";
									}
									operation_in_progress = false;
								}).detach();
							}
						}
					}

					ui::Label("Share config");
					ui::InputText("cloud_name_input", cloud_name, NULL, 64, "Config name");
					ui::InputText("cloud_desc_input", cloud_desc, NULL, 256, "Config description");

					if (ui::Button("Upload current config")) {
						std::string name_str = cloud_name;
						std::string desc_str = cloud_desc;
						
						if (name_str.empty()) {
							operation_status = "Error: Enter name";
						} else {
							operation_in_progress = true;
							operation_status = "Uploading config...";
							
							std::thread([name_str, desc_str]() {
								char tempPath[MAX_PATH]{};
								GetTempPathA(MAX_PATH, tempPath);
								strcat_s(tempPath, "gamesense-cs2-cloud-upload.cfg");
								
								CConfig::get()->SaveToPath(tempPath);
								
								std::ifstream file(tempPath, std::ios::binary);
								std::string data;
								if (file) {
									std::ostringstream ss;
									ss << file.rdbuf();
									data = ss.str();
									file.close();
								}
								std::remove(tempPath);
								
								if (data.empty()) {
									operation_status = "Error: Config read failed";
									operation_in_progress = false;
									return;
								}
								
								auto res = cloud_config::UploadConfig(name_str, desc_str, data);
								if (res == cloud_config::CloudConfigResult::Success) {
									operation_status = "Uploaded config!";
									std::vector<cloud_config::CloudConfigEntry> list;
									auto refresh_res = cloud_config::FetchList(list);
									std::lock_guard<std::mutex> lock(cloud_entries_mutex);
									if (refresh_res == cloud_config::CloudConfigResult::Success) {
										cloud_entries = list;
									}
								} else if (res == cloud_config::CloudConfigResult::AuthError) {
									operation_status = "Error: Session expired";
								} else {
									operation_status = "Error: Network error";
								}
								operation_in_progress = false;
							}).detach();
						}
					}
				}
			}
			ui::EndChild();

			ui::BeginChild("Settings", { Vec2(6, 0), Vec2(3, 4) });
			{
				ui::Label("Menu key");
				ui::KeyBind("misc_menu_key", &cfg->i["menu_key"], &cfg->i["misc_menu_keystyle"]);
				ui::Label("Menu color");
				ui::ColorPicker("MenuColor", cfg->c["MenuColor"]);
				ui::SliderInt("Menu animation speed", &cfg->i["menu_animation_speed"], 25, 200, cfg->i["menu_animation_speed"] <= 25 ? "Off" : "%d%%");
				ui::Checkbox("Menu confirmations", &cfg->b["menu_confirmations"]);
				ui::SingleSelect("DPI scale", &cfg->i["menu_scale"], { "100%", "125%", "150%", "175%", "200%" });
				ui::SingleSelect("Anti-untrusted", &cfg->i["misc_anti_untrusted_mode"], { "Automatic", "None" });
				ui::Checkbox("Streamer mode", &cfg->b["misc_hide_from_obs"]);
				if (ui::Button("Reset menu layout")) {
					ui::ResetChildLayouts(Globals::Gui_Ctx->ChildLayoutVersion + 1);
				}
			}
			ui::EndChild();

			break;
		}

		//
		// LUA
		//
		case 7:
		{
			LuaTab::Draw();
			break;
		}
	}

	// Update Spotify status
	Spotify::Update();

	if (cfg->b["misc_spotify_player"] && !cfg->b["misc_hide_from_obs"]) {
		GuiWindow* parent_window = ui::GetCurrentWindow(); // "Main" window
		if (parent_window) {
			const float scale = ui::GetDpiScale();
			
			// Coordinates aligning with the child windows
			float x = parent_window->Pos.x + ui::Scale(99.f);
			float w = parent_window->Size.x - ui::Scale(122.f);
			float h = ui::Scale(42.f);
			float y = parent_window->Pos.y + parent_window->Size.y - ui::Scale(70.f);

			// Draw Background
			Render::Draw->FilledRect(Vec2(x, y), Vec2(w, h), D3DCOLOR_RGBA(17, 17, 17, Globals::Gui_Ctx->MenuAlpha));
			// Draw Top highlight line
			Render::Draw->FilledRect(Vec2(x + ui::Scale(1.f), y + ui::Scale(1.f)), Vec2(w - ui::Scale(2.f), ui::Scale(1.f)), D3DCOLOR_RGBA(50, 50, 50, Globals::Gui_Ctx->MenuAlpha));
			// Draw Border
			Render::Draw->Rect(Vec2(x, y), Vec2(w, h), ui::Scale(1.f), D3DCOLOR_RGBA(40, 40, 40, Globals::Gui_Ctx->MenuAlpha));

			// Left Side: Text Details
			// "Now Playing" Header in menu matching color
			Render::Draw->Text("NOW PLAYING", x + ui::Scale(10.f), y + ui::Scale(6.f), LEFT, Render::Fonts::Tahombd, false, CMenu::get()->GetMenuColor());
			
			// Song details with clipping bounds
			std::string track = Spotify::GetCurrentTrack();
			Render::Draw->Text(track.c_str(), x + ui::Scale(10.f), y + ui::Scale(20.f), LEFT, Render::Fonts::Tahombd, false, D3DCOLOR_RGBA(205, 205, 205, Globals::Gui_Ctx->MenuAlpha), Vec2(x + w - ui::Scale(120.f), y + ui::Scale(35.f)));

			// Right Side: Control Buttons
			float btn_w = ui::Scale(26.f);
			float btn_h = ui::Scale(26.f);
			float btn_y = y + (h / 2.f) - (btn_h / 2.f);

			// Previous Button
			float prev_x = x + w - ui::Scale(105.f);
			bool prev_hovered = ui::IsInside(prev_x, btn_y, btn_w, btn_h);
			D3DCOLOR prev_bg = prev_hovered ? D3DCOLOR_RGBA(40, 40, 40, Globals::Gui_Ctx->MenuAlpha) : D3DCOLOR_RGBA(25, 25, 25, Globals::Gui_Ctx->MenuAlpha);
			D3DCOLOR prev_icon_color = prev_hovered ? CMenu::get()->GetMenuColor() : D3DCOLOR_RGBA(200, 200, 200, Globals::Gui_Ctx->MenuAlpha);
			Render::Draw->FilledRect(Vec2(prev_x, btn_y), Vec2(btn_w, btn_h), prev_bg);
			Render::Draw->Rect(Vec2(prev_x, btn_y), Vec2(btn_w, btn_h), ui::Scale(1.f), D3DCOLOR_RGBA(12, 12, 12, Globals::Gui_Ctx->MenuAlpha));
			
			// Draw Previous Icon
			float prev_cx = prev_x + (btn_w / 2.f);
			float prev_cy = btn_y + (btn_h / 2.f);
			// Left bar
			Render::Draw->FilledRect(Vec2(prev_cx - ui::Scale(5.f), prev_cy - ui::Scale(4.f)), Vec2(ui::Scale(2.f), ui::Scale(9.f)), prev_icon_color);
			// Left triangle
			Render::Draw->Triangle(
				Vec2(prev_cx - ui::Scale(3.f), prev_cy),
				Vec2(prev_cx + ui::Scale(3.f), prev_cy - ui::Scale(4.f)),
				Vec2(prev_cx + ui::Scale(3.f), prev_cy + ui::Scale(4.f)),
				prev_icon_color
			);
			
			if (prev_hovered && ui::KeyPressed(VK_LBUTTON)) {
				Spotify::PrevTrack();
			}

			// Play/Pause Button
			float play_x = x + w - ui::Scale(71.f);
			bool play_hovered = ui::IsInside(play_x, btn_y, btn_w, btn_h);
			D3DCOLOR play_bg = play_hovered ? D3DCOLOR_RGBA(40, 40, 40, Globals::Gui_Ctx->MenuAlpha) : D3DCOLOR_RGBA(25, 25, 25, Globals::Gui_Ctx->MenuAlpha);
			D3DCOLOR play_icon_color = play_hovered ? CMenu::get()->GetMenuColor() : D3DCOLOR_RGBA(200, 200, 200, Globals::Gui_Ctx->MenuAlpha);
			Render::Draw->FilledRect(Vec2(play_x, btn_y), Vec2(btn_w, btn_h), play_bg);
			Render::Draw->Rect(Vec2(play_x, btn_y), Vec2(btn_w, btn_h), ui::Scale(1.f), D3DCOLOR_RGBA(12, 12, 12, Globals::Gui_Ctx->MenuAlpha));

			// Draw Play/Pause Icon depending on status
			float play_cx = play_x + (btn_w / 2.f);
			float play_cy = btn_y + (btn_h / 2.f);
			bool is_playing = Spotify::IsPlaying();
			if (is_playing) {
				// Pause: two vertical bars
				Render::Draw->FilledRect(Vec2(play_cx - ui::Scale(4.f), play_cy - ui::Scale(5.f)), Vec2(ui::Scale(2.f), ui::Scale(11.f)), play_icon_color);
				Render::Draw->FilledRect(Vec2(play_cx + ui::Scale(2.f), play_cy - ui::Scale(5.f)), Vec2(ui::Scale(2.f), ui::Scale(11.f)), play_icon_color);
			} else {
				// Play: triangle pointing right
				Render::Draw->Triangle(
					Vec2(play_cx + ui::Scale(5.f), play_cy),
					Vec2(play_cx - ui::Scale(3.f), play_cy - ui::Scale(5.f)),
					Vec2(play_cx - ui::Scale(3.f), play_cy + ui::Scale(5.f)),
					play_icon_color
				);
			}

			if (play_hovered && ui::KeyPressed(VK_LBUTTON)) {
				Spotify::PlayPause();
			}

			// Next Button
			float next_x = x + w - ui::Scale(37.f);
			bool next_hovered = ui::IsInside(next_x, btn_y, btn_w, btn_h);
			D3DCOLOR next_bg = next_hovered ? D3DCOLOR_RGBA(40, 40, 40, Globals::Gui_Ctx->MenuAlpha) : D3DCOLOR_RGBA(25, 25, 25, Globals::Gui_Ctx->MenuAlpha);
			D3DCOLOR next_icon_color = next_hovered ? CMenu::get()->GetMenuColor() : D3DCOLOR_RGBA(200, 200, 200, Globals::Gui_Ctx->MenuAlpha);
			Render::Draw->FilledRect(Vec2(next_x, btn_y), Vec2(btn_w, btn_h), next_bg);
			Render::Draw->Rect(Vec2(next_x, btn_y), Vec2(btn_w, btn_h), ui::Scale(1.f), D3DCOLOR_RGBA(12, 12, 12, Globals::Gui_Ctx->MenuAlpha));

			// Draw Next Icon
			float next_cx = next_x + (btn_w / 2.f);
			float next_cy = btn_y + (btn_h / 2.f);
			// Right triangle
			Render::Draw->Triangle(
				Vec2(next_cx + ui::Scale(3.f), next_cy),
				Vec2(next_cx - ui::Scale(3.f), next_cy - ui::Scale(4.f)),
				Vec2(next_cx - ui::Scale(3.f), next_cy + ui::Scale(4.f)),
				next_icon_color
			);
			// Right bar
			Render::Draw->FilledRect(Vec2(next_cx + ui::Scale(4.f), next_cy - ui::Scale(4.f)), Vec2(ui::Scale(2.f), ui::Scale(9.f)), next_icon_color);

			if (next_hovered && ui::KeyPressed(VK_LBUTTON)) {
				Spotify::NextTrack();
			}
		}
	}

	ui::End();
}

bool CMenu::IsMenuOpened() 
{
	return this->m_bIsOpened;
}

void CMenu::SetMenuOpened(bool v) 
{
	this->m_bIsOpened = v;
}

D3DCOLOR CMenu::GetMenuColor() {
	GuiContext* g = Globals::Gui_Ctx;
	CConfig* cfg = CConfig::get();
	return D3DCOLOR_RGBA(cfg->c["MenuColor"][0], cfg->c["MenuColor"][1], cfg->c["MenuColor"][2], min(cfg->c["MenuColor"][3], g->MenuAlpha));
}