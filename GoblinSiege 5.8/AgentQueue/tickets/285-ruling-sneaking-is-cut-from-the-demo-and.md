---
id: 285
title: "RULING: sneaking is cut from the demo, and the bucket brigade stays deferred"
agent: claude-warren
status: done
claimed: 2026-08-24T20:30Z
build: none
waiting_on:
evaluated: 2026-08-24T20:34:49Z
observed: UNOBSERVED 2026-08-24T20:34:50Z - Three ledger rulings and a GDD revision - nothing runnable. AGENT_STATE.md carries what is unproven: whether cutting sneaking is right for the game, as opposed to right for the demo scope it was decided on.
scenario: none - never run
files: 
  - docs/decisions-ledger.md
  - docs/goblin-siege-gdd.md
  - Tools/check_gdd.py
  - AgentQueue/tickets/283-ruling-the-weapon-wheel-migrates-onto-ac.md
---

## Goal

Michael, on being told the bucket brigade was deferred out of the slice: *"We might have to skip the
sneaking portion of the demo all together."* Confirmed after seeing the cost. Rulings only, no code.

## Generate

**Rulings 56-58** in the ledger:

- **56 - the crouch-and-confirm stealth core is CUT from the demo**, and with it noise, takedowns,
  corpse-suspicion and the coin toss. **This removes an item from §12.4's "Never cut" line**, which is
  the strongest commitment the document makes and the only time anything has ever come off it.
- **57 - the bucket brigade stays DEFERRED.** It already was, as a consequence of ruling 16; 56
  strengthens that rather than changing it. The brigade is counterplay to fire and **fouling the well
  is the counter to the brigade** - without the well, and now without the stealth verbs that make
  fouling interesting, it is a mechanic with no answer.
- **58 - the demo is a straight raid**: horn, horde, burn, bank, extract.

**GDD to v1.3.** The "Never cut" line loses the stealth core and **carries a blockquote saying so, in
place** - a Never-cut list that quietly loses entries is worth nothing. Sneaking joins "Cut for this
slice". The cut-order line notes that 56 took it one step past its end. §12.1 row 7's trailing claim
that *"noise is IN and is the next major item (ruling 9)"* is corrected, and the row records that the
perception component sitting on zero actors is what made the argument.

**`Tools/check_gdd.py`: `EXPECTED_NEVER_CUT` 5 -> 4**, with a comment naming the ruling and requiring
that any future drop names its own.

### The argument, because reversing a Never-cut needs one

§12.1 row 7 already graded the stealth five as *"SPLIT - two built, three absent"*, and the detail
that decided it: **the perception component sits on ZERO actors.** The crouch-and-confirm core exists
in full and has **never executed** - the same shape as the interact framework, which cost this project
eight days. Noise, takedown, corpse-suspicion and the coin toss have no code at all.

So the sunk cost is two systems that have never run, and the saving is three-to-four that do not
exist. **The stealth core is the only "Never cut" item that has never run on a single actor**, and
that asymmetry is the whole case.

It also follows the GDD's own cut order to its end rather than inventing one - *self-looting civilians
-> sheep & chickens -> coin toss -> corpse-suspicion -> takedowns* - taking the one step past it.

## Evaluate

**`check_gdd` CLEAN, 10 passed - but only on the second attempt, and the failure was mine.**

The first run failed `never_cut_unwrapped`: *"parsed 3 never-cut items, expected 5... That line must
stay on ONE unwrapped line."* I had reflowed it to a comfortable width. The constant's own comment
reads *"a wrapped line silently truncated this to 3 until 2026-08-19"* - **I reintroduced the exact
bug the check was written to catch, in the act of editing the line it guards.**

The gate did two useful things: it caught the wrap, and it refused to let the count change pass
silently, forcing `EXPECTED_NEVER_CUT` to be edited deliberately with the ruling named. A check that
merely counted would have been satisfied by wrapping.

**Also corrected: #283 contained a false claim, and this ticket fixes it.** That ticket said *"the
bucket brigade is deliberately NOT a ruling"* on the grounds that civilians are IN and NEXT carries
the brigade. It is false - §12.4's consequences paragraph removes the brigade counterplay along with
the well. A correction block was appended to #283 rather than editing the original text, so the
mistake and its fix both stay readable.

**Not established:** whether cutting sneaking is the right call for the *game*. This is a scope
decision about the demo, taken with the costs on the table. Ruling 56 says explicitly that the
`Stealth/` code is not deleted and that whether stealth returns for the full game is not decided here.

## Refine

**The GDD edit was nearly a silent deletion.** The first draft simply removed the stealth core from
the Never-cut line, leaving four items and no trace. That would have been indefensible - the whole
value of such a list is that entries cannot leave quietly. It now carries a blockquote naming the
ruling, the date, the ticket and the argument, directly under the line.

**The checker constant got a comment rather than just a new number**, requiring any future drop to
name the ruling that caused it. A bare `4` would have looked like drift within a month.

**What this unblocks, and what it does not.** It does not authorise deleting anything. `Stealth/`
stays, the perception component stays, `BTTask_Firefight` stays. The work it frees goes to the ACF
weapon-wheel migration (ruling 53) and the raid loop.
