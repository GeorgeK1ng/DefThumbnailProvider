#pragma once
#include <windows.h>
#include <cstddef>
#include <cstdint>

struct DefEditorFrameInfo
{
	uint32_t group;
	uint32_t index;
	uint32_t format;
	uint32_t width;
	uint32_t height;
	wchar_t name[14];
};

extern "C"
{
	__declspec(dllexport) HRESULT __stdcall DefEditorGetFrameCount(const uint8_t *, size_t, uint32_t *);
	__declspec(dllexport) HRESULT __stdcall DefEditorGetFrameInfo(const uint8_t *, size_t, uint32_t, DefEditorFrameInfo *);
	__declspec(dllexport) HRESULT __stdcall DefEditorRenderFrame(
	  const uint8_t *, size_t, uint32_t, const COLORREF *, UINT, HBITMAP *);
}
