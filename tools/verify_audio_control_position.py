"""Run the full control-allocation verifier through positional entry5056a0."""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name('verify_audio_control_start.py')),init_globals={'position_mode':True})
