# Rocket impact missing-sparks policy (2026-09-15)

**Actionable result:** keep the unresolved optional sparks slot absent. The installed original resolves `explosion random bits 2` to -1 and skips its spark spawn path. Substituting `explosion random bits` would change this observed behavior.

Evidence is for RF.exe SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`, with names extracted in authored order from installed tables.vpp/emitters.tbl. `tools/verify_explosion_optional_sparks.py` runs original instructions in isolated Unicorn and records table SHA in `optional-sparks.json`.

- Executes `48df58..48df6d`: actual parser call-site argument setup, entire original lookup497550, actual string comparison5001d0/57c130 and original result store to recipe+80. Parser/tokenizer and allocation are bypassed by supplying the parsed string and name records; the lookup itself is unhooked.
- Executes the original runtime gate `48e9a8..48e9b1` and stops at the selected destination. Missing lookup result -1 goes directly to48ea8c, bypassing sparks initialization and its spawn loop. Positive controls `explosion random bits` and its uppercase spelling resolve index26 and enter48e9b7. The missing name plus trailing space stays missing.
- Existing recipe metadata says sparks count20. This count is never reached by the missing-slot branch; a nonzero count does not cause a fallback.

The four cases pass. This adds original executable evidence where the existing definition-resolver fixture previously validated only the port's chosen missing-optional policy. No source change is needed. The TO-DO wording can now distinguish *confirmed original omission for this installed rocket recipe* from unimplemented spark support for recipes with valid references.

Limits: this harness supplies parsed name storage, does not boot the complete original game, and does not exclude later mods/patches or a different asset version defining the missing name. It does not validate valid-spark rendering, central emission, or complete blast visual parity. No binary/runtime patch or visual substitution was performed.
