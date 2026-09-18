# First-pass armed NPC cadence and ammunition

The live enemy shot loop now uses authored weapon firing timing. Handguns use30-frame spacing; rifles schedule3-shot bursts at6-frame spacing with45 frames between burst starts, subject to existing aim, pain, range and cover gates. A denied range/cover attempt retries after6 frames without consuming a burst round. Target changes clear pending bursts.

Supported handgun/rifle owners use their existing startup-granted inventory. A successful shot consumes one loaded round; an empty magazine starts the authored reload timer, then transfers finite reserve through the existing inventory primitive. Partial final magazines work and exhausted actors no longer fire indefinitely. No automatic refill or weapon-switch fallback was introduced. Reload presentation and broader weapon behavior remain refinement/integration work.

Focused scene_ai_gameplay checks pass for cadence and reload/last-magazine behavior. A240-frame Live Mines replay against actor8456 emits shots at30/31,60/61,90 and ends after player death; source artifacts/ai-gameplay/idle.log. This confirms the live handgun path and damage, not a visually reviewed rifle/reload sequence. The faster fire makes this staged encounter more lethal; balance is intentionally left for tester feedback. The shared NXDK build passes; no new emulator run was needed for this code integration.

Agent-produced helper lives in scene_ai_gameplay.inc; parent owns scene callsites and build wiring. Precision shots and teleport actions are separate pending integrations.
