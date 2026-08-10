# Climbing regression corpus - run inside the Unreal editor over MCP:
#     exec(open("D:/goblinRaid/GoblinSiege 5.8/Tools/ClimbCorpus/climb_corpus.py").read())
#
# WHY THIS EXISTS
# Three separate tickets changed climbing by reasoning about geometry instead of measuring it, and
# two of them shipped a wrong number (#062's 0.35 normal gate; #067's first -120 ledge inset, which
# aimed the top-out at interior floors). Both were caught by simulating the traces against real
# level geometry - never by reading the graph. So this freezes that simulation into something
# repeatable: a fixed set of named world points, both cascades run against each, and a table you
# diff after every change.
#
# The corpus deliberately includes points that MUST REJECT. A cascade that accepts everything is
# not a working cascade, and the mid-wall timber band is the single most important row here - it is
# what threw the player off the wall in climbingissues.mp4.
import unreal, math, json, os

W  = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
SL = unreal.SystemLibrary
TQ = unreal.TraceTypeQuery.ECC_VISIBILITY
HERE = os.path.dirname(os.path.abspath(__file__)) if "__file__" in dir() else \
       "D:/goblinRaid/GoblinSiege 5.8/Tools/ClimbCorpus"

# ---- character constants, measured off BP_GSPlayerCharacter's CDO -------------------------------
R, HH   = 52.0, 120.0      # capsule radius / half-height
OFFSET  = 70.0             # ClimbWallOffset. MUST exceed R or the capsule is born inside the wall
                           # (#067: offset 45 gave initial_overlap on 100% of 11 facades)

# ---- the CURRENT blueprint cascade, as authored today -------------------------------------------
CUR_INSET, CUR_TOP, CUR_BOT, CUR_GATE = -80.0, 580.0, 150.0, 0.35

# ---- the PROPOSED four-stage cascade ------------------------------------------------------------
SUSTAIN_MAX_NZ = 0.55      # above this the surface is no longer "wall"
TOPOUT_MAX_RISE= 180.0
S2_INBOARD     = 60.0      # probe INBOARD of the wall face; at the face you hit the eave, not the deck
S2_DROP        = 260.0     # how far below the lip to keep looking. 40 was far too shallow: once the
                           # capsule top clears the roof, a 40uu window ends above the deck entirely.

# DECK_MIN_NZ is READ FROM THE CHARACTER, never hardcoded.
#
# Three tickets have now guessed at this number and all three were wrong. #062 shipped 0.35 (a 70
# degree slope - unstandable). The rebuild plan specified 0.71 (the ENGINE default WalkableFloorZ)
# and the corpus proved that rejects 11 roofs out of 11. The actual answer was already on the
# character: BP_GSPlayerCharacter sets WalkableFloorAngle = 62 degrees, so WalkableFloorZ = 0.4695,
# and 9 of these 11 village roofs are standable. Someone raised that angle on purpose so the rooftop
# game would work. Read their setting instead of inventing one.
def _walkable_floor_z():
    eas = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
    bp  = eas.load_asset("/Game/Blueprints/BP_GSPlayerCharacter")
    cm  = unreal.get_default_object(bp.generated_class()).get_editor_property('character_movement')
    return cm.get_editor_property('walkable_floor_z')

DECK_MIN_NZ = _walkable_floor_z()

def _hit(h):
    if h is None: return None
    d = h.to_dict()
    return d if d['blocking_hit'] else None

def line(a, b):    return _hit(SL.line_trace_single(W, a, b, TQ, True, [], unreal.DrawDebugTrace.NONE, True))
def sph(a, b, r):  return _hit(SL.sphere_trace_single(W, a, b, r, TQ, True, [], unreal.DrawDebugTrace.NONE, True))
def cap(a, b, r, hh): return _hit(SL.capsule_trace_single(W, a, b, r, hh, TQ, True, [], unreal.DrawDebugTrace.NONE, True))

def V(x, y, z): return unreal.Vector(float(x), float(y), float(z))

def current_cascade(ax, ay, az, nx, ny):
    """What the Blueprint does today: one downward sphere probe, one normal gate, no fit check."""
    foot = az - HH
    px, py = ax + nx * CUR_INSET, ay + ny * CUR_INSET
    d = sph(V(px, py, foot + CUR_TOP), V(px, py, foot + CUR_BOT), 25.0)
    if not d:                       return False, "no hit"
    if d['initial_overlap']:        return False, "initial overlap"
    nz = d['impact_normal'].z
    if nz < CUR_GATE:               return False, "normal %.2f < %.2f" % (nz, CUR_GATE)
    return True, "ACCEPT nz=%.2f z=%.0f" % (nz, d['impact_point'].z)

def proposed_cascade(ax, ay, az, nx, ny):
    """Four stages. Stage 3 (capsule fit) is the one the Blueprint has never had."""
    top = az + HH
    # S1 - is there a lip at all?
    s1a = V(ax - nx*10 + 0, ay - ny*10, top + 20)
    s1b = V(ax - nx*70,     ay - ny*70, top + 20)
    d1 = cap(s1a, s1b, 40.0, 40.0)
    if d1 and d1['impact_normal'].z < SUSTAIN_MAX_NZ:
        return False, "S1 NoLip (wall continues, nz=%.2f)" % d1['impact_normal'].z
    # S2 - find the deck, INBOARD of the wall face and looking well below the lip
    lip = V(s1b.x - nx*S2_INBOARD, s1b.y - ny*S2_INBOARD, s1b.z)
    d2 = sph(V(lip.x, lip.y, lip.z + TOPOUT_MAX_RISE), V(lip.x, lip.y, lip.z - S2_DROP), 25.0)
    if not d2:                    return False, "S2 NoDeck (nothing above)"
    if d2['initial_overlap']:     return False, "S2 NoDeck (start penetrating)"
    nz = d2['impact_normal'].z
    if nz < DECK_MIN_NZ:          return False, "S2 NoDeck (nz=%.2f -> re-plane, not top-out)" % nz
    # S3 - DOES THE CAPSULE ACTUALLY FIT? the missing stage; the timber band dies here.
    #
    # Do NOT compute the standing position as deck + Up*HalfHeight. On a PITCHED roof that drives the
    # capsule into the uphill slope and every village roof fails: at nz 0.65 the surface climbs ~60uu
    # across the capsule radius, so a straight-up offset overlaps by more than the margin. Sweep a
    # capsule DOWN onto the deck instead and let it come to rest - that yields the true standing
    # position AND proves it fits, in one operation, on any slope.
    dp = d2['impact_point']
    s3a = V(dp.x, dp.y, dp.z + HH + 200.0)
    s3b = V(dp.x, dp.y, dp.z + HH - 40.0)
    d3 = cap(s3a, s3b, R, HH)
    if d3 is None:                return False, "S3 NoRoom (capsule never lands)"
    if d3['initial_overlap']:     return False, "S3 NoRoom (start penetrating)"
    fit = V(d3['location'].x, d3['location'].y, d3['location'].z)
    # S3b - is the deck deep enough to stand on, or a thin decorative band?
    #
    # The window must be generous in BOTH directions. On a pitched roof the surface RISES as you go
    # inboard, so a short downward-only probe looks underneath the roof and reports "no floor" - which
    # is how this stage rejected 8 of 11 real roofs on its first draft. Span from above the fit point
    # to well below it, so a rising deck, a level deck and a slightly falling deck all register.
    inx, iny = fit.x - nx*(R+30), fit.y - ny*(R+30)
    d3b = line(V(inx, iny, fit.z + 120.0), V(inx, iny, fit.z - HH - 60.0))
    if not d3b:                   return False, "S3b TooThin (no floor %.0fuu inward)" % (R+30)
    # S4 - can we actually get there?
    #
    # The vertical leg uses a SHRUNK capsule. The mantle is a motion-warped montage that arcs up and
    # over the lip, not a rigid vertical elevator, so testing the full radius against the eave rejects
    # legitimate mantles - the eave protrudes ~84uu on these houses and the full capsule always clips
    # it. Shrinking keeps the check honest about real obstructions while tolerating the brush.
    d4 = cap(V(ax, ay, az), V(ax, ay, fit.z), R * 0.6, HH)
    if d4:                        return False, "S4 PathBlocked (vertical leg)"
    d4b = cap(V(ax, ay, fit.z), fit, R, HH)
    if d4b:                       return False, "S4 PathBlocked (horizontal leg)"
    return True, "ACCEPT nz=%.2f rise=%.0f" % (nz, fit.z - az)

def first_accept(fn, p, lo=100, hi=1700, step=25):
    """Lowest climber height at which a cascade accepts a top-out. This is the number that matters:
    a cascade that never accepts is as broken as one that always does."""
    ax, ay = p['wx'] + p['nx']*OFFSET, p['wy'] + p['ny']*OFFSET
    for zr in range(lo, hi, step):
        ok, why = fn(ax, ay, p['base'] + zr, p['nx'], p['ny'])
        if ok: return zr, why
    return None, None

def run():
    pts = json.load(open(HERE + "/corpus_points.json"))
    print("CLIMB REGRESSION CORPUS   %d points   capsule r%.0f hh%.0f, offset %.0f" % (len(pts), R, HH, OFFSET))
    print("current gate nz>=%.2f  |  proposed deck nz>=%.2f + capsule fit" % (CUR_GATE, DECK_MIN_NZ))
    print("\nLowest climber height at which each cascade commits to a top-out.")
    print("The CURRENT column is where the player gets thrown off the wall; the PROPOSED column")
    print("should sit at the actual roofline, several hundred uu higher.\n")
    print("%-30s %-24s %-24s %s" % ("point", "CURRENT accepts at", "PROPOSED accepts at", "verdict"))
    print("-" * 122)
    never = premature = 0
    for p in pts:
        cz, cw = first_accept(current_cascade, p)
        pz, pw = first_accept(proposed_cascade, p)
        cs = ("Zrel %-5d %s" % (cz, cw.replace("ACCEPT ", "")))[:23] if cz is not None else "never"
        ps = ("Zrel %-5d %s" % (pz, pw.replace("ACCEPT ", "")))[:23] if pz is not None else "NEVER"
        if pz is None:
            verdict = "!! proposed never accepts"; never += 1
        elif cz is not None and pz - cz >= 150:
            verdict = "current fires %duu too low" % (pz - cz); premature += 1
        elif cz is None:
            verdict = "new top-out (current missed it)"
        else:
            verdict = "agree (within %duu)" % abs(pz - (cz or 0))
        print("%-30s %-24s %-24s %s" % (p['name'][:29], cs, ps, verdict))
    print("-" * 122)
    print("points where CURRENT commits a top-out >=150uu below the real roofline: %d / %d" % (premature, len(pts)))
    print("points where PROPOSED never finds a top-out at all:                     %d / %d" % (never, len(pts)))
    print("\nA premature commit is a fall: the player is warped onto something he cannot stand on,")
    print("the montage ends, MOVE_Walking finds no floor, and he drops. That is climbingissues.mp4.")
    print("A 'never' is the opposite failure - the roof becomes unreachable - and is equally a bug.")

run()
