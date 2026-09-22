#pragma once

#include "DefDecoder.h"

namespace defthumb
{
class P32Decoder final
{
public:
	static bool IsP32(const std::vector<uint8_t> & data) noexcept;
	static bool Decode(const std::vector<uint8_t> & data, DecodeResult & result, std::string & error) noexcept;
};
}
