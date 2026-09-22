/*
 * D32 container layout adapted from vcmiextract's extract_def_d32f routine
 * (GPL-2.0). Rewritten as a standalone, bounds-checked decoder.
 */
#include "D32Decoder.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace defthumb
{
namespace
{
	constexpr uint32_t D32_MAGIC = 0x46323344;
	constexpr size_t FILE_HEADER_SIZE = 32;
	constexpr size_t FRAME_HEADER_SIZE = 40;
	constexpr uint32_t MAX_DIMENSION = 16384;
	constexpr uint32_t MAX_FRAME_COUNT = 100000;
	constexpr size_t MAX_PIXEL_COUNT = 64u * 1024u * 1024u;

	struct FrameEntry
	{
		uint32_t group = 0;
		uint32_t index = 0;
		uint32_t offset = 0;
		std::string name;
	};

	class Reader
	{
	public:
		Reader(const std::vector<uint8_t> & data, size_t position = 0)
		  : data_(data)
		  , position_(position)
		{
			if (position > data.size())
				fail();
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
		[[noreturn]] static void fail()
		{
			throw std::runtime_error("truncated or invalid D32 data");
		}

		void require(size_t byteCount) const
		{
			if (byteCount > data_.size() - position_)
				fail();
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

	bool isUseful(const Image & image)
	{
		size_t visiblePixels = 0;
		for (size_t offset = 3; offset < image.bgra.size(); offset += 4)
		{
			if (image.bgra[offset] != 0 && ++visiblePixels >= 2)
				return true;
		}
		return false;
	}

	DecodeResult decodeFrame(const std::vector<uint8_t> & data, const FrameEntry & entry)
	{
		Reader reader(data, entry.offset);
		const uint32_t bitsPerPixel = reader.readU32();
		const uint32_t imageSize = reader.readU32();

		FrameInfo frame{};
		frame.group = entry.group;
		frame.index = entry.index;
		frame.format = 32;
		frame.fullWidth = reader.readU32();
		frame.fullHeight = reader.readU32();
		frame.width = reader.readU32();
		frame.height = reader.readU32();
		frame.leftMargin = static_cast<int32_t>(reader.readU32());
		frame.topMargin = static_cast<int32_t>(reader.readU32());
		frame.name = entry.name;
		const uint32_t marker = reader.readU32();
		const uint32_t variant = reader.readU32();

		if (bitsPerPixel != 32 || marker != 8 || (variant != 0 && variant != 1))
			throw std::runtime_error("unsupported D32 frame header");
		if (frame.width == 0 || frame.height == 0 || frame.fullWidth == 0 || frame.fullHeight == 0 || frame.width > MAX_DIMENSION ||
			frame.height > MAX_DIMENSION || frame.fullWidth > MAX_DIMENSION || frame.fullHeight > MAX_DIMENSION)
			throw std::runtime_error("invalid D32 frame dimensions");
		if (frame.width > frame.fullWidth || frame.height > frame.fullHeight || frame.leftMargin < 0 || frame.topMargin < 0 ||
			uint32_t(frame.leftMargin) > frame.fullWidth - frame.width || uint32_t(frame.topMargin) > frame.fullHeight - frame.height)
			throw std::runtime_error("D32 frame lies outside its canvas");

		size_t storedPixelCount = 0;
		size_t storedByteCount = 0;
		if (!checkedMultiply(frame.width, frame.height, storedPixelCount) || !checkedMultiply(storedPixelCount, 4, storedByteCount) ||
			storedPixelCount > MAX_PIXEL_COUNT || storedByteCount != imageSize)
			throw std::runtime_error("invalid D32 image size");
		const uint8_t * source = reader.take(storedByteCount);

		size_t canvasPixelCount = 0;
		size_t canvasByteCount = 0;
		if (!checkedMultiply(frame.fullWidth, frame.fullHeight, canvasPixelCount) || !checkedMultiply(canvasPixelCount, 4, canvasByteCount) ||
			canvasPixelCount > MAX_PIXEL_COUNT)
			throw std::runtime_error("D32 canvas too large");

		DecodeResult result{};
		result.frame = frame;
		result.image.width = frame.fullWidth;
		result.image.height = frame.fullHeight;
		result.image.bgra.assign(canvasByteCount, 0);

		for (uint32_t sourceY = 0; sourceY < frame.height; ++sourceY)
		{
			const uint32_t destinationY = uint32_t(frame.topMargin) + frame.height - sourceY - 1;
			for (uint32_t sourceX = 0; sourceX < frame.width; ++sourceX)
			{
				const size_t sourceOffset = (size_t(sourceY) * frame.width + sourceX) * 4;
				const size_t destinationOffset = (size_t(destinationY) * frame.fullWidth + uint32_t(frame.leftMargin) + sourceX) * 4;
				result.image.bgra[destinationOffset] = source[sourceOffset];
				result.image.bgra[destinationOffset + 1] = source[sourceOffset + 1];
				result.image.bgra[destinationOffset + 2] = source[sourceOffset + 2];
				result.image.bgra[destinationOffset + 3] = source[sourceOffset + 3];
			}
		}
		return result;
	}
}

bool D32Decoder::IsD32(const std::vector<uint8_t> & data) noexcept
{
	return data.size() >= 4 && data[0] == 'D' && data[1] == '3' && data[2] == '2' && data[3] == 'F';
}

bool D32Decoder::DecodeFirstUseful(const std::vector<uint8_t> & data, DecodeResult & result, std::string & error) noexcept
{
	try
	{
		if (data.size() < FILE_HEADER_SIZE)
			throw std::runtime_error("file too small for D32 header");

		Reader reader(data);
		if (reader.readU32() != D32_MAGIC || reader.readU32() != 1 || reader.readU32() != 24)
			throw std::runtime_error("invalid D32 header");
		(void)reader.readU32();
		(void)reader.readU32();
		const uint32_t groupCount = reader.readU32();
		if (groupCount == 0 || groupCount > MAX_FRAME_COUNT || reader.readU32() != 8)
			throw std::runtime_error("invalid D32 group header");
		const uint32_t groupVariant = reader.readU32();
		if (groupVariant != 1 && groupVariant != 22)
			throw std::runtime_error("unsupported D32 group variant");

		std::vector<FrameEntry> entries;
		for (uint32_t groupNumber = 0; groupNumber < groupCount; ++groupNumber)
		{
			const uint32_t headerSize = reader.readU32();
			const uint32_t groupIndex = reader.readU32();
			const uint32_t frameCount = reader.readU32();
			(void)reader.readU32();
			if (frameCount > MAX_FRAME_COUNT || entries.size() + frameCount > MAX_FRAME_COUNT || uint64_t(frameCount) * 17 + 16 != headerSize)
				throw std::runtime_error("invalid D32 group size");

			std::vector<std::string> names;
			names.reserve(frameCount);
			for (uint32_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
			{
				const uint8_t * rawName = reader.take(13);
				size_t nameLength = 0;
				while (nameLength < 13 && rawName[nameLength] != 0)
					++nameLength;
				names.emplace_back(reinterpret_cast<const char *>(rawName), nameLength);
			}
			for (uint32_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
			{
				const uint32_t frameOffset = reader.readU32();
				if (frameOffset > data.size() || data.size() - frameOffset < FRAME_HEADER_SIZE)
					throw std::runtime_error("D32 frame offset outside file");
				entries.push_back({ groupIndex, frameIndex, frameOffset, std::move(names[frameIndex]) });
			}
		}

		if (entries.empty())
			throw std::runtime_error("D32 has no frames");

		std::string lastError;
		for (const FrameEntry & entry : entries)
		{
			try
			{
				DecodeResult candidate = decodeFrame(data, entry);
				if (isUseful(candidate.image))
				{
					result = std::move(candidate);
					error.clear();
					return true;
				}
			}
			catch (const std::exception & exception)
			{
				lastError = exception.what();
			}
		}
		throw std::runtime_error(lastError.empty() ? "no non-empty D32 frame" : "no usable D32 frame: " + lastError);
	}
	catch (const std::exception & exception)
	{
		error = exception.what();
		return false;
	}
	catch (...)
	{
		error = "unknown D32 decoder failure";
		return false;
	}
}
}
