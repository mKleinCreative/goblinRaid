#!/usr/bin/env bash
# Goblin Siege — create art-asset epics (milestones) + issues on GitHub.
# Run from anywhere:  bash create-art-issues.sh
# Requires: gh CLI, authenticated (gh auth login), with write access to the repo.
# Idempotent-ish: re-running creates DUPLICATE issues. Run once.

set -euo pipefail
REPO="mKleinCreative/goblinRaid"

echo "==> Target repo: $REPO"
gh repo view "$REPO" --json nameWithOwner -q .nameWithOwner >/dev/null

# ---------------------------------------------------------------- labels
echo "==> Creating labels"
mklabel() { gh label create "$1" --repo "$REPO" --color "$2" --description "$3" 2>/dev/null || echo "    label '$1' exists, skipping"; }
mklabel "art"          "C2E0C6" "Art asset work"
mklabel "character"    "5319E7" "Character mesh/rig"
mklabel "prop"         "1D76DB" "Prop / handheld / dressing"
mklabel "building"     "B60205" "Building-scale asset"
mklabel "environment"  "0E8A16" "Foliage / terrain dressing / states"
mklabel "polish"       "FBCA04" "Conformance / sweep / dressing pass"
mklabel "blocker"      "D93F0B" "Blocks a scheduled milestone"

# ---------------------------------------------------------------- milestones (epics)
echo "==> Creating milestones (epics)"
mkms() {
  gh api --method POST "repos/$REPO/milestones" \
    -f title="$1" -f description="$2" >/dev/null 2>&1 \
    && echo "    + $1" \
    || echo "    milestone '$1' exists or failed, continuing"
}
mkms "Epic: Human Characters"              "Every human defender/civilian mesh the slice needs. All export at 411 units native, 4-4.5 head units, painted-simple faces (style guide S3/S9.11). Current in-engine stand-in is the stock UE Mannequin at x2.25."
mkms "Epic: Human Props"                   "Kit and dressing tied to the human faction: defender gear, town fire-response, captive rope, and the smug-detail set."
mkms "Epic: Goblin Characters"             "Manual/Fab track (out of Blender-MCP scope). Scout 240 units native, Goblin Green + Scav Leather + one Warpaint Rust accent."
mkms "Epic: Goblin Props & Weapons"        "The player's verbs made physical: sword, torch, war-horn, and the Warren relay."
mkms "Epic: Hero Buildings"                "The three gap buildings with no Dreamscape coverage. All full custom, GS-atlas style, all currently absent from generated layouts."
mkms "Epic: Runic Site"                    "The third art vocabulary: the spawn/extraction portal set. Saturation ceiling of the whole game."
mkms "Epic: Objective States & Destructibles" "State-variant meshes the burn and smash verbs need. Burn VISUALS stay material-driven; these are the mesh-side deliverables only."
mkms "Epic: Conformance & Dressing Sweep"  "The scheduled W6 art pass: char-mask re-exports, recolors, hardened-seam fixes, and remaining loot dressing."

# ---------------------------------------------------------------- issues
echo "==> Creating issues"
mki() { # title, body, milestone, labels
  gh issue create --repo "$REPO" --title "$1" --body "$2" --milestone "$3" --label "$4" \
    | sed 's/^/    /'
}

REF_FOOT=$'\n\n---\n*Filed from the Cycle-01 art audit. Sources: `claude/goblin-siege-prefab-kit-manifest.md`, `claude/goblin-siege-art-style-guide.md`, `claude/goblin-siege-character-design-log.md`, `race-design-humans.md`, GDD TL;DR.*'

ACCEPT_COMMON=$'\n\n**Acceptance (style guide S10):**\n- [ ] Material class assigned first, its S5 tells present\n- [ ] Silhouette test: flat-black render passes its three-word test at ~15m\n- [ ] Nothing straight (unless stone/runic) — lean/sag per S2.5\n- [ ] Buildability pass (S2.6): every piece traceable to a real construction job\n- [ ] Palette compliance: named atlas cells/ramps only, protected band untouched\n- [ ] UVs: flat cells inset >=25%, gradients vertical shadow-down\n- [ ] Vertex colors: R char mask, G sway, B baked AO\n- [ ] Scale checked against canon (goblin 240 / human 411), not real-world anatomy\n- [ ] Dusk screenshot attached to the manifest'

# ---- Epic 1: Human Characters
MS="Epic: Human Characters"
mki "Militia character mesh + rig" \
"The frontline human defender — the unit the player meets first and most often.

**Must read:** Pennybrook Burgundy / Livery Gold livery, pear body, 4–4.5 head units, disciplined but unimpressive. Stumpier than realistic on purpose (shares a world with 3-head goblins).

**Spec:**
- Export at **411 units native** — do NOT ship an actor-scale multiplier. A non-unit actor scale breaks CharacterMovementComponent speed/step-height and weapon socket sizes.
- Sweep the imported reference pose for non-unit bone scale; a bone at scale 100 means the FBX export is broken even if the mesh looks right.
- Face: painted-simple, big brows, dot-simple eyes — expressive at bark distance.

**Gameplay hooks:** takedown-from-behind target, RAID-phase barracks spawn, counts against the 15-strong raid-response pool.

**Blocks:** the 411 stand-in (\`TEST_ScaleRef_Human\`, stock Mannequin at x2.25) can't survive into playtests that judge silhouette or cover reads.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,character"

mki "Male villager character mesh + rig" \
"Civilian. Not a combatant — the mercy-bonus and capture target.

**Must read:** Peasant Wool, soft and round in silhouette, unmistakably *not* a guard at 15m. If a player can confuse him for militia at distance, the civilian-killed vs civilian-captured score split stops teaching.

**Spec:** 411 units native, 4–4.5 head units, painted-simple face.

**Gameplay hooks:** bind (not kill) from behind; marches home in a rope chain up to 3 per escorting goblin; feeds the \"None Left Behind\" +40 bonus.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,character"

mki "Female villager character mesh + rig" \
"Civilian, second silhouette. Same rules as the male villager, distinct enough that a crowd doesn't read as clones.

**Spec:** 411 units native, Peasant Wool, soft round silhouette, painted-simple face.

**Gameplay hooks:** identical to the male villager — bindable, escortable, mercy-bonus eligible.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,character"

mki "Archer character mesh + rig" \
"Patrol backline. The most dangerous unit in the *stealth* layer, not the combat one — she carries her own signal horn, so failing to silence her sends the alarm to the hamlet from the woods, exactly like a rung bell.

**Must read:** lighter than militia, visibly ranged at a glance (bow silhouette), horn visible on her person so the threat is legible before it fires.

**Spec:** 411 units native. Horn + bow are separate props (see Human Props epic) so the horn can be a visible tell.

**Gameplay hooks:** priority target; kites at range; her horn is an alarm source.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,character"

mki "Knight character mesh + rig" \
"RAID-phase heavy. Arrives once the town escalates — his silhouette is the visual announcement that the quiet phase is over.

**Must read:** top-heavy plate with comically tiny greaves (style guide S3). Knight Steel must read **brighter than prop iron** so armor is distinguishable from set dressing at distance.

**Spec:** 411 units native. The plate is the one place human-made metal gets to look maintained.

**Gameplay hooks:** 25-point kill, draws from the finite 15-strong response pool, arrives from barracks and the distant castle.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,character"

mki "Watchman character mesh + rig" \
"The dozing tower guard — the single most important human for teaching the stealth layer, because silencing him before he reaches the bell is the game's cleanest counterplay lesson (+25).

**Must read:** relaxed/slouched pose reads as *dozing* from the treeline, at dusk, before the player commits to an approach. If the player can't tell he's asleep, the whole watchman-silenced beat fails.

**Spec:** 411 units native. Pairs with the watchtower cushion smug detail and the bell (Human Props).

**Open:** could be a militia kit-variant rather than a distinct base mesh — decide during blockout, but the doze silhouette is non-negotiable either way.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,character"

# ---- Epic 2: Human Props
MS="Epic: Human Props"
mki "Watchtower bell + SOCKET_Bell" \
"The alarm made physical. Separate mesh from the tower so the Town Agent can animate the ring.

**Must read:** polished — Cold Iron base into the **Iron Sheen top ramp**. This is the one thing in Groatsworth they actually maintain, and that gleam is the joke.

**Spec:**
- Separate mesh, mounted via \`SOCKET_Bell\` on \`SM_GS_Bld_Watchtower01\`.
- States: hanging / ringing (anim, ENG-owned) / silenced (no state change — the watchman is just dead).

**Depends on:** watchtower build (Hero Buildings epic, W5).${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,prop"

mki "Militia weapon + kit set" \
"Melee weapon and any shield/kit pieces the militia silhouette needs to read as a soldier rather than a villager holding something.

**Must read:** hardened wood + iron. Metal sits **on top of** wood at every seam (playbook S2.1) — a hoop or strap that runs parallel to the boards it sits beside isn't binding anything.

**Spec:** chunky over detailed — planks 2x too thick, rivets 3x too big. Prop iron must read *duller* than Knight Steel.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,prop"

mki "Archer bow + signal horn" \
"Two props, one issue — they ship together on the archer.

**The horn is gameplay-critical:** it's an alarm source equal to the bell, fired from the woods. It must be visible on the archer's person so an observant player can identify and prioritize her *before* she blows it. An invisible horn makes the patrol encounter feel unfair.

**Spec:** horn = hardened material, small and contained. Bow = human-made, tidy, 1–3 degree settle (not goblin-crooked).${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,prop"

mki "Bucket-brigade bucket" \
"The town visibly fighting back. Civilians carry these to douse fires; fouling the well is what disables the brigade (+30).

**Must read:** raw wood + iron hoops, carryable scale, obviously a water vessel at distance so the brigade behavior is legible without UI.

**Spec:** pivot at **grab point** (handheld/carryable rule, style guide S7). Needs a full and an empty read if the brigade animation calls for it — confirm with Town.

**Depends on:** well fouled insert (Objective States epic) for the counterplay loop to close.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,prop"

mki "Rope / bind kit for captives" \
"Visible rope for the bind verb and the march-home chain — up to 3 civilians per escorting goblin.

**Must read:** the binding must be visible on the captive from behind at courier distance, so a player watching a loot train can tell prisoners from free civilians.

**Spec:** cloth/fiber material class. Chain geometry needs to survive the escort formation without intersecting; coordinate the attach points with Horde (courier state) and Stealth & Interaction (bind gating).${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,prop"

mki "Smug-detail set: poster, plaques, NO GOBLINS sign, cushion" \
"Style guide pillar 2 — *the satire lives in the details.* Every asset family owes at least one smug detail; this issue collects the standalone ones.

**Contents:**
- Barracks recruitment poster
- Assorted plaques (self-congratulatory, oversized)
- NO GOBLINS sign
- Watchman's stool cushion (the tower's one comfort)

**Must read:** prosperity played for comedy — a padlock bigger than the door it guards, that energy. These are cheap meshes with high tone-per-triangle.

**Schedule:** W6 art pass.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,prop,polish"

# ---- Epic 3: Goblin Characters
MS="Epic: Goblin Characters"
mki "Scout hero body — final mesh pass + rig" \
"Replace the current blockout. Michael's assessment of the Blender build was \"pretty bad,\" and the in-engine inspection agreed: \`GOB_BaseBody\` is 30 unjoined primitives with no armature.

**Work:**
- Fuse-and-carve the torso and limbs the way the head was done (join -> voxel remesh -> smooth -> boolean carve -> region-assign materials). Watch for loose islands after remesh.
- **Arms are still robotic-straight** — add elbow bend and outward bow.
- Mouth corners pull back further toward the cheeks; jowls not yet modeled.
- Retopo for export: **no fan-poles** (GDD S14). The fused head alone is ~3.2k faces — tri-budget the whole body before export.

**Export recipe (the one that works — default FBX settings silently corrupt skeletal scale):**
1. Duplicate armature + mesh (object *and* data); name the duplicate armature exactly \`Armature\`.
2. Bake scale into the **data**: \`arm.data.transform(Matrix.Scale(f,4))\`, same for mesh. Object scales stay 1.0.
3. Temporarily set \`scene.unit_settings.scale_length = 0.01\`.
4. Export with \`global_scale=1.0, apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', add_leaf_bones=False, armature_nodetype='NULL', axis_forward='-Z', axis_up='Y'\`.
5. Restore scale_length, delete duplicates.

**Verification that catches the bug — run after every character import:** every bone's \`scale3d.x\` must be within 0.001 of 1.0. A single bone at scale 100 means the export is broken no matter how correct the mesh looks.

**Target:** 240 units native, 3 head units, chest pitched ~32deg, head thrust forward, hump behind neck.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,character,blocker"

mki "Scout kit dressing: loincloth, straps, warpaint accent" \
"The kit that makes the base body read as *the Scout* rather than a generic goblin.

**Contents:** loincloth, harness/straps, Warpaint Rust body accent (exactly one accent — saturation is a currency).

**Must read:** Scav Leather kit over Goblin Green skin. Silhouette-first kit identity — the Scout's read is speed and blades, distinct from Brute scale and Shaman staff profiles even though those are shelved.

**Note:** bow, quiver, band and arrows are already built and unaffected. The dual daggers are deprecated — see the sword issue.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,character"

mki "Horde goblin variant(s)" \
"Up to 10 on screen at once, sprinting from the treeline. This is the money shot of the war-horn.

**Must read:** **Goblin Shade** base (darker than the player, so the player stays readable in a crowd) + randomized Warpaint Rust props for variety without new meshes.

**Spec:** built for crowd rendering — the player Scout is the hero mesh, these are not. Randomization should come from prop/material variation, not unique bodies.

**Schedule:** W3 is horde week — these need to exist for point-command smash targets and courier runs to be evaluable.

**Gameplay hooks:** finite pool of 20; 3–4 per horn blast, max 10 active; couriers carry loot and livestock home.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,character"

# ---- Epic 4: Goblin Props & Weapons
MS="Epic: Goblin Props & Weapons"
mki "Scout sword (replaces deprecated dual daggers)" \
"Per design doc S13 decision 36, the Scout's melee identity consolidated from twin daggers to a single sword. The modeled dagger pair is **not the shipping weapon** — treat it as reusable practice work.

**Must read:** goblin-made — patched, asymmetric, held together with rope and optimism. 4–10 degree crookedness allowed (goblin construction law).

**Spec:**
- Sized against the 240-unit rig, not real-world anatomy.
- Assembly rule: place all parts along one direction vector via \`to_track_quat\` — hand-offset placement misaligns at canted angles (dagger v1 and arrow v1 both failed exactly this way).
- \`DA_Weapon_Scout\` should be authored against a sword moveset from the start.

**Unblocks:** the sword/bow live-swap, and the human matchup table's Sword row (currently carried over from daggers unchanged and owed a gut-check once the moveset exists).${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,prop,blocker"

mki "Torch prop" \
"The core verb of the entire game deserves its own mesh. Universal kit — every class has it.

**Must read:** goblin-made, crooked, cheap. The flame is the saturation ceiling's little sibling — fire is one of the only things allowed into the top saturation band, so the *unlit* mesh must be muted enough that ignition reads as an event.

**Spec:** pivot at grab point. Throwable — needs to read correctly mid-arc, not just in hand.

**Careful:** ambient lantern flames are already in the protected orange band; keep the torch's lit state distinguishable from set-dressing fire so a thrown torch never reads as a street light.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,prop"

mki "War-horn prop" \
"Universal kit. Blowing it summons 3–4 horde goblins per blast — this is the single most dramatic input in the game.

**Must read:** goblin-made and unmistakably *loud-looking* — the visual promise of the noise. Contrast deliberately with the archer's tidy human signal horn: same function, opposite construction philosophy.

**Spec:** pivot at grab point; must read at third-person camera distance during the blow animation.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,prop"

mki "Warren dig-site prop + destroyed state" \
"Decided 2026-07-24. A dug-out field relay — **not** a second portal. Planted once a raid goes loud, held open by one horde goblin (the digger).

**Must read:** improvised and goblin-dug, clearly temporary. It must NOT read as portal-grade — the runic site is the real extraction and the Warren must never look like an alternative to it.

**States:**
- Planted / active (digger present)
- **Destroyed** — guards can find and wreck it; it's gone for the raid

**Gameplay hooks:** becomes the respawn point; banks **loot and prisoners only** (deeds still bank at the real stones). The point is that a struggling raid walks away with what it gathered without making real extraction optional.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,prop"

# ---- Epic 5: Hero Buildings
MS="Epic: Hero Buildings"
mki "SM_GS_Bld_Granary01 — village burn objective (hero)" \
"**Highest-priority gap.** \`gen_village\` currently places no granary at all — the core village's burn objective does not exist in the layout.

**Must read:** raw wood, burnable, bursting with grain. Fattest silhouette in town: 60% silo-belly, one banded top hoop (the single hardened tell), thatch cap, grain leaking Pale Straw at the seams.

**Spec:**
- \`SM_GS_Bld_Granary01\`, target **600 diameter x 700 height** + collapse-debris margin
- <=15k tris pre-fracture, watertight-ish with interior faces at fracture shells
- Char mask hottest on **cap and seams**
- Grain-spill dressing as separate meshes; UCX collision

**States:** intact -> burn stage 1 (cap/seams char) -> burn stage 2 (full char + fire) -> **collapsed** (Chaos, matching the already-scaffolded burn-triggered physics collapse).

**Blockout stand-in (this week):** \`Barrel_04\` scaled ~x5 (staves + hoops give the silo-belly and banded read free) + \`HayStack\` as thatch cap + 2–3 \`SackOpen_Grain\` spilled at the base.

**Open ruling (Michael):** round banded silo vs. rectangular barn-like grain store matching Dreamscape architecture. Recommendation: the silo — round + fat is unique on the map and the objective should be unmistakable.

**Schedule: W2 blocker.** The three objective burn types are the W2 milestone.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,building,blocker"

mki "SM_GS_Bld_Watchtower01 — watchtower shell" \
"Also absent from \`gen_village\`. An orientation landmark like the mill, and the home of the game's cleanest stealth counterplay.

**Must read:** raw wood, spindly, **70% legs**, leaning ~2 degrees. The 70%-legs proportion caricature can't be kitbashed convincingly — that's why this is custom.

**Spec:**
- \`SM_GS_Bld_Watchtower01\`, building shell <=8k
- Bell as a **separate mesh** with \`SOCKET_Bell\` (Human Props epic) so Town can animate the ring
- Cushion on the stool — the smug detail

**Blockout stand-in:** 4x \`VillageFencePost\` scaled tall as legs + \`MarketStallStructure\` platform + \`StallCover_0x\` roof + \`Ladder\` + inverted \`Pot\` as bell + \`Basket\` as cushion. **Do not** stand in with \`WIndmill_Base\` — two mill silhouettes over the treetops would corrupt target-reading, since the mill silhouette IS an objective telegraph.

**Open (Town/C&F, not art):** is the tower itself burnable? Raw-wood law says yes; the watchman/bell gameplay may say no. If no, it needs hardened tells added.

**Schedule:** W5, alongside the watchman and patrols.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,building"

mki "SM_GS_Bld_Barracks01 — stone barracks + damage states" \
"Also absent from \`gen_village\`. The unburnable building — the human faction's structural answer to the torch.

**Must read:** **stone.** Heavy River Stone base, level and plumb — the one straight building in a world where nothing else is. Hardened banded wood roof so its non-flammability doesn't lie. Recruitment poster as the smug detail.

**Why it can't be a recolor:** stone's tells are *shape* tells — big rounded blocks, thick mortar recesses, plumb walls — not color tells. No Dreamscape piece has a stone-building read.

**Spec:** \`SM_GS_Bld_Barracks01\`, <=8k + damage states per Combat & Feel's structural-HP spec.

**Blockout stand-in:** \`House_Medium_01\` with a flat stone-grey material override + \`WarriorStatue\` at the door. **This stand-in lies** — a thatch-and-plaster silhouette reading burnable while gameplay says unburnable. Acceptable for layout blockout only; it must not survive into any playtest where fire verbs are live, or it teaches players a false rule.

**Gameplay hooks:** structural HP, \"beat down the barracks\" +40, spawns guards during RAID from the finite 15-pool.

**Schedule:** W4 — RAID-phase spawning is live on the blockout from W2, real mesh before W5 seed reviews.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,building"

# ---- Epic 6: Runic Site
MS="Epic: Runic Site"
mki "Runic monolith set (6–8 stones) + Runeglow carvings" \
"The third art vocabulary, and the only authored-once zone in the game. Spawn point, extraction point, and the place deeds actually bank.

**Must read:** 6–8 monolithic dark stones (Char-dark base), **deep-carved Runeglow channels**, Mossback at the bases, and **dead-level in a world of leaning things.** Stone alone gets to be solid and plumb — that's part of how it reads as ancient and immune.

**Spec:**
- Hero budget <=15k, \`M_GS_Atlas_Emissive\`
- **The portal FX is the saturation ceiling of the entire game — nothing may outshine it.** Author the stones so the glow has room to dominate.
- Collision ring wants real radii — record them for bounds.json even though placement is authored, not generated.

**Schedule:** start W3, land early W4. The portal is the W4 milestone (first full loop).${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,environment"

# ---- Epic 7: Objective States & Destructibles
MS="Epic: Objective States & Destructibles"
mki "Wheat char pass + SM_GS_Fol_WheatStubble01 burnt-stubble mesh" \
"The farmstead burn objective. Fire spreads row to row — the most visible and hardest-to-douse arson in the game.

**Work (AA side):**
- R-channel char-mask vertex-color pass on \`VillageWheat_01\`/\`02\` re-exports (Dreamscape ships **zero** GS vertex colors)
- New \`SM_GS_Fol_WheatStubble01\` swap mesh, **same footprint and pivot** as the DS wheat so the generator's grid holds. Pivot match guaranteed by AA — no new bounds needed.

**ENG owns:** row-to-row spread logic, ignition, and the burning->stubble swap timing (Combat & Feel's spread system).

**Adjacent freebie:** the scarecrow sits next to the field — free fire-spread comedy, keep it.

**Schedule: W2** — one of the three objective burn reads.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,environment,blocker"

mki "Mill char conformance pass on WIndmill_Base" \
"The windmill burn objective — a landmark visible over the treetops and a **two-stage burn**: Stage 1 Ablaze (exterior, has a visible tell and a bark naming what's still owed), then an aimed torch through a window catches the grain dust for Stage 2 Detonation.

**Work (AA side):** char-mask conformance pass on \`WIndmill_Base\`. Verify the DS mill's footing isn't stone-look — if it is, fire read concentrates on body and sails and the footing stays honest stone.

**Sails:** stay on the pack's rotating BP (\`SM_Windmill_Sail_Blueprint\`, scale 0.8, canonical mount offset). They must **keep turning while burning** — that's the shot. Burnt-sail variant only if C&F rules mesh-swap over material-only.

**Open (C&F):** is the end state a material-only standing char shell, or fractured? GDD specifies no collapse.

**Schedule: W2** — third of the three objective burn reads.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,environment"

mki "Market stall smashed state + goods dressing separation" \
"The horde's point-command smash target. W3 is horde week — these must break on screen.

**States:** intact -> **smashed** -> looted-empty (goods gone, structure intact).

**Work (AA side):**
- Fracture-friendly or authored-smashed stall mesh (pending C&F ruling on Chaos vs. authored variant)
- **Goods dressing must be separate lootable meshes** — 4 dressing variants. Looted-empty is interaction gating hiding goods, not a new stall mesh.

**ENG owns:** smash destruction (C&F); looted-empty gating (Stealth & Interaction).

**Gameplay hooks:** market gold 5–15 per stall; the market is also where decision 9 wants faction Burgundy/Gold concentrated.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,prop"

mki "Fence broken variant (matching DS VillageFence)" \
"Livestock pens break, animals escape, chaos ensues. Dozens of instances across the hamlet.

**Spec:** broken-segment variant matching the DS fence at the **same 331 cm segment metrics** so it swaps in place with no bounds change.

**Proposal to C&F:** mesh-swap rather than Chaos — cheap, and there are dozens of instances. (C&F rules.)

**Schedule:** W3, with the rest of the horde's smashables.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,environment"

mki "Pen gate states: open / closed / broken" \
"\`VillageFenceDoor\`. The quiet-vs-loud choice made physical — open it and the livestock wander, break it and they bolt.

**Work (AA side):** broken variant; open/closed variants if the DS asset lacks a hinge setup.

**ENG owns:** open/close interaction (Stealth & Interaction); livestock-escape consequences (Town).

**Conformance note:** if the gate has iron hinges it's hardened wood — metal must sit **on top of** the wood at every seam (playbook S2.1). Spot-check on import.

**Schedule:** W3.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,environment"

mki "SM_GS_Prp_WellWater_Fouled — well fouled insert" \
"Fouling the well disables the bucket brigade (+30) — physical, readable counterplay against the town's fire response.

**Must read:** green-tinged **desaturated** water. **NOT Runeglow** — nothing may compete with the portal's saturation, and a glowing well would read as magic rather than sabotage.

**Spec:** \`SM_GS_Prp_WellWater_Fouled\` — an **insert mesh sharing the well's pivot** (per the \`_Fouled\` convention). Do not duplicate the DS well.

**ENG owns:** foul interaction gate (S&I), flies FX (C&F/Niagara), brigade disable (Town).

**Open (Town/C&F):** does \`WellRoof\` burn? If it does, fouling and brigade sourcing must survive the well going roofless. If it can't burn, it needs hardened tells or removal — right now it's a flagged liar.

**Schedule:** W5, with the stealth five.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,prop"

# ---- Epic 8: Conformance & Dressing Sweep
MS="Epic: Conformance & Dressing Sweep"
mki "Char-mask conformance re-exports (roofs, barn, hay, scarecrow)" \
"Dreamscape assets ship with **no GS vertex colors**. Every flammable needs an R-channel char-mask re-export pass, or C&F provides a maskless material fallback.

**Priority order:** wheat (done separately, W2) -> haystacks -> granary-adjacent hay -> house/barn thatch roofs -> \`Hay_Scattered_01\` -> scarecrow.

**Authoring rule:** R channel gradient — high on thatch tufts, plank edges, roof lines; low on cores. Flammables only; leave 0 elsewhere.

**Also verify:** the barn MUST be flammable, and the mill must be (it's an objective).

**Scope depends on:** C&F's ruling on whether the burn path requires the mask on Dreamscape materials or ships a maskless fallback. That one answer decides the size of this whole sweep.

**Schedule:** W6.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,polish"

mki "Stall-cover recolors to Pennybrook Burgundy / Livery Gold" \
"Hamlet pivot decision 9 wants faction color concentrated at the market. DS \`StallCover_01\`–\`03\` awnings are burnable cloth (fine) but generic DS colors.

**Work:** material-instance recolor to Pennybrook Burgundy / Livery Gold. No new geometry.

**Open ruling (Michael):** approve the recolor? This is aesthetic, not structural — but it's the cheapest way to make the market read as the faction's proud commercial heart before it gets smashed.

**Schedule:** W6.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,polish"

mki "Hardened-seam spot fixes (metal-on-wood)" \
"Playbook S2.1 conformance: on hardened wood, metal must sit **on top of** the wood at every seam. Anything else reads as decoration rather than reinforcement, and the material-hardness law stops teaching.

**Check list:** \`CrateHandles\` (metal fittings), \`VillageFenceDoor\` (iron hinges, if present), and anything else flagged during import spot-checks.

**Related buildability rule (style guide pillar 6):** a hoop running parallel to the boards it sits beside isn't holding anything — it needs to cross them. This is a mandatory pass separate from silhouette and material class: silhouette asks *does it read*, this asks *could a carpenter or blacksmith actually have built this*.

**Schedule:** W6.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,polish"

mki "Courier sack + loot-pile dressing" \
"The loot economy made visible. Couriers carry goods home on their backs; the stones and the Warren accumulate what's banked.

**Work:**
- Courier sack (carryable, pivot at grab point) — reads as *full of somebody else's stuff*
- Loot-pile dressing at banking sites, so a successful raid visibly accumulates

**Why it matters:** loot banks instantly and permanently while deeds only bank on exit. The pile is the player's running proof that the wipe-proof half of their score is real.

**Adjacent:** \`SackOpen_Grain\` is imported but unused — suggest adding it to the windmill goods pool for the grain read.

**Schedule:** W6.${ACCEPT_COMMON}${REF_FOOT}" \
"$MS" "art,prop,polish"

echo
echo "==> Done. Review at: https://github.com/$REPO/issues"
echo "    Milestones:      https://github.com/$REPO/milestones"
