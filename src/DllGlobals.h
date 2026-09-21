#pragma once
#include <windows.h>
extern HMODULE g_module;
extern volatile long g_objectCount;
extern volatile long g_lockCount;
// {9B4F3E1C-5D90-4D62-8FA2-57D7B9A13C84}
inline constexpr CLSID CLSID_DefThumbnailProvider = { 0x9b4f3e1c, 0x5d90, 0x4d62, { 0x8f, 0xa2, 0x57, 0xd7, 0xb9, 0xa1, 0x3c, 0x84 } };
inline constexpr wchar_t kClsidString[] = L"{9B4F3E1C-5D90-4D62-8FA2-57D7B9A13C84}";
