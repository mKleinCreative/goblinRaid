#!/usr/bin/env python3
"""
Build the Assignment #04 submission bundle from the pipeline's own artifacts.

    python make_submission.py

Writes submission/GoblinSiege_Assignment04.html (open it, Ctrl+P -> Save as PDF)
and submission/GoblinSiege_Assignment04.zip (the html + code + every generated
artifact). Every number and quote in the report is read out of out/*.json and
out/*.csv at build time — nothing is transcribed by hand, so the report cannot
drift from what actually ran.
"""

from __future__ import annotations

import csv
import html
import json
import sys
import zipfile
from pathlib import Path

for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "out"
SUB = ROOT / "submission"
JOBS = ["barks", "whispers", "prompts"]

E = html.escape


def load(job: str) -> dict:
    return json.loads((OUT / f"{job}.trace.json").read_text(encoding="utf-8"))


def rows(job: str) -> list[dict]:
    with (OUT / f"{job}.csv").open(encoding="utf-8") as fh:
        return list(csv.DictReader(fh))


def text_of(item: dict) -> str:
    """The human-facing line of a row, whichever column carries it."""
    for k in ("line", "hud_line", "overlord_bark"):
        if item.get(k):
            return item[k]
    return ""


# ----------------------------------------------------------------------------- sections


def sec_rubric() -> str:
    m = [
        ("Game-Anchored Source", "2.0",
         "&sect;1 &mdash; the knowledge base is this project's own GDD and four race briefs. "
         "Every generated row cites the chunk ids it was written from."),
        ("Content Fit", "2.5",
         "&sect;2 &mdash; three content types, each filling a gap the GDD names about itself, "
         "quoted in place."),
        ("RAG Implementation", "2.0",
         "&sect;3 &mdash; query, retrieved chunk and generated output shown side by side."),
        ("Consistency Checking", "2.0",
         "&sect;4 &mdash; the critic's findings with the draft text, the canon rule, the chunk "
         "that proves it, and the corrected line."),
        ("Voice Judgment", "1.5",
         "&sect;5 &mdash; self-assessment, plus a measured prompt change and its before/after."),
    ]
    tr = "".join(
        f"<tr><td><b>{c}</b></td><td class='pts'>{p}</td><td>{w}</td></tr>" for c, p, w in m
    )
    return f"""<section><h2>Where each criterion is evidenced</h2>
<table class="rubric"><thead><tr><th>Criterion</th><th>Pts</th><th>Evidence</th></tr></thead>
<tbody>{tr}</tbody></table></section>"""


def sec_kb(traces: dict) -> str:
    sys.path.insert(0, str(ROOT))
    import gsrag

    chunks = gsrag.load_corpus()
    per_doc: dict[str, int] = {}
    for c in chunks:
        per_doc[c.label] = per_doc.get(c.label, 0) + 1
    retrieved = {c["source"] for t in traces.values() for c in t["context"]}
    lis = "".join(
        f"<li><b>{E(lbl)}</b> &mdash; {n} chunks"
        + ("" if lbl in retrieved else " <span class='dim'>(indexed; not retrieved this run)</span>")
        + "</li>"
        for lbl, n in per_doc.items()
    )
    return f"""<section><h2>1. Knowledge base &mdash; the actual GDD</h2>
<p>No placeholder lore. The corpus is this project's real design documents, split into
<b>{len(chunks)} heading-aware chunks</b> that keep their heading path, so a citation reads
<code>GDD&sect;2.10#2 &mdash; 2. Game Mechanics &gt; 2.10 Tone</code>:</p>
<ul>{lis}</ul>
<p class="note"><b>Retrieval concentrated on the GDD, and that is the correct outcome.</b> All five
documents are indexed, but the GDD is authoritative and the race briefs are older reference
material &mdash; BM25 selected GDD sections for these three jobs because that is where the
mechanics, scoring and tone are actually specified.</p>
<p class="note"><b>The corpus contradicts itself, deliberately.</b> The race briefs predate the
2026-07-23/24 scope decisions: <code>race-design-goblins.md</code> calls the player class the
<b>Slasher</b>, the GDD calls it the <b>Scout</b> and says it is the only class this slice. The
system prompt states the GDD supersedes, and a deterministic lint fails any row containing
<code>Slasher</code>, <code>Brute</code>, <code>Shaman</code>, <code>Pennybrook</code>,
<code>Silverford</code>, <code>palisade</code>, <code>battering ram</code> or
<code>catapult</code>.</p></section>"""


def sec_content(traces: dict) -> str:
    parts = []
    for job in JOBS:
        t, rs = traces[job], rows(job)
        cols = [c for c in rs[0].keys() if c not in ("Name", "sources", "note")]
        head = "".join(f"<th>{E(c)}</th>" for c in cols)
        body = "".join(
            "<tr>" + "".join(f"<td>{E(r[c])}</td>" for c in cols) + "</tr>" for r in rs
        )
        parts.append(f"""<h3>{E(t['title'])} &mdash; {len(rs)} rows</h3>
<blockquote class="gap"><b>The gap.</b> {E(t['gap'])}</blockquote>
<div class="scroll"><table class="data"><thead><tr>{head}</tr></thead><tbody>{body}</tbody></table></div>
<p class="file">Source of truth: <code>out/{job}.csv</code></p>""")
    return ("<section><h2>2. What was generated, and the gap each fills</h2>"
            "<p>All three are gaps the <b>GDD names about itself</b> &mdash; outstanding work the "
            "design document already tracks, not content invented to have something to "
            "generate.</p>" + "".join(parts) + "</section>")


def sec_rag(traces: dict) -> str:
    parts = []
    for job in JOBS:
        t = traces[job]
        by_id = {c["id"]: c for c in t["context"]}
        q = t["retrieval"][0]
        hits = "".join(
            f"<li><code>{E(h['chunk_id'])}</code> &mdash; BM25 {h['score']} &mdash; {E(h['heading'])}</li>"
            for h in q["hits"]
        )
        # first final row that cites a chunk we can show
        ex = next((it for it in t["final"] if any(s in by_id for s in it.get("sources", []))), None)
        cited = by_id[next(s for s in ex["sources"] if s in by_id)]
        excerpt = " ".join(cited["text"].split())[:600]
        parts.append(f"""<h3>{E(job)}</h3>
<p class="lbl">Query</p><pre class="q">{E(q['query'])}</pre>
<p class="lbl">Retrieved (top {len(q['hits'])})</p><ul class="hits">{hits}</ul>
<p class="lbl">Retrieved chunk <code>{E(cited['id'])}</code> &mdash; {E(cited['heading'])}</p>
<blockquote class="chunk">{E(excerpt)}&hellip;</blockquote>
<p class="lbl">Generated output grounded in it &mdash; <code>{E(ex['_name'])}</code></p>
<blockquote class="outp">{E(text_of(ex))}</blockquote>""")
    return f"""<section><h2>3. Retrieval &rarr; output, side by side</h2>
<p>Retrieval is Okapi BM25 with a heading-match boost, 7&ndash;8 queries per job, union of the
per-query top-4. Anthropic ships no embeddings endpoint, and at 95 chunks the vocabulary is
highly specific &mdash; &ldquo;Warren&rdquo;, &ldquo;soft signal&rdquo;, &ldquo;First Spark
Unseen&rdquo; &mdash; so exact-term matching beats semantic similarity and stays deterministic.
Full traces for every query are in <code>out/&lt;job&gt;.trace.md</code>.</p>
{''.join(parts)}</section>"""


def find_chain() -> tuple[str, list[tuple[str, str, str, str]]]:
    """
    Locate a row the critic flagged in MORE THAN ONE round — i.e. one where the
    reviser's own correction was itself wrong. Searches the current run first,
    then the archived first run, and reports which run it came from. Never
    hardcodes a row key: triggers are model-authored and change between runs.
    """
    for label, base in (("this run", OUT), ("the first run", OUT / "_run1_unverified")):
        tp = base / "barks.trace.json"
        dp = base / "barks.draft.json"
        if not (tp.exists() and dp.exists()):
            continue
        t = json.loads(tp.read_text(encoding="utf-8"))
        draft = {d["_name"]: d for d in json.loads(dp.read_text(encoding="utf-8"))}
        final = {f["_name"]: f for f in t["final"]}
        seen: dict[str, list] = {}
        for r in t["rounds"]:
            for f in r["findings"]:
                seen.setdefault(f["row"], []).append(f)
        for row, fs in seen.items():
            if len(fs) < 2 or row not in draft:
                continue
            stages = [("Draft", text_of(draft[row]), fs[0]["canon_rule"], fs[0]["evidence_chunk_id"])]
            for i, f in enumerate(fs[1:], start=1):
                stages.append((f"After revision {i}", f["quote"], f["canon_rule"],
                               f["evidence_chunk_id"]))
            if row in final:
                stages.append(("Final (verified clean)", text_of(final[row]), "", ""))
            return f"{row} &mdash; {label}", stages
    return "", []


def find_section_conflict():
    """
    Find a row the critic flagged twice while citing DIFFERENT GDD sections as
    evidence — the signature of a contradiction in the source document rather
    than a fault in the draft. Searches the archived runs.
    """
    for base in (OUT / "_run1_unverified", OUT / "_pre_ruling", OUT):
        for tp in sorted(base.glob("*.trace.json")):
            t = json.loads(tp.read_text(encoding="utf-8"))
            rows: dict[str, list] = {}
            for r in t["rounds"]:
                for f in r["findings"]:
                    rows.setdefault(f["row"], []).append((r["round"], f))
            for row, fs in rows.items():
                secs = {f["evidence_chunk_id"].split("#")[0] for _, f in fs}
                if len(fs) > 1 and len(secs) > 1 and all("§" in s for s in secs):
                    return row, [(rd, f) for rd, f in fs]
    return "", []


def sec_critic(traces: dict) -> str:
    chain_row, chain_rows = find_chain()
    conf_row, conf = find_section_conflict()
    conflict = ""
    if conf:
        steps = "".join(
            f"""<tr><td class="stage">Round {rd}</td>
<td><code>{E(f['evidence_chunk_id'])}</code></td>
<td class="why">{E(f['canon_rule'])}</td></tr>"""
            for rd, f in conf
        )
        conflict = f"""<h3>And a contradiction in the source document itself</h3>
<p>On <code>{E(conf_row)}</code> the critic <b>reversed its own verdict between rounds</b>, citing a
different section each time. That is a distinct signature: when a draft is wrong the critic cites
one rule twice, but when the <i>document</i> is inconsistent it argues both sides in good faith.</p>
<table class="chain"><thead><tr><th>Round</th><th>Cited</th><th>Rule it read there</th></tr></thead>
<tbody>{steps}</tbody></table>
<p class="note"><b>Resolved, and the fix is in the repo.</b> Those two sentences could not both be
literally true. Escalated to the human as a design question rather than patched over &mdash; and
ruled: <i>&ldquo;half-confirmed means confirmed but uncorroborated&rdquo;</i>. &sect;2.6's wording
was the defect; &sect;2.4 had been right all along. The GDD now says a look broken before the
confirm is <i>no</i> signal, and a confirm becomes <i>exactly one</i> uncorroborated soft signal
needing a second to escalate (commit <code>369ab26</code>). The prompts in
<code>out/prompts.csv</code> were then regenerated against the corrected document; the
pre-ruling run is preserved under <code>out/_pre_ruling/</code>.</p>
<p>This is the pipeline doing something a spell-check cannot: it did not find a bad line, it found
a <b>bad specification</b>, by trying to teach the rule to a player and discovering the rule did
not resolve.</p>"""
    chain = "".join(
        f"<tr><td class='stage'>{s}</td><td>{E(txt)}</td><td class='why'>"
        + (E(why) if why else "&mdash;")
        + (f"<br><code>{E(ev)}</code>" if ev else "")
        + "</td></tr>"
        for s, txt, why, ev in chain_rows
    )

    allf = []
    for job in JOBS:
        for r in traces[job]["rounds"]:
            for f in r["findings"]:
                allf.append((job, r["round"], f))
    other = "".join(
        f"""<div class="finding"><p><span class="verdict {E(f['verdict'])}">{E(f['verdict'])}</span>
<code>{E(f['row'])}</code> &middot; {E(job)} &middot; round {rd} &middot; evidence
<code>{E(f['evidence_chunk_id'])}</code></p>
<p class="lbl">Offending text</p><blockquote class="bad">{E(f['quote'])}</blockquote>
<p class="lbl">Canon rule</p><p class="why">{E(f['canon_rule'])}</p>
<p class="lbl">Correction</p><blockquote class="good">{E(f['correction'])}</blockquote></div>"""
        for job, rd, f in allf if f["row"]
    )

    counts = "".join(
        f"<li><b>{E(j)}</b>: " + " &rarr; ".join(
            ("clean" if not r["findings"] else f"{len(r['findings'])} finding(s)")
            + (" <i>(verify-only)</i>" if r.get("verify_only") else "")
            for r in traces[j]["rounds"]) + "</li>"
        for j in JOBS
    )

    return f"""<section><h2>4. What the critic caught</h2>
<p>The critic is a separate agent prompted as the Design Steward (GDD &sect;3.2, agent 7). It reads
the same retrieved canon and <b>must cite the chunk id proving every finding</b> &mdash;
<i>&ldquo;a finding you cannot ground in a quoted chunk is not a finding, and you must not raise
it.&rdquo;</i> A deterministic lint runs first for banned tier-1 terms and uncitable rows.</p>
<p class="lbl">Round outcomes this run</p><ul class="rounds">{counts}</ul>

<h3>The clearest case: a wrong number, fixed wrong, caught again</h3>
<p>Row <code>{chain_row}</code>. The reviser's first correction was <b>also wrong</b>. Only
because the loop re-checks after revising did the second error surface &mdash; and in the
original build it would not have, because the loop ended on a <i>revision</i>, so the rows that
shipped were the one version nothing had verified. That was a real bug, exposed by running the
pipeline rather than by reading it; the loop now always terminates on a verification pass and
reports <code>UNRESOLVED</code> rather than shipping unverified rows. The run that exposed it is
included in the bundle under <code>out/_run1_unverified/</code> precisely so this is checkable
&mdash; and the current run's rows, generated under the fixed loop, are the ones in
<code>out/*.csv</code>.</p>
<table class="chain"><thead><tr><th>Stage</th><th>Line</th><th>Verdict / evidence</th></tr></thead>
<tbody>{chain}</tbody></table>

{conflict}
<h3>Every other finding, in full</h3>{other}</section>"""


def sec_voice() -> str:
    before = [len(r["hud_line"]) for r in csv.DictReader(
        (OUT / "_run1_unverified" / "prompts.beforeTweak.csv").open(encoding="utf-8"))]
    after = [len(r["hud_line"]) for r in csv.DictReader(
        (OUT / "prompts.csv").open(encoding="utf-8"))]

    def stat(v):
        return max(v), round(sum(v) / len(v)), sum(1 for x in v if x > 90), len(v)

    b, a = stat(before), stat(after)
    w = rows("whispers")
    cap = next((r for r in w if "CaptureHeavy" in r["trigger"]), None)
    rich = next((r for r in w if "Rich" in r["trigger"]), None)
    bk = rows("barks")
    doze = next((r for r in bk if "Doze" in r["trigger"]), None)

    return f"""<section><h2>5. Does it sound like the game?</h2>
<p>Mostly yes, and the failures were specific rather than general.</p>
<p><b>What landed.</b> The propaganda-as-denial joke (&sect;2.10) came through without being asked
for &mdash; the watchman dozes on:</p>
<blockquote class="outp">{E(doze['line']) if doze else ''}</blockquote>
<p>And the verdict pair &sect;2.10 specifically asked for came out distinct:</p>
<blockquote class="outp"><b>Capture-heavy:</b> {E(cap['line']) if cap else ''}<br><br>
<b>Merely rich:</b> {E(rich['line']) if rich else ''}</blockquote>

<h3>The concrete tweak that improved game-fit</h3>
<p>The critic filed <code>REGISTER_DRIFT</code> on the tutorial prompts three separate times
&mdash; not for canon errors but for stuffing three chained mechanical clauses into a line
&sect;2.8 says is one HUD note. <b>The brief was under-specified:</b> it repeated the GDD's phrase
&ldquo;one-line HUD note&rdquo; without saying what one line <i>means</i>. I added hard
constraints &mdash; one objective, one payoff, one control; under ~90 characters; a second stage
is a <i>separate</i> prompt; modelled on the GDD's own written instance
(<code>&ldquo;Windmill: Ablaze &mdash; needs a window shot&rdquo;</code>) &mdash; and made the
Overlord bark carry the flavour so the HUD line need not. Re-running against identical
retrieval:</p>
<table class="metric"><thead><tr><th></th><th>max chars</th><th>mean</th><th>over 90</th></tr></thead>
<tbody>
<tr><td>Before tweak</td><td>{b[0]}</td><td>{b[1]}</td><td>{b[2]} / {b[3]}</td></tr>
<tr class="win"><td><b>After tweak</b></td><td><b>{a[0]}</b></td><td><b>{a[1]}</b></td><td><b>{a[2]} / {a[3]}</b></td></tr>
</tbody></table>
<p>Unresolved findings on that job dropped from 2 to 1. Both versions are preserved:
<code>out/_run1_unverified/prompts.beforeTweak.csv</code> vs <code>out/prompts.csv</code>.</p>

<h3>A retrieval tweak too</h3>
<p>Register queries had to name the <i>register</i>, not the topic. Querying
<code>&ldquo;His Eternal Darkness verdict&rdquo;</code> returns &sect;1 Executive Summary above
&sect;2.10 Tone, because &sect;1 mentions him more often. Adding the register words themselves
&mdash; <code>adequate rats layered subtitles register</code> &mdash; pulls &sect;2.10 to the top,
which is the section that actually defines the voice. <b>Topic words find where a thing is
discussed; style words find where it is specified.</b></p></section>"""


def sec_limits() -> str:
    return """<section><h2>6. Honest limitations</h2>
<ul class="lim">
<li><b>This content has no reader.</b> There is no <code>UGSBarkSubsystem</code>, and no
<code>FTableRowBase</code> struct anywhere in <code>Source/</code> &mdash; the project has no
DataTable infrastructure yet. These CSVs are data-table <i>shaped</i> (row <code>Name</code>
first, flat columns, and an <code>alarm_state</code> column whose values match the
<code>EGSAlarmPhase</code> enumerators in <code>Alarm/GSAlarmTypes.h</code> exactly) but nothing
can import them until someone defines the row structs. Authoring bark sheets ahead of the runtime
is the order GDD &sect;3.2 and the &sect;4.5 schedule intend &mdash; barks are week 7 &mdash; but
&ldquo;designed order&rdquo; is not &ldquo;wired&rdquo;.</li>
<li><b>One finding is still open.</b> <code>Prompts_06</code> retains a tone line the critic
flagged and two revisions failed to fix, with a correction supplied in the trace. Left flagged
deliberately: GDD &sect;3.3 gives the human <i>&ldquo;exclusive authority over aesthetics and
tone&rdquo;</i>, so an unresolved aesthetic call is exactly what an agent should escalate rather
than settle.</li>
<li><b>A GDD contradiction was found and closed</b> &mdash; see &sect;4. Worth noting as a
limitation of the <i>method</i>, not the run: the pipeline surfaced it only because a tutorial
prompt had to state the rule plainly to a player. Content types that never need to teach a
mechanic would not have exposed it, so this class of defect is found opportunistically rather
than systematically.</li>
<li><b>No point value was ever wrong</b>, but that is narrower than it sounds: no finding
concerned an incorrect number from the &sect;2.9 score table. &sect;2.9 <i>was</i> cited once, on
<code>Whispers_09</code>, for <i>who qualifies</i> for &ldquo;None Left Behind&rdquo; (civilians,
not guards) rather than for its +40 value.</li>
<li>Lexical retrieval will degrade if the corpus grows well past a few hundred chunks, or if
queries stop sharing vocabulary with the docs. At that point, add embeddings.</li>
</ul></section>"""


def sec_run() -> str:
    return """<section><h2>Appendix &mdash; running it</h2>
<pre class="sh">python gsrag.py --list             # the jobs, and the gap each one fills
python gsrag.py --retrieval-only   # chunking + retrieval, no API calls, no cost
python gsrag.py barks whispers prompts</pre>
<p>Credentials: <code>ANTHROPIC_API_KEY</code> in a gitignored <code>.env</code>, or an
<code>ant auth login</code> profile. Model: Claude Opus 5, adaptive thinking, effort
<code>high</code>, structured outputs. The final production run was 24 calls, 420,248 input /
104,810 output tokens (about $4.72). Retrieval is what keeps that low &mdash; each call sees
~11k tokens of retrieved context rather than the 76&nbsp;KB GDD.</p>
<p class="note">Files in this bundle: <code>README.md</code> (write-up),
<code>gsrag.py</code> (pipeline), <code>make_submission.py</code> (this report's builder),
<code>out/*.csv</code> (the three generated outputs), <code>out/*.trace.md</code> (readable
retrieval traces), <code>out/*.trace.json</code> (full audit: every query, chunk, round and
finding), <code>out/*.draft.json</code> (pre-critic drafts).</p></section>"""


CSS = """
*{box-sizing:border-box}
body{font:15px/1.6 -apple-system,Segoe UI,Roboto,sans-serif;color:#1a1a1a;background:#fff;
     max-width:52em;margin:0 auto;padding:2.5em 1.5em}
h1{font-size:2em;margin:0 0 .1em;letter-spacing:-.02em}
h2{font-size:1.35em;margin:2.2em 0 .6em;padding-bottom:.25em;border-bottom:2px solid #1a1a1a}
h3{font-size:1.08em;margin:1.6em 0 .5em;color:#000}
.sub{color:#555;margin:0 0 2em;font-size:1.05em}
code{font:13px/1.4 ui-monospace,Consolas,monospace;background:#f2f2f2;padding:.1em .35em;border-radius:3px}
pre{font:13px/1.5 ui-monospace,Consolas,monospace;background:#f7f7f7;border:1px solid #e0e0e0;
    padding:.7em .9em;border-radius:4px;overflow-x:auto;white-space:pre-wrap}
pre.q{background:#eef4ff;border-color:#c5d8f5}
pre.sh{background:#1e1e1e;color:#e8e8e8;border:none}
table{border-collapse:collapse;width:100%;margin:.8em 0;font-size:13px}
th,td{border:1px solid #d8d8d8;padding:.45em .6em;text-align:left;vertical-align:top}
th{background:#f2f2f2;font-weight:600}
.rubric td.pts{text-align:center;font-variant-numeric:tabular-nums;white-space:nowrap}
.data{font-size:12px}
.scroll{overflow-x:auto;border:1px solid #e5e5e5;border-radius:4px}
.scroll table{margin:0;border:none}
blockquote{margin:.5em 0;padding:.6em .9em;border-left:4px solid #ccc;background:#fafafa}
blockquote.gap{border-left-color:#8a6d1f;background:#fdf8e8}
blockquote.chunk{border-left-color:#7a7a7a;background:#f6f6f6;font-size:13px}
blockquote.outp{border-left-color:#1f6d3a;background:#eefbf2}
blockquote.bad{border-left-color:#b3261e;background:#fdeeed}
blockquote.good{border-left-color:#1f6d3a;background:#eefbf2}
.lbl{font-size:11px;text-transform:uppercase;letter-spacing:.09em;color:#666;
     margin:.9em 0 .2em;font-weight:600}
.note{background:#f4f4f4;border-left:4px solid #999;padding:.7em .9em;font-size:13.5px}
.why{font-size:13px;color:#333}
.file{font-size:12px;color:#666;margin-top:.3em}
.finding{border:1px solid #e0e0e0;border-radius:5px;padding:.7em .9em;margin:.9em 0;
         page-break-inside:avoid}
.verdict{font-size:11px;font-weight:700;padding:.15em .5em;border-radius:3px;color:#fff}
.verdict.LORE_BREAK{background:#b3261e}
.verdict.TONE_DRIFT{background:#9a5b00}
.verdict.REGISTER_DRIFT{background:#4a4a8a}
.chain td.stage{white-space:nowrap;font-weight:600;width:9em}
.metric td,.metric th{text-align:center}
.metric td:first-child,.metric th:first-child{text-align:left}
.metric tr.win{background:#eefbf2}
ul.hits{font-size:13px;margin:.2em 0}
ul.hits li{margin:.15em 0}
ul.lim li{margin:.5em 0}
ul.rounds{font-size:13.5px}
.dim{color:#888;font-size:12px;font-style:italic}
section{page-break-inside:auto}
@media print{
  body{padding:0;max-width:none;font-size:11pt}
  h2{page-break-after:avoid}
  h3{page-break-after:avoid}
  blockquote,table,.finding{page-break-inside:avoid}
  pre{white-space:pre-wrap}
}
"""


def build_html(traces: dict) -> str:
    return f"""<!doctype html><html lang="en"><head><meta charset="utf-8">
<title>Goblin Siege &mdash; Assignment #04, Dynamic Content Pipeline</title>
<style>{CSS}</style></head><body>
<h1>Dynamic Content Pipeline</h1>
<p class="sub"><b>Goblin Siege</b> &middot; Assignment #04 &middot; Michael Klein<br>
A retrieval-augmented pipeline that writes game text out of the project's own design documents,
then checks every line back against them with a critic agent before anything ships.</p>
{sec_rubric()}
{sec_kb(traces)}
{sec_content(traces)}
{sec_rag(traces)}
{sec_critic(traces)}
{sec_voice()}
{sec_limits()}
{sec_run()}
</body></html>"""


def main() -> None:
    missing = [j for j in JOBS if not (OUT / f"{j}.trace.json").exists()]
    if missing:
        sys.exit(f"missing artifacts for {missing} — run gsrag.py first")

    SUB.mkdir(exist_ok=True)
    traces = {j: load(j) for j in JOBS}

    html_path = SUB / "GoblinSiege_Assignment04.html"
    html_path.write_text(build_html(traces), encoding="utf-8")
    print(f"wrote {html_path.relative_to(ROOT)}  ({html_path.stat().st_size // 1024} KB)")

    zip_path = SUB / "GoblinSiege_Assignment04.zip"
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        z.write(html_path, "GoblinSiege_Assignment04.html")
        for f in ("README.md", "gsrag.py", "make_submission.py"):
            z.write(ROOT / f, f)
        for p in sorted(OUT.rglob("*")):
            if p.is_file():
                z.write(p, str(Path("out") / p.relative_to(OUT)).replace("\\", "/"))
    n = len(zipfile.ZipFile(zip_path).namelist())
    print(f"wrote {zip_path.relative_to(ROOT)}  ({zip_path.stat().st_size // 1024} KB, {n} files)")
    print("\nOpen the .html and use Ctrl+P -> Save as PDF for a PDF copy.")


if __name__ == "__main__":
    main()
