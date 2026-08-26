"""Strip the painted fake-transparency checkerboard and give the art real alpha.

The checker is hand-painted, so no rigid grid keys it, and its light band is the
same grey as the pewter fittings, so no colour range separates them either. What
does separate them is local bimodality: every window of checker holds both band
values and almost nothing else, where painted metal is a continuous gradient.
"""
import numpy as np, sys, os
from PIL import Image
from collections import deque

CHROMA = 22     # above this saturation a pixel is paint, never checker
BANDTOL = 13    # how close to a band value a pixel counts as that band
GROW   = 3      # px of antialiased fringe to nibble off afterwards

def box(a, r):
    """Mean over a (2r+1) square, via summed-area table."""
    h, w = a.shape
    p = np.pad(a.astype(np.float64), r+1, mode='edge')
    c = np.pad(p.cumsum(0).cumsum(1), ((1,0),(1,0)))
    k = 2*r+1
    s = c[k:k+h, k:k+w] - c[0:h, k:k+w] - c[k:k+h, 0:w] + c[0:h, 0:w]
    return s / (k*k)

def bands(grey, achrom):
    m = np.zeros(grey.shape, bool)
    m[:16,:] = m[-16:,:] = m[:,:16] = m[:,-16:] = True
    v = grey[m & achrom].astype(int)
    if v.size < 500: return None
    h = np.convolve(np.bincount(v, minlength=256).astype(float), np.ones(5)/5, 'same')
    p1 = int(h.argmax()); g = h.copy(); g[max(0,p1-14):p1+15] = 0; p2 = int(g.argmax())
    if g[p2] < h[p1]*0.05: return None
    return min(p1,p2), max(p1,p2)

def cell_size(grey, lo, hi):
    """Median run length along the border strips -> checker cell size."""
    mid = (lo+hi)/2.0; sizes = []
    for sig in (grey[:14,:].mean(0), grey[-14:,:].mean(0),
                grey[:,:14].mean(1), grey[:,-14:].mean(1)):
        b = sig > mid; runs = []; st = 0
        for i in range(1, len(b)):
            if b[i] != b[i-1]: runs.append(i-st); st = i
        runs = [r for r in runs[1:-1] if 3 <= r <= 90]
        if len(runs) >= 3: sizes.append(np.median(runs))
    return int(round(float(np.median(sizes)))) if sizes else 12

def components(mask):
    h, w = mask.shape
    lab = np.full(h*w, -1, np.int32); flat = mask.ravel(); comps = []
    for s in np.flatnonzero(flat):
        if lab[s] != -1: continue
        cid = len(comps); q = deque([int(s)]); lab[s] = cid; px = []
        while q:
            i = q.popleft(); px.append(i); y, x = divmod(i, w)
            for j, ok in ((i-1,x>0),(i+1,x<w-1),(i-w,y>0),(i+w,y<h-1)):
                if ok and flat[j] and lab[j] == -1: lab[j] = cid; q.append(j)
        comps.append(np.array(px, np.int64))
    return comps

def dilate(m, allow, n):
    for _ in range(n):
        g = np.zeros_like(m)
        g[1:,:] |= m[:-1,:]; g[:-1,:] |= m[1:,:]
        g[:,1:] |= m[:,:-1]; g[:,:-1] |= m[:,1:]
        m = m | (g & allow)
    return m

def dechecker(path, out):
    im = Image.open(path).convert("RGB")
    arr = np.asarray(im); rgb = arr.astype(np.int16)
    h, w, _ = rgb.shape
    grey = rgb.mean(2)
    achrom = (rgb.max(2) - rgb.min(2)) <= CHROMA
    bp = bands(grey, achrom)
    if bp is None:
        print(f"  ! {os.path.basename(path)}: no checker detected, passed through")
        im.convert("RGBA").save(out); return
    lo, hi = bp
    cs = cell_size(grey, lo, hi)
    r = int(max(6, min(34, round(cs * 1.15))))     # window spans about two cells

    # The baked drop shadow dims both bands by roughly the same factor, so the
    # pooled shadow in concave corners is still checker - just darker. Test at a
    # few scales and take the union.
    bimodal = np.zeros(grey.shape, bool)
    for k in (1.0, 0.88, 0.76, 0.64, 0.54):
        f_lo = box((np.abs(grey - lo*k) <= BANDTOL).astype(np.float32), r)
        f_hi = box((np.abs(grey - hi*k) <= BANDTOL).astype(np.float32), r)
        bimodal |= (f_lo > 0.22) & (f_hi > 0.22) & (f_lo + f_hi > 0.80)
    checker = achrom & bimodal

    keep = np.zeros(h*w, bool)
    for c in components(checker):
        if c.size >= 250: keep[c] = True           # drop speckle inside the art
    C = keep.reshape(h, w)

    # Anything unmistakably painted: saturated, or darker than the checker can go.
    A = (((rgb.max(2) - rgb.min(2)) > 55) | (grey < lo*0.54 - 30)) & ~checker

    # The halo between them is a blend and belongs to neither test. Race the two
    # fronts through it: the outline seals the metal off, so only the halo is
    # reachable from the checker side.
    lab = np.full(h*w, -1, np.int8)
    lab[C.ravel()] = 0
    lab[A.ravel() & ~C.ravel()] = 1
    q = deque(int(i) for i in np.flatnonzero(lab >= 0))
    while q:
        i = q.popleft(); v = lab[i]; y, x = divmod(i, w)
        for j, ok in ((i-1,x>0),(i+1,x<w-1),(i-w,y>0),(i+w,y<h-1)):
            if ok and lab[j] == -1:
                lab[j] = v; q.append(j)
    bg = (lab == 0).reshape(h, w)

    # Drop opaque specks stranded in the background.
    solid = ~bg
    for c in components(solid):
        if c.size < 400: bg.ravel()[c] = True

    # The baked drop shadow darkens the checker enough to read as paint, leaving a
    # thin rim. Shave it off the silhouette, then feather so the edge is not jagged.
    op = ~bg
    for _ in range(3):
        e = op.copy()
        e[1:,:] &= op[:-1,:]; e[:-1,:] &= op[1:,:]
        e[:,1:] &= op[:,:-1]; e[:,:-1] &= op[:,1:]
        op = e
    a = op.astype(np.float32)
    for _ in range(2):
        a = box(a, 1)
    rgba = np.dstack([arr, np.clip(a*255, 0, 255).astype(np.uint8)])
    img = Image.fromarray(rgba, "RGBA")
    bb = img.getbbox()
    if bb: img = img.crop(bb)
    img.save(out)
    print(f"  {os.path.basename(out):32s} bands={lo}/{hi} cell={cs}px "
          f"bg={bg.mean()*100:4.1f}% -> {img.size[0]}x{img.size[1]}")

if __name__ == "__main__":
    dechecker(sys.argv[1], sys.argv[2])
