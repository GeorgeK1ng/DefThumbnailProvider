#include "ImageScaler.h"

#include <algorithm>
#include <cmath>

namespace defthumb
{
namespace
{
	HBITMAP
	createThumbnailBitmap(const Image & source, UINT maximumSize, const COLORREF * backgroundColor)
	{
		const size_t expectedBufferSize = static_cast<size_t>(source.width) * source.height * 4;
		if (maximumSize == 0 || source.width == 0 || source.height == 0 || source.bgra.size() != expectedBufferSize)
			return nullptr;

		const double scale = std::min(static_cast<double>(maximumSize) / source.width, static_cast<double>(maximumSize) / source.height);
		const uint32_t targetWidth = std::max(1u, static_cast<uint32_t>(std::floor(source.width * scale + 0.5)));
		const uint32_t targetHeight = std::max(1u, static_cast<uint32_t>(std::floor(source.height * scale + 0.5)));

		BITMAPINFO bitmapInfo{};
		bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bitmapInfo.bmiHeader.biWidth = static_cast<LONG>(targetWidth);
		bitmapInfo.bmiHeader.biHeight = -static_cast<LONG>(targetHeight);
		bitmapInfo.bmiHeader.biPlanes = 1;
		bitmapInfo.bmiHeader.biBitCount = 32;
		bitmapInfo.bmiHeader.biCompression = BI_RGB;

		void * bitmapBits = nullptr;
		HBITMAP bitmap = CreateDIBSection(nullptr, &bitmapInfo, DIB_RGB_COLORS, &bitmapBits, nullptr, 0);
		if (!bitmap || !bitmapBits)
			return nullptr;

		auto * target = static_cast<uint8_t *>(bitmapBits);
		for (uint32_t y = 0; y < targetHeight; ++y)
		{
			for (uint32_t x = 0; x < targetWidth; ++x)
			{
				// Nearest-neighbour scaling keeps Heroes III pixel art crisp and avoids alpha fringes.
				const uint32_t sourceX = std::min(source.width - 1, static_cast<uint32_t>(static_cast<uint64_t>(x) * source.width / targetWidth));
				const uint32_t sourceY = std::min(source.height - 1, static_cast<uint32_t>(static_cast<uint64_t>(y) * source.height / targetHeight));
				const size_t sourceOffset = (static_cast<size_t>(sourceY) * source.width + sourceX) * 4;
				const size_t targetOffset = (static_cast<size_t>(y) * targetWidth + x) * 4;
				const uint32_t alpha = source.bgra[sourceOffset + 3];

				if (backgroundColor)
				{
					const uint32_t inverseAlpha = 255 - alpha;
					target[targetOffset] = static_cast<uint8_t>(
					  (static_cast<uint32_t>(source.bgra[sourceOffset]) * alpha + static_cast<uint32_t>(GetBValue(*backgroundColor)) * inverseAlpha + 127) /
					  255);
					target[targetOffset + 1] = static_cast<uint8_t>(
					  (static_cast<uint32_t>(source.bgra[sourceOffset + 1]) * alpha + static_cast<uint32_t>(GetGValue(*backgroundColor)) * inverseAlpha + 127) /
					  255);
					target[targetOffset + 2] = static_cast<uint8_t>(
					  (static_cast<uint32_t>(source.bgra[sourceOffset + 2]) * alpha + static_cast<uint32_t>(GetRValue(*backgroundColor)) * inverseAlpha + 127) /
					  255);
					target[targetOffset + 3] = 255;
				}
				else
				{
					// Modern Explorer expects premultiplied BGRA pixels for WTSAT_ARGB thumbnails.
					target[targetOffset] = static_cast<uint8_t>((static_cast<uint32_t>(source.bgra[sourceOffset]) * alpha + 127) / 255);
					target[targetOffset + 1] = static_cast<uint8_t>((static_cast<uint32_t>(source.bgra[sourceOffset + 1]) * alpha + 127) / 255);
					target[targetOffset + 2] = static_cast<uint8_t>((static_cast<uint32_t>(source.bgra[sourceOffset + 2]) * alpha + 127) / 255);
					target[targetOffset + 3] = static_cast<uint8_t>(alpha);
				}
			}
		}
		return bitmap;
	}
}

HBITMAP
CreateThumbnailBitmap(const Image & source, UINT maximumSize)
{
	return createThumbnailBitmap(source, maximumSize, nullptr);
}

HBITMAP
CreateThumbnailBitmap(const Image & source, UINT maximumSize, COLORREF backgroundColor)
{
	return createThumbnailBitmap(source, maximumSize, &backgroundColor);
}
}
