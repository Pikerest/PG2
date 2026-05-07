#!/usr/bin/env python3
"""Generate truncated icosahedron OBJ for game hub interior (normals facing inward)."""
import math
from collections import defaultdict

PHI = (1 + math.sqrt(5)) / 2

ICOSA_VERTS = [
    ( 0,  1,  PHI), ( 0, -1,  PHI), ( 0,  1, -PHI), ( 0, -1, -PHI),
    ( 1,  PHI,  0), (-1,  PHI,  0), ( 1, -PHI,  0), (-1, -PHI,  0),
    ( PHI,  0,  1), (-PHI,  0,  1), ( PHI,  0, -1), (-PHI,  0, -1),
]

# CCW winding viewed from outside (outward normals)
ICOSA_FACES = [
    (0, 1, 8),  (0, 8, 4),  (0, 4, 5),  (0, 5, 9),  (0, 9, 1),
    (1, 6, 8),  (8, 6, 10), (8, 10, 4), (4, 10, 2), (4, 2, 5),
    (5, 2, 11), (5, 11, 9), (9, 11, 7), (9, 7, 1),  (1, 7, 6),
    (3, 6, 7),  (3, 7, 11), (3, 11, 2), (3, 2, 10), (3, 10, 6),
]

trunc_verts = []
edge_map = {}

def get_tv(a, b):
    if (a, b) not in edge_map:
        pa, pb = ICOSA_VERTS[a], ICOSA_VERTS[b]
        x = pa[0]*2/3 + pb[0]/3
        y = pa[1]*2/3 + pb[1]/3
        z = pa[2]*2/3 + pb[2]/3
        edge_map[(a, b)] = len(trunc_verts)
        trunc_verts.append((x, y, z))
    return edge_map[(a, b)]

# Hexagonal faces from each icosahedron triangle (6 verts, CCW from outside)
hex_faces = []
for a, b, c in ICOSA_FACES:
    hex_faces.append([
        get_tv(a,b), get_tv(b,a), get_tv(b,c),
        get_tv(c,b), get_tv(c,a), get_tv(a,c),
    ])

# Pentagonal faces around each icosahedron vertex
vert_faces = defaultdict(list)
for fi, (a, b, c) in enumerate(ICOSA_FACES):
    for v in (a, b, c):
        vert_faces[v].append(fi)

def ordered_faces_around(v):
    faces = list(vert_faces[v])
    ordered = [faces[0]]
    while len(ordered) < 5:
        fi = ordered[-1]
        fa, fb, fc = ICOSA_FACES[fi]
        vi = [fa, fb, fc].index(v)
        next_nb = [fa, fb, fc][(vi + 1) % 3]
        for fi2 in faces:
            if fi2 not in ordered:
                fa2, fb2, fc2 = ICOSA_FACES[fi2]
                if next_nb in (fa2, fb2, fc2):
                    ordered.append(fi2)
                    break
    return ordered

pent_faces = []
for v in range(12):
    ord_fi = ordered_faces_around(v)
    pent = []
    for fi in ord_fi:
        fa, fb, fc = ICOSA_FACES[fi]
        vi = [fa, fb, fc].index(v)
        first_nb = [fa, fb, fc][(vi + 1) % 3]
        pent.append(get_tv(v, first_nb))
    pent_faces.append(pent)

assert len(trunc_verts) == 60, f"Expected 60 verts, got {len(trunc_verts)}"

# Normalize to sphere of radius R
R = 17.0
def norm(v, r):
    x, y, z = v
    d = math.sqrt(x*x + y*y + z*z)
    return (x/d*r, y/d*r, z/d*r)

verts = [norm(v, R) for v in trunc_verts]

def suv(x, y, z):
    r = math.sqrt(x*x + y*y + z*z)
    u = 0.5 + math.atan2(z, x) / (2 * math.pi)
    vv = 0.5 - math.asin(max(-1.0, min(1.0, y / r))) / math.pi
    return (u, vv)

# Fan-triangulate with REVERSED winding so normals face inward
def fan_tris_flip(poly):
    a = poly[0]
    return [(a, poly[i+1], poly[i]) for i in range(1, len(poly) - 1)]

lines = [
    '# Truncated icosahedron hub shell — inward-facing normals',
    f'# {len(verts)} vertices, {len(hex_faces)} hexagons + {len(pent_faces)} pentagons (triangulated)',
    '',
]

for x, y, z in verts:
    lines.append(f'v {x:.6f} {y:.6f} {z:.6f}')
lines.append('')

for x, y, z in verts:
    u, vv = suv(x, y, z)
    lines.append(f'vt {u:.6f} {vv:.6f}')
lines.append('')

for x, y, z in verts:
    r = math.sqrt(x*x + y*y + z*z)
    lines.append(f'vn {-x/r:.6f} {-y/r:.6f} {-z/r:.6f}')
lines.append('')

def fline(a, b, c):
    def t(i): return f'{i+1}/{i+1}/{i+1}'
    return f'f {t(a)} {t(b)} {t(c)}'

for h in hex_faces:
    for tri in fan_tris_flip(h):
        lines.append(fline(*tri))

for p in pent_faces:
    for tri in fan_tris_flip(p):
        lines.append(fline(*tri))

out = 'objects/icosahedron_hub.obj'
with open(out, 'w') as f:
    f.write('\n'.join(lines) + '\n')

print(f'Written {len(verts)} verts, {len(hex_faces)*4 + len(pent_faces)*3} triangles to {out}')
