import json
import os

filepath = 'node-red/flows_final_project.json'
with open(filepath, 'r') as f:
    flows = json.load(f)

# Group assignments and ordering
gauges = []
charts = []
sliders = []
texts = []
buttons = []
others = []

for node in flows:
    if node.get('type') == 'ui-gauge':
        node['width'] = "3"
        node['height'] = "3"
        node['group'] = "ui_group_gauges"
        gauges.append(node)
    elif node.get('type') == 'ui-chart':
        node['width'] = "8"
        node['height'] = "5"
        node['group'] = "ui_group_charts"
        charts.append(node)
    elif node.get('type') == 'ui-slider':
        node['width'] = "4"
        node['height'] = "1"
        node['group'] = "ui_group_settings"
        sliders.append(node)
    elif node.get('type') == 'ui-text':
        if "Chart" in node.get('name', '') or "Health" in node.get('name', ''):
            node['width'] = "8"
            node['height'] = "1"
            node['group'] = "ui_group_charts"
            texts.append(node)
        else:
            node['width'] = "4"
            node['height'] = "1"
            node['group'] = "ui_group_settings"
            texts.append(node)
    elif node.get('type') == 'ui-button':
        node['width'] = "2"
        node['height'] = "1"
        node['group'] = "ui_group_settings"
        buttons.append(node)
    elif node.get('type') == 'ui-switch':
        node['width'] = "2"
        node['height'] = "1"
        node['group'] = "ui_group_settings"
        buttons.append(node)

# Apply specific ordering inside groups
# Group 1: Gauges (Health, Temp, Hum/Press, Soil, Light)
g_order = 1
for g in sorted(gauges, key=lambda x: x.get('name', '')):
    g['order'] = g_order
    g_order += 1

# Group 2: Charts (Top to bottom)
c_order = 1
for c in sorted(charts, key=lambda x: x.get('name', '')):
    c['order'] = c_order
    c_order += 1
for t in sorted([t for t in texts if t['group'] == 'ui_group_charts'], key=lambda x: x.get('name', '')):
    t['order'] = c_order
    c_order += 1

# Group 3: Settings (Sliders first, then texts, then buttons)
s_order = 1

# Sort Sliders logically
def slider_sort(n):
    name = n.get('name', '').lower()
    if 'soil' in name: return 1
    if 'temp' in name: return 2
    if 'light' in name: return 3
    return 4
for s in sorted(sliders, key=slider_sort):
    s['order'] = s_order
    s_order += 1
    
# Corresponding text for sliders
for t in sorted([t for t in texts if t['group'] == 'ui_group_settings'], key=lambda x: x.get('name', '')):
    t['order'] = s_order
    s_order += 1

# Buttons
for b in sorted(buttons, key=lambda x: x.get('name', '')):
    b['order'] = s_order
    s_order += 1

with open(filepath, 'w') as f:
    json.dump(flows, f, indent=4)
print("Layout fixed!")
