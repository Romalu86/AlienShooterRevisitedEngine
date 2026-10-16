# Alien Shooter Revisited Engine

A Visual Studio 2022 Win32/x86 C++ project targeting Alien Shooter Revisited.

## Requirements

- Windows and Visual Studio 2022 with Desktop development with C++, the MSVC v143 toolset, and a Windows SDK.
- A legally obtained installation of Alien Shooter Revisited with its original game files.
- The 32-bit DirectX 9 runtime, including `d3dx9_27.dll`.

Original game data and DirectX runtime DLLs are not included. The bundled Ogg and Vorbis sources are compiled into the project.

## Build

Open `AlienShooterRevisited.sln`, select `Release|Win32` or `Debug|Win32`, and build the solution. The executable is written to `build/vs2022/<Configuration>/AlienShooter.exe`.

The project targets PE32/i386 with OS version 5.0, Windows subsystem version 5.0, and 512-byte file alignment. These header values describe the executable target; they do not establish compatibility with every older Windows installation.

## Run

Place the built `AlienShooter.exe` beside the original `AlienShooter.cfg` and game files, and use that game directory as the working directory. Keep a backup of the original executable. The executable name determines the default configuration filename.

## Source layout

- `src/` contains engine and game code, including the shared `zm` systems.
- `third_party/` contains Ogg/Vorbis sources and license notices.
- `resources/` contains the executable resources and import/export definitions.
- `vs2022/` contains the Visual Studio project and file filters.
- `tools/build/` contains the PE header build target.

The universal engine retains shared internal names. Build and compatibility evidence is supplied separately; this source package does not claim complete binary equivalence with the original executable.

## Rights and dependencies

See [LICENSE.md](LICENSE.md) for the source-package rights notice. Alien Shooter Revisited and the original game content belong to their respective rights holders. The executable resource payload remains subject to the relevant rights.

The bundled libraries retain their notices: [libogg](third_party/libogg/COPYING) and [libvorbis](third_party/libvorbis/COPYING).
