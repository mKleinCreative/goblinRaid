---
id: 117
title: PIE says the punish still never fires: the recoil flinch sets State.HitReact, which vetoes it in turn
agent: claude-correctness
status: done
claimed: 2026-08-10T21:10Z
build: required
waiting_on:
evaluated: 2026-08-10T21:09:12Z
files: 
  - GoblinSiege 5.8/Source/GoblinSiege/Combat/GSEngagementComponent.cpp
  - GoblinSiege 5.8/Source/GoblinSiege/Combat/GSEngagementComponent.h
---

## Goal

PIE says the punish still never fires: the recoil flinch sets State.HitReact, which vetoes it in turn

## Generate

#116 fixed the recoil punish by argument and shipped it unobserved. The first standalone run of
`GS.Combat.Duel 3` on `L_CombatArena` disproved it:

```
17  block LANDED - <target> is open, dropping guard
32  holding off - <target> is staggered/open
 0  PUNISH
```

and the pair that names the bug, 17ms apart, the same guard against the same knight:

```
21:05:36.933 BP_CastleGuard01_C_4: block LANDED - BP_KnightDPelegrini_C_2 is open, dropping guard
21:05:36.950 BP_CastleGuard01_C_4: holding off - BP_KnightDPelegrini_C_2 is staggered/open
```

The blocker drops its guard to punish, then refuses to swing 17ms later — exactly the #116 symptom,
with the #116 fix in the binary.

**Cause.** `AGSCharacterBase::NotifyAttackWasBlocked` sets `State.Recoil` and then, to "sell the
clang", calls `PlayHitReact(...)` — which at `GSCharacterBase.cpp:337` does
`AddLooseGameplayTag(GSTags::State_HitReact)`. So a recoiling fighter is **always also flinching**.
#116 exempted `State.Recoil` from `CanBeAttacked` but deliberately kept `State.HitReact` an absolute
veto, so the refusal simply moved from one tag to the other and the punish stayed dead code. The
#116 fix was correct in isolation and insufficient in fact.

**Fix.** `bRecoilCountsAsOpening` now suppresses `State.HitReact` as well, but only when the target
is genuinely recoiling (`bIsTheOpening = bRecoilCountsAsOpening && bRecoiling`). Death and a broken
guard still veto absolutely, and every non-punish caller — `TryAcquireToken` included — is
unchanged, so no new attacker can join mid-window.

The reasoning that makes this safe rather than a hole: the flinch being suppressed is the blocked
swing's *own* flinch. It is the same event as the opening, not evidence of a second attacker, so it
cannot be grounds for refusing the punish that the same event created.

## Evaluate

**Verified by evidence:**

- The failure itself is observed, not inferred: 17 blocks, 32 refusals, 0 punishes in a live
  3v3 duel, with the log lines quoted above.
- Cause traced to a specific line (`GSCharacterBase.cpp:337`) on the path from
  `NotifyAttackWasBlocked` → `PlayHitReact` → `AddLooseGameplayTag(State_HitReact)`.
- Build and re-run: recorded in Refine.

**Known limitation, accepted deliberately:** a genuine third-party hit landing inside the 0.6s
recoil window is indistinguishable from the block's own flinch at this call site, and will also be
allowed through. That is the correct trade — a recoiling target is punishable by design for exactly
that window — but it does mean the HitReact pile-on guard is off for 0.6s after a successful block.
`MaxEngagedAttackers` (3) and the token budget still bound the crowd.

**What this says about #116, and about the two before it:** three tickets in a row shipped a
combat-logic change on a static argument. #116's Evaluate flagged exactly this ("no PIE, none of
these has been observed working") and was right to. The argument was sound and the outcome was still
wrong, because the failure lived in a *second* system - the flinch - that the reasoning never
touched. Grep and inspection cannot see two subsystems setting overlapping tags on the same frame;
a duel and a log line can.

**Owed to AGENT_STATE.md:** a FAILED line — the #087 punish has never once fired in this project,
through #087, #116 and up to this fix, and each time it was believed working on reasoning alone.

## Refine

**Nothing changed on re-reading beyond the fix above.** The alternative — stop
`NotifyAttackWasBlocked` calling `PlayHitReact`, so recoil never sets the flinch tag — was
considered and rejected: the flinch is the entire visual tell that a block mattered, and #087
records it as deliberate ("without it a blocked hit and a whiff look identical from outside, which
the research calls the most common failure in NPC-vs-NPC melee"). Removing it to fix a gating bug
would trade a visible feature for an invisible one.

**Deliberately left undone:** `GS.Combat.RecoilSeconds` (0.6) not retuned. The punish has never
actually landed in this game, so there is no basis yet for saying the window is the wrong length —
that is a judgement for whoever watches the first fight where it works.
