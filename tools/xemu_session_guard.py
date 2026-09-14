"""Prevent a second emulator for this project; never close or control another session."""
import json
import os
import subprocess
from pathlib import Path

def project_xemu_processes(root):
    if os.name != 'nt':
        raise RuntimeError('Project emulator guard requires Windows process inventory')
    script = "Get-CimInstance Win32_Process -Filter \"Name = 'xemu.exe'\" | Select-Object ProcessId,CommandLine | ConvertTo-Json -Compress"
    result = subprocess.run(['powershell.exe', '-NoProfile', '-NonInteractive', '-Command', script],
        check=True, capture_output=True, text=True)
    records = json.loads(result.stdout) if result.stdout.strip() else []
    if isinstance(records, dict): records = [records]
    prefix = str(Path(root).resolve()).replace('\\', '/').casefold().rstrip('/') + '/'
    return [int(row['ProcessId']) for row in records
        if prefix in (row.get('CommandLine') or '').replace('\\', '/').casefold()]

def require_no_project_xemu(root):
    pids = project_xemu_processes(root)
    if pids:
        raise RuntimeError('An existing Red Faction XEMU session is open (PID ' +
            ', '.join(map(str, pids)) + '); refusing to launch a second instance. Existing session left untouched.')
