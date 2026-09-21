#pragma once

#include <Windows.h>
#include <string>
#include <memory>
#include <d3d11.h>
#include <dxgi.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

struct ImFont;
typedef ImFont* LPD3DXFONT;

struct ID3D11ShaderResourceView;
typedef ID3D11ShaderResourceView* LPDIRECT3DTEXTURE9;

typedef unsigned long D3DCOLOR;
#define D3DCOLOR_ARGB(a, r, g, b) ((D3DCOLOR)((((a) & 0xff) << 24) | (((r) & 0xff) << 16) | (((g) & 0xff) << 8) | ((b) & 0xff)))
#define D3DCOLOR_RGBA(r, g, b, a) D3DCOLOR_ARGB(a, r, g, b)
#define D3DCOLOR_XRGB(r, g, b) D3DCOLOR_ARGB(0xFF, r, g, b)

extern HWND g_GameWindow;
