# Hosting the Playable Build (Google Drive)

## Deliverable 1 — Playable Link

**https://drive.google.com/file/d/1fllurR6kmJVv8fUKQ8jR6Yy2rsOb1SYc/view?usp=drive_link**

Verified live in-browser (2026-09-01): General access is "Anyone with the link — Viewer," so a
stranger can open it and download without signing in or requesting access.

**One honest gap against the rubric's exact gate wording** ("play within 2 minutes without setup
instructions"): this is a 2.6GB zip download, then an extract, then launching `MyProject.exe` — a
stranger clicking the link is very unlikely to be playing within 2 minutes on an average
connection, even though the *game itself* needs zero configuration once launched. "No setup
instructions" is arguably satisfied (nothing to configure), but "within 2 minutes" for a multi-GB
download is a real stretch. Worth flagging explicitly in the submission rather than hoping it
isn't checked literally.

## What's built

`Saved/StagedBuilds/Windows/` under the Unreal project is a complete, launchable Windows
Shipping build — the first successful full cook+package in the project's history (packaging
tickets #387-389), since trimmed and re-cooked (ticket #397/#398) to cut package size and fix a
missing-runtime crash on other machines. It's zipped to:

`GoblinSiege_itch_build.zip` (~3.0GB) — in `Deliverables/`, ready to upload.

Launch target inside the zip: `MyProject.exe` at the archive root.

**Why Drive instead of itch.io's own uploader:** itch enforces a hard 1GB cap on its own upload
path. Trimming to fit it turned out to require real feature/quality tradeoffs (see the size-audit
note below) — Drive has no such cap, so the full-fidelity build ships as-is.

## Size-audit note (kept for the record)

The build dropped from an initial 4.1GB to 3.4GB by fixing `Config/DefaultGame.ini`'s packaging
scope (`bCookAll=True` was sweeping the entire ~9.9GB `Content/` folder into every cook,
including unrelated marketplace packs and test/scratch maps that never ship — replaced with a
scoped `DirectoriesToAlwaysCook` list), then to 3.0GB by regenerating all 84 building-fracture
assets at a lower Voronoi cell count (`Content/Destruction`: 1.8GB → 1.2GB, ticket #397/#398).
Getting under itch's 1GB cap from here would mean either much more aggressive fracture cuts (with
uncertain, possibly non-linear returns) or trimming `L_Tutorial_Island`'s own map content — a real
content/quality decision, not a packaging fix. Punted in favor of Drive.

## Known, documented tradeoff (not a bug)

Fracture assets ship at `NumVoronoiCells=3` (down from 8) as of this build — buildings still
crumble on destruction, just in fewer, chunkier pieces. Purely a size/fidelity tradeoff, not a
functional regression; `Source/GoblinSiegeEditor/GSFractureToolsLibrary.h`/`.cpp` document the
generation process and can be re-run at a different cell count later if the visual fidelity is
worth revisiting.

## Google Drive steps

1. Upload `GoblinSiege_itch_build.zip` to Drive.
2. Right-click → **Share** → set to "Anyone with the link" (Viewer is enough — they only need to
   download, not edit).
3. Copy the share link — that's Deliverable 1.
4. Worth a line wherever the link is posted: unzip fully before running, then launch
   `MyProject.exe`. It needs the Microsoft Visual C++ 2015-2022 x64 Redistributable — this build
   now bundles the official installer at `Engine/Extras/Redist/en-us/vc_redist.x64.exe` inside the
   zip (added this pass specifically because a playtester hit the missing-runtime error on a
   machine without it already installed); point people at that if anyone hits the same DLL error
   again.

## Page copy (if you still want an itch.io page linking out to Drive)

Tagline, full description, and the custom-noun field ("raid") are already written in
`ITCH_PAGE.md` in this same folder — usable whether the actual download lives on itch or you just
link out to the Drive file from an itch page. Controls reference card:
https://claude.ai/code/artifact/8e09c06b-0737-431a-ba20-3c9ac19eef82 (full-viewport, screenshot it
for a controls image — covers Movement, Combat, The Horde, World, including the horde order
wheel's all-four-verbs update).
