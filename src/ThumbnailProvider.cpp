#include "ThumbnailProvider.h"

#include "ComHelpers.h"
#include "D32Decoder.h"
#include "DefDecoder.h"
#include "DllGlobals.h"
#include "ImageScaler.h"
#include "P32Decoder.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <vector>

namespace
{
constexpr ULONGLONG MAX_FILE_SIZE = 256ull * 1024ull * 1024ull;
constexpr size_t READ_CHUNK_SIZE = 1024 * 1024;
constexpr wchar_t LEGACY_CACHE_KEY_SUFFIX[] = L"|DefThumb-20260922";

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

HRESULT
readAll(const std::wstring & filePath, std::vector<uint8_t> & bytes)
{
	HANDLE file = CreateFileW(filePath.c_str(),
							  GENERIC_READ,
							  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
							  nullptr,
							  OPEN_EXISTING,
							  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
							  nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return HRESULT_FROM_WIN32(GetLastError());

	LARGE_INTEGER fileSize{};
	if (!GetFileSizeEx(file, &fileSize))
	{
		const DWORD error = GetLastError();
		CloseHandle(file);
		return HRESULT_FROM_WIN32(error);
	}
	if (fileSize.QuadPart < 0 || static_cast<ULONGLONG>(fileSize.QuadPart) > MAX_FILE_SIZE)
	{
		CloseHandle(file);
		return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
	}

	bytes.resize(static_cast<size_t>(fileSize.QuadPart));
	size_t totalBytesRead = 0;
	while (totalBytesRead < bytes.size())
	{
		const DWORD requested = static_cast<DWORD>(std::min(bytes.size() - totalBytesRead, READ_CHUNK_SIZE));
		DWORD bytesRead = 0;
		if (!ReadFile(file, bytes.data() + totalBytesRead, requested, &bytesRead, nullptr))
		{
			const DWORD error = GetLastError();
			CloseHandle(file);
			return HRESULT_FROM_WIN32(error);
		}
		if (bytesRead == 0)
		{
			CloseHandle(file);
			return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
		}
		totalBytesRead += bytesRead;
	}

	CloseHandle(file);
	return S_OK;
}

HRESULT
createThumbnail(const std::vector<uint8_t> & bytes, UINT requestedSize, HBITMAP * bitmap, const COLORREF * backgroundColor = nullptr)
{
	defthumb::DecodeResult decoded;
	std::string error;
	bool decodedSuccessfully = false;
	if (defthumb::D32Decoder::IsD32(bytes))
		decodedSuccessfully = defthumb::D32Decoder::DecodeFirstUseful(bytes, decoded, error);
	else if (defthumb::P32Decoder::IsP32(bytes))
		decodedSuccessfully = defthumb::P32Decoder::Decode(bytes, decoded, error);
	else
		decodedSuccessfully = defthumb::DefDecoder::DecodeFirstUseful(bytes, decoded, error);
	if (!decodedSuccessfully)
	{
		DEFTHUMB_LOG(L"Decode failed");
		return HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
	}

	HBITMAP result = backgroundColor ? defthumb::CreateThumbnailBitmap(decoded.image, requestedSize, *backgroundColor)
									 : defthumb::CreateThumbnailBitmap(decoded.image, requestedSize);
	if (!result)
		return E_OUTOFMEMORY;

	*bitmap = result;
	return S_OK;
}
}

ThumbnailProvider::ThumbnailProvider() noexcept
  : referenceCount_(1)
  , stream_(nullptr)
  , legacyRequestedSize_(0)
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
	else if (IsEqualIID(interfaceId, IID_IPersist) || IsEqualIID(interfaceId, IID_IPersistFile))
		*object = static_cast<IPersistFile *>(this);
	else if (IsEqualIID(interfaceId, IID_IExtractImage))
		*object = static_cast<IExtractImage *>(this);
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
	if (stream_ || !filePath_.empty())
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

		const HRESULT result = createThumbnail(bytes, requestedSize, bitmap);
		if (FAILED(result))
			return result;
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

HRESULT
ThumbnailProvider::GetClassID(CLSID * classId) noexcept
{
	if (!classId)
		return E_POINTER;
	*classId = CLSID_DefThumbnailProvider;
	return S_OK;
}

HRESULT
ThumbnailProvider::IsDirty() noexcept
{
	return S_FALSE;
}

HRESULT
ThumbnailProvider::Load(LPCOLESTR fileName, DWORD) noexcept
{
	if (!fileName)
		return E_INVALIDARG;
	if (stream_ || !filePath_.empty())
		return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);

	try
	{
		filePath_ = fileName;
		return filePath_.empty() ? E_INVALIDARG : S_OK;
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

HRESULT
ThumbnailProvider::Save(LPCOLESTR, BOOL) noexcept
{
	return E_NOTIMPL;
}

HRESULT
ThumbnailProvider::SaveCompleted(LPCOLESTR) noexcept
{
	return E_NOTIMPL;
}

HRESULT
ThumbnailProvider::GetCurFile(LPOLESTR * fileName) noexcept
{
	if (!fileName)
		return E_POINTER;
	*fileName = nullptr;
	if (filePath_.empty())
		return S_FALSE;

	const size_t byteCount = (filePath_.size() + 1) * sizeof(wchar_t);
	auto * result = static_cast<wchar_t *>(CoTaskMemAlloc(byteCount));
	if (!result)
		return E_OUTOFMEMORY;
	std::memcpy(result, filePath_.c_str(), byteCount);
	*fileName = result;
	return S_OK;
}

HRESULT
ThumbnailProvider::GetLocation(LPWSTR pathBuffer, DWORD pathBufferLength, DWORD * priority, const SIZE * requestedSize, DWORD, DWORD * flags) noexcept
{
	if (!pathBuffer || pathBufferLength == 0 || !requestedSize || !flags)
		return E_INVALIDARG;
	if (filePath_.empty())
		return E_UNEXPECTED;
	constexpr size_t suffixLength = (sizeof(LEGACY_CACHE_KEY_SUFFIX) / sizeof(LEGACY_CACHE_KEY_SUFFIX[0])) - 1;
	const size_t requiredLength = filePath_.size() + suffixLength + 1;
	if (requiredLength > pathBufferLength)
		return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);

	std::memcpy(pathBuffer, filePath_.data(), filePath_.size() * sizeof(wchar_t));
	std::memcpy(pathBuffer + filePath_.size(), LEGACY_CACHE_KEY_SUFFIX, (suffixLength + 1) * sizeof(wchar_t));
	if (priority)
		*priority = 0;
	*flags |= IEIFLAG_CACHE;
	legacyRequestedSize_ = static_cast<UINT>(std::max<LONG>(1, std::max(requestedSize->cx, requestedSize->cy)));
	return S_OK;
}

HRESULT
ThumbnailProvider::Extract(HBITMAP * bitmap) noexcept
{
	if (!bitmap)
		return E_POINTER;
	*bitmap = nullptr;
	if (filePath_.empty() || legacyRequestedSize_ == 0)
		return E_UNEXPECTED;

	try
	{
		std::vector<uint8_t> bytes;
		const HRESULT readResult = readAll(filePath_, bytes);
		if (FAILED(readResult))
			return readResult;
		const COLORREF backgroundColor = GetSysColor(COLOR_WINDOW);
		return createThumbnail(bytes, legacyRequestedSize_, bitmap, &backgroundColor);
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
