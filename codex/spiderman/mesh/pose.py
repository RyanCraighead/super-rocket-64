"""Frame-indexed source-derived pose preview, without invented playback timing.

Raw channels are exact decoded samples. Floating-point trigonometry/composition
here is an explicit preview policy until compared with the N64 runtime. Native
axes are retained; renderers may map (x,y,z) to (x,-y,-z) afterward.
"""
import argparse
import json
import math
from pathlib import Path
from codex.spiderman.assets.archive import require

def rotation_yxz(raw):
    """Column-vector Ry*Rx*Rz, angle channels masked to 12 bits."""
    x,y,z=[(a&4095)*math.tau/4096 for a in raw]
    sx,cx=math.sin(x),math.cos(x);sy,cy=math.sin(y),math.cos(y);sz,cz=math.sin(z),math.cos(z)
    return [[cy*cz+sy*sx*sz,-cy*sz+sy*sx*cz,sy*cx],
            [cx*sz,cx*cz,-sx],[-sy*cz+cy*sx*sz,sy*sz+cy*sx*cz,cy*cx]]

def transform(rotation,vector):
    return [sum(row[i]*vector[i] for i in range(3)) for row in rotation]

def compose_frame(model,clip,frame):
    require(clip['status']=='decoded' and 0<=frame<clip['frame_count'],'frame unavailable')
    objects=model['objects'];raw=clip['frames'][frame];n=len(objects)
    require(len(raw)==n,'frame joint count mismatch')
    rotations=[rotation_yxz(row[:3]) for row in raw];translations=[None]*n;visiting=set()
    # Match upstream all-clip placeholder treatment, without collapsing a zero-channel clip.
    has_translation=any(v!=0 for f in clip['frames'] for b in f for v in b[3:])
    def origin(b):
        if translations[b] is not None:return translations[b]
        require(b not in visiting,'hierarchy cycle');visiting.add(b)
        p=objects[b]['parent'];t=list(raw[b][3:])
        if not has_translation:
            t=[v/4096*16 for v in objects[b]['bind_translation_fixed12']]
        elif p>=0:
            require(p<n,'parent out of range');pt=origin(p);t=[a+b for a,b in zip(transform(rotations[p],t),pt)]
        translations[b]=t;visiting.remove(b);return t
    matrices=[]
    for b in range(n):
        t=origin(b);r=rotations[b]
        # Row-major 4x4, applied to COLUMN vectors; already includes 8/36 raw-vertex scale.
        matrices.append([r[0][0]*8/36,r[0][1]*8/36,r[0][2]*8/36,t[0]/36,
                         r[1][0]*8/36,r[1][1]*8/36,r[1][2]*8/36,t[1]/36,
                         r[2][0]*8/36,r[2][1]*8/36,r[2][2]*8/36,t[2]/36,
                         0,0,0,1])
    return {'slot':clip['slot'],'frame':frame,'policy':'source-derived float64 YXZ preview; native N64 execution not yet verified','matrix_layout':'row-major 4x4 applied to column vector [raw_x,raw_y,raw_z,1]','vertex_scale_included':True,'world_axis':'native; map x,-y,-z for glTF-like view','raw_channels':raw,'world_translation_raw':translations,'world_matrices':matrices,'clip_decoded_sha256':clip['decoded_s16le_sha256']}

def export_samples(directory):
    directory=Path(directory);model=json.loads((directory/'spiderman_model.json').read_text());samples=[]
    for slot in (0,1,5,10,20,50,100,150,200,250,298):
        clip=json.loads((directory/model['animations'][slot]['file']).read_text())
        if clip['status']!='decoded':continue
        for f in sorted({0,clip['frame_count']//2,clip['frame_count']-1}):
            samples.append(compose_frame(model,clip,f))
    result={'schema':'n64codexlab.spiderman.pose_samples.v1','source':model['source'],'samples':samples}
    (directory/'pose_samples.json').write_text(json.dumps(result,separators=(',',':'))+'\n')
    print(f'{len(samples)} poses exported')
    return result

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory',nargs='?',default='codex/.runtime/spiderman-assets');args=p.parse_args();export_samples(args.directory)
