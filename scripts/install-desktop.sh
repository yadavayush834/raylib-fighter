#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
project_dir=$PWD
python - "$project_dir" <<'PY'
import json
import shutil
import sys
from pathlib import Path
project = Path(sys.argv[1])
home = Path.home()
destination = home / '.local/share/spirit-clash'
bin_dir = home / '.local/bin'
applications = home / '.local/share/applications'
for directory in (destination, bin_dir, applications):
    directory.mkdir(parents=True, exist_ok=True)
shutil.copy2(project / 'build/spirit-clash', destination / 'spirit-clash.new')
(destination / 'spirit-clash.new').replace(destination / 'spirit-clash')
shutil.copytree(project / 'assets', destination / 'assets', dirs_exist_ok=True)
launcher = bin_dir / 'spirit-clash'
launcher.write_text('#!/usr/bin/env bash\nexec "$HOME/.local/share/spirit-clash/spirit-clash" "$@"\n')
launcher.chmod(0o755)
(applications / 'spirit-clash.desktop').write_text('\n'.join([
    '[Desktop Entry]', 'Type=Application', 'Name=Spirit Clash',
    'Comment=An anime-inspired local arena fighter built with raylib',
    'Exec=' + json.dumps(str(launcher)), 'Icon=' + str(destination / 'assets/icon.svg'),
    'Terminal=false', 'Categories=Game;ActionGame;',
    'StartupWMClass=Spirit Clash | Raylib Fighter',
]) + '\n')
PY
printf 'Installed Spirit Clash. Open it from your application launcher or run spirit-clash.\n'
