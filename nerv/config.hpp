#pragma once
#include <cstdint>
#include "../Backend/Misc/lazy_ptr.hpp"

#ifdef _DEBUG
    #define CONSOLE_ENABLED
#endif

class c_config {
public:
	struct knife_changer_t {
		bool m_enabled = false;
		int m_knife = 0;
		int m_paint_kit = 0;
		float m_wear = 0.0001f;
		int m_seed = 0;
		char m_custom_name[161] = {};
	} knife_changer;

	struct glove_changer_t {
		bool m_enabled = false;
		int m_glove = 0;
		int m_paint_kit = 0;
		float m_wear = 0.0001f;
		int m_seed = 0;
	} glove_changer;

	struct skin_changer_t {
		bool m_enabled = false;
		int m_selected_weapon = 0;

		struct weapon_skin_t {
			int paint_kit = 0;
			float wear = 0.0001f;
			int seed = 0;
			char custom_name[161] = {};
		};

		weapon_skin_t weapon_skins[100];

		static int get_config_index(uint16_t def_index) {
			if (def_index >= 1 && def_index <= 70) return def_index;
			return 0;
		}
	} skin_changer;

	struct model_changer_t {
		int m_selected = 0;
	} model_changer;

	struct agent_changer_t {
		bool m_enabled = false;
		int m_ct_agent = 0;
		int m_t_agent = 0;
	} agent_changer;

};

#define g_cfg g_config
inline lazy_ptr<c_config> g_config;
