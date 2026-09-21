#pragma once

#include <shobjidl.h>
#include <shlobj.h>
#include <thumbcache.h>

#include <string>

class ThumbnailProvider final
  : public IInitializeWithStream
  , public IThumbnailProvider
  , public IPersistFile
  , public IExtractImage
{
public:
	ThumbnailProvider() noexcept;

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interfaceId, void ** object) noexcept override;
	ULONG STDMETHODCALLTYPE AddRef() noexcept override;
	ULONG STDMETHODCALLTYPE Release() noexcept override;
	HRESULT STDMETHODCALLTYPE Initialize(IStream * stream, DWORD mode) noexcept override;
	HRESULT STDMETHODCALLTYPE GetThumbnail(UINT requestedSize, HBITMAP * bitmap, WTS_ALPHATYPE * alphaType) noexcept override;

	HRESULT STDMETHODCALLTYPE GetClassID(CLSID * classId) noexcept override;
	HRESULT STDMETHODCALLTYPE IsDirty() noexcept override;
	HRESULT STDMETHODCALLTYPE Load(LPCOLESTR fileName, DWORD mode) noexcept override;
	HRESULT STDMETHODCALLTYPE Save(LPCOLESTR fileName, BOOL remember) noexcept override;
	HRESULT STDMETHODCALLTYPE SaveCompleted(LPCOLESTR fileName) noexcept override;
	HRESULT STDMETHODCALLTYPE GetCurFile(LPOLESTR * fileName) noexcept override;

	HRESULT STDMETHODCALLTYPE
	GetLocation(LPWSTR pathBuffer, DWORD pathBufferLength, DWORD * priority, const SIZE * requestedSize, DWORD colorDepth, DWORD * flags) noexcept override;
	HRESULT STDMETHODCALLTYPE Extract(HBITMAP * bitmap) noexcept override;

private:
	~ThumbnailProvider();

	volatile long referenceCount_;
	IStream * stream_;
	std::wstring filePath_;
	UINT legacyRequestedSize_;
};
