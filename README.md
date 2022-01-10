# QuickOpen

[![Windows Build](https://github.com/picode98/QuickOpen/actions/workflows/windows-build.yaml/badge.svg?branch=main)](https://github.com/picode98/QuickOpen/actions/workflows/windows-build.yaml)

## About the project

QuickOpen is a cross-platform desktop application which allows ad-hoc transfers of different types of content over local networks (LANs) using a web interface. Currently supported content types include URLs (which can be opened in the recipient's browser) and files, which can be uploaded to a folder of the recipient's choosing.

A web interface was chosen because it has a considerable compatibility advantage over other methods: Only one device in the transfer needs to run QuickOpen; the other simply needs to have a reasonably up-to-date web browser.

## How to use

Most functionality can be accessed using the system tray icon. Left-clicking this shows a status window with a list of links that can be shared with other users to access the QuickOpen webpage. (Efforts are planned to make these links easier to remember and distribute using QR codes and service discovery technologies.) The window also shows current activity, including shared links and files.

Right-clicking the system tray icon shows a menu with items for showing the status window, showing the application settings window, showing the about window, and exiting the program.

**Important note for Ubuntu users:** You may have to install a shell extension for the tray icon to be visible due to limitations of wxWidgets. See [this issue](https://github.com/wxWidgets/Phoenix/issues/754#issuecomment-625738694) for more information.

## How to install

### Installing from binaries

Binaries are available under the "Releases" page of this repository. The following formats are currently supported:

- Windows portable application (ZIP) - x64
- Windows installer (MSI) - x64
- Linux Debian package (DEB) - x64

### Installing from source

#### Windows

The easiest way to install up-to-date versions of all build dependencies is to use [vcpkg](https://github.com/microsoft/vcpkg). vcpkg is a submodule of this repository, so setup would be roughly as follows:

```
git clone --recurse-submodules https://github.com/picode98/QuickOpen
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install wxwidgets:x64-windows nlohmann-json:x64-windows civetweb:x64-windows
mkdir build
cd build
cmake .. -Ax64 -DVCPKG_TARGET_TRIPLET=x64-windows -DCMAKE_BUILD_TYPE=Release "-DCMAKE_TOOLCHAIN_FILE=.\vcpkg\scripts\buildsystems\vcpkg.cmake"
cmake --build . --config Release --target package
```

This should produce a `pkg` subfolder containing all the binary types mentioned under "Installing from binaries".

#### Linux

On Linux, vcpkg can also be used, resulting in a similar procedure:

```
git clone --recurse-submodules https://github.com/picode98/QuickOpen
./vcpkg/bootstrap-vcpkg.sh
./vcpkg/vcpkg install wxwidgets:x64-linux nlohmann-json:x64-linux civetweb:x64-linux
mkdir build
cd build
cmake .. -Ax64 -DVCPKG_TARGET_TRIPLET=x64-linux -DCMAKE_BUILD_TYPE=Release "-DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build . --config Release --target package
```

As mentioned above, this will create packages under the `pkg` subfolder. You can also install the software directly as follows:

```
sudo cmake --build . --config Release --target install
```