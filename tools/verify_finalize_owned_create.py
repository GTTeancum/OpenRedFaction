"""Verify finalizer creation binding and failed partial cleanup on PC/NXDK."""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name("verify_corpse_owned_delete.py")),init_globals={"bridge_mode":True})
