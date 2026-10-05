"""Execute the original N64 cached-frame hierarchy for every decoded frame.

This is optional original-code verification/export. Requires the user's exact
ROM, the extracted original boot, and Unicorn. No original machine code is
embedded in source. OriginalMips performs its own input identity gates.
"""
import argparse
import json
from pathlib import Path
from codex.spiderman.assets.archive import require,sha256
from codex.spiderman.mesh.pose import compose_frame
from codex.spiderman.movement.original_mips import OriginalMips

POLICY={
    'kind':'original N64 MIPS execution, cached decoded 0x2C frame path',
    'entry_address':'0x80071AA4','angle_matrix_address':'0x800521A0',
    'pose_conversion_address':'0x8005A244',
    'matrices_f32_layout':'column-major 4x4, raw model units, absolute per-part rotation and world translation',
    'poses_s16_layout':'9 row-major fixed12 rotation cells followed by3 signed translation cells',
    'input':'exact decoded six-s16 sample grid and authored parent links',
    'excluded':'source-byte decompression, allocator, input/state selection, elapsed time, blending and full rendering',
}

def world_matrices(pose):
    out=[]
    for m in pose['matrices_f32']:
        # Transpose column-major serialization to row-major for column-vector consumers.
        out.append([m[0]*8/36,m[4]*8/36,m[8]*8/36,m[12]/36,
                    m[1]*8/36,m[5]*8/36,m[9]*8/36,m[13]/36,
                    m[2]*8/36,m[6]*8/36,m[10]*8/36,m[14]/36,
                    0,0,0,1])
    return out

def run(rom,directory):
    directory=Path(directory);model_path=directory/'spiderman_model.json';model=json.loads(model_path.read_text())
    oracle=OriginalMips(directory/'boot.bin',rom);parents=[o['parent'] for o in model['objects']]
    evidence={'policy':POLICY,'frame_count':0,'bone_pose_count':0,'max_preview_matrix_error':0,'clips':[]}
    boot=(directory/'boot.bin').read_bytes()
    evidence['routine_sha256']={f'{a:#x}-{b:#x}':sha256(boot[a-0x80016ae0:b-0x80016ae0]) for a,b in ((0x80071aa4,0x80072124),(0x800521a0,0x80052340),(0x8005a244,0x8005a3dc))}
    samples=[]
    for ref in model['animations']:
        path=directory/ref['file'];clip=json.loads(path.read_text());require(clip['status']=='decoded','a failed clip cannot be posed')
        poses=[]
        for frame,raw in enumerate(clip['frames']):
            result=oracle.compressed_frame_pose(raw,parents);poses.append(result)
            preview=compose_frame(model,clip,frame);native=world_matrices(result)
            err=max(abs(a-b) for ma,mb in zip(native,preview['world_matrices']) for a,b in zip(ma,mb))
            evidence['max_preview_matrix_error']=max(evidence['max_preview_matrix_error'],err)
            if ref['slot'] in (0,1,5,10,20,50,100,150,200,250,298) and frame in {0,clip['frame_count']//2,clip['frame_count']-1}:
                samples.append(preview|{'policy':POLICY['kind'],'preview_world_matrices':preview['world_matrices'],'world_matrices':native,'original_matrices_f32':result['matrices_f32'],'original_poses_s16':result['poses_s16'],'original_pose_hash':result['poses_s16be_sha256'],'original_matrix_hash':result['matrices_f32be_sha256'],'preview_max_error':err})
        clip['original_pose_execution']=POLICY;clip['original_poses']=poses
        temp=path.with_suffix('.json.tmp');temp.write_text(json.dumps(clip,separators=(',',':'))+'\n');temp.replace(path)
        evidence['frame_count']+=len(poses);evidence['bone_pose_count']+=len(poses)*len(parents)
        evidence['clips'].append({'slot':clip['slot'],'frames':len(poses),'pose_hashes':[p['poses_s16be_sha256'] for p in poses]})
        ref['original_pose_execution']=True
        if ref['slot']%50==0:print(f"Original N64 poses: slot{ref['slot']}, {evidence['frame_count']}frames",flush=True)
    model['coordinate_contract']['composed_matrix_policy']=POLICY['kind'];model['original_pose_execution']=POLICY
    model['limitations']=[x for x in model['limitations'] if not x.startswith('Pose matrices are')]
    model['limitations'].append('Original pose execution verifies cached frames; input/state selection, cadence, blending, and rasterizer fidelity remain separate')
    model_path.write_text(json.dumps(model,separators=(',',':'))+'\n')
    (directory/'pose_samples.json').write_text(json.dumps({'schema':'n64codexlab.spiderman.pose_samples.v1','source':model['source'],'samples':samples},separators=(',',':'))+'\n')
    (directory/'original-pose-evidence.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(json.dumps({k:v for k,v in evidence.items() if k not in ('clips','routine_sha256','policy')},indent=2))
    return evidence

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('rom');p.add_argument('--directory',default='codex/.runtime/spiderman-assets');args=p.parse_args();run(args.rom,args.directory)
