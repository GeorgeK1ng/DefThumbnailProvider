#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace defthumb
{
struct Image
{
	uint32_t width = 0;
	uint32_t height = 0;
	std::vector<uint8_t> bgra;
};

struct FrameInfo
{
	uint32_t group = 0;
	uint32_t index = 0;
	uint32_t format = 0;
	uint32_t fullWidth = 0;
	uint32_t fullHeight = 0;
	uint32_t width = 0;
	uint32_t height = 0;
	int32_t leftMargin = 0;
	int32_t topMargin = 0;
	std::string name;
};

struct DecodeResult
{
	Image image;
	FrameInfo frame;
};

class DefDecoder final
{
public:
	static bool DecodeFirstUseful(const std::vector<uint8_t> & data, DecodeResult & result, std::string & error) noexcept;
	static bool DecodeAll(const std::vector<uint8_t> & data, std::vector<DecodeResult> & results, std::string & error) noexcept;
};
}
