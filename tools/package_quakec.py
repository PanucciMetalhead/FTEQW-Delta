#!/usr/bin/env python3
"""Package compiled QuakeC and its FTE manifest without platform shell tools."""
import pathlib
import sys
import zipfile

output, source, package, version, title, description = sys.argv[1:]
manifest = (f'{{\npackage {package}\nver "{version}"\ncategory Plugins\n'
            f'title "{title}"\ngamedir "id1"\ndesc "{description}"\n}}\n')
with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    archive.writestr('-', manifest)
    archive.write(source, pathlib.Path(source).name)
