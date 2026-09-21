#pragma once
#include "../../Backend/Globalincludes.h"
#include "../../Backend/Misc/singleton.h"
#include "../Framework/MenuFramework.h"
#include <unordered_map>

class CMenu : public singleton<CMenu> 
{
public:
	void Initialize();
	void Draw();

	bool IsMenuOpened();
	void SetMenuOpened(bool v);

	D3DCOLOR GetMenuColor();
	
	int Menu_key = VK_INSERT;
	bool m_bIsBanned = false;
private:
	bool m_bInitialized = false;
	bool m_bIsOpened = false;

	int m_nCurrentTab = 0;
	int m_nCurrentLegitTab = 0;
	int m_nCurrentVisualsTab = 0;
};