# Authored-text language selection

## Implementation status

Source-written for the next parent-owned Xbox compile/package rollup. No build,
test, runtime, XEMU, HDD, fixture or image work was performed for this slice.
This is a working UI-subset language selector, not full campaign localization.
Port-specific prompts, diagnostics, storage warnings and physical button names
remain English. No translated text was invented or embedded in source.

## Installed-source evidence

Read-only inspection of the installed VPP directory and table bytes establishes:

- `tables.vpp/strings.tbl`: 115,860 bytes, 999 English, German and French entries;
  the French tag at ID975 is uppercase `FR:`. Longest source line: 608 bytes.
- `tables.vpp/credits.tbl`: 4,787 bytes. English is authored; French and German
  sections contain literal `Insert French/German credits here.` placeholders.
- `tables.vpp/endgame.tbl`: 8,225 bytes, 12 named blocks with En/Gr/Fr sections;
  all German/French endings are English placeholders prefixed `(gr)` or `(fr)`.
- `levels1.vpp`: 25 mission-text tables and385 messages per language;
  `levels2.vpp`: 23 tables and332 messages; `levels3.vpp`: 18 tables and201 messages.
  In total all918 non-English message entries per language are placeholders or
  empty: German916 `(gr)` plus2 empty; French913 `(fr)` plus2 empty and3 malformed
  `(fr...` prefixes. The malformed French entries are in L4S4, train02 and L7S4.
- Existing `ui.vpp/rfpc-medium.vf` TEXT font is VF1, first32/count221 (through252).
  No font asset, atlas budget or GPU/backend ownership changed.

The real installed translations therefore provide useful UI labels immediately;
this data pack still displays English mission dialogue, failure descriptions and
credit bodies. Language tags alone are not claimed as translation support.

## Live wiring

Options now offers English, Deutsch or Francais. A change updates the shared
scene text selection immediately and reloads the small optional UI catalog before
rendering. It applies from both the cold frontend and the pause Options menu.
Reset Defaults returns to English without writing. Explicit Save Settings is
still the only settings write. Selection persists across session transitions.

Selected authored UI IDs and consumers:

-42/43/986: New Game, Options and Load Game.
-85–88/123: difficulty names and heading;788: Back.
-138/143/135/169/170/590: defaults, look sensitivity, invert label, On/Off and
  language selection label. Port-only deadzone/layout/mapping/save labels remain.
-145–154: Fire, Alt Fire, Use, Jump, Crouch, Holster, Reload, Next, Previous and
  Fine Aim action names on the mapping page. Input action identities and physical
  button labels stay platform-owned; the map still uses the same10! permutation.
-59/820/821/156: Continue, Quick Save, Quick Load and Message Log on pause.
-202: Cancel on the Dashboard confirmation page;983: Credits heading.

No translated strings enter format strings. The loader normalizes authored
escaped newlines/tabs to spaces for these single-row labels and rejects unknown
escape sequences, oversized fields and unsupported control bytes. Missing,
empty, invalid or placeholder selected labels retain authored English, then the
existing inline English fallback when necessary. A missing/invalid catalog is
optional and cannot stop startup or restore a previous language's stale labels.

Mission-message parsing has explicit language-aware entry points. The existing
entry points remain English for compatibility. Each requested message chooses
available authored text or falls back to its English text; selected oversized
text also falls back. Voice filenames, event identity and the512-byte output ABI
are unchanged. Already received message-history entries retain the text received;
changing language does not replay audio/events or rewrite previous messages.

Endgame and credits select the same language, with bounded English fallback for
missing, malformed, empty or recognized placeholder sections. The installed
`(gr)`/`(fr)` prefixes, including incomplete `(fr`, are never presented as real
translations. Credit placeholders are rejected explicitly. Their existing16KiB
text limits, scrolling, exit flow and audio ownership are preserved. The original
credits section-selection point is43dc80; viewport wrapping43dd4f and40px/s scroll
43df61 are unchanged. Runtime fallback is a practical port policy.

## Explicit budgets and rendering

- Exactly32 current-language labels ×64 bytes =2,048 resident text bytes, plus
 128 bytes of constant IDs and one4-byte language selection.
- One2,176-byte temporary candidate (2,048 text +128 rank), a512-byte VPP read
  cache and a1,024-byte source-line buffer. A64-byte decode scratch is reused.
- Explicit resident/candidate/cache/line/decode payload peak5,956 bytes, plus
 small scalar/pointer frames and existing VPP/stdio owner overhead; no full
 strings table or all-language catalog is retained. Source file cap128KiB.
- For the existing fixed-cell layout, action labels are admitted only up to17
 bytes; On/Off up to9; other selected labels up to24. Oversized translations are
 unavailable and fall back rather than being truncated. The authored selected
 maximum is21 bytes (French Look Sensitivity), and all96 installed selected
 entries fit their admission bounds.
- Actual composed Options rows are at most30 bytes =360px. Bounds permit at most
36 bytes =432px for an invert row; other Options combinations fit the432px row.
- Mapping action(17) + spaces(3) + longest physical control `HOLD RIGHT-STICK
 CLICK`(22) is at most42 bytes =504px in the508px mapping row. The80-byte format
 buffer is larger than this whole bounded composition.
- The existing5×7 fallback font gains only the five required accent forms for
 ß/ä/é/ö/ü, folds lowercase accented bytes to those glyphs, and draws `?` for
 unknown non-space bytes instead of silently dropping them. Existing12px advance
 and untextured geometry ownership remain. This is single-byte text, not UTF-8.
- Options remains one page: indices0–7 are unchanged, Text Language is8. Nine
 rows fit between82 and298; notices begin310. No renderer/backend ABI change.

## RFCS4 storage contract

The existing two36-byte files and offsets remain:
magic0, version4, size8, generation12, sensitivity16, invert20, deadzone24,
packed choices28, FNV-1a checksum32. No native struct is serialized.

RFCS1/2/3 continue decoding exactly as before and imply English.
RFCS4 word28 is `((button_rank * 2 + stick_layout) * 3 + text_language)`, where
English=0, German=1, French=2. Admission requires packed<21,772,800 (3×2×10!).
Decode recovers language by modulo3, divides by3, then uses the existing low
layout bit and permutation rank. Non-English writes require version4; English
writes use the original canonical RFCS1/2/3 bytes at the same generation.
Readers that do not support RFCS4 reject it and refuse to overwrite it.

Checksum, generation parity/adjacency, unknown/corrupt-record refusal, exclusive
inactive-slot revalidation, flush/reread verification, checked close/unmount and
sticky ownership lock are unchanged. A language change never writes by itself,
modifies a campaign checkpoint, or changes controller mapping/rearm semantics.

## Remaining implementation boundary

This slice does not translate all port UI, general HUD/weapon labels, authored
voices, assets or campaign content. Additional authored source packs are needed
for real non-English campaign dialogue, endings and credits. Full localization
must not be reported from this bounded catalog. Broader operational work and
tester feedback remain separate from this source-only implementation.
