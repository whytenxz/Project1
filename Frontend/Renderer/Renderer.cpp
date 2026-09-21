#include "Renderer.h"
#include "../../Backend/Misc/Fonts/Fonts.h"
#include "../../Backend/Misc/Textures/Textures.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_internal.h"

#include <vector>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

HWND g_GameWindow = nullptr;

namespace Render
{
    namespace Fonts
    {
        LPD3DXFONT TabIcons = nullptr;
        LPD3DXFONT LegitTabIcons = nullptr;
        LPD3DXFONT LuaTab = nullptr;
        LPD3DXFONT Verdana = nullptr;
        LPD3DXFONT Tahombd = nullptr;
        LPD3DXFONT SmallFont = nullptr;
        LPD3DXFONT WatermarkFont = nullptr;
        LPD3DXFONT FontAwesome = nullptr;
    }
}

using namespace Render;
using namespace IdaLovesMe;

static ImU32 ToImColor(D3DCOLOR color)
{
    return IM_COL32(get_r(color), get_g(color), get_b(color), get_a(color));
}

static bool LoadTextureFromMemory(ID3D11Device* device, const unsigned char* data, size_t size, ID3D11ShaderResourceView** outSrv, int* outW, int* outH)
{
    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;

    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
        return false;

    if (FAILED(factory->CreateStream(&stream)) ||
        FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(data), static_cast<DWORD>(size))) ||
        FAILED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder)) ||
        FAILED(decoder->GetFrame(0, &frame)) ||
        FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeCustom)))
    {
        if (converter) converter->Release();
        if (frame) frame->Release();
        if (decoder) decoder->Release();
        if (stream) stream->Release();
        if (factory) factory->Release();
        return false;
    }

    UINT width = 0, height = 0;
    converter->GetSize(&width, &height);
    const UINT stride = width * 4;
    const UINT imageSize = stride * height;
    std::vector<BYTE> pixels(imageSize);
    converter->CopyPixels(nullptr, stride, imageSize, pixels.data());

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sub{};
    sub.pSysMem = pixels.data();
    sub.SysMemPitch = stride;

    ID3D11Texture2D* texture = nullptr;
    if (FAILED(device->CreateTexture2D(&desc, &sub, &texture)))
    {
        converter->Release();
        frame->Release();
        decoder->Release();
        stream->Release();
        factory->Release();
        return false;
    }

    device->CreateShaderResourceView(texture, nullptr, outSrv);
    texture->Release();

    if (outW) *outW = static_cast<int>(width);
    if (outH) *outH = static_cast<int>(height);

    converter->Release();
    frame->Release();
    decoder->Release();
    stream->Release();
    factory->Release();
    return true;
}

void CDraw::Init(ID3D11Device* device, ID3D11DeviceContext* context)
{
    if (Initialized)
        return;

    m_Device = device;
    m_Context = context;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    CreateObjects();
    Initialized = true;
}

void CDraw::CreateObjects()
{
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg{};
    cfg.FontDataOwnedByAtlas = false;

    const float s = m_DpiScale > 0.f ? m_DpiScale : 1.f;

    Fonts::Verdana = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\verdana.ttf", 12.f * s);
    Fonts::Tahombd = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\verdanab.ttf", 12.f * s, &cfg);
    if (!Fonts::Tahombd)
        Fonts::Tahombd = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\tahomabd.ttf", 12.f * s, &cfg);
    Fonts::SmallFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\verdanab.ttf", 8.f * s, &cfg);
    if (!Fonts::SmallFont)
        Fonts::SmallFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 8.f * s);

    ImFontConfig watermarkCfg{};
    watermarkCfg.PixelSnapH = true;
    Fonts::WatermarkFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\verdana.ttf", 13.f, &watermarkCfg);

    Fonts::TabIcons = io.Fonts->AddFontFromMemoryTTF((void*)FontsData::TabIcons, 5192, 42.f * s, &cfg);
    Fonts::LegitTabIcons = io.Fonts->AddFontFromMemoryTTF((void*)FontsData::LegitTabIcons, 47320, 32.f * s, &cfg);
    static const ImWchar ranges[] = { 0xf000, 0xf8ff, 0 };
    // Pass cfg with FontDataOwnedByAtlas == false so the atlas does not free our static data.
    Fonts::FontAwesome = io.Fonts->AddFontFromMemoryTTF((void*)FontsData::FontAwesome, 165548, 28.f * s, &cfg, ranges);
    Fonts::LuaTab = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\verdanab.ttf", 22.f * s, &cfg);
    if (!Fonts::LuaTab)
        Fonts::LuaTab = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\tahomabd.ttf", 22.f * s, &cfg);

    if (!Fonts::Verdana) Fonts::Verdana = io.Fonts->AddFontDefault();
    if (!Fonts::Tahombd) Fonts::Tahombd = Fonts::Verdana;
    if (!Fonts::SmallFont) Fonts::SmallFont = Fonts::Verdana;
    if (!Fonts::WatermarkFont) Fonts::WatermarkFont = Fonts::Verdana;
    if (!Fonts::TabIcons) Fonts::TabIcons = Fonts::Verdana;
    if (!Fonts::LegitTabIcons) Fonts::LegitTabIcons = Fonts::Verdana;
    if (!Fonts::FontAwesome) Fonts::FontAwesome = Fonts::Verdana;
    if (!Fonts::LuaTab) Fonts::LuaTab = Fonts::Tahombd;

    io.Fonts->Build();
    ImGui_ImplDX11_CreateDeviceObjects();

    int w = 0, h = 0;
    LoadTextureFromMemory(m_Device, TexturesData::BgTexture, 424852, &m_BgTexture, &w, &h);
}

// Schedule a font/atlas rebuild. Must never run mid-frame (the atlas is locked
// between NewFrame() and EndFrame/Render()), so we only record the request here;
// the actual rebuild happens in ProcessDpiReload() right before the next NewFrame().
void CDraw::SetDpiScaleDeferred(float scale)
{
    if (scale < 1.f)
        scale = 1.f;

    if (std::fabs(m_DpiScale - scale) < 0.001f)
        return;

    m_PendingDpiScale = scale;
}

// Call right before ImGui::NewFrame() (not while a frame is in progress).
void CDraw::ProcessDpiReload()
{
    if (!Initialized)
        return;

    if (m_PendingDpiScale <= 0.f || std::fabs(m_PendingDpiScale - m_DpiScale) < 0.001f) {
        m_PendingDpiScale = -1.f;
        return;
    }

    m_DpiScale = m_PendingDpiScale;
    m_PendingDpiScale = -1.f;

    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.Fonts && !io.Fonts->Locked && "Font atlas reload must happen outside NewFrame()/Render()");

    io.Fonts->Clear();
    ReleaseObjects();
    CreateObjects();
}

void CDraw::SetDpiScale(float scale)
{
    if (scale < 1.f)
        scale = 1.f;

    if (std::fabs(m_DpiScale - scale) < 0.001f)
        return;

    m_DpiScale = scale;

    if (!Initialized)
        return;

    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(!io.Fonts->Locked && "Font atlas is locked between NewFrame() and Render(); use SetDpiScaleDeferred()");

    io.Fonts->Clear();
    ReleaseObjects();
    CreateObjects();
}

void CDraw::ReleaseObjects()
{
    if (m_BgTexture)
    {
        m_BgTexture->Release();
        m_BgTexture = nullptr;
    }
}

void CDraw::Reset()
{
    if (!m_DrawList)
        return;

    Screen.Width = m_DrawList->GetClipRectMax().x - m_DrawList->GetClipRectMin().x;
    Screen.Height = m_DrawList->GetClipRectMax().y - m_DrawList->GetClipRectMin().y;
    Screen.x_center = Screen.Width / 2.f;
    Screen.y_center = Screen.Height / 2.f;
}

void CDraw::SetDrawList(ImDrawList* drawList)
{
    m_DrawList = drawList;
}

void CDraw::PushClipRect(Vec2 min, Vec2 max)
{
    if (m_DrawList)
        m_DrawList->PushClipRect(ImVec2(min.x, min.y), ImVec2(max.x, max.y), true);
}

void CDraw::PopClipRect()
{
    if (m_DrawList)
        m_DrawList->PopClipRect();
}

LPDIRECT3DTEXTURE9 CDraw::GetBgTexture()
{
    return m_BgTexture;
}

Vec2 CDraw::GetTextSize(LPD3DXFONT font, const char* text)
{
    if (!font || !text)
        return Vec2();
    const ImVec2 size = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.f, text);
    return Vec2(size.x, size.y);
}

void CDraw::Line(Vec2 pos, Vec2 pos2, D3DCOLOR color)
{
    if (!m_DrawList)
        return;
    m_DrawList->AddLine(ImVec2(pos.x, pos.y), ImVec2(pos2.x, pos2.y), ToImColor(color));
}

void CDraw::FilledRect(Vec2 pos, Vec2 size, D3DCOLOR color, bool)
{
    if (!m_DrawList)
        return;
    m_DrawList->AddRectFilled(ImVec2(pos.x, pos.y), ImVec2(pos.x + size.x, pos.y + size.y), ToImColor(color));
}

void CDraw::Rect(Vec2 pos, Vec2 size, float lineWidth, D3DCOLOR color, bool)
{
    if (!m_DrawList)
        return;

    if (lineWidth == 0 || lineWidth == 1)
    {
        FilledRect(pos, Vec2(size.x, 1), color);
        FilledRect(Vec2(pos.x, pos.y + size.y - 1), Vec2(size.x, 1), color);
        FilledRect(Vec2(pos.x, pos.y + 1), Vec2(1, size.y - 2), color);
        FilledRect(Vec2(pos.x + size.x - 1, pos.y + 1), Vec2(1, size.y - 2), color);
    }
    else
    {
        FilledRect(pos, Vec2(size.x, lineWidth), color);
        FilledRect(Vec2(pos.x, pos.y + size.y - lineWidth), Vec2(size.x, lineWidth), color);
        FilledRect(Vec2(pos.x, pos.y + lineWidth), Vec2(lineWidth, size.y - 2 * lineWidth), color);
        FilledRect(Vec2(pos.x + size.x - lineWidth, pos.y + lineWidth), Vec2(lineWidth, size.y - 2 * lineWidth), color);
    }
}

void CDraw::BorderedRect(Vec2 pos, Vec2 size, float borderWidth, D3DCOLOR color, D3DCOLOR borderColor)
{
    FilledRect(pos, size, color);
    Rect(Vec2(pos.x - borderWidth, pos.y - borderWidth), Vec2(size.x + 2 * borderWidth, size.y + 2 * borderWidth), borderWidth, borderColor);
}

void CDraw::Gradient(Vec2 pos, Vec2 size, D3DCOLOR lColor, D3DCOLOR rColor, bool vertical, bool)
{
    if (!m_DrawList)
        return;

    const ImU32 c1 = ToImColor(lColor);
    const ImU32 c2 = ToImColor(rColor);
    const ImVec2 p1(pos.x, pos.y);
    const ImVec2 p2(pos.x + size.x, pos.y + size.y);

    if (vertical)
        m_DrawList->AddRectFilledMultiColor(p1, p2, c1, c1, c2, c2);
    else
        m_DrawList->AddRectFilledMultiColor(p1, p2, c1, c2, c2, c1);
}

void CDraw::Triangle(Vec2 top, Vec2 left, Vec2 right, D3DCOLOR color, bool)
{
    if (!m_DrawList)
        return;
    m_DrawList->AddTriangleFilled(ImVec2(top.x, top.y), ImVec2(left.x, left.y), ImVec2(right.x, right.y), ToImColor(color));
}

void CDraw::Sprite(LPDIRECT3DTEXTURE9 texture, Vec2 pos, Vec2 size, D3DCOLOR color)
{
    if (!m_DrawList || !texture)
        return;
    m_DrawList->AddImage(reinterpret_cast<ImTextureID>(texture), ImVec2(pos.x, pos.y), ImVec2(pos.x + size.x, pos.y + size.y), ImVec2(0, 0), ImVec2(1, 1), ToImColor(color));
}

void CDraw::Text(const char* text, float x, float y, int orientation, LPD3DXFONT font, bool bordered, D3DCOLOR color, Vec2 textClipSize)
{
    if (!m_DrawList || !font || !text)
        return;

    const ImU32 col = ToImColor(color);
    const ImU32 shadow = IM_COL32(15, 15, 15, get_a(color));
    ImVec2 pos(x, y);

    if (textClipSize.x != 0 || textClipSize.y != 0)
    {
        m_DrawList->PushClipRect(ImVec2(x, y), ImVec2(textClipSize.x, textClipSize.y), true);
    }

    if (orientation == CENTER)
    {
        const ImVec2 size = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.f, text);
        pos.x -= size.x * 0.5f;
    }
    else if (orientation == RIGHT)
    {
        const ImVec2 size = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.f, text);
        pos.x -= size.x;
    }

    if (bordered)
    {
        for (int ox = -1; ox <= 1; ++ox)
            for (int oy = -1; oy <= 1; ++oy)
                if (ox != 0 || oy != 0)
                    m_DrawList->AddText(font, font->FontSize, ImVec2(pos.x + ox, pos.y + oy), shadow, text);
    }

    m_DrawList->AddText(font, font->FontSize, pos, col, text);

    if (textClipSize.x != 0 || textClipSize.y != 0)
        m_DrawList->PopClipRect();
}
