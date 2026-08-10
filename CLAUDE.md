# Goblin Siege — repo root

You are one level ABOVE the Unreal project. The full project guide, with the shell rules,
build gotchas and engine paths, is **`GoblinSiege 5.8/CLAUDE.md`** — read it before doing
anything with the editor, the engine or C++.

Two paths here contain spaces (`D:\Epic Games\UE_5.8`, `GoblinSiege 5.8`). Quote every path,
every time.

---

# READ FIRST — the agent work queue

**If more than one agent may be working, you take a ticket before you edit anything.** Several
Claude sessions run against this repo at once; this is the only thing keeping them off each
other's files.

```powershell
cd "D:\goblinRaid\GoblinSiege 5.8"
& ".\AgentQueue\gsqueue.ps1" list                                          # what is in flight
& ".\AgentQueue\gsqueue.ps1" claim -Agent <slug> -Title "<t>" -Files "a,b" # take a position
& ".\AgentQueue\gsqueue.ps1" check -Id <n>                                 # anyone ahead of me?
& ".\AgentQueue\gsqueue.ps1" set -Id <n> -Status active                    # start editing
& ".\AgentQueue\gsqueue.ps1" set -Id <n> -Status review                    # G/E/R written
& ".\AgentQueue\gsqueue.ps1" buildgate                                     # exit 0 = safe to build
```

1. **Claim the files you intend to write, before you write them.**
2. **The lower ticket number has right of way.** If an open ticket ahead of you claims your
   file, wait for it to close. Work your unblocked files, or go `blocked` and report.
3. **Report Generate → Evaluate → Refine** in your ticket before handing back. `done` refuses
   a ticket still holding placeholders.
4. **Nobody compiles until the queue is empty.** `Build-GoblinSiege.ps1` enforces this itself.
5. **A ticket flagged STALE is a question for Michael, never something you close yourself.**
   From inside the repo a long-running job and a dead session look identical. Ask him.

Full protocol: `GoblinSiege 5.8/AgentQueue/QUEUE.md`.
Project memory (read at run start): `GoblinSiege 5.8/AGENT_STATE.md`.

---

# Clean up after yourself

**If you create temporary files, scripts or helper files to iterate with, delete them before you
hand the task back.** Scratch generators, one-off probe scripts, `test_*.py` you wrote to check one
thing, dumped JSON, `*.bak`, half-finished patches, debug `.bat` wrappers — if it existed only to
get you to the answer, it does not survive the task.

Prefer writing scratch work **outside the repo** in the first place: your session scratchpad
directory is there for exactly this and never shows up in `git status`. Anything you do write inside
the repo is something you have to remember to remove.

Two things this is not:

- **Not a licence to delete files you did not create.** Another agent's work-in-progress and a
  leftover of your own look identical from here. If you did not write it this session, leave it.
- **Not a reason to throw away something worth keeping.** If a helper turned out to be genuinely
  useful, say so and let Michael decide where it should live — do not silently promote it into the
  repo, and do not silently bin it either. A discarded diff that had real thinking in it can be
  saved as a patch in the scratchpad and referenced from your ticket.

Check with `git status` before you hand back. Untracked files you introduced should be gone or
explained; a working tree that is noisier than when you started is part of the task, not a detail.

---

# Building

**`.\Build-GoblinSiege.ps1`** — the gated entry point. Refuses to run while the editor is open,
checks the build gate, and knows about the stranded-UBT and phantom-lock failures that have cost
days here. Use it.

`-IgnoreQueue` skips the gate — only when Michael says so. These bypass it entirely and should
not be used while other agents are working: `Plugins/VibeUE/BuildAndLaunchGame.ps1` (a separate
git repo, so a gate added there would be lost on a plugin update), `build_gs.bat`,
`_build_now.bat`. (`gs_build.bat` was a third copy of the same raw-UBT wrapper and was deleted
in #115.)
