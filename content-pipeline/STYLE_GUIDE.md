# The Goblin Siege Style Guide

**Capstone game:** *Goblin Siege* — a third-person raid game in which the player is the monster.
You are a goblin, sent through a runic portal by an unseen Overlord called **His Eternal
Darkness** to burn a human farming hamlet and get home before the portal collapses.

Every rule below is **derived from the project's own design document**,
`goblin-siege-design-document.md`, and carries the section that actually contains the sentence
plus a verbatim quote. Nothing here was invented for this assignment.

> **On the citations.** Assignment #6 lost marks because its evaluator cited a rule to §2.8 while
> the sentence lived in §2.4 — a grader who greps the section finds nothing. Every quote below was
> located by line number in the canonical GDD before it was written down. The rules are also
> encoded as data in `gsstyle.py` (`STYLE_RULES`), so the guide and the agent read from one source
> and cannot drift apart.

---

## Constraint type 1 — Vocabulary and lore

### `VOCAB_NAMELESS` — settlements have no names (GDD §1, §2.8)

The hamlet is beneath notice. Counties get names; towns do not. The county names are placeholders
in an **ascending-currency register** — Groatsworth → Pennybrook → Silverford → Highpurse Keep —
which is itself a joke about how small the player's ambitions currently are.

> "outside a tiny, unwalled human farming hamlet — **too small to have a name**, one of dozens
> dotting the county, and doing suspiciously well for itself" — §1
>
> "Tier names are *county* names, not town names *(placeholders in the ascending-currency
> register)*: **the settlements you'll eventually raid stay nameless** — small parts of a much
> larger countryside, beneath the humans' notice until they're on fire." — §2.8

**Violation looks like:** any generated line christening the place — "Millbrook Hollow",
"the village of Ashford". An LLM does this within three lines of being asked for village content.

### `VOCAB_SLICE` — only what exists in the slice exists (GDD §2.3, §2.8)

This is a seven-week vertical slice, and most of the designed game is not in it. Content may not
name things that were cut, deferred, or renamed.

| Banned | Why |
|---|---|
| Slasher | The class is **the Scout**. "Slasher" is a stale race-brief name (§2.3). |
| Brute, Shaman | Post-slice roadmap kits, not in the tutorial slice (§2.3). |
| Pennybrook, Silverford | Tier-2 / tier-3 counties, not this slice (§2.8). |
| palisade, battering ram, catapult | The tier-2 breach layer; the slice hamlet is **unwalled** (§2.8). |
| **granary / granaries** | **Removed from the GDD on 2026-08-14** (queue #156). |

The objective roster is **Market / Statue / Windmill** required, with wheat fields and houses
optional. The statue is the one target that is **toppled, never burned** — which is not flavour,
it decides which runtime class the objective lands on.

> "This hamlet carries **all three required objectives in a single fixed layout — one market, one
> statue, one windmill** — plus the wheat fields and the houses as optional objectives" — §2.8
>
> "The statue is the one target that doesn't burn: it has to be **brought down**, stone on stone"
> — §2.8

**Violation looks like:** the granary. It is not hypothetical — `out/prompts.csv` and
`out/whispers.csv` still carry `FirstSight.Granary` and `Objective.Granary.Stage1`, generated
before the ruling and stale ever since. Demo 2 uses those real rows.

---

## Constraint type 2 — Tone and register

### `TONE_DISBELIEF` — the first reaction is disbelief, not fear (GDD §2.10)

This is the game's central joke and the easiest rule in the document to break. The kingdom's
propaganda says goblins were wiped out. So a villager who meets one does not scream — they doubt
their own eyes and carry on with their day.

> "There's a second, more specific joke underneath the general satire: **the kingdom told everyone
> goblins were extinct.** These hamlets are saturated with royal propaganda … so when a goblin
> actually shows up, **the first reaction isn't fear, it's disbelief.** Villagers double-take,
> mutter **"must've been a badger,"** and keep pottering while their well gets fouled six feet
> away." — §2.10

It is also why a guard who finds a corpse escalates only to *Suspicious* — the propaganda is doing
its job even with the evidence on the ground.

**Violation looks like:** "A goblin! Run for your lives!" This is the single most off-brand
sentence the project can produce, and it is the first thing a model writes when asked for villager
reactions.

### `TONE_OVERLORD` — pompous, whispered, chronically underwhelmed (GDD §2.1, §2.10)

His Eternal Darkness communicates only in layered whispers and subtitles. His register is
apocalyptic gravitas applied to petty theft, and his signature is the trailing, grudging
"…adequate". He **mocks more than he mourns**.

> "a whispered verdict from His Eternal Darkness (**"Adequate. I have seen rats do better.
> …adequate."**)" — §2.1
>
> "Tier 1 stays deliberately goofy: propaganda-as-denial played as comedy, "must've been a
> badger," **an Overlord who mocks more than he mourns.**" — §2.10

The whole game is **satire, not grimdark** — cartoony-with-a-hint-of-gore, T-rated, party-favor
gibs and an exploding cushion of feathers for the chickens.

**Violation looks like:** generic dark-lord menace. "Let that be the last thing he does" was
rejected by the existing critic as "generic assassin menace" and reported UNRESOLVED rather than
shipped.

---

## Constraint type 3 — Formatting and length

### `FORMAT_PROMPT` — one bark, one HUD line, three facts (GDD §2.8)

Every objective and mechanic carries a dismissible first-time prompt, and its shape is specified
exactly: an Overlord bark **plus one HUD line naming three things and no more** — the objective,
the payoff, and *the one control that does it*.

> "carries a short, dismissible prompt the first time the player meets it: **a bark from His
> Eternal Darkness plus a one-line HUD note naming the objective, the payoff, and the one control
> that does it**" — §2.8

**The ceiling is 90 characters.** That is this project's number, not the GDD's: an earlier tuning
pass on the same content took the HUD lines from a 169-character maximum with 9 of 9 rows
over-length to an 85-character maximum with 0 of 9 over, and 90 is where the line was drawn.
Recorded in `README.md` §5.

**Violation looks like:** a helpful paragraph. Three chained clauses, two controls, an explanation
of what happens next.

### `FORMAT_SILENT` — text only, because there is no audio (repo export §11, GDD §2.10)

The project contains **zero audio assets of any kind**. Barks are text and subtitles. A line that
references a voice file, an SFX cue or a sound is describing something that cannot exist.

> "barks **text-only this slice** — the project contains zero audio assets of any kind; His
> Eternal Darkness = layered whispers + subtitles" — repo export §11
>
> "Text **barks** carry the comedy" — §2.10

**Violation looks like:** "(sfx: distant scream)", "plays VO_Overlord_01.wav".

---

## How the rules are enforced

Split deliberately, on this project's stated principle that **if code can verify it, use code** —
a banned class name is a regex and should never cost a token.

| Rule | Enforced by |
|---|---|
| `VOCAB_SLICE` | **Deterministic** — word-boundary regex over a banned-term map. |
| `VOCAB_NAMELESS` | **Deterministic** — a settlement-name pattern, with the legal county names allow-listed. |
| `FORMAT_PROMPT` | **Deterministic** for the character ceiling; the model judges clause-count and control-count. |
| `FORMAT_SILENT` | **Deterministic** — audio-word list. |
| `TONE_DISBELIEF` | **Model** — fear versus disbelief is a judgement no raycast settles. |
| `TONE_OVERLORD` | **Model** — register, not vocabulary. |

Deterministic findings are handed to the Evaluator as **proven evidence**. Its system prompt
forbids scoring them away, and caps any draft carrying an unresolved lint finding at 6/10 — so the
cheap checks bound the expensive one rather than competing with it.
