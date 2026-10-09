"""Compile the renderer's actual shader-building statements without a window."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--cxx', default='c++')
    parser.add_argument('--validator', default='glslangValidator')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    # Compile the same production builder called by the renderer.
    harness = '#include "src/pc/rocket_material_shader.h"\n#include <fstream>\nint main(){for(int i=0;i<3;i++){\n'
    harness += 'auto sources=rocket_material_shader(i==0,i==2);auto &vs=sources.first;auto &fs=sources.second;'
    harness += '\nstd::ofstream(std::to_string(i)+".vert")<<vs;std::ofstream(std::to_string(i)+".frag")<<fs;}}\n'
    env = {k.upper(): v for k, v in os.environ.items()} if os.name == 'nt' else os.environ.copy()
    with tempfile.TemporaryDirectory(prefix='metal-shader-') as directory:
        folder = Path(directory)
        cpp = folder/'build.cpp'; cpp.write_text(harness)
        exe = folder/('build.exe' if os.name == 'nt' else 'build')
        subprocess.run([args.cxx,'-x','c++','-std=c++20','-I'+str(repo),str(cpp),'-o',str(exe)],env=env,check=True)
        subprocess.run([str(exe)],cwd=folder,env=env,check=True)
        for profile in range(3):
            subprocess.run([args.validator,'-l',str(folder/(str(profile)+'.vert')),str(folder/(str(profile)+'.frag'))],env=env,check=True)
    print('PASS production metal GLSL vertex/fragment compile and link: ES 100, compatibility 120, modern 130; no window')

if __name__ == '__main__':
    main()
