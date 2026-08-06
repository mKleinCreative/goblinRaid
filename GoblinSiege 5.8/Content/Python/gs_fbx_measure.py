import struct, zlib, sys

def read_node(b, off, ver):
    if ver >= 7500:
        end, nprop, plen = struct.unpack_from('<QQQ', b, off); off += 24
    else:
        end, nprop, plen = struct.unpack_from('<III', b, off); off += 12
    if end == 0:
        return None, off
    nlen = b[off]; off += 1
    name = b[off:off+nlen].decode('utf-8', 'replace'); off += nlen
    prop_start = off
    props = []
    for _ in range(nprop):
        t = chr(b[off]); off += 1
        if t in 'YCIFDL':
            fmt = {'Y':'<h','C':'<b','I':'<i','F':'<f','D':'<d','L':'<q'}[t]
            props.append(struct.unpack_from(fmt, b, off)[0])
            off += struct.calcsize(fmt)
        elif t in 'fdlib':
            alen, enc, clen = struct.unpack_from('<III', b, off); off += 12
            raw = b[off:off+clen]; off += clen
            if enc == 1:
                raw = zlib.decompress(raw)
            code = {'f':'f','d':'d','l':'q','i':'i','b':'b'}[t]
            props.append(list(struct.unpack('<%d%s' % (alen, code), raw)))
        elif t in 'SR':
            slen = struct.unpack_from('<I', b, off)[0]; off += 4
            props.append(b[off:off+slen]); off += slen
        else:
            raise ValueError('bad prop type %r at %d' % (t, off))
    off = prop_start + plen
    children = []
    null_sz = 25 if ver >= 7500 else 13
    if off < end:
        while off < end - null_sz + 1:
            c, off = read_node(b, off, ver)
            if c is None:
                break
            children.append(c)
        off = end
    return {'name': name, 'props': props, 'children': children}, end


def parse(path):
    b = open(path, 'rb').read()
    ver = struct.unpack_from('<I', b, 23)[0]
    off = 27
    roots = []
    while off < len(b) - 20:
        n, off = read_node(b, off, ver)
        if n is None:
            break
        roots.append(n)
    return ver, roots

def collect(n, name, out):
    if n['name'] == name and n['props']:
        out.append(n['props'][0])
    for c in n['children']:
        collect(c, name, out)

def scale_factors(roots):
    """UnitScaleFactor from GlobalSettings, and any Lcl Scaling on models."""
    info = {}
    for r in roots:
        if r['name'] != 'GlobalSettings':
            continue
        for c in r['children']:
            if c['name'] != 'Properties70':
                continue
            for p in c['children']:
                if p['props'] and p['props'][0] in (b'UnitScaleFactor', b'OriginalUnitScaleFactor'):
                    info[p['props'][0].decode()] = p['props'][-1]
    return info

def main(path):
    ver, roots = parse(path)
    verts = []
    for r in roots:
        collect(r, 'Vertices', verts)
    print('fbx version %d | meshes with Vertices: %d' % (ver, len(verts)))
    print('global settings:', scale_factors(roots))
    for i, v in enumerate(verts):
        xs = v[0::3]; ys = v[1::3]; zs = v[2::3]
        ext = (max(xs)-min(xs), max(ys)-min(ys), max(zs)-min(zs))
        print('  mesh %d: %d verts' % (i, len(xs)))
        print('    X %8.3f .. %8.3f   extent %8.3f' % (min(xs), max(xs), ext[0]))
        print('    Y %8.3f .. %8.3f   extent %8.3f' % (min(ys), max(ys), ext[1]))
        print('    Z %8.3f .. %8.3f   extent %8.3f' % (min(zs), max(zs), ext[2]))
        print('    longest axis: %.3f' % max(ext))

if __name__ == '__main__':
    main(sys.argv[1])
