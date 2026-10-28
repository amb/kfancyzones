#!/usr/bin/env python3
"""Generate the Qt form used by the settings controller in native/settingsplugin.cpp."""
from pathlib import Path
import sys
import xml.etree.ElementTree as E

ROOT = Path(__file__).resolve().parents[1]

def prop(parent, name, kind, value):
    node = E.SubElement(parent, 'property', name=name)
    E.SubElement(node, kind).text = str(value).lower() if isinstance(value, bool) else str(value)
    return node

def size(parent, name, width, height):
    node = E.SubElement(E.SubElement(parent, 'property', name=name), 'size')
    E.SubElement(node, 'width').text = str(width)
    E.SubElement(node, 'height').text = str(height)

def widget(parent, kind, name, **properties):
    item = E.SubElement(parent, 'item') if parent.tag == 'layout' else parent
    result = E.SubElement(item, 'widget', {'class': kind, 'name': name})
    for key, value in properties.items():
        prop(result, key, 'bool' if isinstance(value, bool) else 'number' if isinstance(value, int) else 'string', value)
    return result

def layout(parent, name, kind='QVBoxLayout'):
    return E.SubElement(parent, 'layout', {'class': kind, 'name': name})

def label(parent, name, text):
    result = widget(parent, 'QLabel', name, text=text)
    prop(result, 'wordWrap', 'bool', True)
    return result

def spin(parent, name, default, maximum, minimum=1):
    return widget(parent, 'QSpinBox', 'kcfg_'+name, minimum=minimum, maximum=maximum, value=default)

ui = E.Element('ui', version='4.0')
E.SubElement(ui, 'class').text = 'KFancyZonesSettings'
root = E.SubElement(ui, 'widget', {'class':'QWidget', 'name':'KFancyZonesSettings'})
prop(root, 'windowTitle', 'string', 'KFancyZones')
size(root, 'minimumSize', 480, 0)
shell = layout(root, 'settingsLayout')
tabs = widget(shell, 'QTabWidget', 'settingsTabs', currentIndex=0)
main_page = widget(tabs, 'QWidget', 'layoutTab')
E.SubElement(E.SubElement(main_page, 'attribute', name='title'), 'string').text = 'Layout'
outer = layout(main_page, 'layoutTabLayout')
prop(outer, 'spacing', 'number', 12)
label(outer, 'layoutHeading', '<b>Layout</b>')
controls = layout(E.SubElement(outer, 'item'), 'gridControls', 'QHBoxLayout')
for name, title, default, maximum in [('gridColumns','Columns',4,8),('gridRows','Rows',2,4),('gridGap','Gap',0,100)]:
    label(controls, name+'Label', title)
    control = spin(controls,name,default,maximum,0 if name=='gridGap' else 1)
    if name=='gridGap': prop(control,'suffix','string',' px')
subdivide = widget(outer,'QCheckBox','kcfg_subdivideZones',text='Subdivide each zone into 2×2')
prop(subdivide,'toolTip','string','Doubles the columns and rows. The default 4×2 grid becomes 8×4.')
mode_hint = label(outer,'layoutModeHint','Custom layout is enabled on the Advanced tab. Turn it off to use these grid settings.')
prop(mode_hint,'visible','bool',False)
caption=label(outer,'previewCaption','4 × 2 · 8 zones')
prop(caption,'alignment','set','Qt::AlignCenter')
table=widget(outer,'QTableWidget','layoutPreview',rowCount=2,columnCount=4)
size(table,'minimumSize',400,144);size(table,'maximumSize',400,144)
prop(table,'focusPolicy','enum','Qt::NoFocus')
prop(table,'editTriggers','set','QAbstractItemView::NoEditTriggers')
prop(table,'selectionMode','enum','QAbstractItemView::NoSelection')
prop(table,'horizontalScrollBarPolicy','enum','Qt::ScrollBarAlwaysOff')
prop(table,'verticalScrollBarPolicy','enum','Qt::ScrollBarAlwaysOff')
outer[-1].set('alignment','Qt::AlignHCenter')
label(outer,'gestureHelp','<b>Shift + drag</b> selects a zone. Add <b>Ctrl</b> to combine zones.<br/>Release the <b>mouse first</b> to accept, or release <b>Shift first</b> to cancel.')
shortcuts=label(outer,'shortcutsLink','<a href="systemsettings://kcm_keys">Configure keyboard shortcuts…</a>')
prop(shortcuts,'openExternalLinks','bool',True)
label(outer,'shortcutsHint','Search for KFancyZones in the Shortcuts page.')
advanced_body=widget(tabs,'QWidget','advancedOptions')
E.SubElement(E.SubElement(advanced_body,'attribute',name='title'),'string').text='Advanced'
advanced_layout=layout(advanced_body,'advancedLayout')
widget(advanced_layout,'QCheckBox','kcfg_useCustomLayout',text='Use custom layout')
label(advanced_layout,'customLayoutsHelp','Enable to use custom JSON instead of the grid controls. Invalid JSON uses the default 4×2 grid.')
custom=widget(advanced_layout,'QPlainTextEdit','kcfg_layoutsJson',placeholderText='Layout JSON',enabled=False)
prop(custom,'minimumHeight','number',140)
warning=label(advanced_layout,'customLayoutError','')
prop(warning,'visible','bool',False)
prop(warning,'textFormat','enum','Qt::PlainText')
prop(warning,'styleSheet','string','color: #da4453;')
label(advanced_layout,'applicationsLabel','<b>Applications</b>')
combo=widget(advanced_layout,'QComboBox','kcfg_filterMode',currentIndex=1)
for text in ['Only include these applications','Exclude these applications']:
    prop(E.SubElement(combo,'item'),'text','string',text)
apps=widget(advanced_layout,'QPlainTextEdit','kcfg_filterList',placeholderText='One window class per line (for example, org.kde.konsole)')
prop(apps,'maximumHeight','number',72)
for parent,name in [(outer,'layoutSpacer'),(advanced_layout,'advancedSpacer')]:
    spacer=E.SubElement(E.SubElement(parent,'item'),'spacer',name=name)
    prop(spacer,'orientation','enum','Qt::Vertical');size(spacer,'sizeHint',20,0)
label(shell,'applyHelp','Changes take effect when you click Apply.')
E.SubElement(ui,'resources');E.SubElement(ui,'connections')
E.indent(ui,space=' ')
result='<?xml version="1.0" encoding="UTF-8"?>\n<!-- Generated by bin/generate-settings.py; edit the generator. -->\n'+E.tostring(ui,encoding='unicode')+'\n'
path=ROOT/'src/contents/ui/config.ui'
if '--check' in sys.argv:
    if path.read_text()!=result: raise SystemExit('config.ui is out of date; run python3 bin/generate-settings.py')
else:
    path.write_text(result)
