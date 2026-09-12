"""Run the real owned constructor through the deletion ownership harness."""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name('verify_corpse_owned_delete.py')),init_globals={'create_mode':True})
