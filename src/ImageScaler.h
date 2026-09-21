#pragma once
#include "DefDecoder.h"
#include <windows.h>
namespace defthumb
{
HBITMAP
CreateThumbnailBitmap(const Image & source, UINT maximumSize);

HBITMAP
CreateThumbnailBitmap(const Image & source, UINT maximumSize, COLORREF backgroundColor);
}
