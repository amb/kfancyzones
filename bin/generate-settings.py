#!/usr/bin/env python3
"""Generate the standard Qt form; no custom settings plugin is required.

KWin's generic loader does not run code behind a .ui form. Stacked previews
and ordinary signal/slot connections keep the grid preview reactive anyway.
"""
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

def connect(sender, signal, receiver, slot):
    node = E.SubElement(connections, 'connection')
    for key, value in [('sender', sender), ('signal', signal), ('receiver', receiver), ('slot', slot)]:
        E.SubElement(node, key).text = value

ui = E.Element('ui', version='4.0')
E.SubElement(ui, 'class').text = 'KFancyZonesSettings'
root = E.SubElement(ui, 'widget', {'class':'QWidget', 'name':'KFancyZonesSettings'})
prop(root, 'windowTitle', 'string', 'KFancyZones')
size(root, 'minimumSize', 480, 0)
outer = layout(root, 'settingsLayout')
prop(outer, 'spacing', 'number', 12)
label(outer, 'layoutHeading', '<b>Layout</b>')
controls = layout(E.SubElement(outer, 'item'), 'gridControls', 'QHBoxLayout')
for name, title, default, maximum in [('gridColumns','Columns',4,8),('gridRows','Rows',2,4),('gridGap','Gap',0,100)]:
    label(controls, name+'Label', title)
    control = spin(controls,name,default,maximum,0 if name=='gridGap' else 1)
    if name=='gridGap': prop(control,'suffix','string',' px')
subdivide = widget(outer,'QCheckBox','kcfg_subdivideZones',text='Subdivide each zone into 2×2')
prop(subdivide,'toolTip','string','Doubles the columns and rows. The default 4×2 grid becomes 8×4.')
previews = widget(outer,'QStackedWidget','subdivisionPreview',currentIndex=0)
connections = E.Element('connections')
connect('kcfg_subdivideZones','stateChanged(int)','subdivisionPreview','setCurrentIndex(int)')
for state in range(3):
    if state == 1:
        widget(previews,'QWidget','unusedPartiallyCheckedPreview')
        continue
    factor = 1 if state == 0 else 2
    page = widget(previews,'QWidget','previewMode'+str(factor))
    page_layout = layout(page,'previewModeLayout'+str(factor))
    column_stack = widget(page_layout,'QStackedWidget','columnsPreview'+str(factor),currentIndex=4)
    connect('kcfg_gridColumns','valueChanged(int)','columnsPreview'+str(factor),'setCurrentIndex(int)')
    widget(column_stack,'QWidget','unusedZeroColumns'+str(factor))
    for columns in range(1,9):
        col_page = widget(column_stack,'QWidget',f'columns_{factor}_{columns}')
        col_layout = layout(col_page,f'columnsLayout_{factor}_{columns}')
        row_name=f'rowsPreview_{factor}_{columns}'
        row_stack=widget(col_layout,'QStackedWidget',row_name,currentIndex=2)
        connect('kcfg_gridRows','valueChanged(int)',row_name,'setCurrentIndex(int)')
        widget(row_stack,'QWidget',f'unusedZeroRows_{factor}_{columns}')
        for rows in range(1,5):
            c,r=columns*factor,rows*factor
            name=f'preview_{factor}_{columns}_{rows}'
            row_page=widget(row_stack,'QWidget',name)
            row_layout=layout(row_page,name+'Layout')
            prop(row_layout,'spacing','number',6)
            caption=label(row_layout,name+'Caption',f'{c} × {r} · {c*r} zones')
            prop(caption,'alignment','set','Qt::AlignCenter')
            table=widget(row_layout,'QTableWidget',name+'Grid',rowCount=r,columnCount=c)
            size(table,'minimumSize',400,144);size(table,'maximumSize',400,144)
            prop(table,'focusPolicy','enum','Qt::NoFocus')
            prop(table,'editTriggers','set','QAbstractItemView::NoEditTriggers')
            prop(table,'selectionMode','enum','QAbstractItemView::NoSelection')
            prop(table,'horizontalScrollBarPolicy','enum','Qt::ScrollBarAlwaysOff')
            prop(table,'verticalScrollBarPolicy','enum','Qt::ScrollBarAlwaysOff')
            for key,value in [('horizontalHeaderVisible',False),('verticalHeaderVisible',False),
                              ('horizontalHeaderMinimumSectionSize',8),('verticalHeaderMinimumSectionSize',8),
                              ('horizontalHeaderDefaultSectionSize',398//c),('verticalHeaderDefaultSectionSize',142//r),
                              ('horizontalHeaderStretchLastSection',True),('verticalHeaderStretchLastSection',True)]:
                attr=E.SubElement(table,'attribute',name=key)
                E.SubElement(attr,'bool' if isinstance(value,bool) else 'number').text=str(value).lower()
            row_layout[-1].set('alignment','Qt::AlignHCenter')
help_text=label(outer,'gestureHelp','<b>Shift + drag</b> selects a zone. Add <b>Ctrl</b> to combine zones.<br/>Release the <b>mouse first</b> to accept, or release <b>Shift first</b> to cancel.')
shortcuts=label(outer,'shortcutsLink','<a href="systemsettings://kcm_keys">Configure keyboard shortcuts…</a>')
prop(shortcuts,'openExternalLinks','bool',True)
label(outer,'shortcutsHint','Search for KFancyZones in the Shortcuts page.')
advanced=widget(outer,'QPushButton','advancedToggle',text='Advanced',checkable=True,flat=True)
outer[-1].set('alignment','Qt::AlignLeft')
advanced_body=widget(outer,'QWidget','advancedOptions',visible=False)
advanced_layout=layout(advanced_body,'advancedLayout')
label(advanced_layout,'customLayoutsLabel','<b>Custom layouts</b>')
label(advanced_layout,'customLayoutsHelp','Custom JSON overrides the grid above. Leave empty to use the grid.')
custom=widget(advanced_layout,'QPlainTextEdit','kcfg_layoutsJson',placeholderText='Optional layout JSON')
prop(custom,'maximumHeight','number',110)
label(advanced_layout,'applicationsLabel','<b>Applications</b>')
combo=widget(advanced_layout,'QComboBox','kcfg_filterMode',currentIndex=1)
for text in ['Only include these applications','Exclude these applications']:
    prop(E.SubElement(combo,'item'),'text','string',text)
apps=widget(advanced_layout,'QPlainTextEdit','kcfg_filterList',placeholderText='One window class per line (for example, org.kde.konsole)')
prop(apps,'maximumHeight','number',72)
connect('advancedToggle','toggled(bool)','advancedOptions','setVisible(bool)')
label(outer,'applyHelp','After applying changes, turn KFancyZones off and on again in KWin Scripts.')
spacer=E.SubElement(E.SubElement(outer,'item'),'spacer',name='bottomSpacer')
prop(spacer,'orientation','enum','Qt::Vertical');size(spacer,'sizeHint',20,0)
E.SubElement(ui,'resources');ui.append(connections)
E.indent(ui,space=' ')
result='<?xml version="1.0" encoding="UTF-8"?>\n<!-- Generated by bin/generate-settings.py; edit the generator. -->\n'+E.tostring(ui,encoding='unicode')+'\n'
path=ROOT/'src/contents/ui/config.ui'
if '--check' in sys.argv:
    if path.read_text()!=result: raise SystemExit('config.ui is out of date; run python3 bin/generate-settings.py')
else:
    path.write_text(result)
