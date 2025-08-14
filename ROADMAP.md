# Feature Roadmap

Legend:

- :arrow_up: Top priority, a 1.0 release won't happen without it.
- :arrow_right: I generally want this, but probably needs more thinking about. Won't block a 1.0 release.
- :arrow_down: Maybe?

## RTL Editing

- [X] :arrow_up: Make BiDi control characters visible in the editor with custom glyphs.
    - Using a patch that will hopefully be upstreamed to Qt, but that will happen no earlier than Qt 6.9.
- [X] :arrow_up: Make line direction overrides persistent by inserting the proper control character.
- [X] :arrow_up: Give blank lines the base direction of the previous line, rather than the application's default.
    - To avoid the Logical cursor getting stuck between two RTL paragraphs when the system language is LTR
- [X] :arrow_right: Directionality markers on the line number gutters to improve orientation.

## Typst Integration

- [ ] :arrow_up: True live preview.
- [X] :arrow_up: Configuration panel for monitoring and pruning download cache.
- [X] :arrow_right: Highlight errors directly on the editor.
- [X] :arrow_right: Auto-complete.
- [X] :arrow_right: Compiler-assisted tool tips.
- [X] :arrow_right: Go to definition (as long as it is in the same file).

## General Editing

- [X] :arrow_up: Make automatic backups configurable.
- [X] :arrow_up: Use regex capture groups in text find and replace.
- [X] :arrow_down: Outline pane.
- [ ] :arrow_down: Code folding.
- [ ] :arrow_down: Indentation guides.
- [X] :arrow_down: Symbol picker.

## Platform Integrations

- [ ] :arrow_right: macOS version.
- [X] :arrow_right: Flatpak package.
    - In Flathub, and with as much sandboxing as possible.
- [X] :arrow_right: Windows installer.

## Sandbox Readiness

- [X] :arrow_right: Save auto-backups in an app-local location when sandboxed.
- [ ] :arrow_down: Support a format similar to TextBundle.

## Other

- [ ] :arrow_up: User manual.
