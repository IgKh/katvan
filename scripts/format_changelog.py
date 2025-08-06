#!/usr/bin/env python3
# /// script
# dependencies = [
#   "mistletoe",
# ]
# ///

import argparse
import mistletoe
import re
import sys
import xml.etree.ElementTree as ET

from mistletoe.markdown_renderer import MarkdownRenderer


def extract_version(doc, version, keep_header=False):
    keep = False
    filtered = []

    for elem in doc.children:
        if isinstance(elem, mistletoe.block_token.Heading) and elem.level == 2:
            content = getattr(elem.children[0], 'content', '')
            keep = content.strip().startswith(version)
            if keep and keep_header:
                filtered.append(elem)
            continue

        if keep:
            filtered.append(elem)

    doc.children = filtered

    return doc


def limit_versions(doc, limit):
    seen = 0
    filtered = []

    for elem in doc.children:
        if isinstance(elem, mistletoe.block_token.Heading) and elem.level == 2:
            seen += 1
            if seen > limit:
                break

        filtered.append(elem)

    doc.children = filtered

    return doc


def format_appstream_xml(doc):
    HEADING_REGEX = re.compile(r'v(?P<version>\d+\.\d+\.\d+) \((?P<date>.+)\)')

    root = ET.Element('releases')

    for elem in doc.children:
        if isinstance(elem, mistletoe.block_token.Heading) and elem.level == 2:
            content = getattr(elem.children[0], 'content', '')
            m = HEADING_REGEX.match(content)
            if not m:
                continue

            version = m.group('version')
            date = m.group('date')

            release = ET.SubElement(root, 'release', attrib={
                'version': version,
                'date': date
            })

            url = ET.SubElement(release, 'url', attrib={
                'type': 'details'
            })
            url.text = f'https://github.com/IgKh/katvan/releases/tag/v{version}'

    ET.indent(root, space='    ', level=1)
    return ET.tostring(root, encoding='unicode')


def handle_format(doc, fmt, fout):
    if fmt == 'markdown':
        with MarkdownRenderer() as renderer:
            fout.write(renderer.render(doc))
    elif fmt == 'appstream':
        fout.write(format_appstream_xml(doc))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input', help='Path to CHANGELOG.md')
    parser.add_argument('--output', '-o', help='File to write into')
    parser.add_argument('--format', choices=['markdown', 'appstream'], default='markdown', help='Output format')
    parser.add_argument('--pick', metavar='VERSION', help='Extract changelog for one version')
    parser.add_argument('--limit', type=int, help='Only emit last LIMIT versions')

    args = parser.parse_args()

    with open(args.input, 'r') as fin:
        doc = mistletoe.Document(fin)

    if args.pick is not None:
        doc = extract_version(doc, args.pick, keep_header=(args.format != 'markdown'))

    if args.limit is not None:
        doc = limit_versions(doc, args.limit)

    if args.output:
        with open(args.output, 'wt') as fout:
            handle_format(doc, args.format, fout)
    else:
        handle_format(doc, args.format, sys.stdout)

    return 0


if __name__ == '__main__':
    sys.exit(main())
