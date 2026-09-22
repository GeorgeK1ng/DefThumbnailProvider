#include "Registration.h"

#include "DllGlobals.h"

#include <array>
#include <shlobj.h>
#include <string>
#include <utility>
#include <vector>

namespace
{
constexpr wchar_t THUMBNAIL_HANDLER_GUID[] = L"{e357fccd-a995-4576-b01f-234630154e96}";
constexpr wchar_t EXTRACT_IMAGE_HANDLER_GUID[] = L"{BB2E617C-0920-11D1-9A0B-00C04FC2D6C1}";
constexpr wchar_t PROVIDER_DESCRIPTION[] = L"Heroes III DEF, D32 and P32 Thumbnail Provider";
constexpr wchar_t CLASSES_ROOT_PATH[] = L"Software\\Classes\\CLSID\\";
constexpr wchar_t APPROVED_EXTENSIONS_PATH[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";
constexpr std::array<const wchar_t *, 3> SUPPORTED_EXTENSIONS = { L".def", L".d32", L".p32" };

std::wstring extensionHandlerPath(const wchar_t * extension)
{
	return L"Software\\Classes\\" + std::wstring(extension) + L"\\shellex\\" + THUMBNAIL_HANDLER_GUID;
}

std::wstring systemHandlerPath(const wchar_t * extension)
{
	return L"Software\\Classes\\SystemFileAssociations\\" + std::wstring(extension) + L"\\shellex\\" + THUMBNAIL_HANDLER_GUID;
}

std::wstring legacyHandlerPath(const wchar_t * extension)
{
	return L"Software\\Classes\\" + std::wstring(extension) + L"\\shellex\\" + EXTRACT_IMAGE_HANDLER_GUID;
}

LONG setStringValue(HKEY root, const std::wstring & path, const wchar_t * name, const std::wstring & value)
{
	HKEY key = nullptr;
	DWORD disposition = 0;
	LONG error = RegCreateKeyExW(root, path.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &key, &disposition);
	if (error != ERROR_SUCCESS)
		return error;

	error = RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE *>(value.c_str()), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
	RegCloseKey(key);
	return error;
}

LONG readDefaultValue(HKEY root, const std::wstring & path, std::wstring & value)
{
	HKEY key = nullptr;
	LONG error = RegOpenKeyExW(root, path.c_str(), 0, KEY_QUERY_VALUE, &key);
	if (error != ERROR_SUCCESS)
		return error;

	DWORD type = 0;
	DWORD byteCount = 0;
	error = RegQueryValueExW(key, nullptr, nullptr, &type, nullptr, &byteCount);
	if (error != ERROR_SUCCESS)
	{
		RegCloseKey(key);
		return error;
	}
	if (type != REG_SZ && type != REG_EXPAND_SZ)
	{
		RegCloseKey(key);
		return ERROR_DATATYPE_MISMATCH;
	}

	std::vector<wchar_t> buffer(byteCount / sizeof(wchar_t) + 1, L'\0');
	error = RegQueryValueExW(key, nullptr, nullptr, &type, reinterpret_cast<BYTE *>(buffer.data()), &byteCount);
	RegCloseKey(key);
	if (error == ERROR_SUCCESS)
	{
		buffer.back() = L'\0';
		value.assign(buffer.data());
	}
	return error;
}

LONG deleteRegistryTree(HKEY root, const std::wstring & path)
{
	HKEY key = nullptr;
	LONG error = RegOpenKeyExW(root, path.c_str(), 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE | DELETE, &key);
	if (error != ERROR_SUCCESS)
		return error;

	DWORD maximumSubkeyLength = 0;
	error = RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, &maximumSubkeyLength, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	if (error != ERROR_SUCCESS)
	{
		RegCloseKey(key);
		return error;
	}

	std::vector<wchar_t> subkeyName(static_cast<size_t>(maximumSubkeyLength) + 1);
	for (;;)
	{
		DWORD nameLength = static_cast<DWORD>(subkeyName.size());
		error = RegEnumKeyExW(key, 0, subkeyName.data(), &nameLength, nullptr, nullptr, nullptr, nullptr);
		if (error == ERROR_NO_MORE_ITEMS)
			break;
		if (error != ERROR_SUCCESS)
		{
			RegCloseKey(key);
			return error;
		}

		const std::wstring childPath = path + L"\\" + std::wstring(subkeyName.data(), nameLength);
		error = deleteRegistryTree(root, childPath);
		if (error != ERROR_SUCCESS)
		{
			RegCloseKey(key);
			return error;
		}
	}

	RegCloseKey(key);
	return RegDeleteKeyW(root, path.c_str());
}

HRESULT
resultFromWin32(LONG error)
{
	return error == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32(error);
}

LONG ensureHandlerAvailable(HKEY root, const std::wstring & path)
{
	std::wstring existingProvider;
	const LONG error = readDefaultValue(root, path, existingProvider);
	if (error == ERROR_FILE_NOT_FOUND)
		return ERROR_SUCCESS;
	if (error != ERROR_SUCCESS)
		return error;
	return _wcsicmp(existingProvider.c_str(), kClsidString) == 0 ? ERROR_SUCCESS : ERROR_ALREADY_EXISTS;
}

LONG deleteHandlerIfOwned(HKEY root, const std::wstring & path)
{
	std::wstring existingProvider;
	LONG error = readDefaultValue(root, path, existingProvider);
	if (error == ERROR_FILE_NOT_FOUND)
		return ERROR_SUCCESS;
	if (error != ERROR_SUCCESS)
		return error;
	if (_wcsicmp(existingProvider.c_str(), kClsidString) != 0)
		return ERROR_SUCCESS;

	error = deleteRegistryTree(root, path);
	return error == ERROR_FILE_NOT_FOUND ? ERROR_SUCCESS : error;
}

HRESULT
registerAtRoot(HKEY root)
{
	wchar_t modulePath[MAX_PATH]{};
	const DWORD pathLength = GetModuleFileNameW(g_module, modulePath, MAX_PATH);
	if (pathLength == 0 || pathLength == MAX_PATH)
	{
		const DWORD error = GetLastError();
		return HRESULT_FROM_WIN32(error ? error : ERROR_INSUFFICIENT_BUFFER);
	}

	LONG error = ERROR_SUCCESS;
	for (const wchar_t * extension : SUPPORTED_EXTENSIONS)
	{
		error = ensureHandlerAvailable(root, extensionHandlerPath(extension));
		if (error != ERROR_SUCCESS)
			return resultFromWin32(error);
		error = ensureHandlerAvailable(root, systemHandlerPath(extension));
		if (error != ERROR_SUCCESS)
			return resultFromWin32(error);
		error = ensureHandlerAvailable(root, legacyHandlerPath(extension));
		if (error != ERROR_SUCCESS)
			return resultFromWin32(error);
	}

	const std::wstring classPath = CLASSES_ROOT_PATH + std::wstring(kClsidString);
	error = setStringValue(root, classPath, nullptr, PROVIDER_DESCRIPTION);
	if (error != ERROR_SUCCESS)
		return resultFromWin32(error);
	error = setStringValue(root, classPath + L"\\InprocServer32", nullptr, modulePath);
	if (error != ERROR_SUCCESS)
		return resultFromWin32(error);
	error = setStringValue(root, classPath + L"\\InprocServer32", L"ThreadingModel", L"Apartment");
	if (error != ERROR_SUCCESS)
		return resultFromWin32(error);
	for (const wchar_t * extension : SUPPORTED_EXTENSIONS)
	{
		error = setStringValue(root, extensionHandlerPath(extension), nullptr, kClsidString);
		if (error != ERROR_SUCCESS)
			return resultFromWin32(error);
		error = setStringValue(root, systemHandlerPath(extension), nullptr, kClsidString);
		if (error != ERROR_SUCCESS)
			return resultFromWin32(error);
		error = setStringValue(root, legacyHandlerPath(extension), nullptr, kClsidString);
		if (error != ERROR_SUCCESS)
			return resultFromWin32(error);
	}
	error = setStringValue(root, APPROVED_EXTENSIONS_PATH, kClsidString, PROVIDER_DESCRIPTION);
	if (error != ERROR_SUCCESS)
		return resultFromWin32(error);

	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
	return S_OK;
}

HRESULT
unregisterAtRoot(HKEY root)
{
	const std::wstring classPath = CLASSES_ROOT_PATH + std::wstring(kClsidString);
	LONG classError = deleteRegistryTree(root, classPath);
	if (classError == ERROR_FILE_NOT_FOUND)
		classError = ERROR_SUCCESS;

	LONG handlerError = ERROR_SUCCESS;
	for (const wchar_t * extension : SUPPORTED_EXTENSIONS)
	{
		const std::array<std::wstring, 3> handlerPaths = {
			extensionHandlerPath(extension),
			systemHandlerPath(extension),
			legacyHandlerPath(extension),
		};
		for (const std::wstring & handlerPath : handlerPaths)
		{
			const LONG error = deleteHandlerIfOwned(root, handlerPath);
			if (handlerError == ERROR_SUCCESS && error != ERROR_SUCCESS)
				handlerError = error;
		}
	}

	HKEY approvedExtensions = nullptr;
	if (RegOpenKeyExW(root, APPROVED_EXTENSIONS_PATH, 0, KEY_SET_VALUE, &approvedExtensions) == ERROR_SUCCESS)
	{
		RegDeleteValueW(approvedExtensions, kClsidString);
		RegCloseKey(approvedExtensions);
	}

	SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
	if (classError != ERROR_SUCCESS)
		return resultFromWin32(classError);
	if (handlerError != ERROR_SUCCESS)
		return resultFromWin32(handlerError);
	return S_OK;
}
}

HRESULT
RegisterServer() noexcept
{
	try
	{
		return registerAtRoot(HKEY_CURRENT_USER);
	}
	catch (...)
	{
		return E_FAIL;
	}
}

HRESULT
UnregisterServer() noexcept
{
	try
	{
		return unregisterAtRoot(HKEY_CURRENT_USER);
	}
	catch (...)
	{
		return E_FAIL;
	}
}

HRESULT
RegisterServerMachine() noexcept
{
	try
	{
		return registerAtRoot(HKEY_LOCAL_MACHINE);
	}
	catch (...)
	{
		return E_FAIL;
	}
}

HRESULT
UnregisterServerMachine() noexcept
{
	try
	{
		return unregisterAtRoot(HKEY_LOCAL_MACHINE);
	}
	catch (...)
	{
		return E_FAIL;
	}
}
