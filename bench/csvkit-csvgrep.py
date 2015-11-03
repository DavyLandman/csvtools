## EASY-INSTALL-ENTRY-SCRIPT: 'csvkit==0.9.0','console_scripts','csvjson'
__requires__ = 'csvkit==0.9.0'
import sys
from pkg_resources import load_entry_point

if __name__ == '__main__':
    sys.exit(
        load_entry_point('csvkit==0.9.0', 'console_scripts', 'csvgrep')()
    )
