#include <windows.h>
#include <shobjidl.h>
#include <iostream>
int wmain(int argc, wchar_t ** argv)
{
	if (argc != 2)
	{
		std::wcerr << L"Usage: ShellSmoke.exe input.def\n";
		return 2;
	}
	DWORD attrs = GetFileAttributesW(argv[1]);
	if (attrs == INVALID_FILE_ATTRIBUTES)
	{
		std::wcerr << L"Input file not found: " << GetLastError() << L"\n";
		return 2;
	}
	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(hr))
	{
		std::wcerr << L"CoInitializeEx failed: 0x" << std::hex << hr << L"\n";
		return 3;
	}
	IShellItemImageFactory * f = nullptr;
	hr = SHCreateItemFromParsingName(argv[1], nullptr, IID_PPV_ARGS(&f));
	if (FAILED(hr))
	{
		std::wcerr << L"SHCreateItemFromParsingName failed: 0x" << std::hex << hr << L"\n";
		CoUninitialize();
		return 4;
	}
	SIZE size{ 256, 256 };
	HBITMAP bitmap = nullptr;
	hr = f->GetImage(size, SIIGBF_THUMBNAILONLY | SIIGBF_BIGGERSIZEOK, &bitmap);
	if (SUCCEEDED(hr) && bitmap)
	{
		BITMAP info{};
		GetObjectW(bitmap, sizeof(info), &info);
		std::wcout << L"Shell thumbnail: " << info.bmWidth << L"x" << info.bmHeight << L"\n";
		DeleteObject(bitmap);
	}
	f->Release();
	CoUninitialize();
	if (FAILED(hr))
	{
		std::wcerr << L"IShellItemImageFactory::GetImage failed: 0x" << std::hex << hr << L"\n";
		return 4;
	}
	return 0;
}
