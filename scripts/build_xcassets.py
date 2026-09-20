#!/usr/bin/env python3
#
# Constructs and compiles an Xcode compatible asset bundle for the application
# and file type icons. Needs `rsvg-convert` and the full Xcode installed. This
# only needs to run if icons are changed, as the result is checked in.
#
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile

ICON_SIZES = [16, 32, 128, 256, 512]


def write_contents(directory, images=None):
    contents = {
        'info': {
            'author': 'xcode',
            'version': 1
        }
    }
    if images:
        contents['images'] = images

    with open(directory / 'Contents.json', 'wt') as fout:
        json.dump(contents, fout, indent=2)
        fout.write('\n')


def render_iconset(catalog, name, kind, svg, small_svg=None):
    iconset = catalog / f'{name}.{kind}'
    iconset.mkdir()

    images = []

    for size in ICON_SIZES:
        for scale in (1, 2):
            src = small_svg if small_svg and size <= 32 else svg
            filename = 'icon_{0}x{0}{1}.png'.format(size, '@2x' if scale == 2 else '')
            subprocess.run([
                'rsvg-convert',
                '--width', str(size * scale),
                '--height', str(size * scale),
                '--output', str(iconset / filename),
                src,
            ], check=True)

            images.append({
                'filename': filename,
                'idiom': 'mac',
                'scale': f'{scale}x',
                'size': f'{size}x{size}',
            })

    write_contents(iconset, images)


def compile_catalog(catalog, target):
    subprocess.run([
        'xcrun', 'actool',
        '--output-format', 'human-readable-text',
        '--notices', '--warnings',
        '--platform', 'macosx',
        '--minimum-deployment-target', '12.0',
        '--target-device', 'mac',
        '--app-icon', 'katvan',
        '--output-partial-info-plist', target / 'partial.plist',
        '--compile', target,
        catalog,
    ], check=True)


def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    assets_dir = root / 'assets'
    macshell_dir = root / 'macshell'
    output = macshell_dir / 'Assets.car'

    with tempfile.TemporaryDirectory() as workdir:
        workdir = pathlib.Path(workdir)

        catalog = workdir / 'Assets.xcassets'
        catalog.mkdir()
        write_contents(catalog, images=None)

        render_iconset(catalog, 'katvan', 'appiconset', assets_dir / 'katvan.svg', assets_dir / 'katvan-small.svg')
        render_iconset(catalog, 'typst', 'iconset', assets_dir / 'typst.svg')

        compiled = workdir / 'compiled'
        compiled.mkdir()
        compile_catalog(catalog, compiled)

        shutil.copy(compiled / 'Assets.car', output)

    print(f'Wrote {output}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
