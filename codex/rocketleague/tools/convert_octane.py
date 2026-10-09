"""Convert verified private UE Viewer geometry to the host's small draw format.

Original geometry and normals, simplified blue paint/dark chassis lighting.
The recovered RGB mask is retained privately but is NOT an original UE3 shader.
No decoded data belongs in git. Run export_octane.py first.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from export_octane import BODY_SHA, WHEEL_SHA

def child(root, name):
    p = (root / name).resolve()
    if root not in p.parents:
        raise ValueError('Asset path escapes export')
    return p

def geometry(path, include_uv=False):
    j = json.loads(path.read_text())
    if j['asset']['version'] != '2.0' or len(j['buffers']) != 1 or len(j['meshes']) != 1:
        raise ValueError('Unsupported glTF structure')
    data = child(path.parent.resolve(), j['buffers'][0]['uri']).read_bytes()
    if len(data) != j['buffers'][0]['byteLength'] or len(data) > 32*1024*1024:
        raise ValueError('Invalid geometry buffer length')
    def access(index, kind, component):
        a = j['accessors'][index]
        if a['type'] != kind or a['componentType'] != component or 'sparse' in a or a.get('normalized'):
            raise ValueError('Unsupported accessor')
        v = j['bufferViews'][a['bufferView']]
        if v['buffer'] != 0:
            raise ValueError('Unsupported buffer index')
        fmt = {5123:'H', 5125:'I', 5126:'f'}[component] * {'SCALAR':1, 'VEC2':2, 'VEC3':3}[kind]
        size = struct.calcsize('<'+fmt)
        stride = v.get('byteStride', size)
        start = v.get('byteOffset',0) + a.get('byteOffset',0)
        count = a['count']
        if not 0 < count <= 500000 or stride < size or start < 0 or start + (count-1)*stride + size > min(len(data),v.get('byteOffset',0)+v['byteLength']):
            raise ValueError('Accessor out of bounds')
        return [struct.unpack_from('<'+fmt, data, start+i*stride) for i in range(count)]
    result = []
    for primitive in j['meshes'][0]['primitives']:
        if primitive.get('mode',4) != 4:
            raise ValueError('Only triangles supported')
        attr = primitive['attributes']
        positions, normals = access(attr['POSITION'],'VEC3',5126), access(attr['NORMAL'],'VEC3',5126)
        uv = access(attr['TEXCOORD_0'],'VEC2',5126) if include_uv else None
        idx = primitive['indices']
        indices = access(idx, 'SCALAR', j['accessors'][idx]['componentType'])
        if len(positions) != len(normals) or len(indices)%3 or (uv is not None and len(uv) != len(positions)):
            raise ValueError('Invalid triangle stream')
        stream = []
        for (index,) in indices:
            if not 0 <= index < len(positions):
                raise ValueError('Invalid vertex index')
            p, n = positions[index], normals[index]
            if not all(math.isfinite(f) and abs(f) < 100 for f in p+n):
                raise ValueError('Invalid vertex')
            # Inverse of pinned ExportGltf.cpp TransformPosition/Direction.
            vertex = (p[0]*100,p[2]*100,p[1]*100,n[0],n[2],n[1])
            if uv is not None:
                if not all(math.isfinite(f) and abs(f) < 16 for f in uv[index]):
                    raise ValueError('Invalid material UV')
                vertex += uv[index]
            stream.append(vertex)
        result.append(stream)
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('export', type=Path)
    parser.add_argument('out', type=Path)
    args = parser.parse_args()
    root, out = args.export.resolve(), args.out.resolve()
    report = json.loads((root/'provenance.json').read_text())
    if report['body_sha256'] != BODY_SHA or report['wheel_sha256'] != WHEEL_SHA:
        raise ValueError('Unrecognized original source profile')
    for name, expected in report['files'].items():
        if hashlib.sha256(child(root,name).read_bytes()).hexdigest() != expected:
            raise ValueError('Export changed since provenance was recorded')
    body = geometry(root/'export/CodexOctaneFixed/SkeletalMesh3/Body_Octane_PremiumSkin_SK.gltf')
    wheel = geometry(root/'export/wheel_sport80_SF/StaticMesh3/Wheel_Sport80_SM.gltf')
    if len(body)!=2 or len(wheel)!=1:
        raise ValueError('Unexpected original material slots')
    if out.exists():
        raise ValueError('Output must be a new private directory')
    out.mkdir(parents=True)
    parts=[]
    for name, groups, colors in [('body',body,[(.12,.14,.17,1),(.025,.36,.95,1)]),
                                  ('wheel',wheel,[(.14,.16,.19,1)])]:
        data=b''.join(struct.pack('<10f',*vertex,*color)
                      for group,color in zip(groups,colors) for vertex in group)
        (out/(name+'.bin')).write_bytes(data)
        parts.append(dict(name=name,file=name+'.bin',vertices=len(data)//40,
                          sha256=hashlib.sha256(data).hexdigest()))
    manifest=dict(schema='octane-host-mesh-v1',body_sha256=BODY_SHA,wheel_sha256=WHEEL_SHA,parts=parts,
                  appearance='Original rest geometry/normals; simplified host paint/lighting. Sport80 wheels.',
                  source_export_sha256=hashlib.sha256((root/'provenance.json').read_bytes()).hexdigest())
    (out/'model.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'Converted original geometry into {out}; materials remain approximate')

if __name__ == '__main__':
    main()
