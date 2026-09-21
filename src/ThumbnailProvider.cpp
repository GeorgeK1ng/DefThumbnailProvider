#include "ThumbnailProvider.h"

#include "ComHelpers.h"
#include "DefDecoder.h"
#include "DllGlobals.h"
#include "ImageScaler.h"

#include <algorithm>
#include <new>
#include <vector>

namespace
{
constexpr ULONGLONG MAX_FILE_SIZE = 256ull * 1024ull * 1024ull;
constexpr size_t READ_CHUNK_SIZE = 1024 * 1024;

HRESULT
readAll(IStream * stream, std::vector<uint8_t> & bytes)
{
	LARGE_INTEGER start{};
	HRESULT result = stream->Seek(start, STREAM_SEEK_SET, nullptr);
	if (FAILED(result))
		return result;

	STATSTG statistics{};
	result = stream->Stat(&statistics, STATFLAG_NONAME);
	if (FAILED(result))
		return result;

	if (statistics.cbSize.QuadPart < 0 || static_cast<ULONGLONG>(statistics.cbSize.QuadPart) > MAX_FILE_SIZE)
		return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);

	bytes.resize(static_cast<size_t>(statistics.cbSize.QuadPart));
	size_t bytesRead = 0;
	while (bytesRead < bytes.size())
	{
		const ULONG requested = static_cast<ULONG>(std::min(bytes.size() - bytesRead, READ_CHUNK_SIZE));
		ULONG received = 0;
		result = stream->Read(bytes.data() + bytesRead, requested, &received);
		if (FAILED(result))
			return result;
		if (received == 0)
			return STG_E_READFAULT;
		bytesRead += received;
	}
	return S_OK;
}
}

ThumbnailProvider::ThumbnailProvider() noexcept
  : referenceCount_(1)
  , stream_(nullptr)
{
	InterlockedIncrement(&g_objectCount);
}

ThumbnailProvider::~ThumbnailProvider()
{
	safeRelease(stream_);
	InterlockedDecrement(&g_objectCount);
}

HRESULT
ThumbnailProvider::QueryInterface(REFIID interfaceId, void ** object) noexcept
{
	if (!object)
		return E_POINTER;

	*object = nullptr;
	if (IsEqualIID(interfaceId, IID_IUnknown) || IsEqualIID(interfaceId, IID_IInitializeWithStream))
		*object = static_cast<IInitializeWithStream *>(this);
	else if (IsEqualIID(interfaceId, IID_IThumbnailProvider))
		*object = static_cast<IThumbnailProvider *>(this);
	else
		return E_NOINTERFACE;

	AddRef();
	return S_OK;
}

ULONG
ThumbnailProvider::AddRef() noexcept
{
	return static_cast<ULONG>(InterlockedIncrement(&referenceCount_));
}

ULONG
ThumbnailProvider::Release() noexcept
{
	const ULONG referenceCount = static_cast<ULONG>(InterlockedDecrement(&referenceCount_));
	if (referenceCount == 0)
		delete this;
	return referenceCount;
}

HRESULT
ThumbnailProvider::Initialize(IStream * stream, DWORD) noexcept
{
	if (!stream)
		return E_INVALIDARG;
	if (stream_)
		return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);

	stream->AddRef();
	stream_ = stream;
	return S_OK;
}

HRESULT
ThumbnailProvider::GetThumbnail(UINT requestedSize, HBITMAP * bitmap, WTS_ALPHATYPE * alphaType) noexcept
{
	if (!bitmap || !alphaType)
		return E_POINTER;

	*bitmap = nullptr;
	*alphaType = WTSAT_UNKNOWN;
	if (!stream_ || requestedSize == 0)
		return E_UNEXPECTED;

	try
	{
		std::vector<uint8_t> bytes;
		const HRESULT readResult = readAll(stream_, bytes);
		if (FAILED(readResult))
			return readResult;

		defthumb::DecodeResult decoded;
		std::string error;
		if (!defthumb::DefDecoder::DecodeFirstUseful(bytes, decoded, error))
		{
			DEFTHUMB_LOG(L"Decode failed");
			return HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
		}

		HBITMAP result = defthumb::CreateThumbnailBitmap(decoded.image, requestedSize);
		if (!result)
			return E_OUTOFMEMORY;

		*bitmap = result;
		*alphaType = WTSAT_ARGB;
		return S_OK;
	}
	catch (const std::bad_alloc &)
	{
		return E_OUTOFMEMORY;
	}
	catch (...)
	{
		return E_FAIL;
	}
}
