#include <windows.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <thumbcache.h>
#include <iostream>
using GetClassObjectFn = HRESULT(__stdcall *)(REFCLSID, REFIID, void **);
int wmain(int argc, wchar_t ** argv)
{
	if (argc != 3)
	{
		std::wcerr << L"Usage: ComSmoke.exe provider.dll input.def\n";
		return 2;
	}
	HMODULE dll = LoadLibraryW(argv[1]);
	if (!dll)
	{
		std::wcerr << L"LoadLibrary failed: " << GetLastError() << L"\n";
		return 3;
	}
	auto getClass = reinterpret_cast<GetClassObjectFn>(GetProcAddress(dll, "DllGetClassObject"));
	if (!getClass)
	{
		FreeLibrary(dll);
		return 4;
	}
	const CLSID clsid = { 0x9b4f3e1c, 0x5d90, 0x4d62, { 0x8f, 0xa2, 0x57, 0xd7, 0xb9, 0xa1, 0x3c, 0x84 } };
	IClassFactory * factory = nullptr;
	HRESULT hr = getClass(clsid, IID_PPV_ARGS(&factory));
	IInitializeWithStream * init = nullptr;
	IThumbnailProvider * provider = nullptr;
	IStream * stream = nullptr;
	HBITMAP bitmap = nullptr;
	if (SUCCEEDED(hr))
		hr = factory->CreateInstance(nullptr, IID_PPV_ARGS(&init));
	if (SUCCEEDED(hr))
		hr = init->QueryInterface(IID_PPV_ARGS(&provider));
	if (SUCCEEDED(hr))
		hr = SHCreateStreamOnFileEx(argv[2], STGM_READ | STGM_SHARE_DENY_WRITE, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &stream);
	if (SUCCEEDED(hr))
		hr = init->Initialize(stream, STGM_READ);
	WTS_ALPHATYPE alpha = WTSAT_UNKNOWN;
	if (SUCCEEDED(hr))
		hr = provider->GetThumbnail(256, &bitmap, &alpha);
	if (SUCCEEDED(hr) && bitmap)
	{
		BITMAP info{};
		GetObjectW(bitmap, sizeof(info), &info);
		std::wcout << L"COM thumbnail: " << info.bmWidth << L"x" << info.bmHeight << L", alpha=" << alpha << L"\n";
		DeleteObject(bitmap);
	}
	if (stream)
		stream->Release();
	if (provider)
		provider->Release();
	if (init)
		init->Release();
	if (factory)
		factory->Release();
	FreeLibrary(dll);
	if (FAILED(hr))
	{
		std::wcerr << L"COM smoke failed: 0x" << std::hex << hr << L"\n";
		return 5;
	}
	return 0;
}
