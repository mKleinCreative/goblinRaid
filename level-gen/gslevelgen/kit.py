"""
The Dreamscape modular kit, as measured — never as assumed.

`kit_manifest.py` runs once inside the editor and writes level-gen/kit.json with the real
bounds of every piece. This module loads it and answers "what is a wall", "how big is a
module", "which mesh do I place here".

THE RULE THIS MODULE ENFORCES
-----------------------------
No dimension used by the generator may be a number someone typed. `tools/hamlet/gs_buildings.py`
is the cautionary tale: four rounds of clustering kit pieces by a tuned distance, every value
trading one failure for another, "because distance was never the right question."

So: if kit.json is missing, `load_kit()` raises. A synthetic kit exists for unit tests only, and
plans built from it are stamped `synthetic: true` and refused by the applier.
"""

from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
KIT_JSON = ROOT / "kit.json"


class KitNotMeasured(RuntimeError):
    pass


@dataclass
class Piece:
    name: str
    path: str
    size: tuple[float, float, float]
    pivot_from_min: tuple[float, float, float]

    @property
    def footprint(self) -> tuple[float, float]:
        return (self.size[0], self.size[1])


# Role patterns, matched against the mesh name. Ordered: first match wins, so the more
# specific pattern must come first (a "Wall_5x4_Window_A_01" is a wall, not a window).
ROLE_PATTERNS: list[tuple[str, str]] = [
    ("foundation", r"^SM_House_Foundation"),
    ("floor", r"^SM_House_Floor"),
    ("roof_corner_inner", r"^SM_House_Roof_\d+_Corner_Inner"),
    ("roof_corner_outer", r"^SM_House_Roof_\d+_Corner_Outer"),
    ("roof_end", r"^SM_House_Roof_\d+_End"),
    ("roof_tile", r"^SM_House_Roof_\d+_Tiling"),
    ("roof_wallbottom", r"^SM_House_Roof_\d+_WallBottom"),
    ("wall_window", r"^SM_House_Wall_5x4_Window"),
    ("wall_interior", r"^SM_House_Wall_(5x4_)?Interior"),
    ("corner", r"^SM_House_Corner"),
    ("door", r"^SM_Door_"),
    ("window", r"^SM_House_Window_"),
    ("stairs", r"^SM_Stairs_"),
    ("balcony", r"^SM_Balcony"),
    ("pillar", r"^SM_Pillar_"),
    ("terrace", r"^SM_Terrace_"),
    # Whole-prefab structures — the settlement layer places these directly.
    ("barn", r"^SM_Barn$"),
    ("coop", r"^SM_Chicken_(Coop|Platform)$"),
    ("market", r"^SM_(MarketStall|MarketTable|StallCover)"),
    ("well", r"^SM_Well(Roof)?$"),
    ("windmill", r"^SM_W[Ii]ndmill_"),
    ("village_wall", r"^SM_VillageWall"),
    ("stone_wall", r"^SM_StoneWall"),
]


def classify(name: str) -> str:
    for role, pat in ROLE_PATTERNS:
        if re.match(pat, name):
            return role
    return "other"


@dataclass
class Kit:
    pieces: dict[str, Piece]
    synthetic: bool = False
    by_role: dict[str, list[Piece]] = field(default_factory=dict)

    def __post_init__(self):
        self.by_role = {}
        for p in self.pieces.values():
            self.by_role.setdefault(classify(p.name), []).append(p)
        for lst in self.by_role.values():
            lst.sort(key=lambda p: p.name)

    def role(self, role: str) -> list[Piece]:
        return self.by_role.get(role, [])

    def one(self, role: str) -> Piece:
        lst = self.role(role)
        if not lst:
            raise KeyError(f"kit has no piece in role '{role}'")
        return lst[0]

    @property
    def module(self) -> tuple[float, float]:
        """
        The grid module: the measured footprint of the piece that actually TILES a floor.

        This is `SM_House_Floor_5x4_01`, and measuring it corrected a real mistake. The
        obvious candidate is `SM_House_Foundation_5x4` — it is named for the grid and it
        sounds like a floor plate. It measures [500, 50, 300]: fifty centimetres deep and
        three metres tall. It is a perimeter foundation *wall*, not a plate, and deriving a
        module from it gives a 500 x 50 grid that no house could ever sit on.

        The floor measures [500.75, 500.0, 36]. The module is square, and "5x4" in the name
        describes neither dimension in metres. Exactly why nothing here is inferred from a
        name.
        """
        for p in self.role("floor"):
            if re.match(r"^SM_House_Floor_5x4_01$", p.name):
                return (round(p.footprint[0], 2), round(p.footprint[1], 2))
        for p in self.role("floor"):          # any plain floor plate will do
            if "Hatch" not in p.name and "Overang" not in p.name and "Beam" not in p.name:
                return (round(p.footprint[0], 2), round(p.footprint[1], 2))
        raise KitNotMeasured(
            "kit has no floor plate — the module grid is derived from the piece that tiles a "
            "floor, and no foundation/wall piece is a substitute for it"
        )


def load_kit(path: Path | None = None) -> Kit:
    path = path or KIT_JSON
    if not path.exists():
        raise KitNotMeasured(
            f"{path} not found. The generator will not guess kit dimensions.\n"
            f"Measure them once, in the editor:\n"
            f"    python gs_ue.py level-gen\\kit_manifest.py --timeout 300"
        )
    data = json.loads(path.read_text(encoding="utf-8"))
    pieces = {
        m["name"]: Piece(
            name=m["name"],
            path=m["path"],
            size=tuple(m["size_cm"]),
            pivot_from_min=tuple(m["pivot_from_min_cm"]),
        )
        for m in data["meshes"]
    }
    return Kit(pieces=pieces, synthetic=bool(data.get("synthetic")))


def synthetic_kit() -> Kit:
    """
    A fake kit with round numbers, FOR TESTS ONLY.

    Exists so the solver's logic can be exercised with the editor closed. Everything built
    from it is stamped synthetic and `apply_in_editor.py` refuses to place it — the numbers
    here are invented, which is exactly what the real pipeline is forbidden from doing.
    """
    def mk(name, w, d, h):
        return Piece(name=name, path=f"/Synthetic/{name}", size=(w, d, h),
                     pivot_from_min=(w / 2, d / 2, 0.0))

    names = [
        ("SM_House_Foundation_5x4", 500, 400, 50),
        ("SM_House_Floor_5x4_01", 500, 400, 20),
        ("SM_House_Wall_5x4_Window_A_01", 500, 20, 400),
        ("SM_House_Wall_5x4_Window_B_01", 500, 20, 400),
        ("SM_House_Corner_01", 20, 20, 400),
        ("SM_House_Roof_01_Tiling_Base", 500, 400, 100),
        ("SM_House_Roof_01_Tiling_Top", 500, 400, 100),
        ("SM_House_Roof_01_End_Base", 250, 400, 100),
        ("SM_House_Roof_01_Corner_Outer_Standard", 250, 250, 100),
        ("SM_Door_A", 120, 20, 250),
        ("SM_Barn", 1200, 900, 700),
        ("SM_Chicken_Coop", 400, 300, 250),
        ("SM_MarketStallStructure", 300, 300, 300),
        ("SM_Well", 200, 200, 150),
        ("SM_WIndmill_Base", 900, 900, 1600),
        ("SM_Windmill_Sail", 1200, 100, 1200),
    ]
    return Kit(pieces={n: mk(n, w, d, h) for n, w, d, h in names}, synthetic=True)
