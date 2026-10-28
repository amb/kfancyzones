#!/usr/bin/env python3
"""Package a source build for the current user's standard KWin install path."""
from pathlib import Path
import json
import os
import zipfile

root = Path(__file__).resolve().parents[1]
source = root / 'src'
metadata = json.loads((source / 'metadata.json').read_text())
data = Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share')).expanduser().resolve()
metadata['X-KDE-ConfigModule'] = str(data / 'kwin/scripts/kfancyzones/contents/config/libkfancyzonesconfig.so')
obsolete = {'FilledPreviewOverlay.qml', 'RaisedWindowOverlay.qml', 'NativePreviewFreeOverlay.qml',
            'KFancyZones.qml', 'CompactSettingsOverlay.qml'}
with zipfile.ZipFile(root / 'kfancyzones.kwinscript', 'w', zipfile.ZIP_DEFLATED) as package:
    for path in sorted(source.rglob('*')):
        if not path.is_file(): continue
        relative = path.relative_to(source)
        if 'native' in relative.parts or 'moveinput' in relative.parts or path.name in obsolete: continue
        content = (json.dumps(metadata, indent=4)+'\n').encode() if relative == Path('metadata.json') else path.read_bytes()
        package.writestr('src/'+relative.as_posix(), content)
