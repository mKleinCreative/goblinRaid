---
id: 283
title: "RULING: the weapon wheel migrates onto ACF equipment; Uriel is demo scope, not prototype"
agent: claude-warren
status: done
claimed: 2026-08-24T17:30Z
build: none
waiting_on:
evaluated: 2026-08-24T19:57:14Z
observed: UNOBSERVED 2026-08-24T19:57:15Z - Three ledger rulings and nothing runnable. Ruling 53 answers a question #274 recorded as open and awaiting Michael; 55 removes Uriel from prototype scope. What is unproven is whether the migration 53 authorises can be done without changing how weapons feel - which is the next tickets work, not this one.
scenario: none - never run
files: 
  - docs/decisions-ledger.md
---

## Goal

Record two decisions Michael took in one line, before either turns into work. Rulings only, no code.

## Generate

**Rulings 53-55** in `docs/decisions-ledger.md`:

- **53 - the weapon wheel DOES migrate onto ACF.** This **answers #274's open question**, which was
  deliberately left standing rather than guessed: *"the question worth answering is not tags or enum,
  it is - is the weapon wheel migrating onto ACF at all?"* It is. That retroactively makes #274's
  enum-to-tag swap a stepping stone rather than churn.
- **54 - weapons become items, and lootable in principle.** Follows from 53 rather than being a
  separate choice: once a wheel slot is an ACF equipment slot, what fills it is a `UACFWeapon` in an
  inventory. Whether the player actually strips a sword off a corpse is **not** decided here.
- **55 - Uriel is DEMO scope, not prototype scope.** Supersedes the 2026-08-23 note that recorded his
  armour as "FUTURE, not done" without saying which future. `BP_UrielAPlotexia` staying `Militia`
  with an arming sword is now **correct**, not a defect for someone to fix in passing.

**The bucket brigade is deliberately NOT a ruling.** Civilians are already IN the §12.4 freeze
(ruling 13) and NEXT already carries the brigade as blocked on `BT_Civilian`. #275 built the civilian
archetype and the prop, which is what unblocked it. Adding a ruling for work already in scope would
devalue the ones that gate real scope changes.

## Evaluate

`check_gdd`: **CLEAN, 10 checks passed.** No GDD change was needed and none was made - 53 and 54 are
implementation direction rather than scope, and 55 removes something from scope rather than adding to
it, so §12.4's IN column is untouched. That is a deliberate judgement: the change rule exists for
things joining the tutorial, and none of these do.

**Established:** that 53 answers a question #274 explicitly recorded as open and awaiting Michael,
rather than inventing a decision. Quoted from the ticket.

**Not established, and it is the whole risk:** whether the migration can be done without changing how
weapons feel. The ledger entry names the specific hand-tuned rules that have no ACF equivalent - the
holstered melee weapon hidden for the whole time the bow is out, the quiver that never moves, the
Torch slot outranking an ability's un-ready request - and states plainly that **a migration which
silently changes weapon placement or swap feel has failed even if it compiles and equips.** Nobody
has yet shown it can be done without that.

## Refine

**Ruling 54 was nearly written as "weapons become lootable", and that would have been wrong.** It
would have recorded a *design* decision Michael did not make. What he authorised is the migration;
lootability is a structural consequence of it, and whether corpses actually surrender their weapons
is a separate call. The row says so.

**A "what this does NOT authorise" paragraph was added** after drafting, because ruling 53 reads as
broad permission and the thing most likely to go wrong is not the equipping - #270 already proved
that works - but the visual and feel rules being treated as plumbing on the way past.
