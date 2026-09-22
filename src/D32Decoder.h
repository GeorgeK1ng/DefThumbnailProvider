#pragma once

#include "DefDecoder.h"

namespace defthumb
{
class D32Decoder final
{
public:
	static bool IsD32(const std::vector<uint8_t> & data) noexcept;
	static bool DecodeFirstUseful(const std::vector<uint8_t> & data, DecodeResult & result, std::string & error) noexcept;
};
}
