# Goblin Siege — decisions ledger

**Created 2026-08-19 (#198).** Until today, both design documents pointed at a "§13 decisions ledger"
that **did not exist in either of them**. The rulings were real but scattered — through
`AGENT_STATE.md`'s DECISIONS section (~470 lines), ticket bodies, and chat — which is precisely how
settled questions kept getting re-opened by the next session. This file is the single numbered,
dated, citable list.

## How to use this file

- **A "decided" entry is frozen.** Do not re-litigate it, re-design around it, or "fix" it back. If
  it is genuinely wrong, that is a new ticket and a new dated entry that says which one it supersedes
  — never a silent edit.
- **Every entry carries its source**: a ticket id, an `AGENT_STATE.md` section, or a dated ruling from
  Michael. An entry with no source is not a ruling, it is a recollection.
- **Adding an entry needs a ticket.** Same rule as the GDD (`docs/goblin-siege-gdd.md`, change rule).
- Entries are grouped by date, newest first. Numbering within a group is stable once written.

---

## 2026-08-31 — the horde order wheel is whole

Michael, confirming live play after the overnight packaging session (#387-389), that all four
order-wheel verbs work — not just Attack and Follow, which is what GDD v1.4 still claimed.

| # | Ruling | Consequence |
|---|---|---|
| 73 | **Hold and Loot both land in `BT_HordeGoblin`.** `BTTask_PickUpCargo` and `BTTask_DeliverCargo` now sit in the tree rather than compiling unused — the hand-authored Behaviour Tree edit ticket #213 specified and could not do from Python (a `UBehaviorTree` with an EdGraph regenerates over any Python-injected node the next time the asset is opened) has been done. `BB_HordeGoblin` and `BT_HordeGoblin.uasset` carry the change, uncommitted as of this ruling | GDD §5 and §12.1 rows 6 and 17 re-graded: Horn & horde moves WIRED → BUILT, Loot couriers moves "WIRED, no cargo" → WIRED. **Smash is not part of this** — `BTTask_SmashOrderTarget` was never a wheel command (the wheel is Attack/Hold/Loot/Follow, #141) and #213 never scoped it in, so it stays uncalled |

**Source:** Michael, directly, in session — not a queue ticket's own observed-evidence report. Recorded
under #394, which claims and edits the two design documents per the change rule; the underlying
`BT_HordeGoblin`/`BB_HordeGoblin` edit itself has no ticket of its own (done by hand, outside the
queue, per #213's own conclusion that this specific edit could not safely go through an agent).

---

## 2026-08-27 — the game gets a front end, and it is ACF's

Michael, asked how a main menu and the options screens should be built (#335). Three rulings, and
the first of them reverses an answer this session had already given.

| # | Ruling | Consequence |
|---|---|---|
| 70 | **The project gets a front end** — title screen, pause menu, settings (Video · Audio · Gameplay/Accessibility · Controls) and credits, on a dedicated menu level. Added to GDD §12.4's IN column | Closes the gap `AGENT_STATE.md:875` recorded and never fixed: *"a finished raid produced one log line and no screen, no pause, no restart."* It also moves `GameDefaultMap` off the `L_CombatArena` test map, which GDD §12.2 block D already required |
| 71 | **It is built on ACF's UI stack** — `AscentUITools` + `AscentUINavigationSystem` on CommonUI — **not** hand-rolled UMG. Menus are spawned by `UI.Widget.*` tag through `UANSUIPlayerSubsystem`, never by widget class | ACF ships the main menu, pause menu, tabbed settings shell, all four settings pages, **a complete key-rebinding screen**, and the whole styled base-widget kit (button/slider/checkbox/spinner/combo/popup). Extends ruling 2026-08-12 (ACF adoption) to the last hand-rolled island in the project. **Key rebinding goes back into scope on the strength of it** — it was cut on 2026-08-26 as the most expensive screen to build, and it turns out to be a tab to switch on |
| 72 | **There is no save game. A raid is atomic: you extract, or you fail and earn nothing** | No Continue, no save slots, no load flow — so ACF's `AscentSaveSystem` is linked but unused (it must stay linked: `AACFCharacter` implements `IALSSavableInterface` and our derived class needs the thunks — `GoblinSiege.Build.cs:69-77`). `UGSSaveGame` is unaffected and keeps doing the one thing it does: personal bests across sessions. Settings persist via `UPROPERTY(config)`, which is not a save game |

**What ruling 71 costs, recorded before it bites.** ACF ships its plugin config in its own
`Config/DefaultPlugins.ini`, and **those values do not reach a project's config hierarchy.** This
project has paid for that once already: #228 found that an unreplicated
`[/Script/AscentGASRuntime.ACFGASDeveloperSettings]` block left `HealthAttribute` empty, which meant
`UACFGASStatisticsComponent::BindHealthDelegate` bound to nothing and **nothing in the game could
die**. The UI blocks are still missing and fail the same way — silently, and nothing like their
cause. `WidgetRegistryAsset` unset makes every `SpawnWidgetByTag` return null;
`GameUserSettingsClassName` unset makes every settings widget's cast fail so the audio and gameplay
tabs do nothing at all. **Replicating that config is stage 1 of the work and is verified by reading
the CDOs back at runtime, not by reading the ini.**

**Two ACF defects found while planning this, recorded so nobody rediscovers them.**

1. **No widget-registry asset ships anywhere.** The `ui-navigation` skill says to duplicate
   `DA_ACFWidgetRegistry` from `/Game/FullSample/`, but FullSample is not installed here, and a
   content grep for `ANSUIWidgetRegistryDataAsset` across the entire plugin returns zero instances.
   The registry must be **authored from scratch**; ACF's shipped ini value is a dangling reference.
2. **`UAUTUIFunctionLibrary::SetSoundClassVolume` writes into the `USoundClass` asset** —
   `TargetClass->Properties.Volume = NewVolume;`, no sound mix, no override — so ACF's audio sliders
   dirty the sound assets in the editor and cannot be layered or reverted. **We keep ACF's widgets
   and replace its application**, pushing volumes through `SetSoundMixClassOverride` on
   `SMix_GS_Base`, which #249 built for exactly this. Note also that `AudioVolumeLevels` is a bare
   `TArray<float>` indexed **positionally** against `AUTDeveloperSettings.DefaultSoundClasses` — so
   the order of that ini array is a load-bearing contract and reordering it silently reassigns every
   saved volume.

**What this does NOT decide.** Gamepad navigation is keyboard/mouse-first for now — ACF's stack makes
that a later switch rather than a rewrite. The raid HUD (`WBP_GSPlayerHUD`, 2,089 lines) is **docked
onto `UI.Layer.HUD` unchanged**, not re-authored onto `UAUT*` widgets; that restyle is a later,
optional pass. And ACF's `ANS_RadialMenu` / `UAUTRadialMenu` are noted as prior art for the two
hand-built wheels (#039–#041) — icebox, not this work.

---

## 2026-08-24 — atrocity corrupts faster than battle (world corruption, cont.)

Michael, asked whether a civilian death should turn the land as much as a soldier's now that
civilians actually exist (#275): **civilians count MORE.** Ticket #296. Continues the block of
2026-08-21 (rulings 40–45) and closes the one question it deliberately left open.

| # | Ruling | Consequence |
|---|---|---|
| 62 | **A civilian kill corrupts the world MORE than an armed defender's.** Killing the hamlet's people turns the land faster than beating its defence does | This is the first thing in the game that makes an **ugly choice a real one**. Under ruling 59 it is squarely primary-player experience — the sky is the feedback, and it rewards the player for something the fiction should make them feel. It also finally gives `GSCharacterBase.cpp:258` the *"real case"* its own comment has been asking for since rulings 13/14: the not-a-goblin-is-a-human test now has a second consumer that must tell a peasant from a militiaman, and the archetype or a `Civilian` tag is where that comes from — **not** a class-name string match |

**What this does NOT decide.** The multiplier itself is a feel number, not a ruling — it starts as a
cvar (`GS.Corruption.CivilianWeight`) and graduates onto `DA_Corruption_Default` once it is settled,
per the project's tuning tiers. And it says nothing about **hostility**: the same comment at
`GSCharacterBase.cpp:258` notes civilians are *"human by race but should not be hostile to anyone"*,
which is a separate live defect this ruling neither fixes nor worsens.

**A sizing consequence that follows from the 2026-08-23 roster ruling.** The kill term's soft knee
was drafted at 12, sized against the finite 15-defender pool of ruling 19. The castle guards are
Militia and *"there's just a decent amount of them"* — so the real body count in a full raid is
higher than 15, and a knee of 12 saturates too early. The knee is re-sized when the term is built,
against a counted roster rather than a remembered one.

---

## 2026-08-25 — goblins hate water

Michael, deciding the map edge after four failed attempts to trace a coastline: *"let's have anything
further than waist deep will drown you, We can also make it cannon that Goblins notoriously hate
water."* Ticket #308.

| # | Ruling | Consequence |
|---|--------|-------------|
| 67 | **Water deeper than the waist drowns you.** Goblins notoriously hate water - it is canon, not a limitation | **This replaces the coast wall entirely.** The playable area is closed by forest on the land side and by drowning on the water side, so no invisible barrier is needed along a shoreline nobody could trace. A rule the player can discover and explain beats a wall they walk into |
| 68 | **Swimming is NOT a verb in this game** | #054 built `AGSWaterVolume`, `bCanSwim`, `MaxSwimSpeed` and a drowning path on the say-so of a pre-v1.0 GDD draft; the canonical GDD (v1.0, #198) contains **no mention of water, swimming, drowning, shore or boundary at all**. There is no swim animation on the goblin skeleton - not one clip - so swimming would ship as the run animation sliding across the sea. Ruling 67 makes that irrelevant rather than a debt |
| 69 | **The sea stops being a floor** | `Plane2` - the 11 km water plane - has blocked pawns since 2026-08-06, so the ocean was WALKABLE for 5.5 km in every direction. That was the actual hole in the map, and no coast wall would have closed it while the fix was one collision response |

**Waist deep is not a hand-tuned number.** A character's physics volume is chosen by its capsule
CENTRE, so the water volume engages exactly when the goblin is more than half submerged. The rule and
the implementation are the same fact.

**What this does NOT decide:** whether drowning is the right punishment long-term, or whether it should
warn first. The drain is 25/s against a 100 pool - about four seconds - and lives on the placed volume,
so it is one field to change.

## 2026-08-25 — the objective set, and what the portal waits for

Michael, defining the raid's win condition in his own words: *"The objective we need to complete, is to
burn down a percentage of houses, the market stalls, the Statue and the field. Everything else is
optional, but the prompt to leave doesn't come back unless you've completed those missions, or you've
ran out of lives."* Ticket #305.

| # | Ruling | Consequence |
|---|--------|-------------|
| 63 | **The required set is: a PERCENTAGE of houses, the market stalls, the statue, and the field. Everything else is optional** | The mill was placed, tagged and required, so it would have held the portal shut after everything Michael asked for was already done. It is now Optional: it still lists, still burns, still scores, and never gates |
| 64 | **A type completes at a FRACTION of its carriers, not at the first one. Supersedes ruling Q-32 (2026-07-31)** | Q-32 said the first carrier of a type to burn demotes every sibling to Optional. That was right when a type meant "the mill" - one building, one objective - and wrong for 67 houses, where it meant a single cottage satisfied them all. The demotion pass now fires when the threshold is met. Types with no fraction still need one carrier, so the market and the field are unchanged |
| 65 | **A percentage rather than a flat count, because the count would visibly lie** | Michael, having played it: *"some of the village houses count as multiple pieces. it's strange... which is why I don't want a firm number, because it'd look strange."* Two village houses are each split into two objectives, one per storey - residue of the multi-storey split `InteriorNameFilters` was written to fix. "Burn 8 houses" is a number the player can watch be wrong; a fraction absorbs it |
| 66 | **The statue is a required objective, satisfied by TOPPLING** | It counted for nothing: the raid director only ever swept `AGSBurnObjectiveBase`, and `AGSObjective_ToppleStatue` hard-casts to `AGSDestructibleObjective`, which completes by **burning** - so its total stayed 0 and its condition was true on the first broadcast. Monuments are now swept separately and satisfy their type through `OnToppled` |

**The house fraction is 0.4**, calibrated against the fire spread landed in #304: one well-placed torch
completes 23 of the island's 67 houses, so 40% (27) needs a second fire or the outliers. It rewards
choosing where to light rather than counting doors, and it is one number to change.

**What this does NOT decide:** how the player is told. "The prompt to leave doesn't come back" implies
a prompt; nothing renders one yet.

## 2026-08-24 — Uriel is replaced by a Knight

Michael, clearing the last of the ACF migration backlog: *"just replace him with a knight. Don't worry
about the boss in the demo. We NEED to get moving on the actual demo."* Ticket #294.

| # | Ruling | Consequence |
|---|--------|-------------|
| 60 | **Uriel A Plotexia is replaced by a Knight, and there is no bespoke boss character in the demo** | **Supersedes ruling 55**, which parked him in demo scope and told everyone to leave him alone. That was the right call while the demo was undefined; ruling 58 has since defined it as a straight raid - horn, horde, burn, bank, extract - which has no boss in it. `BP_KnightDPelegrini` already exists, is armoured, is on ACF as of #292 and was signed off in play. A second armoured human would be work with nothing behind it |
| 61 | **`BP_UrielAPlotexia` is deleted rather than left dormant** | He was placed in **no level**, referenced by **no asset**, and spawned from **no code** - and he carried a live defect: **no `CharacterInitDataAsset` at all**, the same fault #275 found on the peasant, so his ACF stats would never have initialised. A dormant asset that is broken in a way nobody can see is a trap for whoever finds it next and assumes it works. Recoverable from git if he is ever wanted back |

**What this does NOT decide:** that the demo has no climactic encounter. It decides there is no
*bespoke character* for one. A Knight can be a boss.

## 2026-08-24 — the primary player experience is the bar

Michael, closing #287 after finding a second axe on the scout's back and choosing not to chase it:
*"It doesn't bother the primary player and that's how we want to judge things from now on. Primary
player experience."* Ticket #288.

| # | Ruling | Consequence |
|---|---|---|
| 59 | **A defect is judged by what the PRIMARY PLAYER experiences.** If it does not reach the person holding the controller, it is not urgent - whatever it looks like in an outliner, a log or a details panel | This is a **triage rule, not a quality rule**. It does not license shipping things that are wrong; it decides what gets fixed *now*. A stray mesh nobody can see, a warning nobody reads and a value that is untidy but never observed are all below the line. A camera that lurches when you swing is above it |

**Why this needed saying, and why now.** #287 produced both kinds in one session. The swinging axe
blocking the camera probe was **above** the line - it happened to the player, every swing, and it was
fixed immediately. The spare axe on the scout's back is **below** it: visible from a debug angle,
invisible to the person playing. The old instinct was to treat both as defects because both are
"wrong"; ruling 59 says only the first is a defect *today*.

**How this interacts with the project's other rules, because it could be misread as loosening them.**

- **It does not weaken "somebody must have WATCHED it run".** The opposite: the observation gate asks
  what the thing *did*, and 59 says whose experience decides whether what it did matters. A ticket
  still cannot close on a compile.
- **It does not license silent breakage.** Ruling 53's warning stands - a migration that changes how
  weapons feel has failed even if it equips - precisely *because* feel is primary-player experience.
- **It does not make the below-the-line items disappear.** They are recorded, not ignored. The
  difference between "accepted, with a reason" and "unnoticed" is the whole value of writing it down.

**Recorded below the line as of today, and NOT to be fixed in passing:** the scout carries a visible
second axe on his back while the primary weapon is drawn. Cause not investigated - Michael judged it
not worth the time, which under 59 is the correct call. If it ever reaches the player - reflected in a
cutscene, seen over the shoulder in a tighter camera - that is when it becomes work.

---

## 2026-08-24 — sneaking comes out of the demo

Michael, on being shown that the bucket brigade was deferred out of the slice as a consequence of
ruling 16: *"We might have to skip the sneaking portion of the demo all together."* Confirmed after
seeing what it would cost. Ticket #285.

**This reverses a "Never cut" line, which is the strongest commitment the GDD makes**, so it is
recorded loudly rather than edited quietly.

| # | Ruling | Consequence |
|---|---|---|
| 56 | **The crouch-and-confirm stealth core is CUT from the demo.** With it go noise, takedowns, corpse-suspicion and the coin toss - the whole sneaking portion | **Supersedes the §12.4 "Never cut" line**, which read *"the horn/horde · the three-objective destruction structure · the crouch-and-confirm stealth core · the runic-site banking loop · the score screen"*. The stealth core is removed from that list; the other four stand untouched |
| 57 | **The bucket brigade stays DEFERRED** | It was already deferred as a stated consequence of ruling 16 - *"deferring the watchtower, bell and well removes three scoring lines from §10 and the bucket-brigade counterplay with them"*. Ruling 56 strengthens that rather than changing it: the brigade is counterplay to fire, and **fouling the well is the counter to the brigade**. Without the well, and now without the stealth verbs that make fouling interesting, the brigade would be a mechanic with no answer |
| 58 | **The demo is a straight raid**: horn, horde, burn, bank, extract | What is left after 56 is exactly the part that already works and the part every other "Never cut" item protects |

**Why this costs less than it looks.** §12.1 row 7 grades the stealth five as *"SPLIT - two built,
three absent"*: the ~1.5s confirm and crouch detection are real and correct in `Stealth/`, but **the
perception component sits on ZERO actors** - so like the interact framework before it, the stealth
core exists in full and has **never executed**. Noise, takedown, corpse-suspicion and the coin toss
have no code at all.

So the sunk cost is two systems that have never run, and the saving is three-to-four systems that do
not exist - including noise, which ruling 9 had made *"the next major item"*. **The stealth core is
the only "Never cut" item that has never run on a single actor**, and that asymmetry is the argument.

**This follows the GDD's own cut order to its end rather than inventing one.** §12.4 already carried
*"Cut order if late blocks slip: self-looting civilians -> sheep & chickens -> coin toss ->
corpse-suspicion -> takedowns"*. Ruling 56 goes one step past the end of that list. The document
anticipated shedding stealth piece by piece under pressure; this takes the last step.

**Not decided here.** Whether stealth returns for the full game. It is cut from the **demo**, in the
same sense ruling 55 put Uriel there - a scope statement about this build, not a deletion of the
design. The `Stealth/` code is not deleted and no ticket should delete it.

---

## 2026-08-24 — the wheel goes to ACF, and Uriel goes to the demo

Michael, asked to pick up the two things left open at the end of the finite-arrows work:
*"Let's move the weapon wheel to ACF and also the bucket brigade. Uriel in general will be with the
public facing demo, not the prototype."* Ticket #283.

| # | Ruling | Consequence |
|---|---|---|
| 53 | **The weapon wheel DOES migrate onto ACF equipment.** Each wheel slot becomes an ACF equipment slot and switching becomes `UACFEquipmentComponent::UseEquippedItemBySlot(tag)` | **This answers #274's open question**, which was deliberately left standing: *"the question worth answering is not tags or enum, it is - is the weapon wheel migrating onto ACF at all?"* It is. That makes #274's tag swap a stepping stone rather than churn, retroactively justifying it, and it means the two overlapping systems #269 recorded stop overlapping |
| 54 | **Weapons become items, and therefore lootable in principle** | Follows from 53 rather than being chosen separately: once a wheel slot is an ACF equipment slot, what fills it is a `UACFWeapon` item in an inventory. Whether the player actually strips a sword off a corpse is a **separate** design question and is NOT decided here - #270's own note that "nothing draws the axe" is still an open design question, not a bug |
| 55 | **Uriel A Plotexia is DEMO scope, not prototype scope** | Supersedes the 2026-08-23 note that recorded his armour as "FUTURE, not done" without saying which future. He is not part of the tutorial slice at all, so `BP_UrielAPlotexia` staying archetype `Militia` with an arming sword is **correct for now** and is not a defect anyone should fix in passing |

**The bucket brigade needs no ruling and is not one.** Civilians are already IN the §12.4 scope freeze
(ruling 13), and `AGENT_STATE.md`'s NEXT list already carries *"Civilians + livestock (routines,
disbelief, brigade, flee)"* as blocked on `BT_Civilian`. #275 built the civilian archetype and the
bucket prop, which is what unblocked it. The brigade is implementation of something already in scope.

**What ruling 53 does NOT authorise, recorded because the temptation is real.** The migration is a
refactor of how weapons are held, not a licence to change how they FEEL. `UGSWeaponComponent` carries
project-policy visual rules with no ACF equivalent - the holstered melee weapon hidden for the whole
time the bow is out, the quiver that never moves and is never hidden, the Torch slot outranking an
ability's request to un-ready - and every one of those is a watched decision, not plumbing. Ruling 51
already refused ACF's shooting component on the same grounds. **A migration that silently changes
weapon placement or swap feel has failed even if it compiles and equips.**

---

## 2026-08-24 — the quiver empties (finite arrows)

Michael, asked whether picking up torches and arrows as consumables would be part of moving the
weapon wheel onto ACF. He first asked for full scarcity on both, then narrowed it on being shown the
standing objections below. Ticket #277.

This lands against §12.4's scope freeze — items, inventory and consumable resources appear in **no
column**, not IN, not CUT, not DEFERRED — so it needs a ruling before a line of code, exactly as
world corruption did (ruling 40).

| # | Ruling | Consequence |
|---|---|---|
| 46 | **Arrows become finite, for the player.** A starting quiver, spent per shot. The bow becomes a resource verb | The counting backend **already exists and is empty**: `AGSCharacterBase` derives `AACFCharacter`, whose constructor creates a `UACFEquipmentComponent` extending `UACFInventoryComponent`, so every character already carries a replicated, stacking, weight-aware inventory that nothing reads. This is authoring one item and reading a count, not integrating an inventory system |
| 47 | **Torches stay infinite and untouched** | Torch-spam already has its answer in score weighting - a house is priced at 10 *precisely* so the tally is not "decided by whoever had spare torches" (ruling 21). `UGSGA_TorchToss` and `AGSTorchProjectile` are out of scope entirely, and the spawn seam at `GSGA_TorchToss.cpp:159` is **not** a gate. Recorded so nobody adds torch scarcity later thinking it was merely deferred |
| 48 | **AI archers do not run out** | The gate keys on the presence of `UGSBowTimingComponent` **and** a non-null `ArrowItemClass` on the equipped weapon - both off for Erika. Deliberately NOT `IsPlayerControlled()`: the moment anyone possesses an archer for a debug session that test flips and the archer stops shooting. Component presence is a fact about how the pawn was built. No behaviour-tree work, no empty-quiver AI state |
| 49 | **Spent arrows are litter and are not recovered** | `SetLifeSpan(5.f)` at `GSArrowProjectile.cpp:365` stands, and its comment ("an arrow is just litter") is **promoted from a code comment to a ruling** - reversing it now needs a new row rather than an edit. This is also what removes the main argument for ACF's shooting component; see 51 |
| 50 | **Resupply is walk-over pickup, from the ground and from corpses** | Never through `UGSCarryComponent` (one-object-in-hands, applies `State.Carrying`, blocks bow and torch - a quiver you cannot fight while holding is not a quiver), and never through `UGSInteractionComponent`'s hold-to-channel, which **aborts on damage** and is therefore exactly wrong for restocking mid-fight |
| 51 | **ACF supplies the inventory store only. `UACFShootingComponent` is REJECTED** | Same shape as ruling 44. With spent-arrow recovery (49), reload and magazine all out of scope, its one remaining free win - replicated ammo state for the HUD - is **already emitted** by `UACFInventoryComponent`'s FastArray callbacks. Adopting it would replace `AGSArrowProjectile` and delete the derived arrow-mesh offset (`GSArrowProjectile.cpp:76-84`), the distance-to-head-bone headshot (`:183-218`), the bow timing minigame's draw-quality read (`GSGA_BowShot.cpp:186-195`) and Erika's `BTTask_RangedAttack` aim path. The whole tuned ranged stack, for nothing this scope wants |
| 52 | **An arrow that sticks in an ally is still a wasted shot** - the 2026-08-06 ruling is NOT re-opened | But finite arrows make it materially harsher, and that is recorded rather than discovered. See below |

**Two findings recorded so they are not re-investigated from scratch** (precedent: ruling 43):

- **The equipment component already exists on every character, and is empty.** Nothing had to be added to any character class to get an inventory.
- **`AACFPickup` does NOT work standalone.** Its `PickUpCapsule` is created in the constructor and never attached to the root and never bound to an overlap delegate; `bPickOnOverlap` is read only inside `OnInteractableRegisteredByPawn`, whose sole caller in the plugin is **ACF's own** `UACFInteractionComponent`, which we do not have. A `BP_ACFPickup` dropped in a level does nothing and **logs nothing**. This is why ruling 50 is served by a small actor of ours rather than by inheriting ACF's.

**Three objections were put to Michael before he answered, and they are why the scope narrowed.**
Recorded so nobody re-discovers them and re-opens this:

- **Torch-spam already has a settled answer, and it is scoring, not scarcity.** §10 prices a house at
  10 *precisely* so the tally is not *"decided by whoever had spare torches"* (ruling 21). Finite
  torches would re-solve a solved problem, so **torches were dropped from the scope entirely.**
- **Bow-spam already has a settled answer too** — fire rate, #034, `RangedAttackCooldownSeconds`.
  Finite arrows therefore have to earn their place as *their own feature* — running dry as a real
  state you play around — and not as a second fix for a problem that is already fixed.
- **The wood economy, this project's only resource-economy precedent, is DEFERRED to tier-2**
  (§12.1 rows 9-10). An arrow supply is structurally the same kind of system. Michael took it anyway,
  which is his call; it is recorded here so the inconsistency is deliberate rather than unnoticed.

**The design interaction that matters, and it is not a new ruling.** The 2026-08-06 ranged ruling
stands untouched: *an arrow STICKS in an ally, deals nothing, and the shot is wasted.* With unlimited
arrows that cost a player a miss. **With a finite quiver it costs a consumable**, so a horde goblin
wandering into your line stops being an annoyance and becomes a material loss. That ruling was taken
with the horde case explicitly on the table and is **not** re-opened here — but it is now sharper than
when it was decided, and it should be watched in play rather than discovered.

**Deliberately not decided.** Whether the weapon wheel migrates onto ACF equipment at all (#274's open
question) is untouched by this. Arrows use ACF purely as a count store; `UGSWeaponComponent` keeps the
weapon meshes, sockets, offsets and the wheel, and `AGSArrowProjectile` keeps the shooting. ACF's
`UACFShootingComponent` was examined and **rejected** — adopting its ammo loop would replace our
arrow projectile and delete the tuned mesh offset, the distance-to-head-bone headshot and the bow
timing minigame, to buy free features this scope does not want.

---

## 2026-08-23 — who wears plate (the defender roster, settled)

Michael, asked directly while closing #272, which had surfaced that every defender in the game reads
`armor 0` and therefore that the whole directional-plate system from #091 was doing nothing at any
angle.

**Decided.**

1. **The castle guards are Militia, and that is correct.** *"The CastleGuards are militia, there's
   just a decent amount of them."* They are a numbers problem, not a durability problem — 30 HP,
   armour 0, and the plate system is deliberately not in play against them. `BP_CastleGuard01` and
   `BP_CastleGuard02` are already authored this way; **nothing needed changing.** This supersedes the
   suspicion raised in #272 that they might have been mis-rowed, and the historical `armor 6.0` line
   for `BP_CastleGuard01_C_0` in #087 is stale, not evidence of intent.

2. **Knights are a named person, not a class of mook.** *"We already have a character model for
   Knights, it's a named person. KnightDPelegrini."* `BP_KnightDPelegrini` is archetype `Knight`,
   75 HP / armour 6, and is wired correctly today. Armoured plate is a *character*, not a difficulty
   tier — so the plate system having exactly one live consumer is the design working, not a gap.

3. **Uriel A Plotexia should be armoured too — FUTURE, not done.** Michael, same session: *"Uriel A
   Plotexia should be armored as well, make a note for that in the future."* `BP_UrielAPlotexia` is
   currently archetype `Militia` with `DA_Weapon_ArmingSword` (30 HP, armour 0). **This is recorded
   as intent and has NOT been actioned** — no ticket has been opened and no asset has been touched.
   Whoever picks it up: the change is the archetype row on the Blueprint, and it should be a
   deliberate choice whether Uriel is `Knight` (75/6, i.e. Pelegrini's equal) or wants a new row.

**Verified at runtime the same session** (`GS.Stats.Dump`, PIE on `L_CombatArena`): Erika 20/20, both
castle guards 30/30, ARS agreeing with the GS attribute set on all seven characters, zero mismatches.
`DA_Race_Human` carries Militia 30/0, Archer 20/0, Knight 75/6 — the GDD §8 roster exactly.

**Left open, not decided.** `BP_PeasantMan` is also archetype `Militia` and carries an arming sword,
i.e. a peasant with 30 HP who fights. GDD §8 says *"civilians 10 HP with disbelief → panic"*, and
`DA_Race_Human` has **no civilian row at all**. Either civilians are simply not built yet or the
peasant is mis-authored; nobody has ruled, and this ledger will not guess.

---

## 2026-08-21 — the land turns as you raid (world corruption)

Michael, asked for *"a system where the environment as a whole will start to look more sinister, as if
we're converting the land to Mordor as we burn objectives and kill humans in the area"*. The four
shape decisions were taken in the same session: global rather than zoned or radial; all four visual
layers; all four drivers; and monotonic. Tickets #252 (abandoned) and **#296** (re-filed).

This is the first system the GDD acquires that exists to be *looked at* rather than played, and it
lands against §12.4's scope freeze — hence a ruling before a line of code.

| # | Ruling | Consequence |
|---|---|---|
| 40 | **World corruption joins the slice.** One **global** 0..1 scalar, driven by objectives burned, humans killed, structures destroyed and raid clock / horde presence, driving sky, fog, sun, a post-process grade, world materials, ash-and-ember VFX and ambience | It also gives **decision 11** its missing half. *"No HUD arrows, no waypoints — the environment does the leading"* has had nothing behind it since wayfinding was deferred (ruling 17), leaving objective **names** as the only progress readout the player gets. A world that visibly darkens as it burns is the first thing that actually does the leading |
| 41 | **Corruption is monotonic. It never recedes.** A doused field does not give the sky back | Follows GDD §8's *"No alarm reducers — goblins don't de-escalate; they leave"*, and it is also a **correctness constraint, not a taste call**: `UGSBurnMaskSubsystem`'s R channel is monotonic *by construction*, so a freely-falling global scalar would render a clean blue sky over permanently black ground. The horde-presence and clock terms may fall individually; a high-water ratchet holds the total |
| 42 | **Corruption is NOT the alarm meter, and is not derived from deeds** | Both obvious shortcuts are wrong and wrong silently. `AGSGameState::TriggerHordeWave` resets `Alarm` to `MaxAlarm * 0.4` on every wave (`GSGameState.cpp:82`) — reusing it gives a world that **un-Mordors itself when reinforcements arrive**, a bug that first appears on the *second* wave. And `GetDeeds()` is polluted (GDD §12.1 row 12: any burn objective pays deeds, so ~30 houses swamp the tally), which would make burning houses darken the sky faster than detonating the mill. The objective term reads `GetCompletion01()` off the roster instead |
| 43 | **The output stage is hand-lerped C++, not Epic's DaySequence** — for now | `UDaySequenceModifierComponent::SetUserBlendWeight(float)` blends a whole Sequencer-authored environment override on one 0..1 weight and is *exactly* this feature's output stage, artist-authorable. It is rejected for the slice because it needs an Experimental plugin enabled and an `ADaySequenceActor` **in every level** — which `L_CombatArena` (the default startup map, and the only map anything since 2026-08-07 has been watched in) and the PCG hamlets do not have. **Revisit when a hand-authored level exists.** Recorded so it is not re-investigated from scratch |
| 44 | **ACF contributes nothing here, and no ACF module joins `GoblinSiege.Build.cs`** | Swept all 48 modules: zero hits for `MaterialParameterCollection`, `SkyAtmosphere`, `ExponentialHeightFog`, `Weather`, `WorldState`, `Karma`. `UACFMoralityComponent` is a 70-line per-**player** unnormalised tag→float bag with no bands (and a live `INT32_MIN`-vs-float bug in `GetMoralityAlignment`); `UACFMusicComponent` is hardcoded to `EBattleState` and would log `Missing ACFGameState!` here because `AGSGameState` does not derive `AACFGameState`; `AACFAssaultPoint` has no progress float at all. Only `AscentSaveSystem` stays in view, and only if cross-raid persistence is ever wanted — see the note under ruling 45 |
| 45 | **Corruption is per-raid. It does not persist between raids** | Keeps the canonical float on the subsystem, which is the clean home. Recorded because the constraint is not obvious: `IALSSavableInterface` is actor/component-shaped, so a `UWorldSubsystem` **cannot be saved by ALS** at all. If persistence is ever wanted, the canonical `UPROPERTY(SaveGame) float` has to move onto the director actor with the subsystem demoted to a query face — a refactor, not an addition |

*The civilian question left open here was settled on 2026-08-24 — see ruling 62. Civilians count
**more** than soldiers, not the same. The ticket authorising this block is **#296**; #252 was
abandoned as bookkeeping on a stale session without its edits being reverted, which left canon
citing a ticket that was not standing behind it.*

---

## 2026-08-21 — the Warren is built, and the horn changes shape

Michael, on how the Warren comes into existence: *"No digger, just a magic spot where they can summon
goblins and drop off loot"*. On the horn: *"if click, one goblin appears, if you hold down MMB, they
pop out of the warren one at a time until you get the full squad"*, then *"a full squad of 10
eventually, but it summons them as you blast the horn"*. Ticket #236.

| # | Ruling | Consequence |
|---|---|---|
| 36 | **The Warren is PLACED, not planted.** No planting channel, no digger goblin, open from `BeginPlay`. Supersedes the planting language in GDD §6 and the digger in §2.6 | `UGSHordeSubsystem::NotifyGoblinSpentOnWarren()` is now a pool exit with **no caller**. It stays — deleting it would have to be re-derived the day a digger lands, and its absence is what makes the fourth pool state (spent, not dead) correct in advance |
| 37 | **The horn is tap-or-hold.** A tap summons exactly one goblin; a hold streams them one at a time up to the active cap of 10. **Supersedes decision 9** (`SummonsPerBlast` is a fixed 4) | The debit moves from `SummonWave` to the new `SummonOne` — decision 40 ("the summon is the only debit") is unchanged in substance and changed in location. `SummonsPerBlast` survives only as the batch size for `GS.Horde.SpawnTest`. Input is press+release as **two bindings**, never a Hold trigger: #207/#208 established that a Hold trigger does not stop the `Started` pin firing |
| 38 | **The Warren banks loot on overlap, and that is the seam for both banking points** | Gives `UGSScoreSubsystem::AddLoot` its **first caller project-wide** — the loot half of the two-kind score had never run. Loot value is one `int32` on the existing `UGSInteractableComponent`, not a new component, because every bankable actor already owns one and a new `UCLASS` costs a six-minute editor-closed build here. The runic site becomes the second consumer when mid-raid banking lands |
| 39 | **Deeds still never bank at the Warren** — reaffirmed, not re-decided | Stated here because the implementation makes it tempting: the banking path is generic and adding `AddDeeds` beside `AddLoot` is one line. GDD §9: *"a second place to bank deeds would erase the reason to ever risk the run home."* `GSWarren.h` carries the same warning |

---

## 2026-08-20 — the ACF migration resumes, and the attribute question is settled

Michael: *"let's go ahead and finish the ACF migration"*, then *"let's pick the ACF system because we
get more out of the box"*.

| # | Ruling |
|---|---|
| 24 | **Finish the ACF migration.** Phases 1 → 4 of `ACF_HORDE_MIGRATION.md`, each ending somewhere watchable, none started before the previous is watched. |
| 25 | **Phase 2 attributes: ACF's `AdvancedRPGSystem` (ARS) is the owner.** Chosen for out-of-the-box coverage over keeping stock GAS with `UGSAttributeSetBase`. This settles the migration doc's open risk 3 — *"pick one owner per concept before Phase 2 rather than discovering a second health bar at runtime"*. |
| 26 | **#213 (hand-authored Hold branch) is abandoned rather than done.** Phase 3 delivers Hold as `AICommand.StayThere` through `UACFCommandsManagerComponent`, with `BTTask_ExecuteCommand` already wired into `ACFBT`. Ten minutes of BT editing to build something a later phase deletes is not worth spending. |
| 27 | **Move speed moves to ACF too — ARS/ACF owns every attribute, and `MoveSpeedMultiplier` is re-expressed as ACF locomotion.** Chosen over keeping a small project attribute set alongside ARS (my recommendation), on the grounds that one owner beats two. **The problem this ruling must solve, recorded before it bites:** `UGSGE_MoveSpeedScalar` composes *multiplicatively from independent sources* (carry + block + swing + recovery may all apply at once, and `MultiplyCompound` is what makes two 0.55 slows read 0.30 rather than 1.10), while `ELocomotionState` is four discrete values — Idle/Walk/Jog/Sprint — with no multiplier, modifier or scalar concept anywhere in `UACFCharacterMovementComponent`. ACF's only runtime seam is `SetLocomotionStateSpeed(State, speed, swimSpeed)`, a server RPC that overwrites the authored speed of one state. **So the composition stays ours and only the result crosses over:** one component keeps the multiplicative product of active slow sources and pushes `BaseSpeed[State] * Product` into `SetLocomotionStateSpeed` for each state on change. What must NOT happen is callers writing that setter directly — that is `MaxWalkSpeed` cache-and-restore all over again, the exact bug the GAS-scalar rule in `CLAUDE.md` exists to stop. |
| 28 | **Teams: adopt `UACFTeamComponent` and the team manager; RaceTag survives for everything that is not hostility.** `AGSCharacterBase::IsHostileTo` becomes a thin wrapper over `AreTeamsHostile`, so the standing ruling that there is exactly ONE friend/foe predicate is preserved rather than broken — `BTService_AcquireTarget` and the melee sweep keep sharing it. RaceTag stays race data: animation sets, bark selection, race data assets. **Two traps verified in ACF source, not assumed:** (a) `ACFTeamManagerSubsystem.cpp:175` returns `GetDefaultAttitude()` when `TeamA == TeamB`, and `ACFTeamManagerComponent.h:69` defaults it to `ETeamAttitude::Neutral` — *same team is not automatically friendly*, it must be authored; (b) the lookup finds TeamA's entry then reads `Relationship.Find(TeamB)` and **nothing mirrors it** — an unauthored reverse direction silently falls through to the default, so A hunts B while B ignores A. Author both directions of every pair. What this buys that RaceTag cannot express: three-way asymmetric relations — civilians who flee goblins, are protected by guards, and are targets of neither — which `RaceTag != RaceTag` makes hostile-to-all-unlike-them by definition. Rulings 13, 14 and 19 depend on it. |
| 29 | **The ability port is piloted on `GSGA_DodgeRoll` alone before the other four move.** Five of our eight fit `UACFActionAbility`'s montage-plays-then-ends shape (Dodge, SwordLight, TorchToss, Horn, GrappleThrow); `GSGA_Block` is a held state and goes to `UACFDefenseStanceComponent` instead; `GSGA_Interact` stays put (see ruling 30). Dodge is the pilot because it is the only one carrying all three risks at once — a resource cost, a root-motion montage, and a working instrument (`GS.Combat.LogDodge`) to watch it with. The risk it is chosen to expose: `FActionConfig::ActionCost` is a `TArray<FStatisticValue>` resolved against ARS, and per ruling 25's notes a mistyped statistic tag **fails open** — the dodge becomes free rather than erroring. That is a silent-ship bug, and finding it once beats finding it five times. |
| 30 | **`GSGA_Interact` and `UGSInteractableComponent` stay ours for now; ACF's interaction is revisited when the goblin smash/loot verbs are built.** ACF genuinely offers things we lack — motion-warp alignment to the object (`UACFInteractActionAbility::GetWarpTransform`), a per-object interact montage (`InteractionActionTag` / `InteractionMontageOverride`), camera lock, replicated Busy/Free so two pawns cannot loot one chest, and, the one that matters, **`ServerInteractOnBehalf` plus the `ACFInteractSmartObjectsTask` BT task, which is an AI interacting with world objects**. But ACF has **no hold-to-channel**: grepped both `ACFInteractableComponent.h` and `ACFInteractionComponent.h` for hold/duration/progress/percent and found none — interaction is instantaneous Free→Busy, while ours runs a 1.5 s `ChannelSeconds` fill that Block A of the tutorial plan is defined by. So this is a *gain the presentation and the AI seam, keep the channel* job, and it belongs to the looting work rather than to the ability port. |
| 31 | **Combo timing moves onto montage notifies; the stage machine stays ours. `AscentComboGraph` is deferred, not rejected.** `FGSSwingStage`'s `WindupSeconds`/`DamageWindowSeconds` are replaced by `ACFActivateDamageNotifyState`, and the `bComboQueued` buffer by `ACFInputBufferNotifyState` — both authored on the montage timeline, which removes the number-vs-animation disagreement that produced the 'goblin resets before the second swing' hitch chased on 2026-08-19. **What we are NOT taking yet:** ACF's combo *graph* — a real asset type (`UACFComboNode` = montage + damage activation + trace channels + `FAttributesSetModifier`; `UACFTransition` = input tag + conditions + `Priority` + optional `bUseWeightedPriorities`/`Weight`), giving branching light/heavy trees and weighted follow-ups that our single linear chain cannot express. It is deferred because adopting it drags our sweep model with it: `UACFComboNode` has no slot for `SweepRadius`, `SweepArcDegrees`, `SweepForwardOffset/HeightOffset`, `LungeSpeed` or `GuardBreakStaggerSeconds`, taking damage geometry from the **weapon's** trace channels instead — which makes it a rider on the item-2 damage decision rather than a timing decision. Revisit once the `UACFDamageCalculation` subclass lands. **Note `AscentComboGraph` is absent from `GoblinSiege.Build.cs`** — option A would be a module add, not just content. Per-swing `MoveSpeedScale`/`RecoveryMoveSpeedScale` stay on the stage and become sources feeding ruling 27's composition. |
| 31 | **Adopt ACF's combo graph.** `AscentComboGraph` is added to `GoblinSiege.Build.cs` (it is absent today, so this is a module add, not just content), the sword chain is authored as a `UACFComboGraph` asset, and `FGSSwingStage`'s timer fields are replaced by timeline notifies — `ACFActivateDamageNotifyState` for the damage window, `ACFInputBufferNotifyState` for the combo buffer. This removes the number-vs-animation disagreement behind the 'goblin resets before the second swing' hitch (2026-08-19) at the root rather than patching it. What it buys beyond the hitch: `UACFTransition` keys on an **input tag** with conditions, `Priority` and optional `bUseWeightedPriorities`/`Weight`, so light/heavy **branching** trees and weighted follow-ups become expressible — our single linear chain cannot represent either. **The consequence, chosen with eyes open (option B, notifies-only, was recommended and declined):** `UACFComboNode` carries only montage + `DamageToActivate` + trace channels + `FAttributesSetModifier`. It has **no slot** for `SweepRadius`, `SweepArcDegrees`, `SweepForwardOffset/HeightOffset`, `LungeSpeed` or `GuardBreakStaggerSeconds`, and takes damage geometry from the **weapon's** trace channels instead. So this ruling is now **coupled to the item-2 damage decision** — the sweep model moves with it, and the two must be sequenced together rather than independently. Per-swing `MoveSpeedScale`/`RecoveryMoveSpeedScale` map onto `ComboModifier` and remain sources feeding ruling 27's composition. |
| 31a | **Consequence for ruling 29:** `GSGA_SwordLight` is no longer a plain `UACFActionAbility` port. Its ACF counterpart is `UACFComboAttackAction` (`AscentComboGraph`) driving a graph, with `UACFComboAIAttackAction` for the AI side. Dodge remains the pilot; sword moves after the damage calculator, not with the other three. |
| 32 | **Adopt `UACFDefenseStanceComponent`; `GSGA_Block` retires, but RECOIL (#087) survives as our addition on top of it.** ACF brings four things we do not have: guard break as resource attrition (`DamagedAttribute`, normally Stamina, drained by blocked damage until `ActionToBeTriggeredOnDefenceBreak` fires, with `MinimumDamageStatisticToStartBlocking = 5` refusing a guard you cannot hold); **parry** (`StartParry`/`CanParry`/`ActionToBeTriggeredOnParry`); **counter-attack** (`EnableCounterAttackWindow`, `TryCounterAttack`, `CounterAttackAction`); and per-shield mitigation via `UACFBlockComponent::GetDamagedStatisticMultiplier`. `ActionsThatPreventBlocking` gives us a tag list of actions during which no guard is possible. **The one place ACF and this project disagree, verified rather than assumed:** every branch of `TryBlockIncomingDamage` acts on `damageEvent.DamageReceiver`, and `grep -i recoil` over the whole ACF source returns nothing — **ACF's block costs the DEFENDER and does nothing to the attacker.** Ours does the reverse: a blocked swing turns the attacker aside for `GS.Combat.RecoilSeconds`, which is the project's anti-turtle answer and is deliberately symmetric. This is not a conflict, it is a half ACF never wrote: bind our recoil to the `OnDamageBlocked` delegate, which broadcasts on exactly that event. Cost is a delegate binding, not a fork. **Bonus for ruling 27:** ACF models the block slow as a locomotion *stance* — `DeactivateLocomotionStance(ACF::BlockTag)` on stop, plus `LocomotionStateWhileBlocking` — so block is already re-expressed and drops out of the multiplicative composition we owe. |
| 33 | **`UGSEngagementComponent` stays; ACF's ticketing is NOT adopted — and `MaxAttackersPerTarget` is raised anyway as insurance.** ACF's crowd control is one integer: `UACFAIManagerComponent::RequestTicket(Target, Controller, Duration)` gated by `MaxAttackersPerTarget`, **which defaults to 1**, with no spatial model whatsoever. Ours carries a weighted `TokenBudget = 4`, `MaxEngagedAttackers = 6`, and `RingSlotCount = 6` with `GetRingSlotLocation` giving each attacker an actual position on the ring, plus a recoil-aware `CanBeAttacked(bRecoilCountsAsOpening)`. Adopting ACF's would replace a six-slot spatial ring with a one-attacker integer. **Verified before deciding:** ACF ticketing is currently INERT for us — its only consumer is `ACFCombatBehaviourComponent.cpp:146` inside the `ActionByCondition` path, and our AI attacks from our own behavior tree, so Phase 1's reparent did not switch it on. It is therefore **not** the cause of anything observed. The insurance half of this ruling: set `MaxAttackersPerTarget` above 1 regardless, so that if a future ACF system (combat behaviour, group AI, waves) does start consuming tickets, it does not silently clamp the horde to one attacker. |
| 33a | **Owed, and explicitly not closed by ruling 33:** Michael observed goblins *'attacking one at a time... they go in a line instead of a mob'* (2026-08-19, before Phase 1). Ruling 33 establishes that ACF is not responsible and that ours is the better model — it does **not** explain the behaviour. `GS.Combat.CrowdStats` exists and has never been run **during** a fight; it must be, on a horn-summoned goblin per the test-path rule, to say whether ring slots are being claimed at all or whether the weighted `TokenBudget = 4` is starving all but one attacker. Diagnosing this from source rather than from the instrument is the exact failure mode this project has paid for repeatedly. |
| 34 | **The Attack order stays uncapped; the fix is the overflow's POSITION, not its admission.** Confirms the intent already written at `GSHordeSubsystem.cpp:549-555` — converging on one guard is what the player asked for — so `RegisterEngaged` is NOT made to refuse past `MaxEngagedAttackers`. The defect is downstream: `BTService_AcquireTarget.cpp:372-383` sends a slotless attacker to `StandoffRadius * 1.8` = 360uu **on its own current bearing, with no exclusivity**, so two overflow goblins can be handed the same point while melee range is 250uu (`BTTask_MeleeAttack.cpp:76`). Fix: give the overflow a distinct outer-ring bearing so they menace at 360uu instead of stacking. |
| 35 | **Stamina storage moves to ARS; `UGSStaminaComponent` survives as a policy layer.** It stops owning a replicated float and instead reads and writes the ARS Stamina attribute, keeping what a bare attribute cannot express: `SetDrainRate` (continuous drain for sprint and swim), `bRegenSuppressed`, `RegenDelaySeconds`, and `bExhausted` with `OnExhausted`/`OnRecovered` — hysteresis, not a threshold — plus the climb freeze from #072/#076. **What forces this rather than merely recommending it:** ruling 32 adopted `UACFDefenseStanceComponent`, whose `DamagedAttribute` is an `FGameplayAttribute`, so blocking drains ARS Stamina. If dodge keeps checking our own float, the project ships **two stamina bars that cannot see each other** — ruling 25's failure mode arriving through the back door. Note `FACFStatisticsSet` also ships `Equilibrium`/`MaxEquilibrium`/`EquilibriumRegen`, a poise/stagger stat we have no equivalent of and did not have to build. |
| 36 | **Crowd feel, three calls taken on measurement (2026-08-21):** (a) **`TokenBudget` 4 -> 6**; (b) **a flinch or recoil now refuses only the attacker that caused it**, not the whole gang; (c) **an Attack order past capacity spills to the nearest other enemy with room** — *"are they focusing on another enemy if one is over saturated? that's how I would love them to work"*. **The measurement:** 16 CrowdStats samples during a live fight showed `swinging` on gangs of 6+ was **bimodal** — either `4` with `weight 4/4` saturated, or `0-1`, never between. The mob ran full-throttle, stalled, ran full-throttle. (a) raises the throttle, (b) removes the stall. **The cost of (b), recorded before it bites:** the blanket veto was deliberate — piling onto a flinching target is the deletion `UGSEngagementComponent` exists to prevent, and #087's recoil design assumes it. With a per-attacker veto AND a budget of 6, a flinching defender can be hit by six goblins at once. If defenders start melting, ruling 36 is why, and the fix is to tune the budget down rather than restore the blanket veto, which would bring the stall back. **(c) REFINES ruling 34, it does not reverse it:** the order still converges the warband on the named victim and nothing caps the first six — only the surplus spills, into the `HasEngagementRoom` + nearest-with-room path that #132 already built and ordered goblins had never used. Predicted side effect to check on the next watch: `OVER ENGAGED` should become rare and ruling 34's outer ring should sit mostly empty, degrading to the fallback for when every nearby enemy is full. **CONFIRMED 2026-08-21 00:49**, single order then hands off, 47 CrowdStats samples: zero `OVER ENGAGED`, max engaged exactly 6, up to **9 victims engaged at once across 12 distinct victims** (the pre-change run had one victim carrying 10). `swinging` on gangs of 4+ now spans 0-6 with a mode of 5 and 15/21 rows taking a middling value — the bimodality is gone. **17 of 22 `immune` rows still show `swinging > 0`**, which is the per-attacker veto working: a flinch no longer stops the gang. The outer ring is now claimed in **0** rows, exactly as predicted. No defender died (6 goblins did), so the melting risk did not materialise at budget 6 — but note this also means six goblins on one guard for ~25 seconds killed nobody, which is a damage-vs-health question for another ticket, not a crowd one. |

### What ruling 25 puts at risk, recorded before it bites

Adopting ARS is the right call for breadth, but it is **not** a free swap, and the cost is concentrated
in the one area this project has tuned hardest. The following all sit on our own attribute/damage path
and none of them come from ACF:

- `UGSDamageExecCalculation` — **directional plate** (#091: 150° frontal arc, 0.3 scalar, flat armour
  skipped entirely from flank/back/takedown, bow ignores it at any angle), the **minimum damage floor**
  (#093, Michael's ruling that mitigation may never zero a hit), and the **NPC-vs-NPC scalar**.
- **Recoil** (#087) — a blocked swing is turned aside, the attacker opened up for
  `GS.Combat.RecoilSeconds`. This is the project's anti-turtle answer and it is symmetric.
- `UGSEngagementComponent` (#090) — the victim-owned token budget, engagement capacity and exclusive
  ring slots, tuned across #105–#132 and again by Michael on 2026-08-11.
- `UGSStaminaComponent` — climb freeze (#072/#076), swim drain, sprint, and as of #209 the dodge cost.
  ARS carries statistics of its own, so stamina is squarely in scope for duplication.

ACF ships its own damage path (`ACFBaseDamageTypeCalculator`, `ACFGASDamageCalculatorBP`), so the
failure mode is not "two health bars" but **two damage models**, with our tuned one quietly bypassed.
Migrating means deciding, per behaviour, whether ARS replaces it or whether it is re-hosted on top of
ARS's statistics.

**None of that argues against the ruling** — it argues for Phase 2 being planned rather than
attempted. The migration doc already says Phase 2 is "the expensive step".

### An ACF defect found while doing Phase 1 (#215)

`AACFBaseAIController` declares `public IACFEntityInterface` and implements **two of its four**
methods. `IsEntityAlive_Implementation` and `GetEntityExtentRadius_Implementation` are never
overridden, and `AscentCoreInterfaces/Private/Interfaces/ACFEntityInterface.cpp` is empty apart from a
comment. Nothing deriving from that class in another module can link until it supplies both. ACF's own
module links because nothing in it forces those thunks into a linked translation unit.

**`AACFCharacter` overrides both itself, so Phase 2 will not hit this** — it is controller-side only.

Also worth carrying forward: `AACFAIController`'s constructor replaces the path-following component
with a `UCrowdFollowingComponent`, and its `OnPossess` early-returns above `StartTree()` on a
non-`AACFCharacter` pawn. Our trees only keep running through Phase 1 because our own `OnPossess`
overrides call `RunBehaviorTree()` after `Super`.

---

## 2026-08-19 — the reconcile-and-lock rulings (#198)

Taken while reconciling the GDD against the live tree. Every one is Michael's, given in session.

| # | Ruling | Why it was asked |
|---|---|---|
| 1 | **`docs/goblin-siege-gdd.md` is canonical**; the repo-root `goblin-siege-design-document.md` freezes as the submitted Assignment #02 artifact | Two documents disagreed and neither was authoritative. The export was the status-honest one, and #158 had already established that when an export and a canon disagree you check which is *older*, not which is derived |
| 2 | **Lock with all four mechanisms**: versioned freeze + change rule · machine drift check · consolidated ledger · scope-freeze table | A doc that drifts silently is what produced the granary the evaluator kept enforcing after the design had dropped it |
| 3 | **The tutorial map is `L_Groatsworth`, built from the generator plan** | The generator won its week-2 timebox on 2026-08-18 — 8 seeds, 8 passes, on the measured kit |
| 4 | **"Finished" means the full §12.2 A–H block plan** | Sets the done bar before work starts, so scope cannot quietly shrink to "a raid ran once" |
| 5 | **Take a build window now and clear all seven stale tickets** (178, 179, 184, 185, 186, 189, 190) | The build gate was closed and four of the seven were waiting on the same build. QUEUE.md rule 6 makes a STALE ticket Michael's call, never an agent's |
| 6 | **Teach the evaluator the tutorial roster rule** — a `--tutorial` fixed roster plus a one-of-each check — rather than hand-picking a lucky seed | The passing seed-1 plan was *statue / windmill / statue*: legal under §1's generic "never more than two of a kind", wrong for a tutorial that must teach all three kinds of destruction. A hand-picked seed leaves the rule unenforced and free to break on the next regeneration |
| 7 | **Build the windmill's Stage 1 "Ablaze"** — the design is right, the code is behind | The shipped enum is `Intact/Smouldering/Detonated` and exterior fire is a deliberate no-op. The two-stage revision exists so a torch is never *silently* wasted: the player must see what they bought and what is still owed |
| 8 | **Remove the statue's burn gate** — topple is the only path | `AGSDestructibleObjective` gated the fracture behind burn-complete, so the statue had to be set on fire before it could be pulled down — contradicting the one line the objective exists to deliver ("the one target that doesn't burn") |
| 9 | **Noise is IN**, and is flagged as a major next item | A sound pack is plugged in, removing the excuse the 2026-08-14 audio cut rested on. Noise is what makes crouch a decision rather than a speed penalty |
| 10 | **Audio is IN for world/SFX; barks stay text-only** | `Content/NaPH_RPG_Fantasy_Sounds_Bundle` holds ~2,000 assets, so "the project contains zero audio assets" was dead. But no VO pipeline exists, so the Overlord and the guards stay subtitled. `gsstyle.py`'s banned-audio-word rule **narrows rather than lifts** |
| 11 | **Traversal §5b: climb ships in Blueprint** — re-grade the row, do not rebuild it | `UGSClimbLibrary` has zero C++ callers, which reads like a corpse; the climb actually lives in the player Blueprint calling it. Vault and mantle genuinely do not exist |
| 12 | **The grapple is a core Scout verb** — into §4's kit and §6 as how the statue falls | It shipped across #173–#177, #183 and #193 and appeared in no design document at all |
| 13 | **Civilians: IN** | §2.6 wants 6–10 on routines, and the bucket brigade already depends on them. ACF's `UACFAIRoutineComponent` covers most of it |
| 14 | **Patrols: IN, configured not built** | ACF ships `UACFAIPatrolComponent` / `ACFUpdatePatrolBTService`, which **retires the planned `UGSPatrolDirector`**. The overdue-patrol check-in soft signal stays ours — ACF patrols walk waypoints, they do not report to a stealth director |
| 15 | **Gore / gibs: CUT** | The existing ragdoll death covers it |
| 16 | **Watchtower, bell and well: DEFERRED to the full game** | None of the three exists in any form. **Consequence:** removes *watchman silenced +25*, *well fouled +30* and the bucket-brigade counterplay from §10 |
| 17 | **Wayfinding signposts: DEFERRED** | **Consequence:** with decision 11 banning HUD arrows and waypoints, objective *names* become the player's only guidance. The generator already emits roads and signposts per plan, so reversing this is placement and a mesh, not a system |
| 18 | **The Warren: IN, on `N_ChaosRune2`** — it is both the goblin arrival mouth and the turn-in point for livestock and looted crate/chest/barrel goods | Satisfies the settled no-pop-in mandate (goblins emerge from a hole, they do not fade in) and gives loot a second permanent banking point. **Deeds still bank only through the real portal** |
| 19 | **The finite 15-defender raid-response pool and Highpurse Keep: IN** | "Highpurse" returns zero hits in Source; what ships is an escalating spawn-rate tier with no floor. The design promises a garrison you can actually exhaust — *the hamlet can't stop you; the county can, exactly once* |
| 20 | **ACF Phase 2 takes Option A** — `AGSCharacterBase : AACFCharacter`, moving horde, defenders **and the player** together | ACF's player is an `AACFCharacter`; Option B would leave the player on a different damage path from everything he fights, and would force the combat verbs to be duplicated or hoisted |
| 21 | **Optional objective scores: wheat field 50, house 10** | Fills the last TBD. Burning every house tops out near 300 against the required trio's 450, honouring §1's "deliberately below 100" clause. Deferred lines are struck from the table rather than left unwired |
| 22 | **Retire `L_Hamlet_T1`** (159 MB, unreferenced) after a reference check; keep the small scratch maps | Zero tickets is not zero references — verify before deleting |
| 23 | **Horde pool is 20.** Delete the disagreement flag | Not a ruling so much as a resolution: `ActiveCap = 10 × ReserveMultiplier = 2` derives 20 and never carries a literal, and §2.8 said 20 all along. Only the stale 2026-08-04 export text dissented |

### Two findings recorded the same day (not rulings, but load-bearing)

- **The two maps have diverged.** `L_Tutorial_Island` holds the 2026-08-05 world and contains **zero**
  raid markers, interactables, loot, breakables, grapple anchors or the topplable statue. **Every
  system shipped since ~2026-08-07 exists only in `L_CombatArena`.** Any claim that a system "works
  in the tutorial" is false by default until re-checked.
- **The ×1.5 extraction multiplier does not exist anywhere in Source.** Not stubbed, not unwired —
  absent. The rule that makes deeds provisional, and the reason the run home is a decision, has never
  been implemented.

---

## 2026-08-14 (Michael) — the roster revision

Recorded in tickets #156, #157, #158.

1. **The required roster is Market / Statue / Windmill.** The granary is removed entirely — *"when
   initially written I had meant a different building, but had never created it."* It never had a
   mesh, a Blueprint or a placed instance. **The wheat fields and the houses become optional** — worth
   points, not gating extraction.
2. **Bind / capture is cut** for this slice; civilians are retained on the strength of the
   bucket-brigade logic. The civilian pair now offers the takedown choice only.
3. **The hamlet map is a week-2 generator timebox with a hand-authored fallback**, superseding the
   2026-07-23 hand-authored-only ruling. The framing that made this cheap: *the generator authors ONE
   hamlet* (in scope) is a different question from *the generator varies the layout raid to raid*
   (still post-slice, funding-gated). Superseded by ruling 3 above — the timebox was won.
4. **Barks are text-only** this slice. *(Amended by ruling 10: world SFX is now IN; bark VO stays out.)*
5. **The tutorial-prompt toggle ships as a config bool.**
6. **A player holding cargo cannot blow the horn** — drop it first. `State.Carrying` blocks it,
   deliberately.

---

## 2026-08-12 (Michael) — ACF adoption

Source: `ACF_HORDE_MIGRATION.md`.

1. **Full adoption.** The horde reparents onto the ACF hierarchy and orders go through ACF's command
   system. #141's hand-rolled order board is parked.
2. **The defenders come too** — *"defenders are eventually going to need to do patrols, I believe
   it's worth having them follow."* One AI stack, not two. This is what makes patrols and civilian
   routines a configuration job instead of a build.
3. **What stays ours, and must not be swallowed by the migration:** the radial order wheel, the
   world-space order marker, the courier verb, formations and ring slots, and **the horn plus the
   finite pool of 20** — ACF spawns groups and has no concept of a depleting shared pool.
4. Phase 2's fork was left open here and is settled by ruling 20 above.

---

## 2026-08-08 — the climbing rulings

Source: `AGENT_STATE.md` DECISIONS. These took the climb from "stalls at the same lip every time,
four sessions running" to working. **Settled — do not re-open.**

1. **Ledge detection is a SEARCH, not a tuned constant.** `FindClimbLedge` sweeps insets 80–340 step
   20 and takes the first surface that is walkable *and* has open sky above. Measured on 56 roof lips
   across 14 houses: the best fixed inset scores 88%, the search scores 96%. *"Just tune it" is the
   trap that cost a day.*
2. **The sky check is load-bearing.** A candidate deck with a roof above it is an interior floor;
   deleting the test puts the player inside the house.
3. **Goblins have claws: `WalkableFloorAngle` is 65°**, and that angle is simultaneously the steepest
   walkable surface and the boundary above which a surface must be climbed. Do not add a looser gate
   inside the ledge search.
4. **Climb animation play rate is DERIVED per frame**, not set. `GetVelocity()` is the wrong input —
   during a climb it reports what `ClimbTick` commanded, not what moved.
5. **Braced hops stay.** The target feel is the Moria scene — *smooth and a little hectic*. Smooth
   means no sliding; it does not mean a continuous climb cycle.
6. **Hold-E climbs.** Do not repurpose E as a release verb.
7. **Stamina freezes on the wall** — no drain, no regen. That is correct behaviour, not a stuck tick.
8. **Roof continuation is CLOSED, not deferred** — *"the roofs are fine, consider it closed."* 98% of
   roof lips resolved across 20 houses is done, not 98% of the way to done.

---

## 2026-08-07 — the horde rulings

Source: `AGENT_STATE.md` DECISIONS, taken while #069 was built. **Settled.** The first four are the
ones a later agent is most likely to "fix" back.

1. **The war-horn is on MIDDLE MOUSE**, not G. G is `IA_Block`. F is `IA_Interact`. Both GDDs' "G" is
   an erratum.
2. **The horde arrives from a portal / Warren mouth, not the treeline.** Emerging from a hole
   satisfies the same no-pop-in mandate. **Do not "restore" the treeline.** (See ruling 18 — the
   placeholder is now `N_ChaosRune2` and is built as the Warren-to-be.)
3. **Combat verbs live on `AGSCharacterBase`.** Reparenting `AGSHordeGoblin` to `AGSEnemyCharacter` is
   the tempting one-liner and silently flips five class-identity checks, none of which fail loudly.
4. **`AGSHordeSpawnMarker` will NEVER be built** — the `Marker.HordeArrival` tag and
   `AGSRaidMarker::GatherByType` already do the whole job.
5. **Active cap is per player (10 each); the raid pool of 20 is shared.**
6. **Fire kills your own horde and that is intended.** Do not "fix" it in `AGSFireVolume` — that would
   make the player fire-immune too.
7. **Corpses are never destroyed** — *"we want to see where things died."* Accepted knowingly as a
   perf cost.
8. **The horn raises the alarm straight to RAID**, not Suspicious. §2.6's Suspicious-tier horn is a
   *patrol's* horn — a different event sharing a noun.
9. **`SummonsPerBlast` is a fixed 4**, not a random 3–4 — a player counting his pool should not have
   to guess.
10. **A dead horde goblin is a stat, not a score** — no deed, no loot, no penalty.
11. **The navmesh gets WIDENED, not switched to invokers.** Invokers give no navmesh where no invoker
    stands, which would break patrols.
12. **AI vault is deferred** until the climb rebuild settles. Decision 41-a still stands, but nothing
    in the project can vault from code.

---

## 2026-08-06 (Michael) — the ranged rulings

1. **An arrow STICKS in an ally; it does not pass through.** It is stopped by an allied body, deals
   nothing, and the shot is wasted. Positioning is the player's problem. Asked with the horde case
   explicitly on the table and answered anyway — so an agent finding "every shot eaten by a friendly"
   is looking at intended behaviour, not a bug.
2. **The radial weapon wheel is built C++-first**: the enum and selection maths in code with
   BlueprintReadOnly state; the UMG widget comes after, against a working backend.

---

## 2026-08-04 — the interact framework rulings

1. **E with full hands: the focused interactable wins.** Carry-to-extract has to work, so
   "hands full = drop" is wrong.
2. **Carry stays one slot, attacks blocked, for the slice.** The "chickens are weightless, carry two,
   fight one-handed" rule needs weight classes and waits for livestock.
3. **`MoveSpeedMultiplier` gets wired into CharacterMovement once**, retiring the cache-and-restore
   pattern that `OnStartCrouch` silently defeats.
4. **The co-op server path goes in now** — cheaper before five systems hook the completion delegate
   than after.
5. **No interacting or blocking while staggered.**
6. **Full hands cannot throw a torch** — drop the sack first.
7. **Extraction is an auto-bank circle, not a hold-E verb.** `Interact.Extract` stays declared but
   unused.

---

## Earlier — decisions 1–41 and the Q-series

These predate the single-file agent memory and are cited throughout both design documents. The ones
**load-bearing for code**:

| Ref | Ruling |
|---|---|
| Q-38 | The canonical class name is **SCOUT** (amends decision 36); sword ⇄ bow |
| Q-32 / Q-37 | **Win by type-flags** — burn one of each required TYPE, then extract; siblings demote Required → Optional on first same-type completion |
| 40 | **Pool debits on spawn only** — a delivered courier rejoins the reserve; only death spends a goblin for good |
| 41-a | Horde goblins are **ground-bound but can vault**; never climb or mantle. *(See §12.1 row 5b — no vault exists in the project, so this rule currently has nothing to stand on)* |
| Q-35 | Firebreak **Unburnt → Doused** is allowed |
| Q-36 | **Replicate cheap root state only** — single-player slice, co-op-ready |
| Q-28 | The runic site sits in the mountains |
| Q-29 | The granary was deferred out of the tutorial set in July — which is why the 2026-08-14 removal was smaller in code than it looked in prose |
| 11 | **No HUD arrows, no waypoints.** Objectives are named, not pointed at — the environment does the leading. *(See ruling 17: with wayfinding deferred, names are currently all there is)* |
| 37 | The slice **is the game's tutorial, framed in fiction** — the clan auditions for His Eternal Darkness, and the score screen is his verdict |
| 2026-08-01 | **Burn-visual mandate:** anything burnable chars black and smoulders; mill sails spin while burning and stop at Detonated |
