# Original subdivision cutter setup

`tools/probe_geomod_piece_cutter.py` executes4667f4 through the call boundary at
4668b6 for RF.exe SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.
The solid constructor4eed90 is intercepted to capture its six arguments and
return a dummy solid.4f80a0 is skipped. CRT rand57312d supplies the recovered
LCG draws. All cone sampling, oriented basis and offset arithmetic execute in
the original binary. CSG4de630 and cutter geometry construction are not executed.

Recovered numeric contract after admission:

- Constructor dimension arguments are(2*longest,2*longest,stored-float0.2),
  followed by three zero subdivision counts.
- The longest principal axis is perturbed by original cone sampling with
  minimum cosine stored-float0.95, consuming two draws.
- Original4fcfa0 constructs a basis around that perturbed direction.
- The offset uses a third draw between separately stored floats
  longest*(-0.1f) and longest*(0.1f), multiplied by the unperturbed principal
  axis. It does not offset along the tilted direction.
- The outer worker increments its batch attempt count before constructing
  the cutter. This helper does not own attempts or queue mutation.

Shared `rf_geomod_piece_cutter_prepare` matches all15 output float words and
final RNG state in45 executions: three principal axes, lengths3/10/20.25, and
five seeds. Invalid principal axes preserve RNG and output. This helper assumes
its caller already applied the verified radius/aspect/attempt admission gate.

Release geomod_disconnected passes the new45 cases and retained component,
shape, clipping and placement checks. Stock-profile NXDK build is recorded in
artifacts/geomod-postedit-re/piece-cutter-xbox.log. No live or visual claim.

Remaining: cutter solid creation, CSG application to potentially nonconvex
pieces, extraction/recentering of results, queue limits and terminal body
publication. The original requeues only children no larger than the parent's
pre-cut radius and destroys rejected children; that lifecycle still needs
execution and integration. These numeric fixtures are not full subdivision.
