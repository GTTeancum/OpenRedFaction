"""Read projection constants from the fingerprinted original binary."""
import hashlib
import json
import struct
from pathlib import Path
import pefile

root = Path(__file__).resolve().parents[1]
binary = root/'Installed_Game/RF.exe'
sha = hashlib.sha256(binary.read_bytes()).hexdigest()
assert sha == 'b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836'
pe = pefile.PE(str(binary))
constants = {}
for address in (0x589428, 0x5893c0, 0x58a29c, 0x589488, 0x58a298, 0x589410, 0x58a294, 0x58a290, 0x5893e0, 0x5893f8):
    raw = pe.get_data(address-pe.OPTIONAL_HEADER.ImageBase, 4)
    constants[hex(address)] = dict(bits=raw.hex(), float32=struct.unpack('<f', raw)[0])
report = dict(sha256=sha, function='0x547150', constants=constants,
              camera_position_getter='0x40d760', first_person_entry='0x40ddf0',
              limitation='Raw constants and partial decompilation; not complete gameplay camera behavior')
(root/'artifacts/camera-evidence.json').write_text(json.dumps(report, indent=2))
print(json.dumps(constants, indent=2))
