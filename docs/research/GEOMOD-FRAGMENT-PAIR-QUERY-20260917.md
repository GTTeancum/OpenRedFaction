# Fragment collision query groundwork

The new rf_geomod_piece_registry_body_sweep_excluding performs the existing body-sphere/polygon sweep while excluding exactly one source batch/piece identity. Other pieces remain eligible, including those in the same batch. UINT32_MAX batch means no exclusion; invalid source identities preserve outputs. Existing player/body queries retain their previous all-piece behavior through a wrapper. No allocation or registry ownership changes are introduced.

The query retains stable nearest-hit ordering, actual current-pose polygons, retirement filtering, source sphere identity, target batch/piece/face and target linear velocity. The extraction test verifies excluded self geometry cannot win, a distinct extracted body remains hittable with its actual velocity, invalid exclusions preserve outputs, and the old all-piece query produces the identical result. All121 freshly rebuilt PC tests pass. Stock64MiB NXDK compilation/link succeeds; no native gameplay change is claimed.

The current scene_detached_query still queries terrain only. Its rf_physics_solid_step response accepts no moving counterpart. Initial concern that this necessarily required a symmetric two-body solver was not supported by the subsequent original-code probe below. Contact admission and scheduling still require evidence before enabling pair queries in gameplay.

## Original admitted-contact response

`python tools/probe_fragment_contact_dispatch.py` passes27 original-code cases. It executes full48a400 dispatch with kind3 and liquid-transition marker+1ec zero; real412b40 returns policy2 without object mutation. It then executes full49d330 with explicit material coefficient returns. Three axis/oblique normals cross three counterpart inverse masses and three counterpart velocities.

Memory-read hooks cover contact counterpart fields+1d4..+1e7 and the valid mapped counterpart object. Neither is read. The counterpart remains byte-identical, and the source body output is identical across all counterpart metadata variations for each normal. Original4a0349..4a0362 instruction inspection confirms this policy2 dispatch invokes49d330. This differs from the player's49d7e0 dynamic response; do not substitute that solver based only on the expectation of realistic two-body physics.

Limits: the probe supplies an admitted contact. It does not execute the preceding collision query or prove that every kind3 pair is admitted, that no earlier response modifies the counterpart, or that scheduling is order-independent. The next task is the original pair-admission and scheduling path. The existing single-body response may be the appropriate downstream solver if those gates admit a pair; no new symmetric impulse should be invented without evidence.

Evidence: tools/probe_fragment_contact_dispatch.py and artifacts/geomod-postedit-re/fragment-contact-dispatch.json; original SHA is checked by the script. No production source or emulator change in this investigation.
