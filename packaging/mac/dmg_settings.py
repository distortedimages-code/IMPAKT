# dmgbuild settings for IMPAKT's disk image (packaging/mac/make_dmg.sh: dmgbuild -s dmg_settings.py -D stage=... -D bg=...)
# The window: the installer on the left; on the right the plugins, the app and CHIMERA's sounds (the folder IMPAKT), each
# beside a shortcut to its folder.
# The icon places match packaging/mac/draw_background.py (the background is drawn around them).
import os

stage = defines['stage']          # the folder with Install IMPAKT.pkg, IMPAKT.vst3, IMPAKT.component, IMPAKT.app
background = defines['bg']        # dmg-background.png (its @2x beside it)

files = [os.path.join(stage, n) for n in ('Install IMPAKT.pkg', 'IMPAKT.vst3', 'IMPAKT.component', 'IMPAKT.app', 'IMPAKT', 'LISEZ-MOI.txt')]
symlinks = {'VST3': '/Library/Audio/Plug-Ins/VST3', 'Components': '/Library/Audio/Plug-Ins/Components', 'Applications': '/Applications',
            'Application Support': '/Library/Application Support'}
icon_locations = {
    'Install IMPAKT.pkg': (150, 250),
    'IMPAKT.vst3': (410, 177), 'VST3': (600, 177),
    'IMPAKT.component': (410, 267), 'Components': (600, 267),
    'IMPAKT.app': (410, 357), 'Applications': (600, 357),
    'IMPAKT': (410, 447), 'Application Support': (600, 447),
    'LISEZ-MOI.txt': (150, 470),  # (the Terminal command)
}
format = 'ULFO'                   # (lzfse: macOS 10.11 and later)
filesystem = 'HFS+'
size = None
window_rect = ((200, 120), (720, 540))
default_view = 'icon-view'
show_icon_preview = False
show_status_bar = False
show_tab_view = False
show_toolbar = False
show_pathbar = False
show_sidebar = False
icon_size = 56
text_size = 13
arrange_by = None
label_pos = 'bottom'
