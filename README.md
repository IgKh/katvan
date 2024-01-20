# Katvan

A bare-bones graphical editor for [typst](https://github.com/typst/typst) files, with a strong bias for Right-to-Left editing.

## Screenshot

![App Screenshot](.github/assets/screenshot.png)

## Motivation

For University, I have a need to write documents that are mainly in Hebrew but also heavily incorporate math and inline English terms. For me, any solution is preferably offline, efficient to write (i.e. does not require building equations by selecting parts from a toolbar) and runs on Linux natively. I ran across _typst_, a typesetting system which ticks all of these boxes and has very good BiDi support right out of the box, which makes it very attractive.

The question is now which text editor to use with it? Typesetting input files are a bit weird in that they are source code, but primarily consist of natural language. When that language is written from right to left, code editors typically don't display that too well - even if the final result looks good, it is hard to write and harder to edit. Even the better ones don't align different parts to their natural order. Plain text editors tend to be better, but lack many conveniences that make working on source nicer.

Therefore Katvan is a new editor application, with a very specific focus on this particular use case; starting with a plain text editor that gets the basics right (at least for me), and goes from there.

## Features

Not a whole lot so far, but for now we have:
- Reasonably good RTL editing
    - Mostly thanks the Qt Rich Text Framework
    - But also specific additional functionallity, like toggling between logical and visual cursor movement, manually flipping paragraph direction, and commands to insert Unicode direction isolates when the BiDi algorithm doesn't lead to the right result (e.g for inline math).
- Live-ish previews[^1]

[^1]: Previews are currently rendered by running the entire file through the _typst_ CLI after each change. It is plenty fast at least for smaller documents, so good enough for now.

## Installation

For now it is only possible to install Katvan by building it from source code. You'll need development files for Qt 6.5 (or a later 6.x release) and CMake 3.16 or above.

Do it the usual way:

```bash
  mkdir build
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
  cmake --build build
  sudo cmake --build build -t install
```

This was tested only on Linux. Windows and macOS may or may not work.

In addition, for previews and PDF export to work the `typst` CLI needs to be installed and available via the `PATH` in runtime.

## Contributing

Contributions aren't really expected. Issues and PRs in Github are open to create, but please don't expect much. This exists to scratch my personal need, and made available in hope it is useful for others with similar needs.

## License

[GPL v3](https://choosealicense.com/licenses/gpl-3.0/)

## Roadmap

A few things I'd like and may happen at some point, in no particular order:

- Syntax highlighting
- Spell checking
- Better controls to insert and see BiDi control characters
- Better integration with `typst` (incremental previews, highlight errors)
- Properly test on Windows and create pre-built binaries
