"""Report connected mesh components and bounds; never alters the source file."""
import array, hashlib, json, sys
from collections import Counter
from pathlib import Path
import bpy
from mathutils import Vector
args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=2: raise ValueError('mesh name and new report path required')
obj=bpy.data.objects.get(args[0]);out=Path(args[1]).resolve();src=Path(bpy.data.filepath).resolve()
if not obj or obj.type!='MESH' or out.exists(): raise RuntimeError('missing mesh or report exists')
def sha():
 with src.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha();mesh=obj.data;n=len(mesh.vertices);parents=list(range(n));ranks=bytearray(n)
def root(x):
 while parents[x]!=x:
  parents[x]=parents[parents[x]];x=parents[x]
 return x
edges=array.array('i',[0])*(len(mesh.edges)*2);mesh.edges.foreach_get('vertices',edges)
for i in range(0,len(edges),2):
 a,b=root(edges[i]),root(edges[i+1])
 if a!=b:
  if ranks[a]<ranks[b]:a,b=b,a
  parents[b]=a
  if ranks[a]==ranks[b]:ranks[a]+=1
roots=[root(i) for i in range(n)];counts=Counter(roots);top=counts.most_common(30)
bounds={k:[[float('inf')]*3,[float('-inf')]*3] for k,_ in top}
for v in mesh.vertices:
 r=roots[v.index]
 if r in bounds:
  p=obj.matrix_world@v.co;lo,hi=bounds[r]
  for k in range(3):lo[k]=min(lo[k],p[k]);hi[k]=max(hi[k],p[k])
after=sha();report={'source':str(src),'source_before':before,'source_after':after,'mesh':obj.name,'vertices':n,'edges':len(mesh.edges),'components':len(counts),'largest':[{'vertices':c,'min':bounds[r][0],'max':bounds[r][1]} for r,c in top], 'scope':'connected topology evidence, not an approved articulation split'}
out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if before!=after:raise RuntimeError('source changed')
print('OV_COMPONENT_REPORT',out)
