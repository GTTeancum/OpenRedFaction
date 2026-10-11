# NPC velocity-save rejection observation

Source baseline076c300d2036de5053172cad5991ddc6b01da60c. This is diagnostic-only code, not a new save owner.

## Actual blocker and evidence limit

The 2026-10-11 05:00 Xbox build passed. The original L3S1 producer reached180 presentations, but its ordinary save at120 rejected RF_RANGE/stage2 with `[929,846470194,1,3227908399,4294967292,120]`: source-body velocity Y=-3.5933339595794678. No fresh-load consumer ran.

Actual UID929 is guard1 with the ordinary12mm handgun and authored L3S1Guard5 Ping Pong path34→9, whose Y coordinates are approximately0 and-3. That proves a reachable vertical patrol, not the actor's live route phase. The six reject words do not identify living versus terminal-dead admission, body mode/flags or support. Terminal-dead capture can reach the same velocity guard. The observed value being consistent with22 gravity increments does not establish living fall3 or an exact corrective implementation.

The current capture guard rejects nonfinite or absolute velocity>.001. Fresh ordinary restore initializes velocity to zero and separately checks support, so simply bypassing the guard would lose momentum and leave airborne placement dependent on legacy live/constructor flags. Original falling predicate42a020 and prepared translation49e750/49e8b7–49e9e6, plus the existing ordinary NPC step, establish that velocity, force/mass and inherited support velocity are real continuation inputs. Angular steering also retains angular velocity/torque. Existing script-physics, Drone and authored-route lanes have distinct ownership; this observer does not alter them.

## Implementation

A new fixed512-byte `rf_scene_npc_checkpoint_velocity_observation[128]` is filled only after the existing velocity rejection has recorded its six-word tuple. It snapshots the same passed owner/index and already-computed terminal_dead flag without recapturing, querying geometry, demanding animation, looking up support, scanning actors, allocating, invoking callbacks or mutating the owner. All optional bank reads are bounds/NULL guarded. A validity mask distinguishes missing optional data from zero. Source-body allocation and model-owned-pose presence are recorded independently. Floats and the full64-bit existing scene_robot_death_epoch are copied bit-for-bit.

Every existing rejection clears the extra bank first, preventing a stale velocity sample from appearing to describe a later different rejection. The original predicate, six-word record, amount conversion and return status remain unchanged. The natural-save harness adds this one128-word symbol to its existing terminal observation union. Inputs, save frame, producer/consumer duration, live sampling, retry prohibition and failure stops are unchanged.

Interpret only format1/count128 with an exact six-word tuple match and relevant validity bits. Format0 means no current velocity sample, not a settled actor. Contact handles/tokens are raw observations, not portable references or newly admitted collision owners. Detailed word layout is appended below.

## Cost and validation

Source accounting:512 static bytes and16 bytes of explicit helper locals on Xbox32, with a conservative64-byte local/argument allowance excluding compiler spills/ABI/library frames. No dynamic allocation. Static review is byte-bound; this candidate has not been compiled, imported, tested or run. Actual live state remains unknown until the parent-authorized06 batch; no retry or earlier runtime is authorized. Airborne continuation and ordinary reload remain deferred; no RFNC version is allocated.

Final staged patch SHA256: d56e9da2732f13bc57137bf18fccf9edaf4709bec9f879f6d3752a935512545a.

Independent exact review SHA256:457128d03e3fd7f472b5b2b385ac733b32f386bb557237798220c390b77ed2e4.

# Rejected NPC velocity observation v1

128 raw uint32 words,512 bytes. Signed integers retain two's-complement bits; floats are copied without arithmetic or normalization.

Validity bits:1 owner array row;2 class definition;4 movement descriptor;8 model-owner row;16 seed pose row;32 allocated physical source body;64 persistence row. The owner row validity permits reading its scalar/vector storage; bit32 separately establishes that a source body is allocated. Metadata validity never claims a valid collision response, active contact, live corpse or posed matrices. Invalid optional groups remain zero.

0: format version1
1: word count128
2: validity mask (published last)
3–8: exact existing reject tuple: UID, reason hash, part, value bits, status, frame
9: same owner index
10: class index (validity2)
11–12: registration handle, view-present boolean
13: health (f32 bits)
14–17: object flags, view810, damage810, damage814
18–19: already-admitted terminal_dead argument, death action824
20–21: movement slot, resolved descriptor index (descriptor requires validity4)
22–24: class use-kind, flags, flags2 (validity2)
25–28: source-body flags, state124, scripted_physics, scripted_physics_seen
29–30: support handle, material
31–34: piece tag, piece index, publication serial, reacquire
35–37: current AI action, patrol enabled, patrol reverse
38–48: script_move active, origin, event, follow, path_index, path_mode, path_reverse, path_count, route_index, retry, retained_count
49: script_move fall_speed (f32)
50–52: source-body position (f32 XYZ)
53–55: source-body next_position (f32 XYZ)
56–58: published position (f32 XYZ)
59–61: body linear velocity (f32 XYZ)
62–64: body angular velocity/vector_c8 (f32 XYZ)
65–67: body momentum/mass_vector_d4 (f32 XYZ)
68–70: body force/vector_e0 (f32 XYZ)
71–73: body torque/vector_ec (f32 XYZ)
74–76: additive support_velocity (f32 XYZ)
77–79: retained contact normal/vector138 (f32 XYZ)
80–83: contact scalar144 (f32), reference15c, word164, word168
84–86: command714 (f32 XYZ)
87–89: script_move target (f32 XYZ)
90–93: collision-contact material, reference token, part, inverse_mass (f32)
94–96: collision-contact velocity (f32 XYZ)
97–99: collision-contact point (f32 XYZ)
100: source-body mass (f32)
101–102: source-body allocated bytes, sphere count
103–105: linked handle, attachment75c, view type
106–107: model-owner owned flag, model pose-present boolean (validity8)
108: seed pose-bank row present (validity16), never pose-content validity
109–111: body look angles (f32 XYZ)
112–114: angular command708 (f32 XYZ)
115–116: existing uint64 scene_robot_death_epoch (low/high words on Xbox32)
117: persistence retired flag (validity64)
118–120: sphere-storage pointer-present boolean, sphere allocated bytes, script_move stop
121–127: reserved zero
