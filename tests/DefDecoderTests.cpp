#include "DefDecoder.h"
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
namespace
{
void appendU32(std::vector<uint8_t> & data, uint32_t value)
{
	for (int i = 0; i < 4; ++i)
		data.push_back(static_cast<uint8_t>(value >> (8 * i)));
}

std::vector<uint8_t> makeUncompressedDef()
{
	std::vector<uint8_t> data;
	appendU32(data, 0x47);
	appendU32(data, 2);
	appendU32(data, 2);
	appendU32(data, 1);
	for (int i = 0; i < 256; ++i)
	{
		data.push_back(static_cast<uint8_t>(i));
		data.push_back(static_cast<uint8_t>(i));
		data.push_back(static_cast<uint8_t>(i));
	}
	appendU32(data, 0);
	appendU32(data, 1);
	appendU32(data, 0);
	appendU32(data, 0);
	for (int i = 0; i < 13; ++i)
		data.push_back(i == 0 ? 'T' : 0);
	const uint32_t frameOffset = 16 + 768 + 16 + 13 + 4;
	appendU32(data, frameOffset);
	appendU32(data, 4);
	appendU32(data, 0);
	appendU32(data, 2);
	appendU32(data, 2);
	appendU32(data, 2);
	appendU32(data, 2);
	appendU32(data, 0);
	appendU32(data, 0);
	data.insert(data.end(), { 10, 20, 30, 40 });
	return data;
}
}
int main()
{
	defthumb::DecodeResult out;
	std::string err;
	auto valid = makeUncompressedDef();
	if (!defthumb::DefDecoder::DecodeFirstUseful(valid, out, err) || out.frame.format != 0 || out.image.width != 2 || out.image.height != 2)
	{
		std::cerr << "valid decode failed: " << err << "\n";
		return 1;
	}
	valid.resize(20);
	if (defthumb::DefDecoder::DecodeFirstUseful(valid, out, err))
	{
		std::cerr << "truncated input accepted\n";
		return 2;
	}
	std::vector<uint8_t> empty;
	if (defthumb::DefDecoder::DecodeFirstUseful(empty, out, err))
	{
		std::cerr << "empty input accepted\n";
		return 3;
	}
	return 0;
}
