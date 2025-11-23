# Katvan

A bare-bones graphical editor for [Typst](https://github.com/typst/typst) files, with a strong bias for Right-to-Left editing.

## Screenshots

![App Screenshot](https://katvan.app/assets/screenshot-main.png)
![App Screenshot - Dark Mode](https://katvan.app/assets/screenshot-main-dark.png)

## Motivation

For University, I have a need to write documents that are mainly in Hebrew but also heavily incorporate math and inline English terms. For me, any solution is preferably offline, efficient to write (i.e. does not require building equations by selecting parts from a toolbar) and runs on Linux natively. I ran across _Typst_, a typesetting system which ticks all of these boxes and has very good BiDi support right out of the box, which makes it very attractive.

The question is now which text editor to use with it? Typesetting input files are a bit weird in that they are source code, but primarily consist of natural language. When that language is written from right to left, code editors typically don't display that too well - even if the final result looks good, it is hard to write and harder to edit. Even the better ones don't align different parts to their natural order. Plain text editors tend to be better, but lack many conveniences that make working on source nicer.

Therefore Katvan is a new editor application, with a very specific focus on this particular use case; starting with a plain text editor that gets the basics right (at least for me), and goes from there.

## Features

- Reasonably good RTL editing
    - Mostly thanks to Qt's excellent Rich Text Framework
    - But also specific additional functionality, for example:
      - Syntax-aware line directionality heuristic
      - Automatic directionality isolation of inline math and code
      - Displaying BiDi control characters with a special custom font[^1]
      - Toggling between logical and visual cursor movement
      - Manually flipping paragraph direction (using both Windows style `Ctrl+RShift`/`LShift`, or Firefox style `Ctrl+Shift+X`)
      - Handy commands to manually insert BiDi control marks and isolates for when the algorithm doesn't quite lead to the right result (e.g. for some instances of inline raw text)
- Live previews
- Syntax highlighting
- Syntax-aware spell checking
- Autocomplete
- Typical code editor niceties - auto indentation, bracket insertion, etc.
- Forward and inverse search
- [Modelines](https://github.com/IgKh/katvan/wiki/Editor-Modelines)
- Supported on Linux and Windows 10/11.
- **Experimental** support for macOS 12 ("Monterey") and above.

## Installation

<details>

<summary>For versions 0.6.0 and prior only</summary>

In addition to Katvan itself, it is required to install the `typst` CLI and make it available via the system path or by placing it next to the `katvan` executable. Without it previews and PDF export will not work. [See here](https://github.com/typst/typst#installation) for details. _This is not required for the latest release_.

</details>

### Linux

There are several options available. If not sure, the AppImage format is recommended.

#### AppImage

Pre-built AppImage packages for the `x86_64` and `aarch64` architectures are available from the project [releases page](https://github.com/IgKh/katvan/releases). An AppImage is a single executable file containing everything needed to run Katvan and start working with Typst on most contemporary Linux distributions. Note that it contains the spell checker library, but not any dictionaries; install any required hunspell dictionaries system-wide from your distribution's repositories.

#### Flatpak

For those who prefer using Flatpak, Katvan is available to install from [Flathub](https://flathub.org/apps/app.katvan.Katvan) for the `x86_64` and `aarch64` architectures.

[![Get it on Flathub](https://flathub.org/api/badge?locale=en)](https://flathub.org/apps/app.katvan.Katvan)

> [!IMPORTANT]
> Apps installed by Flatpak run inside a sandbox which isolates them from the host system and limits their access to resources such as the file system. The Katvan Flatpak package contains a modified build which attempts to streamline this. However, there are still differences - in particular, to include any external files in your documents, you must allow access to a directory tree containing them via `Tools > Settings > Compiler > Allowed Paths`. There may also be some issues with using files stored on external media.
>
> Users which do not specifically require or prefer using Flatpak should default to using the AppImage build of Katvan.

#### Other Options

If none of the above options is suitable, you'll need to build from source code. See below for instructions.

There is also an [AUR package](https://aur.archlinux.org/packages/katvan) to automate this for Arch Linux users.

### Windows

Builds for 64-bit Windows 10/11 are available from the project [releases page](https://github.com/IgKh/katvan/releases). There is a traditional installer, as well as a portable build. The portable build will store settings and the cache for downloaded Universe packages in the same directory as the main executable, so make sure to extract the archive in a writable location. To write settings to the registry instead, run the `katvan.exe` binary with the `--no-portable` flag.

Both builds use the built-in Windows spell checker, and will therefore make use of the language packs available on the host.

### macOS

Due to various reasons, there are no pre-built binaries available for macOS. Users should compile from source on every machine they intend to run Katvan on; see instructions below. Note that the full Xcode installation is not required - the command line tools are sufficient. Katvan on macOS uses the system's spell checker, so no particular setup is required on that front.

### From Source

To compile and install Katvan from source code, you'll need:
- A C++ compiler toolchain that supports C++20, and is [supported by Qt](https://doc.qt.io/qt-6/supported-platforms.html)
- A recent stable Rust toolchain
- Development files for Qt 6.8 (or any later 6.x release - Qt 6.9.1 or above is highly recommended).
- CMake 3.22 or later
- [Corrosion](https://github.com/corrosion-rs/corrosion) (optional, will be automatically downloaded if missing)
- A working `pkg-config`
- [hunspell](http://hunspell.github.io/) (required only on Linux)
- [libarchive](https://libarchive.org/)
- [GoogleTest](https://google.github.io/googletest/) (optional, for running unit tests)
- [python3-mistletoe](https://github.com/miyuchina/mistletoe) (optional, only on Linux, for generating AppStream metainfo)

Get those from your distribution repositories, vcpkg, Homebrew, or wherever.

To build, perform a usual CMake invocation. For example, on Linux this might look like:

```bash
  mkdir build
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
  cmake --build build
  sudo cmake --build build -t install
```

For macOS, a build invocation will be something like:

```bash
  mkdir build
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -t install -j
```

If all is successful, an application bundle will appear in the `build/dist` directory. It can be dragged to the `Applications` library for installation, after which the build directory can be discarded (but not the installed dependencies).

## Contributing

At this point in the project's life, code contributions are generally not accepted. This project exists to meet my personal need, and is currently rapidly evolving with a well-defined goal in mind. It is made available in hope that it is useful for others with similar needs.

Issues detailing bug reports and suggestions are welcome, but please don't expect much.

## License

[GPL v3](https://choosealicense.com/licenses/gpl-3.0/)

## Roadmap

See the dedicated [ROADMAP.md](ROADMAP.md) file.

[^1]: Requires Qt 6.9 or above, if building from source.
