"""Derive reusable test-scene parts from the unchanged 96539cf corridor."""
from pathlib import Path
import hashlib, json, math, shutil
ROOT=Path(__file__).resolve().parents[1]
SRC=ROOT/'asset/corridor/obj/corridor.obj'
OUT=ROOT/'asset/corridor/test_scene'
EXPECTED='0b4fd161359182ff9c7993b8ecd95849ce5e4ff5cb12f93e47f15fed8b13cc50'
assert hashlib.sha256(SRC.read_bytes()).hexdigest()==EXPECTED, 'Source changed: re-audit before regenerating'
verts=[]; normals=[]; parts={}; part=None
for line in SRC.read_text().splitlines():
    t=line.split()
    if not t: continue
    if t[0]=='o': part={'mat':None,'faces':[]}; parts[t[1]]=part
    elif t[0]=='v': verts.append(tuple(map(float,t[1:])))
    elif t[0]=='vn': normals.append(tuple(map(float,t[1:])))
    elif t[0]=='usemtl': part['mat']=t[1]
    elif t[0]=='f':
        assert len(t)==4
        part['faces'].append([tuple(int(v) if v else 0 for v in q.split('/')) for q in t[1:]])
def names(ids): return [f'part_{i:03}' for i in ids]
def export(name, ids, origin):
    lines=['# metres; +Y up; local front +Z; triangles; derived from corridor.obj','mtllib materials.mtl']; vi=ni=0; points=[]; tris=0
    for key in names(ids):
        p=parts[key]; vm={}; nm={}
        lines += ['o '+key,'usemtl '+p['mat']]
        for face in p['faces']:
            for v,vt,n in face:
                assert vt==0
                if v not in vm:
                    pt=tuple(verts[v-1][i]-origin[i] for i in range(3));points.append(pt);vi+=1;vm[v]=vi
                    lines.append('v '+' '.join(f'{x:.6f}' for x in pt))
                if n not in nm:
                    ni+=1;nm[n]=ni;lines.append('vn '+' '.join(f'{x:.6f}' for x in normals[n-1]))
            lines.append('f '+' '.join(f'{vm[v]}//{nm[n]}' for v,vt,n in face));tris+=1
    (OUT/name).write_text('\n'.join(lines)+'\n',encoding='ascii')
    return {'file':name,'source_parts':names(ids),'source_origin':origin,'bounds':{'min':[min(p[i] for p in points) for i in range(3)],'max':[max(p[i] for p in points) for i in range(3)]},'triangles':tris}
def box(id,lo,hi): return {'id':id,'min':lo,'max':hi}
OUT.mkdir(parents=True,exist_ok=True)
shutil.copyfile(SRC.with_name('materials.mtl'),OUT/'materials.mtl')
# The original full backing panel blocks the doorway even when its doors move.
# Keep the source untouched, exclude that panel from the derived environment.
assets={}
assets['corridor']=export('corridor_static.obj',[i for i in range(333) if i not in range(291,304)],(0,.028,3))
assets['door_frame']=export('door_frame.obj',[292,293,297,298,302,303],(0,.028,-14.81))
assets['door_left']=export('door_left.obj',[294,295,296],(-.72,.028,-14.81))
assets['door_right']=export('door_right.obj',[299,300,301],(.72,.028,-14.81))
assets['cover']=export('cover_cabinet.obj',range(313,320),(-3.0155,.15,-4.7))
manifest={
 'schema_version':1,'source_commit':'96539cf8ccf7c383549363801b0ec9f82b73fc1a','source_sha256':EXPECTED,
 'units':'metres','up':'+Y','front':'+Z','corridor_travel_direction':'-Z',
 'source_to_test_translation':[0,-.028,-3],
 'origin_definition':'entrance floor centre, original (0, 0.028, 3)',
 'assets':assets,
 'entrance':[0,0,0],'door_plane':[0,0,-17.81],
 'walkthrough_exit_target':[0,0,-18.15],
 'interior':{'wall_panel_clear_width':6.62,'conservative_rib_clear_width':6.19,'conservative_ceiling_height':3.392,'floor_top':0},
 'placements':[
  {'asset':'corridor','parent':None,'translation':[0,0,0],'yaw_degrees':0},
  {'asset':'door_frame','parent':None,'translation':[0,0,-17.81],'yaw_degrees':0},
  {'asset':'door_left','parent':'door_frame','translation':[-.72,0,0],'motion_axis':[-1,0,0],'open_distance':1.45,'initial_state':'closed'},
  {'asset':'door_right','parent':'door_frame','translation':[.72,0,0],'motion_axis':[1,0,0],'open_distance':1.45,'initial_state':'closed'},
  {'asset':'cover','parent':None,'translation':[-1.2,0,-8.4],'yaw_degrees':90,'note':'reused existing cabinet, no newly designed geometry'}],
 'collision_policy':'Conservative gameplay AABB proxies, not exact render-mesh bounds. Rigid yaw=0 for corridor and frame; recompute bounds when rotating scene. Decorative lights/arrows ignored.',
 'static_colliders':[
  box('floor',[-3.58,-.278,-18.285],[3.58,0,.2]),
  box('wall_left',[-3.58,0,-18.285],[-3.095,3.917,.2]),
  box('wall_right',[3.095,0,-18.285],[3.58,3.917,.2]),
  box('ceiling',[-3.58,3.392,-18.285],[3.58,3.917,.2]),
  box('end_left',[-3.58,0,-18.285],[-1.375,3.392,-17.64]),
  box('end_right',[1.375,0,-18.285],[3.58,3.392,-17.64]),
  box('end_header',[-1.375,3.002,-18.285],[1.375,3.392,-17.62]),
  box('mounted_cabinet_1',[-3.31,0,-8.215],[-2.721,1.262,-7.185]),
  box('mounted_cabinet_2',[-3.31,0,-14.415],[-2.721,1.262,-13.385]),
  box('cover',[-1.715,0,-8.6945],[-.685,1.14,-8.1055])],
 'dynamic_colliders':[
  {'id':'door_left','asset_bounds':'door_left','parent':'door_frame','translation_closed':[-.72,0,0],'translation_open':[-2.17,0,0]},
  {'id':'door_right','asset_bounds':'door_right','parent':'door_frame','translation_closed':[.72,0,0],'translation_open':[2.17,0,0]}],
 'door_opening':{'clear_width':2.75,'clear_height':3.002,'note':'doors slide horizontally; no yaw hinge. Open leaves tuck behind fixed side panels. Collision follows leaf translation.'},
 'verification_files':['validation_report.json','loader_inspection.txt'], 'runtime_status':'not integrated or tested in game'}
(OUT/'scene_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print('Generated',len(assets),'OBJ files; original source unchanged')

