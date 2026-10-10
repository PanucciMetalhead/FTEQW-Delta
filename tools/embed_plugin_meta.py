#!/usr/bin/env python3
"""Append FTE's plugin manifest ZIP using the same format on every build host."""
import sys
import zipfile

path, name, version, title, description = sys.argv[1:]
manifest = (f'{{\npackage fteplug_{name}\nver "{version}"\ncategory Plugins\n'
            f'title "{title}"\ngamedir ""\ndesc "{description}"\n}}\n')
with zipfile.ZipFile(path, 'a', compression=zipfile.ZIP_DEFLATED) as archive:
    archive.writestr('-', manifest)
