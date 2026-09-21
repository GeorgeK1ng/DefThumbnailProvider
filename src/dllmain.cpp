#include "ClassFactory.h"
#include "DllGlobals.h"
#include "Registration.h"

#include <new>

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		g_module = module;
		DisableThreadLibraryCalls(module);
	}
	return TRUE;
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID classId, REFIID interfaceId, void ** object)
{
	if (!IsEqualCLSID(classId, CLSID_DefThumbnailProvider))
		return CLASS_E_CLASSNOTAVAILABLE;

	auto * factory = new (std::nothrow) ClassFactory();
	if (!factory)
		return E_OUTOFMEMORY;

	const HRESULT result = factory->QueryInterface(interfaceId, object);
	factory->Release();
	return result;
}

extern "C" HRESULT __stdcall DllCanUnloadNow()
{
	return g_objectCount == 0 && g_lockCount == 0 ? S_OK : S_FALSE;
}

extern "C" HRESULT __stdcall DllRegisterServer()
{
	return RegisterServer();
}

extern "C" HRESULT __stdcall DllUnregisterServer()
{
	return UnregisterServer();
}

extern "C" HRESULT __stdcall DllInstall(BOOL install, LPCWSTR commandLine)
{
	if (!commandLine || _wcsicmp(commandLine, L"machine") != 0)
		return E_INVALIDARG;
	return install ? RegisterServerMachine() : UnregisterServerMachine();
}
