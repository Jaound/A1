"""에디터 LOD0 삼각형에서 선체 쉴드 반경과 도면에 없는 무장의 장착면을 굽는다."""
from bisect import bisect_left, bisect_right
from pathlib import Path
from math import sqrt
import unreal as u

project = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir()))
out = project / 'Source/OrbitalVanguard/OVHullPresentationSurface.inl'
root = '/Game/Presentation/Ships/'
lod = 0
paths = {p.parent.name[:-4].lower(): str(p).replace('\\', '/') for p in
         (Path(s.split('.')[0]) for s in u.EditorAssetLibrary.list_assets(root))
         if p.name == 'SM_Hull' and p.parent.name.endswith('Full')}
paths['corvette'] = root + 'SM_PC14_FullMesh'
paths.pop('fighter', None)  # 함재기는 별도 형상 경로를 사용한다.
assert len(paths) == 16, paths
def vec(v):
    return 'FVector(' + ', '.join('%.6f' % c for c in v) + ')'
lines = ['// Tools/BakeHullPresentationSurface.py, UE LOD0, ship-local cm.']
for name, path in sorted(paths.items()):
    mesh = u.load_asset(path)
    box = mesh.get_bounding_box()
    extent = (box.max - box.min) * .5
    center = (box.max + box.min) * .5
    fit = min(175 / extent.x, 75 / extent.y, 30 / extent.z)
    ex, ey, ez = extent.x*fit, extent.y*fit, extent.z*fit
    xs = [ex*i/6 for i in range(-5,6)]
    ys = [ey*i/5 for i in range(-4,5)]
    seats = {}
    axial_radius = ex * 1.16
    max_transverse = 0
    for sec in range(mesh.get_num_sections(lod)):
        vs, indices, *_ = u.ProceduralMeshLibrary.get_section_from_static_mesh(mesh,lod,sec)
        pts = [((center.x-v.x)*fit,(center.y-v.y)*fit,(v.z-center.z)*fit) for v in vs]
        max_transverse = max(max_transverse,max(((y/ey)**2+(z/ez)**2)/(1-(x/axial_radius)**2) for x,y,z in pts))
        indices=list(indices)
        for a,b,c in zip(indices[::3],indices[1::3],indices[2::3]):
            x,y,z=pts[a]; bx,by,bz=pts[b]; cx,cy,cz=pts[c]
            dx,dy,dz=bx-x,by-y,bz-z
            ux,uy,uz=cx-x,cy-y,cz-z
            det=dx*uy-dy*ux
            if abs(det)<1e-8: continue
            nx,ny=dy*uz-dz*uy,dz*ux-dx*uz
            norm=sqrt(nx*nx+ny*ny+det*det)
            for ax in xs[bisect_left(xs,min(x,bx,cx)):bisect_right(xs,max(x,bx,cx))]:
                for ay in ys[bisect_left(ys,min(y,by,cy)):bisect_right(ys,max(y,by,cy))]:
                    s=((ax-x)*uy-(ay-y)*ux)/det
                    t=(dx*(ay-y)-dy*(ax-x))/det
                    if min(s,t,1-s-t)<-1e-7: continue
                    h=z+s*dz+t*uz
                    for side in (1,-1):
                        key=(ax,ay,side)
                        if key not in seats or h*side>seats[key][0]*side:
                            flip=side*(1 if det>0 else -1)
                            seats[key]=(h,(flip*nx/norm,flip*ny/norm,flip*det/norm))
    assert max_transverse > 0 and seats,name
    k=sqrt(max_transverse)*1.05
    lines.append('OV_HULL_SHIELD(TEXT("%s"), %s)'%(name,vec((axial_radius,ey*k,ez*k))))
    # Prefer broad, nearly horizontal surfaces; spread choices along both sides.
    for side in (1,-1):
        pool=[((x,y,h),n) for (x,y,d),(h,n) in seats.items() if d==side and abs(n[2])>.82]
        picked=[]
        for _ in range(min(10,len(pool))):
            def rank(item):
                x,y,z=item[0]
                dist=min(((x-p[0])**2+(y-p[1])**2 for p in picked),default=0)
                return dist+.25*ex*(1-abs(x/ex))+.1*ey*(1-abs(y/ey))
            item=max(pool,key=rank); pool.remove(item); picked.append(item[0])
            lines.append('OV_HULL_SEAT(TEXT("%s"), %s, %s)'%(name,vec(item[0]),vec(item[1])))
    u.log('OV_SURFACE '+name+' '+str(len(seats)))
out.write_text('\n'.join(lines)+'\n',encoding='utf8')
