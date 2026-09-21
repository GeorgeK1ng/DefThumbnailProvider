#pragma once

#include <shobjidl.h>
#include <thumbcache.h>

class ThumbnailProvider final
  : public IInitializeWithStream
  , public IThumbnailProvider
{
public:
	ThumbnailProvider() noexcept;

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interfaceId, void ** object) noexcept override;
	ULONG STDMETHODCALLTYPE AddRef() noexcept override;
	ULONG STDMETHODCALLTYPE Release() noexcept override;
	HRESULT STDMETHODCALLTYPE Initialize(IStream * stream, DWORD mode) noexcept override;
	HRESULT STDMETHODCALLTYPE GetThumbnail(UINT requestedSize, HBITMAP * bitmap, WTS_ALPHATYPE * alphaType) noexcept override;

private:
	~ThumbnailProvider();

	volatile long referenceCount_;
	IStream * stream_;
};
