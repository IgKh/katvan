## v0.10.0 (2025-06-03)

### Typst Version

- The included Typst compiler version remains `0.13.1`.

### New Features

- Indentation is now preserved in continuations of wrapped lines.
- Explicitly inserted BiDi control characters are now visible and can be easily interacted with. This requires Qt 6.9 and above, and can be turned off through the settings dialog.
- A new Outline page shows all sections in the edited files based on their headings, and allows quick navigation among them.
- Cursor location history is now tracked through various jump commands. You can go back and forth through the cursor location history using the commands in the new `Go` menu and their corresponding shortcuts.
- You can now go to the definition of a Typst symbol, as long as it is defined inside the edited document. For symbols provided in the Typst standard library, the documentation tooltip is shown instead. Trigger this by `Ctrl`-clicking the symbol in the editor, or by pressing `F12`.
- Auto-completion can now be triggered using the more common `Ctrl+Space` shortcut, in addition to the previously available `Ctrl+E`.
- Auto-completion will automatically show some suggestions as you type. This can be disabled in the settings dialog.
- Typst universe `@preview` packages now show up in the auto-completion suggestions for the `#import` statement.
- Editor font size can be temporarily zoomed in and out using the `View` menu, the corresponding shortcuts, or by using the mouse wheel or trackpad while holding `Ctrl`.
- The editor theme can be explicitly changed to be always dark or always light in the settings dialog. Note that this affects only the editing area, not the entire UI.
- The edited file being externally modified or moved on disk is now detected, and the editor offers appropriate actions.
- Using the arrow keys while holding `Ctrl+Alt` will perform a visual cursor move, regardless of the default move style selected.
- The `TYPST_FONT_PATHS` environment variable used by Typst CLI is now respected by Katvan as well. It can be used to make non-installed fonts available to the Typst compiler.

### Fixes

- Line directionality heuristics that are affected by Typst syntax now re-apply correctly immediately after edits.

### Packaging Changes

- Bundled Qt version in pre-built packages is now `6.9.1`.
- Preparatory work was done to make Katvan behave well in the Flatpak sandbox. A build is expected to be submitted to Flathub in the near future.

## v0.9.0 (2025-03-30)

### Typst Version

- The included Typst compiler version was upgraded to `0.13.1`. As always, note that some Universe packages used in your documents may be incompatible and will need upgrading.

### New Features

- The editor now automatically handles directionality isolation where it may be need according to Typst syntax - e.g. for inline math inside RTL content, label references and inline code invocations, and content blocks inside code regions. This means that you get correct rendering and cursor movement without needing to manually insert isolates using the Insert menu in most cases.
- Compiler assisted tool tips are greatly enhanced - long values can be scrolled, more of the documentation is shown for function calls, and direct links to the online reference documentation are offered when applicable.
- Compiler error and warnings are now highlighted on the text causing them in the editor itself.
- Spell checking on Windows now uses the built-in system spell checker instead of Hunspell (**BREAKING CHANGE**).
- Automatic bracket insertion can be disabled through the settings dialog and/or via modeline.

### Fixes

- Fixed long standing (and very annoying) bug that caused the code model to drift from the visible text after editing content. This bug used to cause syntax-sensitive feature (like matching bracket highlighting and the directionality heuristic) to misbehave.
- Fixed automatic insertion of closing bracket for a content block argument trailing normal function call arguments.
- The Windows 11 style is used also in Windows 10 if possible, as it has a proper dark mode palette.
- Fixed the package cache statistics on Windows.
- The [Noto Sans Math](https://github.com/notofonts/math) font is now bundled, to make sure there is a fallback font for math symbol preview in the auto-completion suggestions menu.
- More stability fixes for when the preview is following the editor cursor.

### Packaging Changes

- Bundled Qt version in pre-built packages is now `6.8.3`.

## v0.8.1 (2025-02-07)

### Typst Version

- The included Typst compiler version remains at `0.12`.

### New Features

- It is now possible to configure additional paths from which documents can include resources (images, modules, etc), in addition to the document's own directory. This is done from the Compiler Settings tab. Note that all paths are still resolved relative to the document file location (#8).

### Fixes

- Fixed blurry previews when display scaling is used (#9).

### Packaging Changes

- Bundled Qt version in pre-built packages is now `6.8.1`.

## v0.8.0 (2024-11-15)

### Typst Version

- The included Typst compiler version was upgraded to `0.12.0` ([blog](https://typst.app/blog/2024/typst-0.12/), [release notes](https://typst.app/docs/changelog/0.12.0/)). Please note that some Universe packages used in your documents may show deprecation warnings and will need upgrading.

### New Features

- Document preview colors can be inverted (light to dark and vice-versa). This is independent of the editor's overall color scheme.
- Compiler output pane was redesigned to show compiler diagnostics in a more structured way. It will also now show warnings encountered during PDF export.
- Line directionality is now shown on the editor gutters - if the a line is right-to-left, the right gutter will be highlighted and vice-versa.
- Added basic auto-completion. For now it needs to be manually triggered by using `Ctrl+E` (`Cmd+E` on macOS). Expected to be enhanced in the next few releases.
- Added basic compiler-assisted tool tips.
- Added configuration tab for compiler settings:
    - Use of Typst Universe packages in the `@preview` namespace can be disabled, which will also disable all internet access.
    - Shows the size of the package download cache directory.
- Auto save interval is now configurable, and can be disabled entirely.
- When finding text using regular expressions, capture groups back references can now be used in text replacements.
- Compilation is much more incremental now, by syncing editor edits directly with the Typst compiler.
    - This will help unlock true instant previews in the near future, following some more UX work.
- Line directionality heuristic was enhanced:
    - It now takes syntax into consideration - if a line consists of only closing brackets, its' directionality will be the same as the line with the relevant opening bracket; this is useful for content block parameters.
    - Isolated runs are now ignored.

### Fixes

- Preview is now more stable when following the editor cursor.
- When jumping to preview of a source element that appears more than one time in the final document (e.g headings being both in the document body and in a generated TOC), the instance closest to the current preview page is targeted.
- Fixed words being wrongly showed as misspelled when next to BiDi control characters.
- Performance optimizations to the Typst syntax highlighting parser.
- Fixed icons not being properly visible in dark mode on some platforms.

### Packaging Changes

- A windows installer is now available.

### Heads Up

- Support for displaying BiDi control characters in the editor was developed, using a specialized font designed with the very gracious help of Noam Sahar ([LinkedIn](https://www.linkedin.com/in/noam-sahar/)). This requires Qt 6.9, which is not released yet, so still not available in the practical sense.

## v0.7.1 (2024-10-09)

_This release is dedicated to all those who are still wet from this October Rain_

### Typst Version

- The included Typst compiler version remains at `0.11.1`. An upgrade to `0.12` will be part of a future Katvan version, after it will have a full release.

### New Features

- Line directionality overrides are now persistent, with Katvan automatically maintaining the appropriate control characters.
- New line directionality heuristic, that gives lines that don't yet have a strongly-directional character or explicit override the same directionality as the previous line.
- **Experimental** macOS support.

### Fixes

- Fix the output of the `datetime.today()` Typst function.
- Code identifiers that contain hyphens or trailing underscores are now correctly recognized by the syntax parser.
- Fix endless flicker loop when previewer zoom mode was "Fit Width", document had only one page, and the preview pane was a specific size.
- Fix issue where images could not be included in new files, even after saving them, until re-opened.
- Fix applying spelling corrections on Hebrew words with included punctuation (Geresh or Gershayim).
- Fix some memory leaks and optimize memory usage of previews.

### Packaging Changes

- AppImage build now includes the Gtk 3 platform theme plugin, for better integration with Gtk based Linux desktop environments.
- Bundled Qt version in pre-built packages is now `6.7.3`.

## v0.7.0 (2024-08-24)

**IMPORTANT:** Starting from this release, Katvan directly embeds the Typst compiler. This means that separately installing the Typst CLI is not required anymore, and the binary releases contain everything required for Katvan to fully work (except, perhaps, hunspell dictionaries). Release 0.7.0 includes Typst 0.11.1, and until Typst has stable releases each version of Katvan will include and only work with a single recent released version of Typst. Please note significant changes to dependencies required when building from source.

### New Features

- The preview pane was completely rebuilt, and now renders pages directly from the Typst intermediate result only as needed, with no round-trip via PDF. It is now much more responsive, and should suffer less flicker when being moved or resized. The preview also has kinetic scrolling now.
- Katvan now has forward search - jump from the editor to the corresponding location in the preview with `Ctrl+J`, or you can set the preview to automatically follow the editor cursor around.
- There is also inverse search support - `Ctrl+Click` on the preview to jump to the corresponding source location in the editor (if it is in the current document).
- Emphasis and strong emphasis markers are now auto-closed as well, in content mode.

### Fixes

- Automatic bracket insertion for closing brackets of immediate function calls in content and math modes works.
- File paths are properly displayed in the UI, with platform specific separators and correct directionality in Hebrew localized UI.
- Loading a recent file that was deleted will display an error rather than silently fail.

### Other Changes

-  AppImage build now has self-update information included.

## v0.6.0 (2024-08-05)

### New Features

- Matching brackets are highlighted.
- Closing brackets are automatically inserted in a sytnax aware way when an opening bracket is typed.
- It is now possible to set an auto-indentation mode if desired - Normal for simply maintaining the previous line's indentation, or Smart which will automatically increase or decrease indentation based on the relevant Typst syntax mode.
- `Home` and `Shift+Home` keys now work as is typical in code editors and will skip over initial indentation.
- Added a compilation status indicator in the status bar.
- The preview pane can be undocked into a floating window.

### Fixes

- Fixed the editor scroll position jerking when saving a document with a modeline.
- Fixed UI freeze when indenting or unindenting the last line.
- Horizonal scroll position of the preview pane is maintained when preview is updated.
- If a word is added to the personal dictionary, all other instances of it are also not marked as spelling errors.
- Fixed Windows-style text direction toggle keys to work under recent KDE version.

### Other Changes

-  AppImage build no longer accepts the `--portable` command line flag, since it doesn't actually work. Use the AppImageKit provided mechanism (`--appimage-portable-home`) if needed.

## v0.5.0 (2024-06-22)

### New Features

- The Typst compiler is now being run in the background in continuous watch mode, so follow up compilation is faster.
- If compilation fails, it is possible to jump from the error message in the compiler output panel directly to the error location in the editor.
- If Katvan crashes while editing a file, the next time the same file is opened it will suggest recovering unsaved changes using the compilation buffer file.
- Line number gutters are now configurable: you can choose between one or two gutters, or none at all.
- Katvan now attempts to detect a system-wide dark theme (assuming the active Qt style supports it) and respect it by using system palette colors where appropriate and different syntax highlight colors.

### Fixes

- Syntax highlighting now correctly detects URL literals and math mode directly within code mode.
- Fix crash on exit that could happen when PDF previewer was in a certain scroll position.

### Other Changes

- Windows and AppImage binary releases now contain Qt 6.7.2

## v0.4.0 (2024-06-08)

### New Features

- Initial editor configuration system, with a settings dialog and [modeline](https://github.com/IgKh/katvan/wiki/Editor-Modelines) support.
- The editor now has the typical indent/unindent behavior for the Tab/Shift+Tab/Backspace keys. Indent style (tabs/spaces) and width are configurable.
- Added find in selection mode, and find/replace functionality.

### Fixes

- Spell checking dictionaries are now loaded in the background, which prevents the UI from hanging while a large dictionary is being loaded.
- Fixed highlighting of Typst labels and references.

### Other Changes

- Syntax parser updated to follow Typst version 0.11 rules.
- AppImage baseline was raised to Ubuntu 22.04, and now includes Wayland support.

## v0.3.0 (2024-02-24)

### New Features

- As-you-type spell checking, powered by `hunspell`.
  - It is aware of Typst syntax, so only content and markup are checked.
- Extended "Insert" menu.
- On Linux, Windows-style `Ctrl+RShift` and `Ctrl+LShift` keyboard shortcuts may now also be used to manually set line direction.

### Fixes

- Headings and lists at the start of a content block are now properly highlighted.

## v0.2.0 (2024-02-10)

### New Features

- Syntax highlighting of Typst sources
- Official support for Windows 10 and 11
- Enhancements to text finding - search by regex, case matching, etc

## v0.1.0 (2024-01-22)

### New Features

- Initial release!
