#pragma once
#include <windows.h>
#ifdef DEFTHUMB_ENABLE_DEBUG_LOG
#define DEFTHUMB_LOG(s) OutputDebugStringW(L"DefThumbnailProvider: " s L"\n")
#else
#define DEFTHUMB_LOG(s) ((void)0)
#endif
template<typename T>
void safeRelease(T *& pointer) noexcept
{
	if (pointer)
	{
		pointer->Release();
		pointer = nullptr;
	}
}
