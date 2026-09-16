# Future gameplay reverse-engineering probes

These execute selected instructions from the locally supplied, fingerprinted original RF.exe under Unicorn. They do not launch the stock game, control the desktop, or implement gameplay in the port.

Run from the project root with Python, for example `python tools/future_re/campaign_cutscene_lifecycle.py`. Dependencies are the existing local/python packages and Installed_Game/RF.exe. Generated results remain under artifacts/future-campaign-re or artifacts/future-vehicles-re.

Reports and priorities are indexed in docs/research/FUTURE-CAMPAIGN-INDEX.md and FUTURE-VEHICLES-INDEX.md. Each report identifies intercepted subsystem boundaries and remaining uncertainty; passing a probe does not establish complete cutscene or vehicle behavior. Primary review reran the first six probes:40 campaign and256 vehicle cases passed.

Follow-up retention reran all15 retained probes: boarding prerequisites, shield/liquid delay, commit ordering, input/fire ownership, vehicle events, authored cutscene timelines, phases and cubic paths. Four older vehicle result files now preserve their own scope description instead of an imported harness module docstring. These remain bounded original-code probes, not playable vehicle or campaign acceptance.
