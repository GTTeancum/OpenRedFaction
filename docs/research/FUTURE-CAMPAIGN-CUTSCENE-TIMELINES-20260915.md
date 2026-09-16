# Authored cutscene timeline loader

## Verified result

`tools/future_re/campaign_cutscene_timeline_loader.py` executes original `465d50` on all12 installed level cutscene sections and checks all84 points against a separate bounded parser. It also executes original duration helper `45b200` for every point. Result: PASS12 timelines/84 points, complete byte consumption. Detailed source values and results are in `artifacts/future-campaign-re/cutscene-timeline-loader.json`. Original executable SHA256: `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

This closes the previous lifecycle report's selector ambiguity: **every installed descriptor selector equals its Cutscene55 event UID**, in all12 levels. Start action passes event+24 to descriptor lookup45b280; the authored resource confirms the intended join. This is not one of the event's generic words/float/text fields; those are empty in the authored Cutscene events.

Scope: actual loader control flow, versioned primitive integer/boolean/float readers and duration arithmetic execute. File transport, heap allocation, constructor initialization, registry append and string internals are intercepted boundaries. String bytes are decoded as original 16-bit length-prefixed data. No camera movement, full timeline, stock process, port build or emulator runs.

## File and ABI

Level section dispatch `460f63..460f8c` maps section type **0x4000** to cdecl `465d50(reader*)`. All observed level versions are180. Data begins with u32 descriptor count. Each descriptor is:

| Serialized order | Runtime destination | Meaning justified by evidence |
| --- | --- | --- |
|u32|+0|Selector matching Cutscene event UID|
|u8|+859|Hide/control gate used by start45bb00; read threshold143/defaultfalse|
|f32|+85c|Camera FOV applied during start; read threshold146/default90|
|u32|+4|Point count|
|repeated points|+8 + index*32|Timeline records below|

Each serialized point: u32 camera selector, three f32 durations, two u32 handles/words, then u16 byte length and CP1252 string bytes. Runtime point layout is +0 camera selector, +4/+8/+12 durations, +16/+20 words, +24 eight-byte string object. Registry pointer array is645fa8. Allocation is0x860 bytes per descriptor. The inline point region from+8 through+807 fits64 entries. Original loader does not visibly bound count before writing; the port must reject >64 before allocation/publication rather than reproduce this overflow. Authored maximum is23 points.

The exact meanings of the three duration phases and two words remain unknown here. Keep neutral field names until interpolation/event dispatch consumers are recovered. Do not name the words target/event handles without examining consumers.

## Timing and point selection

`45b200(desc,index)` returns ST0 = point.duration3 + duration2 + duration1; null descriptor or negative index returns the constant at5893e0. It does not check upper index. All84 valid authored sums execute and match their float-rounded sums exactly. In `45b3f0(desc,index)`, this sum is multiplied by constant5897b4 and rounded through573528 after addition5893c0, then passed to deadline setter4fa360 for descriptor+810. Confirm constant values/clock and zero-duration behavior during timeline execution rather than guessing units from loader alone.

Point initialization uses camera selector through45b230, saving lookup index at desc+80c; lookup searches array644f10 and returns-1 if absent. Point pointer is retained at+81c. Nonempty string is resolved through45b590; result retained+820. A successful result also gets an initial timer at+814 from the first duration. Runtime flags+854/+858 reset. Subsequent code uses camera array entries for pose, so validate referenced camera resources before control handoff; the original code uses the returned index without an obvious missing-camera guard.

Do not confuse section0x5000 loader465b90 with this descriptor loader: that section populates another capped64-entry structure at644f20/644f08. The camera registry reader for644f10 and interpolation are the next bounded targets.

## Authored examples and size

| Level | UID | Points | FOV |
| --- | ---: | ---: | ---: |
|L6S3|3696|14|45|
|L11S3|10626|23|45|
|L20S2|18355|1|45|
|L7S1|5495|4|45|
|L7S2|4948|3|45|
|L7S3|7886|3|45|
|L7S4|10562|11|40|
|L8S4|6816|12|45|
|L13S3|8771|6|45|
|L14S3|9618|5|45|
|L15S4|9613|1|90|
|L17S4|18248|1|90|

Most first point strings are `none`; L20S2 uses `mtruck`, L14S3 `path1`, L17S4 `blowup`. Do not treat the literal `none` as an empty string automatically:45b3f0 calls its string resolver whenever length>0. Resolver behavior still needs recovery. L6S3 first point camera6851 has durations(0,0,3.2), words(-1,6852). L11S3 first camera10627 has(2.6666,0,0), words(-1,10624). These contrasting cases should anchor the next interpolation tests.

## Minimal implementation preparation

Add bounded section0x4000 parsing beside existing RFL section readers, then join descriptor UID to Cutscene55 event UID during scene setup. Store only source records and per-scene active index/deadline/control state, not original pointer layouts. Current authored maximum23 points is tiny under64MiB, but preserve a bounded64-point format limit and string bytes budget. Validate counts, finite FOV/durations, byte spans and camera references before entering cutscene. Defer camera interpolation implementation until its actual duration phase/word semantics are known.

Regression cases: every installed section consumes exactly its declared bytes; all12 joins resolve; zero/over64 points; truncated fields/strings; invalid FOV/nonfinite durations; missing camera; duplicate selector; natural finish vs cancellation, ensuring only natural completion broadcasts When_Cutscene_Over83 once. Only valid authored load/duration cases are executed by this report; malformed parser tests belong to the later implementation.
