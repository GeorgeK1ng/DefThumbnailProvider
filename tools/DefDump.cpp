#include "DefDecoder.h"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
namespace
{
bool readFile(const wchar_t * path, std::vector<uint8_t> & data)
{
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f)
		return false;
	auto n = f.tellg();
	if (n < 0 || static_cast<unsigned long long>(n) > 256ull * 1024ull * 1024ull)
		return false;
	data.resize(static_cast<size_t>(n));
	f.seekg(0);
	return data.empty() || bool(f.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(data.size())));
}
bool writeBitmap(const wchar_t * path, const defthumb::Image & image)
{
	if (image.width > uint32_t(std::numeric_limits<LONG>::max()) || image.height > uint32_t(std::numeric_limits<LONG>::max()))
		return false;
	BITMAPFILEHEADER file{};
	BITMAPV5HEADER dib{};
	const uint32_t bytes = uint32_t(image.bgra.size());
	file.bfType = 0x4D42;
	file.bfOffBits = sizeof(file) + sizeof(dib);
	file.bfSize = file.bfOffBits + bytes;
	dib.bV5Size = sizeof(dib);
	dib.bV5Width = static_cast<LONG>(image.width);
	dib.bV5Height = -static_cast<LONG>(image.height);
	dib.bV5Planes = 1;
	dib.bV5BitCount = 32;
	dib.bV5Compression = BI_BITFIELDS;
	dib.bV5SizeImage = bytes;
	dib.bV5RedMask = 0x00FF0000;
	dib.bV5GreenMask = 0x0000FF00;
	dib.bV5BlueMask = 0x000000FF;
	dib.bV5AlphaMask = 0xFF000000;
	dib.bV5CSType = LCS_sRGB;
	std::ofstream out(path, std::ios::binary);
	return out && out.write(reinterpret_cast<const char *>(&file), sizeof(file)) && out.write(reinterpret_cast<const char *>(&dib), sizeof(dib)) &&
		   out.write(reinterpret_cast<const char *>(image.bgra.data()), image.bgra.size());
}
}
int wmain(int argc, wchar_t ** argv)
{
	if (argc != 3)
	{
		std::wcerr << L"Usage: DefDump.exe input.def output.bmp\n";
		return 2;
	}
	std::vector<uint8_t> bytes;
	if (!readFile(argv[1], bytes))
	{
		std::wcerr << L"Cannot read input file\n";
		return 3;
	}
	defthumb::DecodeResult result;
	std::string error;
	if (!defthumb::DefDecoder::DecodeFirstUseful(bytes, result, error))
	{
		std::cerr << "Decode failed: " << error << "\n";
		return 4;
	}
	if (!writeBitmap(argv[2], result.image))
	{
		std::wcerr << L"Cannot write BMP\n";
		return 5;
	}
	std::cout << "Decoded group=" << result.frame.group << " frame=" << result.frame.index << " format=" << result.frame.format
			  << " size=" << result.image.width << "x" << result.image.height << " name=" << result.frame.name << "\n";
	return 0;
}
