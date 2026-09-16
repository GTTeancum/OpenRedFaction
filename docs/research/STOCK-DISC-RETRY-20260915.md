# Stock disc retry (2026-09-15)

Two bounded owned-process retries after the user mounted a disc timed out before the main-frame hook. Artifacts: `artifacts/stock-reference-disc-02`, `artifacts/stock-reference-disc-03`, and `artifacts/stock-reference-build/disc-03-launch.log`. No reference images were produced. The second launcher records owned main-thread registers, stack words and module ranges before terminating its own process.

The user supplied a screenshot showing “Insert Red Faction CD #2 (ESC exits game)”. Read-only inspection found optical E:, volume RFACTION2_2, with AutoRun.exe, Autorun.ico, Autorun.inf, data3.cab and a Crack directory. No files from the disc were executed.

Original scanner inspection by the RE worker identifies 4b2ed0 (called from startup 4b31a0): optical drives are enumerated, volume information queried, then root RedFaction.ico and music.vpp are checked as disc1/disc2 markers. The mounted image lacks music.vpp. The volume name alone does not establish the game/version, and the inspected check does not compare it. Further stock launches are paused pending a matching image; no disc checks were changed.

This blocks stock visual-reference capture only. Offline GeoMod, vehicle and campaign research can continue. Overall and GeoMod estimates remain approximately49% and66% respectively.
