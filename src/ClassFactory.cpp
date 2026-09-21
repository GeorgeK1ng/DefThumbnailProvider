#include "ClassFactory.h"
#include "DllGlobals.h"
#include "ThumbnailProvider.h"
#include <new>
ClassFactory::ClassFactory() noexcept
  : referenceCount_(1)
{
	InterlockedIncrement(&g_objectCount);
}

ClassFactory::~ClassFactory()
{
	InterlockedDecrement(&g_objectCount);
}

HRESULT
ClassFactory::QueryInterface(REFIID interfaceId, void ** object) noexcept
{
	if (!object)
		return E_POINTER;

	*object = nullptr;
	if (IsEqualIID(interfaceId, IID_IUnknown) || IsEqualIID(interfaceId, IID_IClassFactory))
		*object = static_cast<IClassFactory *>(this);
	else
		return E_NOINTERFACE;

	AddRef();
	return S_OK;
}

ULONG
ClassFactory::AddRef() noexcept
{
	return static_cast<ULONG>(InterlockedIncrement(&referenceCount_));
}

ULONG
ClassFactory::Release() noexcept
{
	const ULONG referenceCount = static_cast<ULONG>(InterlockedDecrement(&referenceCount_));
	if (referenceCount == 0)
		delete this;
	return referenceCount;
}

HRESULT
ClassFactory::CreateInstance(IUnknown * outer, REFIID interfaceId, void ** object) noexcept
{
	if (!object)
		return E_POINTER;

	*object = nullptr;
	if (outer)
		return CLASS_E_NOAGGREGATION;

	auto * provider = new (std::nothrow) ThumbnailProvider();
	if (!provider)
		return E_OUTOFMEMORY;

	const HRESULT result = provider->QueryInterface(interfaceId, object);
	provider->Release();
	return result;
}

HRESULT
ClassFactory::LockServer(BOOL lock) noexcept
{
	if (lock)
		InterlockedIncrement(&g_lockCount);
	else
		InterlockedDecrement(&g_lockCount);
	return S_OK;
}
