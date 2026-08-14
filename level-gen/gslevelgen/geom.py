"""
2D geometry for the settlement solver. Centimetres, top-down, +X east / +Y north.

Deliberately tiny and dependency-free: the whole GER loop has to run with the editor
closed (CLAUDE.md constraint 2 makes the editor a scarce, human-gated resource), so
nothing here may import `unreal` or numpy.
"""

from __future__ import annotations

import math
from dataclasses import dataclass


@dataclass(frozen=True)
class Rect:
    """Axis-aligned footprint. (x, y) is the min corner."""

    x: float
    y: float
    w: float
    h: float

    @property
    def x2(self) -> float:
        return self.x + self.w

    @property
    def y2(self) -> float:
        return self.y + self.h

    @property
    def center(self) -> tuple[float, float]:
        return (self.x + self.w / 2.0, self.y + self.h / 2.0)

    def inflated(self, m: float) -> "Rect":
        return Rect(self.x - m, self.y - m, self.w + 2 * m, self.h + 2 * m)

    def contains(self, px: float, py: float) -> bool:
        return self.x <= px <= self.x2 and self.y <= py <= self.y2

    def overlaps(self, other: "Rect", margin: float = 0.0) -> bool:
        a = self.inflated(margin)
        return not (a.x2 <= other.x or other.x2 <= a.x or a.y2 <= other.y or other.y2 <= a.y)

    def corners(self) -> list[tuple[float, float]]:
        return [(self.x, self.y), (self.x2, self.y), (self.x2, self.y2), (self.x, self.y2)]


@dataclass(frozen=True)
class Disc:
    """A copse, hedgerow clump or any round cover volume."""

    x: float
    y: float
    r: float

    def contains(self, px: float, py: float) -> bool:
        return (px - self.x) ** 2 + (py - self.y) ** 2 <= self.r * self.r


def seg_intersects_rect(ax: float, ay: float, bx: float, by: float, r: Rect) -> bool:
    """Segment A->B vs an AABB (slab method). Endpoints inside count as intersecting."""
    if r.contains(ax, ay) or r.contains(bx, by):
        return True
    dx, dy = bx - ax, by - ay
    t0, t1 = 0.0, 1.0
    for p, q in ((-dx, ax - r.x), (dx, r.x2 - ax), (-dy, ay - r.y), (dy, r.y2 - ay)):
        if abs(p) < 1e-9:
            if q < 0:
                return False           # parallel and outside this slab
            continue
        t = q / p
        if p < 0:
            if t > t1:
                return False
            t0 = max(t0, t)
        else:
            if t < t0:
                return False
            t1 = min(t1, t)
    return t0 <= t1


def seg_intersects_disc(ax: float, ay: float, bx: float, by: float, d: Disc) -> bool:
    """Closest approach of segment A->B to the disc centre."""
    dx, dy = bx - ax, by - ay
    L2 = dx * dx + dy * dy
    if L2 < 1e-9:
        return d.contains(ax, ay)
    t = max(0.0, min(1.0, ((d.x - ax) * dx + (d.y - ay) * dy) / L2))
    cx, cy = ax + t * dx, ay + t * dy
    return (cx - d.x) ** 2 + (cy - d.y) ** 2 <= d.r * d.r


def ring_points(cx: float, cy: float, radius: float, n: int) -> list[tuple[float, float]]:
    """n evenly spaced points on a circle — the treeline sampling ring."""
    return [
        (cx + radius * math.cos(2 * math.pi * i / n), cy + radius * math.sin(2 * math.pi * i / n))
        for i in range(n)
    ]


def flood_reachable(
    blockers: list[Rect],
    start: tuple[float, float],
    bounds: Rect,
    cell: float,
    margin: float = 0.0,
) -> set[tuple[int, int]]:
    """
    Grid flood fill from `start`, treating inflated blockers as solid.

    Coarse on purpose: this answers "can a goblin get there at all", not "what is the
    optimal path". The real navmesh is baked in-editor (GDD 4.1); this is the cheap
    pre-check that stops an unreachable objective ever reaching the editor.
    """
    nx = max(1, int(bounds.w // cell))
    ny = max(1, int(bounds.h // cell))
    solid = [b.inflated(margin) for b in blockers]

    def blocked(ix: int, iy: int) -> bool:
        px = bounds.x + (ix + 0.5) * cell
        py = bounds.y + (iy + 0.5) * cell
        return any(s.contains(px, py) for s in solid)

    sx = int((start[0] - bounds.x) // cell)
    sy = int((start[1] - bounds.y) // cell)
    sx = max(0, min(nx - 1, sx))
    sy = max(0, min(ny - 1, sy))

    seen: set[tuple[int, int]] = set()
    if blocked(sx, sy):
        # Nudge off a blocked start rather than declaring the whole map unreachable.
        for ox, oy in ((1, 0), (-1, 0), (0, 1), (0, -1), (2, 0), (0, 2)):
            if 0 <= sx + ox < nx and 0 <= sy + oy < ny and not blocked(sx + ox, sy + oy):
                sx, sy = sx + ox, sy + oy
                break
        else:
            return seen

    stack = [(sx, sy)]
    seen.add((sx, sy))
    while stack:
        ix, iy = stack.pop()
        for jx, jy in ((ix + 1, iy), (ix - 1, iy), (ix, iy + 1), (ix, iy - 1)):
            if 0 <= jx < nx and 0 <= jy < ny and (jx, jy) not in seen and not blocked(jx, jy):
                seen.add((jx, jy))
                stack.append((jx, jy))
    return seen


def cell_of(p: tuple[float, float], bounds: Rect, cell: float) -> tuple[int, int]:
    return (int((p[0] - bounds.x) // cell), int((p[1] - bounds.y) // cell))
