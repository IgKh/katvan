#!/usr/bin/env python3
# /// script
# dependencies = [
#   "mistletoe",
# ]
# ///

import argparse
import mistletoe
import sys

from mistletoe.markdown_renderer import MarkdownRenderer


def extract_version(doc, version):
    keep = False
    filtered = []

    for elem in doc.children:
        if isinstance(elem, mistletoe.block_token.Heading) and elem.level == 2:
            content = getattr(elem.children[0], 'content', '')
            keep = content.strip().startswith(version)
            continue

        if keep:
            filtered.append(elem)

    doc.children = filtered

    return doc


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input', help='Path to CHANGELOG.md')
    parser.add_argument('--output', '-o', required=True, help='File to write into')
    parser.add_argument('--pick', metavar='VERSION', help='Extract changelog for one version')

    args = parser.parse_args()

    with open(args.input, 'r') as fin:
        doc = mistletoe.Document(fin)

    if args.pick is not None:
        doc = extract_version(doc, args.pick)

    with open(args.output, 'w') as fout:
        with MarkdownRenderer() as renderer:
            fout.write(renderer.render(doc))

    return 0


if __name__ == '__main__':
    sys.exit(main())
