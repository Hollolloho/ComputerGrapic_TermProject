"""Structural and proxy-layout checks, NOT runtime/player collision verification."""
from pathlib import Path
import json,hashlib,math
ROOT=Path(__file__).resolve().parents[1]; OUT=ROOT/'asset/corridor/test_scene'
m=json.loads((OUT/'scene_manifest.json').read_text())
assert hashlib.sha256((ROOT/'asset/corridor/obj/corridor.obj').read_bytes()).hexdigest()==m['source_sha256']
materials={l.split()[1] for l in (OUT/'materials.mtl').read_text().splitlines() if l.startswith('newmtl ')}
reports=[]
for key,a in m['assets'].items():
    v=[];n=[];triangles=0; used=set()
    for line in (OUT/a['file']).read_text().splitlines():
        t=line.split()
        if not t:continue
        if t[0]=='v': v.append(tuple(map(float,t[1:])))
        elif t[0]=='vn': n.append(tuple(map(float,t[1:])))
        elif t[0]=='usemtl': assert t[1] in materials;used.add(t[1])
        elif t[0]=='mtllib': assert (OUT/t[1]).is_file()
        elif t[0]=='f':
            assert len(t)==4
            ids=[list(map(int,q.split('//'))) for q in t[1:]]
            assert all(0<i<=len(v) and 0<j<=len(n) for i,j in ids)
            pts=[v[i-1] for i,j in ids]
            u=[pts[1][k]-pts[0][k] for k in range(3)]; w=[pts[2][k]-pts[0][k] for k in range(3)]
            cross=[u[1]*w[2]-u[2]*w[1],u[2]*w[0]-u[0]*w[2],u[0]*w[1]-u[1]*w[0]]
            assert sum(x*x for x in cross)>1e-18, 'degenerate triangle'
            assert sum(cross[k]*n[ids[0][1]-1][k] for k in range(3))>0, 'winding mismatch'
            triangles+=1
    assert all(math.isfinite(c) for p in v+n for c in p)
    assert triangles==a['triangles']
    for i in range(3):
        assert abs(min(p[i] for p in v)-a['bounds']['min'][i])<1e-6
        assert abs(max(p[i] for p in v)-a['bounds']['max'][i])<1e-6
    reports.append({'asset':key,'triangles':triangles,'vertices':len(v),'materials':sorted(used),'status':'pass'})
def intersects(p,box):
    # Synthetic standing capsule approximated by an AABB, radius .3, height 1.8.
    lo=[p[0]-.3,p[1]+.01,p[2]-.3];hi=[p[0]+.3,p[1]+1.8,p[2]+.3]
    return all(lo[i]<box['max'][i] and hi[i]>box['min'][i] for i in range(3))
def door_boxes(state):
    boxes=[]
    for d in m['dynamic_colliders']:
        b=m['assets'][d['asset_bounds']]['bounds'];t=d['translation_'+state]
        boxes.append({'min':[b['min'][i]+t[i]+([0,0,-17.81][i]) for i in range(3)],'max':[b['max'][i]+t[i]+([0,0,-17.81][i]) for i in range(3)]})
    return boxes
route=[[0,0,-i*.05] for i in range(1,364)]
assert not any(intersects(p,b) for p in route for b in m['static_colliders']), 'central route blocked by static proxy'
assert not any(intersects(p,b) for p in route for b in door_boxes('open')), 'open route blocked'
assert any(intersects(p,b) for p in route for b in door_boxes('closed')), 'closed door fails blocking'
assert 'part_291' not in m['assets']['corridor']['source_parts']
result={'status':'pass','models':reports,'tests':['original SHA256 unchanged','triangles/indices/normals/winding/MTL/bounds','synthetic centre route clear with open doors','synthetic centre route blocked with closed doors'],'not_tested':['rendered appearance','actual game player collision','game animation','game material display']}
(OUT/'validation_report.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
