#define _CRT_SECURE_NO_WARNINGS
#include "Config.h"
#include "../Utilities/Utilities.h"
#include "../../Backend/Globalincludes.h"
#include "../../nerv/config.hpp"
#include "../../nerv/features/skin_changer/skin_changer.hpp"
#include "../../cs2/model_changer.h"
#include "../../cs2/agent_changer.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ShlObj.h>

#pragma comment(lib, "shell32.lib")

std::string color_to_string(int col[4]) {
	return std::to_string((int)(col[0])) + "," + std::to_string((int)(col[1])) + "," + std::to_string((int)(col[2])) + "," + std::to_string((int)(col[3]));
	/*char hexcol[16];
	snprintf(hexcol, sizeof hexcol, "%02X%02X%02X%02X", col[0], col[1], col[2], col[3]);
	return hexcol;*/
}

float* string_to_color(std::string s) {
	static auto split = [](std::string str, const char* del) -> std::vector<std::string>
	{
		char* pTempStr = _strdup(str.c_str());
		char* pWord = strtok(pTempStr, del);
		std::vector<std::string> dest;

		while (pWord != NULL)
		{
			dest.push_back(pWord);
			pWord = strtok(NULL, del);
		}

		free(pTempStr);

		return dest;
	};

	std::vector<std::string> col = split(s, ",");
	return new float[4]{
		(float)std::stoi(col.at(0)),
		(float)std::stoi(col.at(1)),
		(float)std::stoi(col.at(2)),
		(float)std::stoi(col.at(3))
	};
}

void CConfig::GetFileName(char* _buffer, bool db) {
	(void)db;
	const char* name = this->s["config_name"];
	if (!name || name[0] == '\0')
		strcpy_s(_buffer, 255, "default");
	else
		strcpy_s(_buffer, 255, name);
}

void CConfig::GetFilePath(char* _buffer, bool db) {
	char filename[255]{};
	GetFileName(filename);

	char base[MAX_PATH]{};
	if (FAILED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, base)))
		GetCurrentDirectoryA(MAX_PATH, base);

	char buffer[MAX_PATH]{};
	if (!db)
		sprintf_s(buffer, "%s\\gamesense-cs2\\%s.cfg", base, filename);
	else
		sprintf_s(buffer, "%s\\gamesense-cs2", base);

	strcpy_s(_buffer, 260, buffer);
}

void CConfig::LoadDefaults() {
	CConfig* cfg = CConfig::get();
	if (cfg->s["config_name"][0] == '\0')
		strcpy_s(cfg->s["config_name"], "default");
	cfg->c["MenuColor"][0] = 163;
	cfg->c["MenuColor"][1] = 212;
	cfg->c["MenuColor"][2] = 31;
	cfg->c["MenuColor"][3] = 255;

	// RAGE
	cfg->b["rage_remove_recoil"] = false;
	cfg->i["rage_accuracy_boost"] = 0;
	cfg->b["rage_delay_shot"] = false;
	cfg->b["rage_quick_stop"] = false;
	cfg->i["rage_quick_stop_bind"] = 0;
	cfg->i["rage_quick_stop_bind_style"] = 0;
	cfg->b["rage_quick_peek_assist"] = false;
	cfg->i["rage_quick_peek_bind"] = 0;
	cfg->i["rage_quick_peek_bind_style"] = 0;
	cfg->i["rage_quick_peek_distance"] = 16;
	cfg->b["rage_aa_correction"] = false;
	cfg->i["rage_correction_override_bind"] = 0;
	cfg->i["rage_correction_override_bind_style"] = 0;
	cfg->b["rage_prefer_body_aim"] = false;
	cfg->i["rage_baim_bind"] = 0;
	cfg->i["rage_baim_bind_style"] = 0;
	cfg->b["rage_force_baim"] = false;
	cfg->i["rage_fakeduck_bind"] = 0;
	cfg->i["rage_fakeduck_bind_style"] = 0;
	cfg->b["rage_doubletap"] = false;
	cfg->i["rage_doubletap_bind"] = 0;
	cfg->i["rage_doubletap_bind_style"] = 0;
	cfg->i["rage_dooubletap_mode"] = 0;
	cfg->i["rage_dt_hitchance"] = 0;
	cfg->i["rage_dt_fakelag_limit"] = 0;
	cfg->b["rage_enabled"] = false;
	cfg->i["rage_enabled_bind"] = 0;
	cfg->i["rage_enabled_bind_style"] = 0;
	cfg->i["rage_target_selection"] = 0;
	cfg->i["rage_multi_point_amount"] = 0;
	cfg->i["rage_multi_point_scale"] = 24;
	cfg->i["rage_multi_point_bind"] = 0;
	cfg->i["rage_multi_point_bind_style"] = 0;
	cfg->b["rage_prefer_safe_point"] = false;
	cfg->i["rage_prefer_safe_point_bind"] = 0;
	cfg->i["rage_prefer_safe_point_bind_style"] = 0;
	cfg->b["rage_autofire"] = false;
	cfg->b["rage_auto_penetration"] = false;
	cfg->b["rage_silent_aim"] = false;
	cfg->i["rage_hitchance"] = 0;
	cfg->i["rage_mindmg"] = 0;
	cfg->b["rage_autoscope"] = false;
	cfg->b["rage_reduce_aimstep"] = false;
	cfg->i["rage_fov"] = 0;
	cfg->b["rage_log_misses"] = false;
	cfg->c["quick_peek_assist_colorpicker"][0] = 255;
	cfg->c["quick_peek_assist_colorpicker"][1] = 255;
	cfg->c["quick_peek_assist_colorpicker"][2] = 255;
	cfg->c["quick_peek_assist_colorpicker"][3] = 255;

	cfg->b["misc_bhop"] = false;
	cfg->b["misc_antiflash"] = false;
	cfg->b["misc_hide_from_obs"] = false;
	cfg->b["misc_spotify_player"] = false;
	cfg->b["misc_walkbot"] = false;
	cfg->i["misc_walkbot_key"] = 'X';
	cfg->i["misc_walkbot_keystyle"] = 2;
	cfg->s["misc_walkbot_nav_path"][0] = '\0';
	cfg->b["misc_viewmodel_fov"] = false;
	cfg->f["misc_viewmodel_fov"] = 68.f;
	// AA
	cfg->i["aa_yaw_180"] = 8;
	cfg->i["aa_yaw_spin"] = 80;
	cfg->i["aa_fake_yaw_limit"] = 60;
	//AA OTHER
	cfg->i["aa_fakelag_limit"] = 13;
	//VISUALS
	cfg->b["visuals_player_esp_bounding_box"] = false;
	cfg->b["visuals_player_esp_health_bar"] = false;
	cfg->b["visuals_player_esp_name"] = false;
	cfg->b["visuals_player_esp_teammates"] = false;
	cfg->b["visuals_player_esp_dormant"] = false;
	cfg->b["visuals_player_esp_flags"] = false;
	cfg->b["visuals_player_esp_filled_box"] = false;
	cfg->b["visuals_player_esp_health_text"] = false;
	cfg->b["visuals_player_esp_armor_bar"] = false;
	cfg->b["visuals_player_esp_snaplines"] = false;
	cfg->b["visuals_player_esp_head_dot"] = false;
	cfg->b["visuals_player_esp_skeleton"] = false;
	cfg->b["visuals_player_esp_distance"] = false;
	cfg->i["visuals_player_esp_box_style"] = 0;
	cfg->i["visuals_player_esp_max_distance"] = 3000;

	cfg->c["visuals_player_esp_bounding_box_color"][0] = 255;
	cfg->c["visuals_player_esp_bounding_box_color"][1] = 255;
	cfg->c["visuals_player_esp_bounding_box_color"][2] = 255;
	cfg->c["visuals_player_esp_bounding_box_color"][3] = 130;

	cfg->c["visuals_player_esp_filled_box_color"][0] = 255;
	cfg->c["visuals_player_esp_filled_box_color"][1] = 255;
	cfg->c["visuals_player_esp_filled_box_color"][2] = 255;
	cfg->c["visuals_player_esp_filled_box_color"][3] = 40;

	cfg->c["visuals_player_esp_name_color"][0] = 255;
	cfg->c["visuals_player_esp_name_color"][1] = 255;
	cfg->c["visuals_player_esp_name_color"][2] = 255;
	cfg->c["visuals_player_esp_name_color"][3] = 200;

	cfg->c["visuals_player_esp_snaplines_color"][0] = 163;
	cfg->c["visuals_player_esp_snaplines_color"][1] = 212;
	cfg->c["visuals_player_esp_snaplines_color"][2] = 31;
	cfg->c["visuals_player_esp_snaplines_color"][3] = 180;

	cfg->c["visuals_player_esp_head_dot_color"][0] = 255;
	cfg->c["visuals_player_esp_head_dot_color"][1] = 80;
	cfg->c["visuals_player_esp_head_dot_color"][2] = 80;
	cfg->c["visuals_player_esp_head_dot_color"][3] = 255;

	cfg->c["visuals_player_esp_skeleton_color"][0] = 255;
	cfg->c["visuals_player_esp_skeleton_color"][1] = 255;
	cfg->c["visuals_player_esp_skeleton_color"][2] = 255;
	cfg->c["visuals_player_esp_skeleton_color"][3] = 220;
	//MISC
	cfg->b["misc_watermark"] = false;
	cfg->b["misc_watermark_fps"] = false;
	cfg->b["misc_watermark_time"] = false;
	cfg->b["misc_watermark_username"] = false;
	cfg->b["misc_watermark_ping"] = false;
	cfg->b["misc_viewmodel_fov"] = false;
	cfg->f["misc_viewmodel_fov"] = 68.f;
	cfg->i["misc_watermark_ping_value"] = 0;
	cfg->b["misc_hit_logs"] = false;

	cfg->b["visuals_player_esp_weapon"] = false;
	cfg->c["visuals_player_esp_weapon_color"][0] = 255;
	cfg->c["visuals_player_esp_weapon_color"][1] = 255;
	cfg->c["visuals_player_esp_weapon_color"][2] = 255;
	cfg->c["visuals_player_esp_weapon_color"][3] = 220;

	cfg->b["visuals_oof_arrows"] = false;
	cfg->i["visuals_oof_arrows_radius"] = 140;
	cfg->i["visuals_oof_arrows_size"] = 12;
	cfg->c["visuals_oof_arrows_color"][0] = 163;
	cfg->c["visuals_oof_arrows_color"][1] = 212;
	cfg->c["visuals_oof_arrows_color"][2] = 31;
	cfg->c["visuals_oof_arrows_color"][3] = 255;

	cfg->b["visuals_bomb_timer"] = false;
	cfg->b["visuals_spectator_list"] = false;
	cfg->b["visuals_night_mode"] = false;
	cfg->f["visuals_night_mode_exposure"] = 1.2f;
	cfg->f["visuals_night_mode_sky_brightness"] = 2.f;
	cfg->b["visuals_skybox_tint"] = false;
	cfg->c["visuals_skybox_tint_color"][0] = 255;
	cfg->c["visuals_skybox_tint_color"][1] = 255;
	cfg->c["visuals_skybox_tint_color"][2] = 255;
	cfg->c["visuals_skybox_tint_color"][3] = 255;
	cfg->b["visuals_light_modulation"] = false;
	cfg->b["visuals_light_disable"] = false;
	cfg->c["visuals_light_color"][0] = 255;
	cfg->c["visuals_light_color"][1] = 255;
	cfg->c["visuals_light_color"][2] = 255;
	cfg->c["visuals_light_color"][3] = 255;
	cfg->b["visuals_light_shadows"] = false;
	cfg->b["visuals_light_baked_shadows"] = false;
	cfg->b["visuals_light_change_rotation"] = false;
	cfg->f["visuals_light_rot_x"] = 0.f;
	cfg->f["visuals_light_rot_y"] = 0.f;
	cfg->b["visuals_player_esp_glow"] = false;
	cfg->c["visuals_player_esp_glow_color"][0] = 255;
	cfg->c["visuals_player_esp_glow_color"][1] = 0;
	cfg->c["visuals_player_esp_glow_color"][2] = 0;
	cfg->c["visuals_player_esp_glow_color"][3] = 255;
	cfg->b["visuals_chams"] = false;
	cfg->b["visuals_chams_ignorez"] = false;
	cfg->i["visuals_chams_type"] = 0;
	cfg->c["visuals_chams_visible_color"][0] = 163;
	cfg->c["visuals_chams_visible_color"][1] = 212;
	cfg->c["visuals_chams_visible_color"][2] = 31;
	cfg->c["visuals_chams_visible_color"][3] = 255;
	cfg->c["visuals_chams_hidden_color"][0] = 255;
	cfg->c["visuals_chams_hidden_color"][1] = 60;
	cfg->c["visuals_chams_hidden_color"][2] = 60;
	cfg->c["visuals_chams_hidden_color"][3] = 200;
	cfg->b["visuals_chams_teammates"] = false;
	cfg->b["visuals_chams_teammates_ignorez"] = false;
	cfg->i["visuals_chams_teammates_type"] = 0;
	cfg->c["visuals_chams_teammates_visible_color"][0] = 163;
	cfg->c["visuals_chams_teammates_visible_color"][1] = 212;
	cfg->c["visuals_chams_teammates_visible_color"][2] = 31;
	cfg->c["visuals_chams_teammates_visible_color"][3] = 255;
	cfg->c["visuals_chams_teammates_hidden_color"][0] = 255;
	cfg->c["visuals_chams_teammates_hidden_color"][1] = 60;
	cfg->c["visuals_chams_teammates_hidden_color"][2] = 60;
	cfg->c["visuals_chams_teammates_hidden_color"][3] = 200;
	cfg->b["visuals_bullet_tracers"] = false;
	cfg->b["misc_third_person"] = false;
	cfg->i["misc_third_person_key"] = 'V';
	cfg->i["misc_third_person_keystyle"] = 2;
	cfg->b["misc_tp_glitch"] = false;
	cfg->i["misc_tp_glitch_key"] = 'T';
	cfg->i["misc_tp_glitch_keystyle"] = 1;
	cfg->f["misc_tp_glitch_distance"] = 96.f;
	cfg->f["misc_tp_glitch_through"] = 18.f;

	cfg->b["legit_aimbot"] = false;
	cfg->b["legit_aimbot_always_on"] = false;
	cfg->b["legit_aimbot_visible_only"] = false;
	cfg->b["legit_aimbot_rcs"] = false;
	cfg->f["legit_aimbot_fov"] = 8.f;
	cfg->f["legit_aimbot_smooth"] = 8.f;
	cfg->i["legit_aimbot_hitbox"] = 0;
	cfg->i["legit_aimbot_dynamic_mode"] = 0;
	cfg->i["legit_aimbot_key"] = VK_XBUTTON2;
	cfg->i["legit_aimbot_keystyle"] = 1;
	cfg->b["legit_aimbot_fov_circle"] = false;
	cfg->c["legit_aimbot_fov_circle_color"][0] = 163;
	cfg->c["legit_aimbot_fov_circle_color"][1] = 212;
	cfg->c["legit_aimbot_fov_circle_color"][2] = 31;
	cfg->c["legit_aimbot_fov_circle_color"][3] = 180;

	cfg->b["legit_triggerbot"] = false;
	cfg->b["legit_triggerbot_always_on"] = false;
	cfg->b["legit_triggerbot_visible_only"] = false;
	cfg->i["legit_triggerbot_key"] = VK_XBUTTON1;
	cfg->i["legit_triggerbot_keystyle"] = 1;
	cfg->i["legit_triggerbot_delay"] = 50;
	cfg->b["legit_smoke_check"] = false;

	for (int w = 0; w <= 5; ++w) {
		const std::string prefix = "legit_w" + std::to_string(w);
		cfg->b[prefix + "_globalize"] = false;
		cfg->f[prefix + "_fov"] = 8.f;
		cfg->f[prefix + "_smooth"] = 8.f;
		cfg->i[prefix + "_hitbox"] = 0;
		cfg->i[prefix + "_dynamic_mode"] = 0;
	}

	cfg->b["misc_spectator_list"] = false;
	cfg->b["misc_keybind_list"] = false;
	cfg->f["misc_keybind_list_x"] = 20.f;
	cfg->f["misc_keybind_list_y"] = 220.f;
	cfg->i["players_priority"] = 0;
	cfg->b["players_whitelist"] = false;
	cfg->b["players_blacklist"] = false;

	cfg->i["misc_menu_key"] = VK_INSERT;
	cfg->i["menu_key"] = VK_INSERT;
	cfg->i["menu_scale"] = 0;
	cfg->i["menu_animation_speed"] = 25;
	cfg->b["menu_confirmations"] = true;
	cfg->i["misc_anti_untrusted_mode"] = 0;
	cfg->i["visuals_player_esp_activation_type_key"] = 0;
	cfg->i["visuals_player_esp_activation_type_keystyle"] = 0;
}

static bool ShouldPersistKey(const std::string& key) {
	return key.find('_') != 0;
}

namespace {
	void WriteSkinBool(const char* filePath, const char* key, bool value) {
		WritePrivateProfileStringA("skins", key, value ? "1" : "0", filePath);
	}

	void WriteSkinInt(const char* filePath, const char* key, int value) {
		char buffer[32]{};
		_itoa(value, buffer, 10);
		WritePrivateProfileStringA("skins", key, buffer, filePath);
	}

	void WriteSkinFloat(const char* filePath, const char* key, float value) {
		char buffer[64]{};
		sprintf_s(buffer, "%f", value);
		WritePrivateProfileStringA("skins", key, buffer, filePath);
	}

	int ReadSkinInt(const char* filePath, const char* key, int defaultValue) {
		return GetPrivateProfileIntA("skins", key, defaultValue, filePath);
	}

	float ReadSkinFloat(const char* filePath, const char* key, float defaultValue) {
		char buffer[64]{};
		GetPrivateProfileStringA("skins", key, "", buffer, sizeof(buffer), filePath);
		if (buffer[0] == '\0')
			return defaultValue;
		return static_cast<float>(atof(buffer));
	}

	bool ReadSkinBool(const char* filePath, const char* key, bool defaultValue) {
		return GetPrivateProfileIntA("skins", key, defaultValue ? 1 : 0, filePath) != 0;
	}

	void SaveSkinConfig(const char* filePath) {
		if (!filePath || filePath[0] == '\0')
			return;

		WriteSkinBool(filePath, "knife_enabled", g_cfg->knife_changer.m_enabled);
		WriteSkinInt(filePath, "knife_model", g_cfg->knife_changer.m_knife);
		WriteSkinInt(filePath, "knife_paint_kit", g_cfg->knife_changer.m_paint_kit);
		WriteSkinFloat(filePath, "knife_wear", g_cfg->knife_changer.m_wear);
		WriteSkinInt(filePath, "knife_seed", g_cfg->knife_changer.m_seed);
		WritePrivateProfileStringA("skins", "knife_custom_name", g_cfg->knife_changer.m_custom_name, filePath);

		WriteSkinBool(filePath, "glove_enabled", g_cfg->glove_changer.m_enabled);
		WriteSkinInt(filePath, "glove_model", g_cfg->glove_changer.m_glove);
		WriteSkinInt(filePath, "glove_paint_kit", g_cfg->glove_changer.m_paint_kit);
		WriteSkinFloat(filePath, "glove_wear", g_cfg->glove_changer.m_wear);
		WriteSkinInt(filePath, "glove_seed", g_cfg->glove_changer.m_seed);

		WriteSkinBool(filePath, "skin_enabled", g_cfg->skin_changer.m_enabled);
		WriteSkinInt(filePath, "skin_selected_weapon", g_cfg->skin_changer.m_selected_weapon);
		WriteSkinInt(filePath, "model_selected", g_cfg->model_changer.m_selected);
		WriteSkinBool(filePath, "agent_enabled", g_cfg->agent_changer.m_enabled);
		WriteSkinInt(filePath, "agent_ct", g_cfg->agent_changer.m_ct_agent);
		WriteSkinInt(filePath, "agent_t", g_cfg->agent_changer.m_t_agent);

		for (int i = 1; i <= 70; ++i) {
			const auto& weaponSkin = g_cfg->skin_changer.weapon_skins[i];
			if (weaponSkin.paint_kit == 0 && weaponSkin.seed == 0 && weaponSkin.custom_name[0] == '\0')
				continue;

			char key[32]{};
			sprintf_s(key, "w%d_kit", i);
			WriteSkinInt(filePath, key, weaponSkin.paint_kit);

			sprintf_s(key, "w%d_wear", i);
			WriteSkinFloat(filePath, key, weaponSkin.wear);

			sprintf_s(key, "w%d_seed", i);
			WriteSkinInt(filePath, key, weaponSkin.seed);

			sprintf_s(key, "w%d_name", i);
			WritePrivateProfileStringA("skins", key, weaponSkin.custom_name, filePath);
		}
	}

	void LoadSkinConfig(const char* filePath) {
		if (!filePath || filePath[0] == '\0')
			return;

		if (!std::filesystem::exists(filePath))
			return;

		g_cfg->knife_changer.m_enabled = ReadSkinBool(filePath, "knife_enabled", false);
		g_cfg->knife_changer.m_knife = ReadSkinInt(filePath, "knife_model", 0);
		g_cfg->knife_changer.m_paint_kit = ReadSkinInt(filePath, "knife_paint_kit", 0);
		g_cfg->knife_changer.m_wear = ReadSkinFloat(filePath, "knife_wear", 0.0001f);
		g_cfg->knife_changer.m_seed = ReadSkinInt(filePath, "knife_seed", 0);
		GetPrivateProfileStringA("skins", "knife_custom_name", "", g_cfg->knife_changer.m_custom_name, sizeof(g_cfg->knife_changer.m_custom_name), filePath);

		g_cfg->glove_changer.m_enabled = ReadSkinBool(filePath, "glove_enabled", false);
		g_cfg->glove_changer.m_glove = ReadSkinInt(filePath, "glove_model", 0);
		g_cfg->glove_changer.m_paint_kit = ReadSkinInt(filePath, "glove_paint_kit", 0);
		g_cfg->glove_changer.m_wear = ReadSkinFloat(filePath, "glove_wear", 0.0001f);
		g_cfg->glove_changer.m_seed = ReadSkinInt(filePath, "glove_seed", 0);

		g_cfg->skin_changer.m_enabled = ReadSkinBool(filePath, "skin_enabled", false);
		g_cfg->skin_changer.m_selected_weapon = ReadSkinInt(filePath, "skin_selected_weapon", 0);
		g_cfg->model_changer.m_selected = ReadSkinInt(filePath, "model_selected", 0);
		g_cfg->agent_changer.m_enabled = ReadSkinBool(filePath, "agent_enabled", false);
		g_cfg->agent_changer.m_ct_agent = ReadSkinInt(filePath, "agent_ct", 0);
		g_cfg->agent_changer.m_t_agent = ReadSkinInt(filePath, "agent_t", 0);

		for (int i = 0; i < 100; ++i)
			g_cfg->skin_changer.weapon_skins[i] = {};

		for (int i = 1; i <= 70; ++i) {
			char key[32]{};
			sprintf_s(key, "w%d_kit", i);
			const int paintKit = ReadSkinInt(filePath, key, 0);
			if (paintKit == 0)
				continue;

			auto& weaponSkin = g_cfg->skin_changer.weapon_skins[i];
			weaponSkin.paint_kit = paintKit;

			sprintf_s(key, "w%d_wear", i);
			weaponSkin.wear = ReadSkinFloat(filePath, key, 0.0001f);

			sprintf_s(key, "w%d_seed", i);
			weaponSkin.seed = ReadSkinInt(filePath, key, 0);

			sprintf_s(key, "w%d_name", i);
			GetPrivateProfileStringA("skins", key, "", weaponSkin.custom_name, sizeof(weaponSkin.custom_name), filePath);
		}
	}
}

void CConfig::LoadFromPath(const char* filePath) {
	if (!filePath || filePath[0] == '\0')
		return;

	if (!std::filesystem::exists(filePath))
		return;

	char buffer[4096]{};

	for (auto& e : b) {
		if (!ShouldPersistKey(e.first))
			continue;
		e.second = GetPrivateProfileIntA("b", e.first.c_str(), e.second ? 1 : 0, filePath) != 0;
	}

	for (auto& e : i) {
		if (!ShouldPersistKey(e.first))
			continue;
		e.second = GetPrivateProfileIntA("i", e.first.c_str(), e.second, filePath);
	}

	for (auto& e : f) {
		if (!ShouldPersistKey(e.first))
			continue;
		char fbuf[64]{};
		GetPrivateProfileStringA("f", e.first.c_str(), "", fbuf, sizeof(fbuf), filePath);
		if (fbuf[0] != '\0')
			e.second = static_cast<float>(atof(fbuf));
	}

	for (auto& e : c) {
		if (!ShouldPersistKey(e.first))
			continue;
		GetPrivateProfileStringA("c", e.first.c_str(), "", buffer, sizeof(buffer), filePath);
		if (buffer[0] == '\0')
			continue;

		float* parsed = string_to_color(buffer);
		e.second[0] = static_cast<int>(parsed[0]);
		e.second[1] = static_cast<int>(parsed[1]);
		e.second[2] = static_cast<int>(parsed[2]);
		e.second[3] = static_cast<int>(parsed[3]);
	}

	for (auto& e : m) {
		if (!ShouldPersistKey(e.first))
			continue;
		GetPrivateProfileStringA("m", e.first.c_str(), "", buffer, sizeof(buffer), filePath);
		if (buffer[0] == '\0')
			continue;

		std::string values = buffer;
		size_t pos = 0;
		while (pos < values.size()) {
			const size_t delim = values.find('|', pos);
			const std::string pair = values.substr(pos, delim == std::string::npos ? std::string::npos : delim - pos);
			const size_t colon = pair.find(':');
			if (colon != std::string::npos) {
				const int index = std::stoi(pair.substr(0, colon));
				const bool value = std::stoi(pair.substr(colon + 1)) != 0;
				e.second[index] = value;
			}
			if (delim == std::string::npos)
				break;
			pos = delim + 1;
		}
	}

	GetPrivateProfileStringA("s", "config_name", this->s["config_name"], buffer, sizeof(buffer), filePath);
	if (buffer[0] != '\0')
		strcpy_s(this->s["config_name"], buffer);

	LoadSkinConfig(filePath);
	g_skin_changer->should_update = true;
	if (g_cfg->model_changer.m_selected > 0)
		model_changer::RequestApply();
	if (g_cfg->agent_changer.m_enabled)
		agent_changer::RequestApply();
}

void CConfig::Load() {
	if (!this->s["config_name"] || this->s["config_name"][0] == '\0')
		return;

	char file_path[MAX_PATH]{};
	GetFilePath(file_path);

	if (!std::filesystem::exists(file_path)) {
		this->Current = this->s["config_name"];
		Refresh();
		return;
	}

	LoadFromPath(file_path);

	this->Current = this->s["config_name"];
	Refresh();
}

void CConfig::SaveToPath(const char* filePath) {
	if (!filePath || filePath[0] == '\0')
		return;

	for (auto e : b) {
		if (!ShouldPersistKey(e.first)) continue;
		char buffer[8] = { 0 }; _itoa(e.second, buffer, 10);
		WritePrivateProfileStringA("b", e.first.c_str(), std::string(buffer).c_str(), filePath);
	}

	for (auto e : i) {
		if (!ShouldPersistKey(e.first)) continue;
		char buffer[32] = { 0 }; _itoa(e.second, buffer, 10);
		WritePrivateProfileStringA("i", e.first.c_str(), std::string(buffer).c_str(), filePath);
	}

	for (auto e : f) {
		if (!ShouldPersistKey(e.first)) continue;
		char buffer[64] = { 0 }; sprintf(buffer, "%f", e.second);
		WritePrivateProfileStringA("f", e.first.c_str(), std::string(buffer).c_str(), filePath);
	}

	for (auto e : c) {
		if (!ShouldPersistKey(e.first)) continue;
		WritePrivateProfileStringA("c", e.first.c_str(), color_to_string(e.second).c_str(), filePath);
	}

	for (auto e : m) {
		if (!ShouldPersistKey(e.first)) continue;

		std::string vs = "";
		for (auto v : e.second)
			vs += std::to_string(v.first) + ":" + std::to_string(v.second) + "|";

		WritePrivateProfileStringA("m", e.first.c_str(), vs.c_str(), filePath);
	}

	WritePrivateProfileStringA("s", "config_name", this->s["config_name"], filePath);
	SaveSkinConfig(filePath);
}

void CConfig::Save() {
	if (!this->s["config_name"] || this->s["config_name"][0] == '\0')
		return;

	char configs_path[260]{};
	GetFilePath(configs_path, true);
	std::error_code ec;
	std::filesystem::create_directories(configs_path, ec);

	char file_path[MAX_PATH] = { 0 };
	GetFilePath(file_path);
	Misc::Utilities->Game_Msg(file_path);

	SaveToPath(file_path);
	this->Current = this->s["config_name"];
	Refresh();
}

static bool ReadClipboardText(std::string& out) {
	if (!OpenClipboard(nullptr))
		return false;

	HANDLE data = GetClipboardData(CF_TEXT);
	if (!data) {
		CloseClipboard();
		return false;
	}

	const char* text = static_cast<const char*>(GlobalLock(data));
	if (!text) {
		CloseClipboard();
		return false;
	}

	out = text;
	GlobalUnlock(data);
	CloseClipboard();
	return !out.empty();
}

static bool WriteClipboardText(const std::string& text) {
	if (!OpenClipboard(nullptr))
		return false;

	EmptyClipboard();

	const size_t size = text.size() + 1;
	HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
	if (!memory) {
		CloseClipboard();
		return false;
	}

	memcpy(GlobalLock(memory), text.c_str(), size);
	GlobalUnlock(memory);
	SetClipboardData(CF_TEXT, memory);
	CloseClipboard();
	return true;
}

static bool ReadFileText(const char* path, std::string& out) {
	std::ifstream file(path, std::ios::binary);
	if (!file)
		return false;

	std::ostringstream ss;
	ss << file.rdbuf();
	out = ss.str();
	return !out.empty();
}

static bool WriteFileText(const char* path, const std::string& text) {
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file)
		return false;

	file << text;
	return file.good();
}

bool CConfig::ExportToClipboard() {
	char tempPath[MAX_PATH]{};
	GetTempPathA(MAX_PATH, tempPath);
	strcat_s(tempPath, "gamesense-cs2-export.cfg");

	SaveToPath(tempPath);

	std::string data;
	if (!ReadFileText(tempPath, data)) {
		std::remove(tempPath);
		return false;
	}

	const bool ok = WriteClipboardText(data);
	std::remove(tempPath);

	if (ok)
		Misc::Utilities->Game_Msg("Config exported to clipboard");

	return ok;
}

bool CConfig::ImportFromClipboard() {
	std::string data;
	if (!ReadClipboardText(data))
		return false;

	char tempPath[MAX_PATH]{};
	GetTempPathA(MAX_PATH, tempPath);
	strcat_s(tempPath, "gamesense-cs2-import.cfg");

	if (!WriteFileText(tempPath, data))
		return false;

	LoadFromPath(tempPath);
	std::remove(tempPath);

	Misc::Utilities->Game_Msg("Config imported from clipboard");
	return true;
}

void CConfig::Delete() {
	char path[260];
	GetFilePath(path);
	std::remove(path);
	this->s["config_name"][0] = '\0';
	this->Current = "";
	Refresh();
}

void CConfig::Refresh() {
	this->List.clear();
	char path[260]{};
	GetFilePath(path, true);

	std::error_code ec;
	std::filesystem::create_directories(path, ec);

	for (auto& entry : std::filesystem::directory_iterator(path, ec)) {
		if (ec)
			break;

		if (entry.path().extension() == ".cfg") {
			const auto filename = entry.path().filename().string();
			if (filename != "tempbuffer.cfg")
				this->List.push_back(filename.substr(0, filename.length() - 4));
		}
	}
}

bool CConfig::IsBindActive(std::string key) {
	const int style = this->i[key + "style"];
	const int vk = this->i[key];

	switch (style) {
	case 0:
		return true;
	case 1:
		return (GetAsyncKeyState(vk) & 0x8000) != 0;
	case 2: {
		static std::unordered_map<std::string, bool> toggled;
		if (vk && (GetAsyncKeyState(vk) & 1))
			toggled[key] = !toggled[key];
		return toggled[key];
	}
	case 3:
		return (GetAsyncKeyState(vk) & 0x8000) == 0;
	default:
		return true;
	}
}