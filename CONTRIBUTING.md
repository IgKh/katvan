# Katvan Contribution Guide

Thank you for your interest in contributing to Katvan! Please carefully review this document to understand the project's contribution policy.

## Project Goals

This section describes the design goals behind Katvan. They should help you understand which contributions are considered in scope for the project (and which aren't). The goals are listed in order of decreasing priority; in case of a conflict between two goals, the earlier one has precedence.

### RTL First

"Bias for Right-to-Left editing" isn't just a tagline, it is a promise! Katvan's first and foremost goal is to make typesetting technical and scientific documents primarily written in languages that utilize a Right-to-Left script as painless as possible. Features (and feature ideas) that directly contribute to this goal are always very gratefully accepted and have first priority. Features that improve the experience for users of both RTL and LTR languages are also in scope. However, features that only benefit users of LTR languages are out of scope for this project.

### Typst First

Katvan is an editor for Typst files, and only Typst files. Not LaTeX, not Markdown, not HTML, etc. Tight integration with the Typst language and compiler is not only allowed, it is preferred.

### Simple and Familiar

Katvan aims for having a basic and familiar editing experience, similar to other common basic text editors, with a user interface centered around the SDI (Single Document Interface) paradigm and modeless editing. The project aims to work best for short to medium standalone documents.

### Cross Platform

Katvan aims to support the three main desktop operating systems - Linux (both Wayland and X11), Windows and macOS. On each platform, the project aims to integrate well and adapt to common usage conventions. Therefore, Katvan aims to offer equivalent, although not necessarily identical, features across all platforms. Platform-specific new features don’t have to be implemented on all platforms from the get go, but they should come with a general reasonable plan on how to do so.

## Ways to Contribute

### Reporting Issues

If you discover a bug or some other problem while using Katvan, we'd like to hear about it! Testing can't cover every use case on every platform, so bug reports are amongst the most valuable ways to contribute.

Please make sure that the bug still happens on the most recent release of Katvan, and that a similar issue isn’t already open. If so, report the bug by opening a Github [Issue](https://github.com/IgKh/katvan/issues/new) in Katvan’s repository; please select the appropriate issue type.

### Suggesting Features

Ideas for new features that advance Katvan's mission are appreciated.

To suggest a feature, open a Github [Issue](https://github.com/IgKh/katvan/issues/new) with the appropriate type. Make sure that the idea is within the project's scope as laid out by the design goals above; you'll be asked to justify the feature according to them.

### Code Contributions

Meaningful source code patches are also accepted, although more selectively. Patches should be submitted through Github's Pull Request feature.

**VERY IMPORTANT**: all non-trivial pull requests must first be discussed in a corresponding bug or feature issue, ideally before implementation starts. No "drive by" pull requests please! Avoid wasted work and mutual displeasure by making sure that everyone is aligned on a plan. Remember that merged patches represent a future maintenance burden on the project; thank you for your understanding.

Additional more specific guidelines for code contributions are listed below in this document.

## Technical Guidelines for Code Contributions

### Developer Certificate of Origin (DCO)

All external contributors must confirm their legal right to provide a patch to the project under the project's license - which is version 3 (or later) of the GNU General Public License - by signing off on the [Developer Certificate of Origin](https://developercertificate.org/) statement. This is done by adding a `Signed-off-by` trailer to every commit message with your name and e-mail address; git can handle this automatically by using the `-s` or `--signoff` flag to the `git commit` command.

For your convenience, the full text of the Developer Certificate of Origin statement is hereby reproduced:

```
Developer Certificate of Origin
Version 1.1

Copyright (C) 2004, 2006 The Linux Foundation and its contributors.

Everyone is permitted to copy and distribute verbatim copies of this
license document, but changing it is not allowed.


Developer's Certificate of Origin 1.1

By making a contribution to this project, I certify that:

(a) The contribution was created in whole or in part by me and I
    have the right to submit it under the open source license
    indicated in the file; or

(b) The contribution is based upon previous work that, to the best
    of my knowledge, is covered under an appropriate open source
    license and I have the right under that license to submit that
    work with modifications, whether created in whole or in part
    by me, under the same open source license (unless I am
    permitted to submit under a different license), as indicated
    in the file; or

(c) The contribution was provided directly to me by some other
    person who certified (a), (b) or (c) and I have not modified
    it.

(d) I understand and agree that this project and the contribution
    are public and that a record of the contribution (including all
    personal information I submit with it, including my sign-off) is
    maintained indefinitely and may be redistributed consistent with
    this project or the open source license(s) involved.
```

### Generative AI Policy

The increasing use of Generative AI based developer tools holds significant uncertainty for Open Source projects in terms of maintenance challenges and copyright on one hand. On the other hand, the utility of such tools in helping developers get started with a project can't be ignored.

Therefore this project does not outright ban the use of such tools. However, we do have some requirements around their use that contributors are expected to follow:

- If any such tools were used, their names and the scope of their use must be included in the pull request description.
- Issue and pull request descriptions and comments must NOT be generated by AI. Contributors must use their own words when describing their work and ideas.
- The submitter of a pull request should be able to stand behind it fully:
    - You should be able to explain the purpose and reasoning behind every line code, should this be requested of you.
    - Remember that you also must sign off on a DCO - make sure you are comfortable making this commitment.

This policy does not apply for the use of AI tools for research purposes only or for non-significant use (e.g. just as single line code autocomplete).

### Other Guidelines

- No use of graphical UI designers, that is - no Qt Creator and no Xcode Interface Builder.
- Rust code should be formatted with `cargo fmt`. There is no auto-formatting setup for C++ and Object C code, so try to match the style of existing code as much as possible.
- Make reasonable effort to include at least basic automatic tests for new features. It is not always straightforward to do so, and existing coverage is not ideal, but tests really help the project avoid regressing features.
