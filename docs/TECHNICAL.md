# Technical documentation

[Back to the main page](../README.md)

DEF Thumbnail Provider is a native Windows Shell extension that shows image
thumbnails for Heroes of Might and Magic III `.def` files in File Explorer.

The project contains no .NET code and has no runtime dependency on VCMI, SDL,
or Qt. It supports the standard Heroes III DEF compression formats 0, 1, 2,
and 3. DEF data is read from the `IStream` supplied by Windows Explorer and is
validated before decoding.

## Origin and license

The DEF decoding rules are adapted from the
[VCMI](https://github.com/vcmi/vcmi) project, mainly its `CDefFile` decoder and
palette handling. The standalone decoder in this repository was rewritten to
remove VCMI engine dependencies and add strict bounds checking.

VCMI is licensed under GPL-2.0-or-later. Because this project contains adapted
VCMI decoding code, this project is also distributed under GPL-2.0-or-later.
See [VCMI-NOTICE.txt](../LICENSES/VCMI-NOTICE.txt) and
[GPL-2.0-or-later.txt](../LICENSES/GPL-2.0-or-later.txt).

## Download

GitHub Actions builds these artifacts:

- `DefThumbnailProvider-x64` for normal 64-bit Windows Explorer
- `DefThumbnailProvider-x86` for 32-bit shell hosts
- `DefThumbnailProvider-arm64` for Windows on ARM

Use the package that matches the architecture of the shell process. Most
Windows PCs need the x64 package.

## Install

Extract the artifact to a permanent directory. Do not move or delete the DLL
after registration.

Open **Command Prompt as Administrator**, change to the extracted directory,
and run:

```bat
regsvr32 /n /i:machine DefThumbnailProvider.dll
ie4uinit.exe -ClearIconCache
ie4uinit.exe -show
```

Restart **Windows Explorer** from Task Manager. Open a folder containing DEF
files and select **Large icons** or **Extra large icons**.

The machine-wide installation is recommended because the Windows thumbnail
surrogate may not see a per-user COM registration on every system.

## Uninstall

Open **Command Prompt as Administrator** in the installation directory:

```bat
regsvr32 /u /n /i:machine DefThumbnailProvider.dll
ie4uinit.exe -show
```

The uninstaller removes only registry entries owned by this provider. It does
not change the application associated with `.def` files.

## Test the installation

After registration, run:

```bat
ShellSmoke.exe "C:\path\to\file.def"
```

A successful result looks like:

```text
Shell thumbnail: 256x171
```

To test only the decoder without installing the Shell extension:

```bat
DefDump.exe "C:\path\to\file.def" output.bmp
```

## Build from source

Requirements:

- Windows 10 or later
- CMake 3.15 or later
- Visual Studio 2019 or later with the C++ desktop workload

x64 example:

```bat
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Other architectures:

```bat
cmake -S . -B build-x86 -A Win32
cmake -S . -B build-arm64 -A ARM64
```

The main output is `build\Release\DefThumbnailProvider.dll`.

## Windows XP

The DLL is built against the Windows XP Win32 API baseline and does not import
the post-XP `RegGetValueW` or `RegDeleteTreeW` functions. Registry access uses
`RegQueryValueExW`, `RegEnumKeyExW`, and `RegDeleteKeyW` instead.

Windows Vista and later use `IThumbnailProvider` with `IInitializeWithStream`.
Windows XP Explorer uses the older `IExtractImage` interface with
`IPersistFile`; the provider implements both paths using the same decoder and
renderer. Registration installs both Shell handler keys without changing the
normal `.def` file association.

Modern Explorer receives premultiplied ARGB with real transparency. Windows XP
Explorer does not reliably honor the bitmap alpha channel, so its legacy path
composites transparent pixels onto the current `COLOR_WINDOW` system color.

Use the x86 package on 32-bit Windows XP. Register it from an Administrator
Command Prompt:

```bat
regsvr32 /n /i:machine DefThumbnailProvider.dll
```

Restart Explorer after registration. Existing `Thumbs.db` files may contain
cached generic icons; remove the relevant `Thumbs.db` while Explorer is closed
if thumbnails do not refresh. Windows XP support is intended for Service Pack
3. The current automated builds and tests do not run inside Windows XP itself.

## Implementation

The provider implements:

- `IInitializeWithStream`
- `IThumbnailProvider`
- `IPersistFile`
- `IExtractImage`
- `IClassFactory`
- `DllGetClassObject` and `DllCanUnloadNow`
- per-user registration through `DllRegisterServer`
- machine-wide registration through `DllInstall`

Provider CLSID:

```text
{9B4F3E1C-5D90-4D62-8FA2-57D7B9A13C84}
```

The decoder and COM objects do not use mutable global image state. Exceptions
are caught at COM boundaries so malformed DEF files cannot propagate C++
exceptions into Windows Explorer.
