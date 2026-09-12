"""Verify staged owned-constructor failure cleanup in the shared lifecycle harness."""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name('verify_corpse_owned_delete.py')),init_globals={'abort_mode':True})
