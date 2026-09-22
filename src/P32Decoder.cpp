/*
 * P32 layout adapted from vcmiextract's load_image_pcx routine (GPL-2.0).
 * Rewritten as a standalone, bounds-checked decoder.
 */
#include "P32Decoder.h"

#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace defthumb
{
namespace
{
	constexpr uint32_t P32_MAGIC = 0x46323350;
	constexpr uint32_t HEADER_SIZE = 40;
	constexpr uint32_t MAX_DIMENSION = 16384;
	constexpr size_t MAX_PIXEL_COUNT = 64u * 1024u * 1024u;

	class Reader
	{
	public:
		explicit Reader(const std::vector<uint8_t> & data)
		  : data_(data)
		  , position_(0)
		{
		}

		uint32_t readU32()
		{
			require(4);
			const uint32_t value = uint32_t(data_[position_]) | (uint32_t(data_[position_ + 1]) << 8) | (uint32_t(data_[position_ + 2]) << 16) |
								   (uint32_t(data_[position_ + 3]) << 24);
			position_ += 4;
			return value;
		}

		const uint8_t * take(size_t byteCount)
		{
			require(byteCount);
			const uint8_t * result = data_.data() + position_;
			position_ += byteCount;
			return result;
		}

	private:
		void require(size_t byteCount) const
		{
			if (byteCount > data_.size() - position_)
				throw std::runtime_error("truncated P32 data");
		}

		const std::vector<uint8_t> & data_;
		size_t position_;
	};

	bool checkedMultiply(size_t left, size_t right, size_t & result)
	{
		if (left != 0 && right > std::numeric_limits<size_t>::max() / left)
			return false;
		result = left * right;
		return true;
	}
}

bool P32Decoder::IsP32(const std::vector<uint8_t> & data) noexcept
{
	return data.size() >= 4 && data[0] == 'P' && data[1] == '3' && data[2] == '2' && data[3] == 'F';
}

bool P32Decoder::Decode(const std::vector<uint8_t> & data, DecodeResult & result, std::string & error) noexcept
{
	try
	{
		if (data.size() < HEADER_SIZE)
			throw std::runtime_error("file too small for P32 header");

		Reader reader(data);
		if (reader.readU32() != P32_MAGIC || reader.readU32() != 0 || reader.readU32() != 32)
			throw std::runtime_error("invalid P32 header");
		const uint32_t rawSize = reader.readU32();
		if (reader.readU32() != HEADER_SIZE)
			throw std::runtime_error("unsupported P32 header size");
		const uint32_t dataSize = reader.readU32();
		const uint32_t width = reader.readU32();
		const uint32_t height = reader.readU32();
		const uint32_t marker = reader.readU32();
		const uint32_t variant = reader.readU32();

		if ((marker != 0 && marker != 8) || variant != 0)
			throw std::runtime_error("unsupported P32 variant");
		if (width == 0 || height == 0 || width > MAX_DIMENSION || height > MAX_DIMENSION)
			throw std::runtime_error("invalid P32 dimensions");

		size_t pixelCount = 0;
		size_t expectedDataSize = 0;
		if (!checkedMultiply(width, height, pixelCount) || !checkedMultiply(pixelCount, 4, expectedDataSize) || pixelCount > MAX_PIXEL_COUNT ||
			expectedDataSize != dataSize || uint64_t(HEADER_SIZE) + dataSize != rawSize || rawSize > data.size())
			throw std::runtime_error("invalid P32 data size");

		const uint8_t * source = reader.take(expectedDataSize);
		DecodeResult decoded{};
		decoded.frame.format = 32;
		decoded.frame.fullWidth = width;
		decoded.frame.fullHeight = height;
		decoded.frame.width = width;
		decoded.frame.height = height;
		decoded.image.width = width;
		decoded.image.height = height;
		decoded.image.bgra.resize(expectedDataSize);

		const size_t rowByteCount = size_t(width) * 4;
		for (uint32_t sourceY = 0; sourceY < height; ++sourceY)
		{
			const uint32_t destinationY = height - sourceY - 1;
			std::memcpy(decoded.image.bgra.data() + size_t(destinationY) * rowByteCount, source + size_t(sourceY) * rowByteCount, rowByteCount);
		}

		result = std::move(decoded);
		error.clear();
		return true;
	}
	catch (const std::exception & exception)
	{
		error = exception.what();
		return false;
	}
	catch (...)
	{
		error = "unknown P32 decoder failure";
		return false;
	}
}
}
