#pragma once
#include <unknwn.h>
class ClassFactory final : public IClassFactory
{
public:
	ClassFactory() noexcept;

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interfaceId, void ** object) noexcept override;
	ULONG STDMETHODCALLTYPE AddRef() noexcept override;
	ULONG STDMETHODCALLTYPE Release() noexcept override;
	HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown * outer, REFIID interfaceId, void ** object) noexcept override;
	HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) noexcept override;

private:
	~ClassFactory();

	volatile long referenceCount_;
};
