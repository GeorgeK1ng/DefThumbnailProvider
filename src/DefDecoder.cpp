/*
 * DEF parsing/RLE rules adapted from VCMI client/render/CDefFile.cpp and
 * mapeditor/Animation.cpp (GPL-2.0-or-later). Rewritten with bounded reads
 * and without VCMI, SDL, or Qt dependencies.
 */
#include "DefDecoder.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>
namespace defthumb
{
namespace
{
	constexpr size_t HEADER = 16, PALSIZE = 768, FRAMEHDR = 32;
	constexpr uint32_t MAXDIM = 16384, MAXFRAMES = 100000;
	constexpr size_t MAXPIXELS = 64u * 1024u * 1024u;
	struct Color
	{
		uint8_t b, g, r, a;
	};
	struct Entry
	{
		uint32_t group, index, offset;
		std::string name;
	};
	class Reader
	{
	public:
		Reader(const std::vector<uint8_t> & b, size_t p, size_t e)
		  : b_(b)
		  , p_(p)
		  , e_(std::min(e, b.size()))
		{
			if (p > e_)
				fail();
		}
		size_t pos() const
		{
			return p_;
		}
		size_t remaining() const
		{
			return e_ - p_;
		}
		void skip(size_t n)
		{
			req(n);
			p_ += n;
		}
		uint8_t u8()
		{
			req(1);
			return b_[p_++];
		}
		uint16_t u16()
		{
			req(2);
			uint16_t v = uint16_t(b_[p_]) | (uint16_t(b_[p_ + 1]) << 8);
			p_ += 2;
			return v;
		}
		uint32_t u32()
		{
			req(4);
			uint32_t v = uint32_t(b_[p_]) | (uint32_t(b_[p_ + 1]) << 8) | (uint32_t(b_[p_ + 2]) << 16) | (uint32_t(b_[p_ + 3]) << 24);
			p_ += 4;
			return v;
		}
		int32_t i32()
		{
			return static_cast<int32_t>(u32());
		}
		const uint8_t * take(size_t n)
		{
			req(n);
			const auto * q = b_.data() + p_;
			p_ += n;
			return q;
		}

	private:
		[[noreturn]] static void fail()
		{
			throw std::runtime_error("truncated or invalid DEF data");
		}
		void req(size_t n) const
		{
			if (n > e_ - p_)
				fail();
		}
		const std::vector<uint8_t> & b_;
		size_t p_, e_;
	};
	bool mul(size_t a, size_t b, size_t & o)
	{
		if (a && b > std::numeric_limits<size_t>::max() / a)
			return false;
		o = a * b;
		return true;
	}
	bool similar(uint8_t r, uint8_t g, uint8_t b, uint8_t er, uint8_t eg, uint8_t eb)
	{
		return std::abs(int(r) - er) < 8 && std::abs(int(g) - eg) < 8 && std::abs(int(b) - eb) < 8;
	}
	std::array<Color, 256> palette(Reader & r)
	{
		std::array<Color, 256> p{};
		for (auto & c : p)
		{
			c.r = r.u8();
			c.g = r.u8();
			c.b = r.u8();
			c.a = 255;
		}
		const uint8_t src[8][3] = { { 0, 255, 255 }, { 255, 150, 255 }, { 255, 100, 255 }, { 255, 50, 255 },
									{ 255, 0, 255 }, { 255, 255, 0 },   { 180, 0, 255 },   { 0, 255, 0 } };
		const uint8_t alpha[8] = { 0, 64, 64, 128, 128, 0, 128, 64 };
		p[0] = { 0, 0, 0, 0 };
		p[1] = { 0, 0, 0, 64 };
		p[4] = { 0, 0, 0, 128 };
		for (size_t i = 0; i < 8; ++i)
			if (similar(p[i].r, p[i].g, p[i].b, src[i][0], src[i][1], src[i][2]))
				p[i] = { 0, 0, 0, alpha[i] };
		return p;
	}
	void run(std::vector<uint8_t> & out, size_t row, uint32_t width, uint32_t & x, const uint8_t * src, uint32_t n, uint8_t value, bool raw)
	{
		if (!n || x > width || n > width - x)
			throw std::runtime_error("RLE row exceeds frame width");
		if (raw)
			std::memcpy(out.data() + row + x, src, n);
		else
			std::memset(out.data() + row + x, value, n);
		x += n;
	}
	DecodeResult decode(const std::vector<uint8_t> & data, const std::array<Color, 256> & pal, const Entry & e, size_t end)
	{
		Reader h(data, e.offset, end);
		uint32_t packed = h.u32();
		FrameInfo f{};
		f.group = e.group;
		f.index = e.index;
		f.name = e.name;
		f.format = h.u32();
		f.fullWidth = h.u32();
		f.fullHeight = h.u32();
		f.width = h.u32();
		f.height = h.u32();
		f.leftMargin = h.i32();
		f.topMargin = h.i32();
		if (f.format > 3)
			throw std::runtime_error("unsupported DEF compression format");
		if (!f.width || !f.height || !f.fullWidth || !f.fullHeight || f.width > MAXDIM || f.height > MAXDIM || f.fullWidth > MAXDIM || f.fullHeight > MAXDIM)
			throw std::runtime_error("invalid frame dimensions");
		size_t base = size_t(e.offset) + FRAMEHDR;
		if (f.format == 1 && f.width > f.fullWidth && f.height > f.fullHeight)
		{
			f.leftMargin = f.topMargin = 0;
			f.width = f.fullWidth;
			f.height = f.fullHeight;
			base -= 16;
		}
		// The first field is the encoded payload size, excluding the 32-byte frame header.
		if (packed)
		{
			const size_t limit = std::numeric_limits<size_t>::max();
			if (size_t(packed) > limit - FRAMEHDR || size_t(e.offset) > limit - FRAMEHDR - size_t(packed))
				throw std::runtime_error("frame byte size overflow");
			size_t de = size_t(e.offset) + FRAMEHDR + packed;
			if (de > data.size())
				throw std::runtime_error("invalid frame byte size");
			end = std::min(end, de);
		}
		if (base > end)
			throw std::runtime_error("truncated frame header");
		size_t pixels = 0;
		if (!mul(f.width, f.height, pixels) || pixels > MAXPIXELS)
			throw std::runtime_error("frame too large");
		std::vector<uint8_t> idx(pixels);
		if (f.format == 0)
		{
			Reader r(data, base, end);
			std::memcpy(idx.data(), r.take(pixels), pixels);
		}
		else if (f.format == 1)
		{
			Reader t(data, base, end);
			std::vector<uint32_t> rows(f.height);
			for (auto & o : rows)
				o = t.u32();
			for (uint32_t y = 0; y < f.height; ++y)
			{
				size_t rp = base + rows[y];
				if (rp < base || rp > end)
					throw std::runtime_error("bad row offset");
				Reader r(data, rp, end);
				uint32_t x = 0;
				while (x < f.width)
				{
					uint8_t c = r.u8();
					uint32_t n = uint32_t(r.u8()) + 1;
					const uint8_t * s = c == 255 ? r.take(n) : nullptr;
					run(idx, size_t(y) * f.width, f.width, x, s, n, c, c == 255);
				}
			}
		}
		else if (f.format == 2)
		{
			Reader t(data, base, end);
			size_t sp = base + t.u16();
			if (sp < base || sp > end)
				throw std::runtime_error("bad format-2 offset");
			Reader r(data, sp, end);
			for (uint32_t y = 0; y < f.height; ++y)
			{
				uint32_t x = 0;
				while (x < f.width)
				{
					uint8_t s = r.u8(), c = s / 32;
					uint32_t n = (s & 31) + 1;
					const uint8_t * q = c == 7 ? r.take(n) : nullptr;
					run(idx, size_t(y) * f.width, f.width, x, q, n, c, c == 7);
				}
			}
		}
		else
		{
			if (f.width < 32 || f.width % 32)
				throw std::runtime_error("invalid format-3 width");
			size_t per = f.width / 32, ents = 0, bytes = 0;
			if (!mul(per, f.height, ents) || !mul(ents, 2, bytes) || bytes > end - base)
				throw std::runtime_error("truncated format-3 table");
			for (uint32_t y = 0; y < f.height; ++y)
			{
				Reader t(data, base + size_t(y) * per * 2, end);
				size_t rp = base + t.u16();
				if (rp < base || rp > end)
					throw std::runtime_error("bad format-3 row offset");
				Reader r(data, rp, end);
				uint32_t x = 0;
				while (x < f.width)
				{
					uint8_t s = r.u8(), c = s / 32;
					uint32_t n = (s & 31) + 1;
					const uint8_t * q = c == 7 ? r.take(n) : nullptr;
					run(idx, size_t(y) * f.width, f.width, x, q, n, c, c == 7);
				}
			}
		}
		size_t cp = 0, cb = 0;
		if (!mul(f.fullWidth, f.fullHeight, cp) || cp > MAXPIXELS || !mul(cp, 4, cb))
			throw std::runtime_error("canvas too large");
		DecodeResult o{};
		o.frame = f;
		o.image.width = f.fullWidth;
		o.image.height = f.fullHeight;
		o.image.bgra.assign(cb, 0);
		for (uint32_t y = 0; y < f.height; ++y)
			for (uint32_t x = 0; x < f.width; ++x)
			{
				int64_t dx = int64_t(f.leftMargin) + x, dy = int64_t(f.topMargin) + y;
				if (dx < 0 || dy < 0 || dx >= f.fullWidth || dy >= f.fullHeight)
					continue;
				const auto & c = pal[idx[size_t(y) * f.width + x]];
				size_t d = (size_t(dy) * f.fullWidth + size_t(dx)) * 4;
				o.image.bgra[d] = c.b;
				o.image.bgra[d + 1] = c.g;
				o.image.bgra[d + 2] = c.r;
				o.image.bgra[d + 3] = c.a;
			}
		return o;
	}
	bool useful(const Image & i)
	{
		size_t n = 0;
		for (size_t p = 3; p < i.bgra.size(); p += 4)
			if (i.bgra[p] && ++n >= 2)
				return true;
		return false;
	}
}
bool DefDecoder::DecodeAll(const std::vector<uint8_t> & data, std::vector<DecodeResult> & results, std::string & error) noexcept
{
	try
	{
		results.clear();
		if (data.size() < HEADER + PALSIZE)
			throw std::runtime_error("file too small for DEF header");
		Reader r(data, 0, data.size());
		(void)r.u32();
		(void)r.u32();
		(void)r.u32();
		uint32_t groups = r.u32();
		if (!groups || groups > MAXFRAMES)
			throw std::runtime_error("invalid group count");
		auto pal = palette(r);
		std::vector<Entry> es;
		for (uint32_t g = 0; g < groups; ++g)
		{
			uint32_t id = r.u32(), count = r.u32();
			r.skip(8);
			if (count > MAXFRAMES || es.size() + count > MAXFRAMES)
				throw std::runtime_error("too many frames");
			std::vector<std::string> names;
			names.reserve(count);
			for (uint32_t i = 0; i < count; ++i)
			{
				auto * q = r.take(13);
				size_t n = 0;
				while (n < 13 && q[n])
					++n;
				names.emplace_back(reinterpret_cast<const char *>(q), n);
			}
			for (uint32_t i = 0; i < count; ++i)
			{
				uint32_t off = r.u32();
				if (off > data.size() || data.size() - off < FRAMEHDR)
					throw std::runtime_error("frame offset outside file");
				es.push_back({ id, i, off, std::move(names[i]) });
			}
		}
		if (es.empty())
			throw std::runtime_error("DEF has no frames");
		std::string last;
		for (const auto & e : es)
		{
			size_t end = data.size();
			for (const auto & x : es)
				if (x.offset > e.offset)
					end = std::min(end, size_t(x.offset));
			try
			{
				results.push_back(decode(data, pal, e, end));
			}
			catch (const std::exception & ex)
			{
				last = ex.what();
			}
		}
		if (results.empty())
			throw std::runtime_error(last.empty() ? "no decodable frame" : "no decodable frame: " + last);
		error.clear();
		return true;
	}
	catch (const std::exception & ex)
	{
		error = ex.what();
		return false;
	}
	catch (...)
	{
		error = "unknown decoder failure";
		return false;
	}
}

bool DefDecoder::DecodeFirstUseful(const std::vector<uint8_t> & data, DecodeResult & result, std::string & error) noexcept
{
	std::vector<DecodeResult> results;
	if (!DecodeAll(data, results, error))
		return false;
	for (auto & candidate : results)
		if (useful(candidate.image))
		{
			result = std::move(candidate);
			error.clear();
			return true;
		}
	error = "no non-empty frame";
	return false;
}
}
