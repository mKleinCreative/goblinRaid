---
id: 043
title: Aim camera: lengthen the arm at high pitch instead of shortening it - the goblin was crowding the frame
agent: claude-cam
status: done
claimed: 2026-08-06T20:01Z
build: required
waiting_on:
evaluated: 2026-08-06T20:03:32Z
files: 
  - Source/GoblinSiege/Characters/GSPlayerCharacter.h
  - Source/GoblinSiege/Characters/GSPlayerCharacter.cpp
---

## Goal

Aim camera: lengthen the arm at high pitch instead of shortening it - the goblin was crowding the frame

## Generate

Michael, 2026-08-06, on #042's fix: *"the problem is the player model crowds the camera a little too
much."*

**#042 had the trade backwards.** It bought ground clearance by halving the aim arm to 125uu at full
pitch, on the correct reasoning that the camera's drop is proportional to arm length. Correct, and
the wrong currency: it puts the goblin's back in frame at exactly the moment you are trying to read
an arc past him. My own Evaluate on #042 named this as the risk I could not predict - "half the aim
arm is close, and the goblin's back will fill more of the frame" - and it was the first thing to bite.

**Inverted.** Clearance is now entirely the pivot lift's job, and the arm GROWS with pitch so the
model shrinks as you aim higher.

- `AimHighPitchArmScale` 0.5 -> **1.15** (lengthens rather than shortens; ClampMax raised to 2.0)
- `AimHighPitchLift` 90 -> **190**

Recomputed against the real rig (pivot 165uu above the feet, aim arm 250):

| pitch | arm | camera Z above feet |
|---|---|---|
| 0 deg | 250 | 165 |
| 15 deg | 262 | 160 |
| 30 deg | 275 | 154 |
| 45 deg | 287 | 152 |
| 60 deg | 287 | 106 |

Flatter than #042's version *and* the arm never drops below its resting length.

**Separately, and live in the editor with no rebuild:** `AimArmLength` 250 -> **320** on
`BP_GSPlayerCharacter`. That is the base aim framing and the actual source of the crowding - #042's
scaling only made an existing tightness worse. Hip is 450, so 320 still reads clearly as "closer"
while giving the goblin room. Verified by re-reading the CDO off a fresh load after saving.

## Evaluate

**The `AimArmLength` 250 -> 320 change is LIVE NOW** and needs no build - verified by re-reading the
CDO off a fresh load. The two C++ constants are **NOT COMPILED**; the editor is open.

**So the state Michael can feel right now is half the fix.** Base aim framing is roomier; the
pitch-driven part still has #042's shortening compiled into the DLL until the next build. If he aims
up before then, the camera will still pull in - that is the old binary, not a failed fix.

**Everything here is still geometry, not gameplay.** The table says where the camera sits; it does
not say the throw feels good. Two rounds of this now: #042 was arithmetically sound and wrong in
practice, which is the whole reason this ticket exists.

**Concrete risks in this version:**
- **320 may be too far.** It is a 28% jump on a value chosen deliberately during the ranged work for
  an over-the-shoulder read. Nothing about "crowds a little too much" says how much less is right;
  I picked a number. It is one MCP call to change again, and no rebuild either way.
- **190uu of lift is a lot of vertical travel.** The camera rises most of a goblin's height as you
  pitch from level to 45 degrees. Smooth, but it is a bigger unrequested motion than #042's, and
  "drifts upward while aiming" is a plausible complaint about this build.
- **Past 45 degrees clearance falls off fast** (106uu at 60, 77 at 75) because the lift is clamped at
  full while `sin` keeps climbing. Fine for lobbing, still tight for a near-vertical throw.
- Slopes and rooftops are unaddressed, exactly as in #042.

**Owed AGENT_STATE.md** - a DECISIONS line once this is confirmed: *aim-camera ground clearance comes
from raising the pivot, never from shortening the arm - shortening trades a floor collision for the
player model filling the frame.* Holding it until Michael has played it, because that is precisely
the kind of claim #042 made on arithmetic and got wrong.

**Touched outside the goal:** `BP_GSPlayerCharacter.uasset` was NOT in this ticket's claim - I claimed
only the two source files. It was claimed under #041 and #042 (both closed), so nothing was contended,
but the claim should have been widened before the edit rather than noted after it. Second time today.

## Refine

- **Fixed the base value, not just the multiplier.** The quick read of "the model crowds the camera"
  is that my new scaling caused it, so revert the scaling and stop. But the scaling is not compiled
  yet - what he is looking at is `AimArmLength = 250`, which has been the aim framing since the ranged
  work. Treating the complaint as being about the multiplier alone would have shipped a fix for
  something he had not seen and left the thing he had.
- **Made the data change through the editor rather than folding it into the C++ default.** It is the
  value most likely to need another pass, and this way each pass costs one call instead of a
  five-minute build and a five-minute reload.
- **Kept the correction curve rather than reverting #042 wholesale.** The underground camera is real
  and measured; only the currency it was paid in was wrong. Reverting both would have restored a
  camera that sits 12uu below the goblin's feet at the max-range throw angle.
- **Raised `AimHighPitchArmScale`'s ClampMax to 2.0.** It was clamped 0.1-1.0, which encoded
  "this can only shorten" - the exact assumption that turned out to be wrong. A clamp that forbids
  the correct answer is worse than no clamp.
- **Wrote the reversal into the property comment, not just the ticket.** The next person to see
  `1.15` will otherwise assume shortening was never tried and re-try it; nothing reads closed tickets
  at run start.

**Deliberately left undone:** tuning 320 and 190 (they need play, not arithmetic); clearance above
45 degrees; slopes and rooftops; and the AGENT_STATE DECISIONS line, which waits for confirmation.
