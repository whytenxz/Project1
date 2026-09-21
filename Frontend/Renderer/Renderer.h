#pragma once
#include "../../Backend/Globalincludes.h"
#include "../Framework/MenuFramework.h"
#include "../../Backend/Misc/lazy_ptr.hpp"

#define get_a(col) (((col) & 0xff000000) >> 24)
#define get_r(col) (((col) & 0x00ff0000) >> 16)
#define get_g(col) (((col) & 0x0000ff00) >> 8)
#define get_b(col) ((col) & 0x000000ff)

enum circle_type { FULL, HALF, QUARTER };
enum text_alignment { LEFT, CENTER, RIGHT };

struct ImDrawList;

namespace Render
{
    using IdaLovesMe::Vec2;

    namespace Fonts
    {
        extern LPD3DXFONT TabIcons;
        extern LPD3DXFONT LegitTabIcons;
        extern LPD3DXFONT LuaTab;
        extern LPD3DXFONT Verdana;
        extern LPD3DXFONT Tahombd;
        extern LPD3DXFONT SmallFont;
        extern LPD3DXFONT WatermarkFont;
        extern LPD3DXFONT FontAwesome;
    }

    class CDraw
    {
    public:
        struct sScreen
        {
            float Width;
            float Height;
            float x_center;
            float y_center;
        } Screen;

        void Sprite(LPDIRECT3DTEXTURE9 Texture, Vec2 Pos, Vec2 Size, D3DCOLOR Color);
        void Line(Vec2 Pos, Vec2 Pos2, D3DCOLOR Color);
        void Rect(Vec2 Pos, Vec2 Size, float linewidth, D3DCOLOR Color, bool Antialias = false);
        void FilledRect(Vec2 Pos, Vec2 Size, D3DCOLOR color, bool Antialias = false);
        void BorderedRect(Vec2 Pos, Vec2 Size, float BorderWidth, D3DCOLOR Color, D3DCOLOR BorderColor);
        void Gradient(Vec2 Pos, Vec2 Size, D3DCOLOR LColor, D3DCOLOR ROtherColor, bool Vertical = false, bool Antialias = false);
        void Triangle(Vec2 Top, Vec2 Left, Vec2 Right, D3DCOLOR Color, bool antialias = false);
        void Text(const char* Text, float X, float Y, int Orientation, LPD3DXFONT Font, bool Bordered, D3DCOLOR Color, Vec2 MaxSize = Vec2(0, 0));

        void Init(ID3D11Device* device, ID3D11DeviceContext* context);
        void CreateObjects();
        void SetDpiScaleDeferred(float scale);
        void ProcessDpiReload();
        void SetDpiScale(float scale);
        void ReleaseObjects();
        void Reset();
        void SetDrawList(ImDrawList* drawList);
        void PushClipRect(Vec2 min, Vec2 max);
        void PopClipRect();

        LPDIRECT3DTEXTURE9 GetBgTexture();
        Vec2 GetTextSize(LPD3DXFONT Font, const char* Text);

    private:
        ID3D11Device* m_Device = nullptr;
        ID3D11DeviceContext* m_Context = nullptr;
        ImDrawList* m_DrawList = nullptr;
        LPDIRECT3DTEXTURE9 m_BgTexture = nullptr;
        float m_PendingDpiScale = -1.f;
        bool Initialized = false;
        float m_DpiScale = 1.f;
    };

    inline lazy_ptr<CDraw> Draw;
}
