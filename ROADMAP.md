# Feature Roadmap

Legend:

- :arrow_up: Top priority, a 1.0 release won't happen without it.
- :arrow_right: I generally want this, but probably needs more thinking about. Won't block a 1.0 release.
- :arrow_down: Maybe?

## RTL Editing

- [ ] :arrow_up: Make BiDi control characters visible in the editor with custom glyphs.
    - Using a patch that will hopefully be upstreamed to Qt, but that will happen no earlier than Qt 6.9.
- [X] :arrow_up: Make line direction overrides persistent by inserting the proper control character.
- [X] :arrow_up: Give blank lines the base direction of the previous line, rather than the application's default.
    - To avoid the Logical cursor getting stuck between two RTL paragraphs when the system language is LTR
- [ ] :arrow_right: Directionality markers on the line number gutters to improve orientation.

## Typst Integration

- [ ] :arrow_up: True live preview.
- [ ] :arrow_up: Configuration panel for monitoring and pruning download cache.
- [ ] :arrow_right: Highlight errors directly on the editor.
- [ ] :arrow_right: Auto-complete.
- [ ] :arrow_right: Compiler-assisted tool tips.
- [ ] :arrow_right: Go to definition (as long as it is in the same file).

## General Editing

- [ ] :arrow_up: Make automatic backups configurable.
- [ ] :arrow_up: Use regex capture groups in text find and replace.
- [ ] :arrow_down: Outline pane.
- [ ] :arrow_down: Code folding.
- [ ] :arrow_down: Indentation guides.

## Platform Integrations

- [ ] :arrow_right: macOS version.
- [ ] :arrow_right: Flatpak package.
    - In Flathub, and with as much sandboxing as possible.
- [ ] :arrow_right: Windows installer.

## Sandbox Readiness

- [ ] :arrow_right: Save auto-backups in an app-local location when sandboxed.
- [ ] :arrow_down: Support a format similar to TextBundle.

## Other

- [ ] :arrow_up: User manual.
