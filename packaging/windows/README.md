# Windows packaging

Windows release packaging is x64-only. There are two build profiles which use
the same source code, staging logic, dependency walker, and NSIS script:

* `modern` (`win10-x64` artifacts): current Windows 10/11 baseline. This is the
  required CI profile and is free to adopt newer Windows APIs behind normal
  compile/runtime guards.
* `legacy-win7` (`win7-x64` artifacts): `_WIN32_WINNT=0x0601` / `WINVER=0x0601`
  plus a stricter PE dependency audit. This profile is intentionally best-effort
  and must not block normal development when legacy compatibility eventually
  becomes impossible.

Jump Lists do not require a modern-only build: Psi currently uses the Windows 7
shell interfaces (`ITaskbarList3`, `ICustomDestinationList`, `IShellLink`). New
Windows-specific features can nevertheless target the modern profile without
forcing the whole project to remain on the Windows 7 SDK/API baseline forever.

For each profile one canonical staging tree feeds both deliverables:

* `psi-<version>-<profile>-x64.zip` — portable archive;
* `psi-<version>-<profile>-x64-setup.exe` — NSIS installer.

The staging tree is first populated with `cmake --install`, then completed by
`windeployqt`. `prepare_package.py` copies the QCA3/Qt5Keychain SDK runtimes and
walks every PE file recursively with MinGW `objdump` to copy all non-system DLL
dependencies from the SDK or MSYS2 MINGW64. New plugin DLLs and
configuration-specific dependencies therefore join the same dependency closure
without a hand-maintained runtime DLL list.

Every PE payload is required to be x64. The legacy profile additionally rejects
a conservative list of DLL/API-set imports known to require a post-Windows-7
system. This is a useful guardrail, not a replacement for a real Windows 7 SP1
smoke test; imports of newer functions from DLLs that already existed on Windows
7 need deeper API analysis or an actual legacy test VM.

`generate_nsis_manifest.py` generates uninstall commands from the exact final
staging tree. The installer removes only packaged files and then removes empty
directories, so user-created data inside the install directory is not deleted
recursively.

## Visual C++ Redistributable

The current MSYS2 MINGW64 build normally has no MSVC runtime dependency, so a VC
Redistributable is not bundled unconditionally. The PE scan records imports of
`vcruntime*.dll`, `msvcp*.dll`, and `concrt*.dll`. Only when such an import is
present does `build_profile.sh` download a Microsoft-hosted redistributable over
TLS and embed it in NSIS:

* modern: Microsoft's current `https://aka.ms/vc14/vc_redist.x64.exe` permalink;
* legacy Win7: the Visual Studio 2022 `https://aka.ms/vs/17/release/vc_redist.x64.exe`
  line instead of the Visual Studio 2026 runtime, whose current system
  requirements no longer include Windows 7.

No checksum is pinned for these Microsoft redirect URLs; their purpose is to
track Microsoft's serviced redistributable for the selected runtime line.
