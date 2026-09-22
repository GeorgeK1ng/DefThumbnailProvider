#include "D32Decoder.h"
#include "DefDecoder.h"
#include "P32Decoder.h"
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

std::vector<uint8_t> makeD32()
{
	std::vector<uint8_t> data;
	appendU32(data, 0x46323344);
	appendU32(data, 1);
	appendU32(data, 24);
	appendU32(data, 2);
	appendU32(data, 2);
	appendU32(data, 1);
	appendU32(data, 8);
	appendU32(data, 1);
	appendU32(data, 33);
	appendU32(data, 7);
	appendU32(data, 1);
	appendU32(data, 0);
	for (int index = 0; index < 13; ++index)
		data.push_back(index == 0 ? 'D' : 0);
	appendU32(data, 65);
	appendU32(data, 32);
	appendU32(data, 16);
	appendU32(data, 2);
	appendU32(data, 2);
	appendU32(data, 2);
	appendU32(data, 2);
	appendU32(data, 0);
	appendU32(data, 0);
	appendU32(data, 8);
	appendU32(data, 0);
	data.insert(data.end(),
				{
				  255,
				  0,
				  0,
				  255,
				  0,
				  255,
				  255,
				  255,
				  0,
				  0,
				  255,
				  255,
				  0,
				  255,
				  0,
				  255,
				});
	return data;
}

std::vector<uint8_t> makeP32()
{
	std::vector<uint8_t> data;
	appendU32(data, 0x46323350);
	appendU32(data, 0);
	appendU32(data, 32);
	appendU32(data, 56);
	appendU32(data, 40);
	appendU32(data, 16);
	appendU32(data, 2);
	appendU32(data, 2);
	appendU32(data, 8);
	appendU32(data, 0);
	data.insert(data.end(),
				{
				  255,
				  0,
				  0,
				  255,
				  0,
				  255,
				  255,
				  255,
				  0,
				  0,
				  255,
				  255,
				  0,
				  255,
				  0,
				  255,
				});
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

	auto d32 = makeD32();
	if (!defthumb::D32Decoder::IsD32(d32) || !defthumb::D32Decoder::DecodeFirstUseful(d32, out, err) || out.frame.group != 7 || out.image.width != 2 ||
		out.image.height != 2 || out.image.bgra[0] != 0 || out.image.bgra[1] != 0 || out.image.bgra[2] != 255 || out.image.bgra[3] != 255)
	{
		std::cerr << "valid D32 decode failed: " << err << "\n";
		return 4;
	}
	d32.resize(80);
	if (defthumb::D32Decoder::DecodeFirstUseful(d32, out, err))
	{
		std::cerr << "truncated D32 input accepted\n";
		return 5;
	}

	auto p32 = makeP32();
	if (!defthumb::P32Decoder::IsP32(p32) || !defthumb::P32Decoder::Decode(p32, out, err) || out.image.width != 2 || out.image.height != 2 ||
		out.image.bgra[0] != 0 || out.image.bgra[1] != 0 || out.image.bgra[2] != 255 || out.image.bgra[3] != 255)
	{
		std::cerr << "valid P32 decode failed: " << err << "\n";
		return 6;
	}
	p32.resize(48);
	if (defthumb::P32Decoder::Decode(p32, out, err))
	{
		std::cerr << "truncated P32 input accepted\n";
		return 7;
	}
	return 0;
}
