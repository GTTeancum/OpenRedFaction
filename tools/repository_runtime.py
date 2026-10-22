"""Repository-confined paths and child-process state for local validation."""
import os
from pathlib import Path


def confined_path(root, path):
    root = Path(root).resolve()
    path = Path(path).resolve()
    if not path.is_relative_to(root):
        raise ValueError('Runtime path escapes repository: ' + str(path))
    return path


def child_environment(root, folder):
    folder = confined_path(root, folder)
    env = dict(os.environ)
    destinations = {
        'TEMP': 'temp', 'TMP': 'temp', 'TMPDIR': 'temp',
        'HOME': 'profile', 'USERPROFILE': 'profile',
        'APPDATA': 'profile/roaming', 'LOCALAPPDATA': 'profile/local',
        'XDG_CONFIG_HOME': 'profile/config', 'XDG_DATA_HOME': 'profile/data',
        'XDG_CACHE_HOME': 'cache', 'MESA_SHADER_CACHE_DIR': 'cache/mesa',
        '__GL_SHADER_DISK_CACHE_PATH': 'cache/gl',
        'PYTHONPYCACHEPREFIX': 'cache/python',
    }
    for variable, suffix in destinations.items():
        path = confined_path(root, folder / suffix)
        path.mkdir(parents=True, exist_ok=True)
        env[variable] = str(path)
    profile = Path(env['USERPROFILE'])
    if os.name == 'nt':
        env['HOMEDRIVE'] = profile.drive
        env['HOMEPATH'] = str(profile)[len(profile.drive):]
    env.update(SDL_AUDIO_DRIVER='dummy', SDL_AUDIODRIVER='dummy',
               PYTHONDONTWRITEBYTECODE='1')
    return env
