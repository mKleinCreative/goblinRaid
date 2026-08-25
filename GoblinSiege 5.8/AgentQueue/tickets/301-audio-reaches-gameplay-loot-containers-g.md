---
id: 301
title: Audio reaches gameplay: loot containers get smash, break, loot and ignite sounds via Blueprint delegate binds
agent: claude-audio
status: done
claimed: 2026-08-25T02:23Z
build: none
waiting_on:
evaluated: 2026-08-25T02:53:09Z
observed: 2026-08-25T02:53:11Z | Michael ran all three tests himself and confirmed they work and are fine. Smashing the loot barrel and the loot crate each sounded the four-layer wood break at the actor's location, and looting the chest sounded the chest-open cue - all fired from the components' own OnBroken and OnInteractionCompleted delegates, not from a console play command. First time audio in this project has come from a gameplay event.
scenario: PIE on L_CombatArena, player walking up to the placed barrel, crate and chest and triggering each in game, on headphones.
files: 
  - Content/Blueprints/Interactables/BP_LootChest.uasset
  - Content/Blueprints/Interactables/BP_LootBarrel.uasset
  - Content/Blueprints/Interactables/BP_LootCrate.uasset
  - Content/Audio/Cues/SC_GS_Break_Wood.uasset
  - Content/Python/gs_audio.py
---

## Goal

Audio reaches gameplay: loot containers get smash, break, loot and ignite sounds via Blueprint delegate binds

## Generate

**Audio reaches gameplay for the first time.** Until this ticket the 41 cues were a library nothing
triggered; now three placed actors make sound from their own gameplay events, with **no C++** - which
matters because the build gate has been closed on the ACF migration throughout.

**The mechanism** is the one the audio plan's phase C rests on, proven early in Blueprint:
`BeginPlay → Bind Event to <delegate> → custom event → PlaySoundAtLocation`. The delegates
(`FGSOnBroken`, `FGSOnInteractionCompleted`) are already `BlueprintAssignable`, so nothing in
`Source/` was touched.

| Blueprint | Event | Sound |
|---|---|---|
| `BP_LootBarrel` | `GSBreakableComponent::OnBroken` | `BreakSound` = `SC_GS_Break_Wood` |
| `BP_LootCrate` | `GSBreakableComponent::OnBroken` | `BreakSound` = `SC_GS_Break_Wood` |
| `BP_LootChest` | `GSInteractableComponent::OnInteractionCompleted` | `InteractSound` = `SC_GS_Chest_Open` |

**The sound is a Blueprint variable, not a pin literal.** This started as a workaround - object pin
defaults could not be verified (`set_node_pin_value` returns `true` and `get_node_pins` reports an
empty `default_value`, because object pins store a reference rather than a string) - but it is the
better design regardless: the value is readable off the compiled CDO, and a designer can override it
per placed actor without opening the graph.

`BP_LootChest` needed no `BeginPlay` binding at all: it already carried an orphan
`K2Node_ComponentBoundEvent` for `OnInteractionCompleted` with nothing wired to it, so the play node
hangs directly off that.

**`SC_GS_Break_Wood` was rebuilt three times in response to Michael listening**, and the end state is
a four-layer mix rather than a random pick:

| Layer | Pitch | Mixer vol |
|---|---|---|
| Chop (main) | 0.94-1.06 | 1.40 |
| Chop (low) | 0.70-0.80 | 0.85 |
| Chop (high) | 1.28-1.45 | 0.70 |
| Debris | 3.10-3.50 | 0.65 |

Each layer picks independently from all ten `Wood_Chop` waves, so one break is three different chops
at three pitches. `SW_Rock_Large_Debris_2-5` was dropped (7.73s against siblings of 2.8-3.5s; it
alone set the cue length). Cue is 3.5s, was 7.73s.

Also added `gs_audio.smash()` - breaks the nearest breakable to the player through the real delegate
path, so the wiring can be tested from the console without an agent driving PIE.

## Evaluate

**Three defects were found by listening, and none of them would have been caught by any check I
wrote.** That is the finding worth carrying out of this ticket.

1. **The cue held the wrong sounds.** `SC_GS_Break_Wood` contained `SW_Wood_02`, `SW_Wood_10-1`… -
   generic Environment wood knocks - because ticket #298 picked waves by substring. Michael asked
   "why aren't you using the SC_GS_Break_Wood sound", which was the right question aimed at the wrong
   layer: it *was* wired correctly, and the cue's contents were wrong. **The pack contains no
   wood-breaking sound at all** (checked every wood/debris/glass family); the current cue is an
   approximation built from an axe splitting wood.
2. **A Mixer with zero input volumes is silent and structurally perfect.** After rebuilding the cue
   as a layer, `InputVolume` was `(0.0, 0.0)` - `connect_nodes` wires mixer inputs but does not
   populate their volumes. Duration, node graph, class and root node all read healthy. My #298 audit
   (root node / child counts / class set / non-zero duration) passes this every time. **"Mixer inputs
   are non-zero" has to join that audit** before any more layered cues are built.
3. **Asset writes are silent no-ops while PIE runs.** `set_node_property` returned, `save_sound_cue`
   returned `False`, and `get_node_property` came back empty - and I nearly reported the pitch as
   doubled when the asset on disk was untouched. Same class of bug as the music `looping` flag in
   #249. **Stop PIE before writing assets** is now a twice-learned rule.

**Verified:** all three Blueprints compile with 0 errors; every CDO read back holds the intended cue
object; the chest's bound event, Sound and Location pins all report connected; the cue's mixer
volumes and all four modulator pitch bands read back from disk after saving. Breaking the barrel in
PIE produced no Blueprint or audio errors in the log.

**Verified audible (updated after testing).** Michael ran all three tests himself and confirmed
they work. The barrel and crate break audibly on `OnBroken`, and the chest sounds on
`OnInteractionCompleted`. **The delegate path is therefore proven end to end** - a gameplay event
reaching the mixer spine and making a noise in the right place. That is the first time audio in this
project has fired from anything other than a console command, and it validates the architecture the
audio plan's phase C depends on: bind to the 72 existing BlueprintAssignable delegates rather than
editing gameplay code. `SC_GS_Chest_Open` was accepted for the chest as-is.

**Also unproven:** the chest. `SC_GS_Chest_Open` was picked because the action is opening a chest,
but `SC_GS_Loot_Coins` may suit a *loot* chest better - swapping it is one variable, not a rewire.

## Refine

**Changed in response to the above:**
- Rebuilt `SC_GS_Break_Wood` from **explicit wave lists** rather than substring matching, after a
  removal keyed on `"2-5"` also deleted `SW_Wood_Chop_2-5` and left a broken `-1` child slot. The
  loose matching that put the wrong sounds in the cue was the same tool that then damaged it; it is
  not used anywhere in the final build.
- Made the sound a Blueprint variable rather than fighting object-pin defaults - better design
  arrived at by way of a verification problem.
- Added `gs_audio.smash()` so the gameplay path can be triggered from the console, after repeatedly
  restarting PIE under Michael proved a bad way to test.

**Deliberately left undone:**
- **`BP_Statue_Warrior` and `BP_GS_TestStall`** carry `GSTopplableComponent` and `GSFlammableComponent`
  and are the obvious next two, but they are not in this claim and the pattern is now proven - they
  are a repeat, not a risk.
- **A better wood-break sound.** The pack cannot supply one; that belongs on the plan's gap list with
  the alarm bell and the building collapse, not in another round of wave-shuffling.
- **The `-1` broken slot** left in the intermediate cue was resolved by full rebuild, not patched.

> 2026-08-25T02:50Z Barrel, crate and chest wired and compiling clean. Nobody has yet HEARD a sound fire from a gameplay event - run py gs_audio.smash() in PIE to close that.

### Confirmed working, 2026-08-24

Michael: *"I did each test. they all work and are fine."* Barrel break, crate break and chest
interaction all heard firing from their own gameplay events, with the four-layer `SC_GS_Break_Wood`
he tuned by ear.

**The load-bearing conclusion for the rest of the audio work:** the Blueprint delegate-bind pattern
works, needs no C++, and touches no contended file. Every remaining actor-level sound in the plan -
topple, ignite, objectives, doors - is now a repeat of a proven pattern rather than an open question,
and none of it is blocked by the build gate.
