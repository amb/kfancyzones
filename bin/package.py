#!/usr/bin/env python3
"""Package the source and native build products without writing into src/."""
from pathlib import Path
import json
import os
import zipfile

root = Path(__file__).resolve().parents[1]
source = root / 'src'
build = root / 'build/native'
metadata = json.loads((source / 'metadata.json').read_text())
module = 'libkfancyzonesconfig-' + metadata['KPlugin']['Version'] + '.so'
data = Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share')).expanduser().resolve()
metadata['X-KDE-ConfigModule'] = str(data / 'kwin/scripts/kfancyzones/contents/config' / module)

with zipfile.ZipFile(root / 'kfancyzones.kwinscript', 'w', zipfile.ZIP_DEFLATED) as package:
    def add(path, relative):
        package.write(path, 'src/' + relative)

    package.writestr('src/metadata.json', json.dumps(metadata, indent=4) + '\n')
    add(root / 'LICENSE.md', 'LICENSE.md')
    for path in sorted((source / 'contents').rglob('*')):
        if path.is_file():
            add(path, path.relative_to(source).as_posix())
    # KWin may still select canonical main.qml. A separate entry URL also lets
    # an existing session load updated root QML without its previous cache entry.
    add(source / 'contents/ui/main.qml', 'contents/' + metadata['X-Plasma-MainScript'])
    add(build / 'config' / module, 'contents/config/' + module)
    for name in ['qmldir', 'libkfancyzonesliveinputplugin.so']:
        add(build / 'qml' / name, 'contents/ui/liveinput/' + name)
print('Built kfancyzones.kwinscript')
