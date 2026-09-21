#pragma once
#include "../Misc/lazy_ptr.hpp"

class CSettings
	{
	public:
		bool ShowConsole = false;
		bool UnloadCheat = false;
		bool ResetLayout = false;

		void LoadDefaults();
	};

inline lazy_ptr<CSettings> Settings;
