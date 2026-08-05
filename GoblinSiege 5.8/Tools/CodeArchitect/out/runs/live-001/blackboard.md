# Code Architect — run live-001

*Every decision below was logged before any file was written anywhere.*


## WHAT IT RECEIVED

- GDD: `D:\goblinRaid\GoblinSiege 5.8\Tools\CodeArchitect\docs\goblin-siege-gdd.md` — parsed **21** systems from §12.1, 8 blocks from §12.2

## WHAT IT PERCEIVED

- Source files scanned: **94**; folders: AI, Alarm, Attributes, Characters, Combat, Core, Destruction, Horde, Missions, Progression, UI, Weapons
- UCLASS/USTRUCT declarations: **57** (38 wired, 19 stubbed/header-only)
- Content data-layer sweep: {"BP_GS": 4, "DA_": 1, "GA_": 4, "IA_": 13, "IMC_": 1, "L_": 8}

## WHAT IT SCORED

| # | feature | block | utility | eligible | rationale |
|---|---------|-------|---------|----------|-----------|
| 1 | Interact framework â€” hold-E channels + carry | A | 10.0 | yes | impact 5; urgency 5; unblocks 5 open feature(s) [stealth_five, runic_site, loot_couriers, score_system, civilians_livestock]; effort 3; block A; completeness 0% |
| 2 | Death & hit-reaction clips retargeted ('nothing can die on screen') | B | 6.0 | EDITOR | impact 4; urgency 4; unblocks 0 open feature(s) [-]; effort 2; block B; completeness 0%; REQUIRES_EDITOR -> never headless-generatable; route to supervised session |
| 3 | Someone to fight â€” human race data, BT_Militia, enemy attack path | B | 5.75 | yes | impact 5; urgency 4; unblocks 3 open feature(s) [horde, stealth_five, patrol_director]; effort 4; block B; completeness 25% |
| 4 | Lives / respawn on PlayerState | E | 4.0 | yes | impact 3; urgency 2; unblocks 0 open feature(s) [-]; effort 2; block E; completeness 0% |
| 5 | Runic site â€” spawn/respawn, objective-gated portal, staging, 90s collapse | E | 4.0 | blocked | impact 5; urgency 2; unblocks 0 open feature(s) [-]; effort 3; block E; completeness 0%; BLOCKED by ['interact_framework'] |
| 6 | Score system â€” deeds/loot two-kind tally + end screen | G | 4.0 | blocked | impact 5; urgency 2; unblocks 0 open feature(s) [-]; effort 3; block G; completeness 0%; BLOCKED by ['interact_framework'] |
| 7 | Horn & horde (subsystem, pool, BT, point command) | D | 3.2 | blocked | impact 5; urgency 3; unblocks 1 open feature(s) [loot_couriers]; effort 5; block D; completeness 0%; BLOCKED by ['militia_enemy'] |
| 8 | The stealth five (noise, crouch-detect, takedown, corpse-suspicion, coin toss) | F | 3.0 | blocked | impact 5; urgency 2; unblocks 0 open feature(s) [-]; effort 4; block F; completeness 0%; BLOCKED by ['interact_framework', 'militia_enemy'] |
| 9 | Gore/gib system (intensity scalar, feather-poof) | G | 2.5 | blocked | impact 2; urgency 1; unblocks 0 open feature(s) [-]; effort 2; block G; completeness 0%; BLOCKED by ['militia_enemy'] |
| 10 | Barks + Overlord whispers (runtime side) | H | 2.5 | yes | impact 2; urgency 1; unblocks 0 open feature(s) [-]; effort 2; block H; completeness 0% |
| 11 | Patrol director â€” 5-7 min cadence + castle reinforcements | F | 2.33 | blocked | impact 3; urgency 1; unblocks 0 open feature(s) [-]; effort 3; block F; completeness 0%; BLOCKED by ['militia_enemy'] |
| 12 | Loot couriers â€” sacks + livestock cargo, point-to-courier | G | 2.25 | blocked | impact 4; urgency 1; unblocks 0 open feature(s) [-]; effort 4; block G; completeness 50%; BLOCKED by ['interact_framework', 'horde'] |
| 13 | Civilians + livestock (routines, disbelief, brigade, flee) | G | 1.75 | blocked | impact 3; urgency 1; unblocks 0 open feature(s) [-]; effort 4; block G; completeness 33%; BLOCKED by ['militia_enemy', 'interact_framework'] |
