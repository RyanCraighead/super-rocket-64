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
    source = (repo/'src/pc/rocket_runtime.cpp').read_text()
    start = source.index('    std::string prefix=')
    end = source.index('    GLuint v=shader(', start)
    # Copy production C++ expressions verbatim; this does not maintain a second
    # version of the shader or replace it with a test implementation.
    builder = source[start:end]
    harness = '#include <string>\n#include <fstream>\nint main(){for(int i=0;i<3;i++){struct{bool es,modern;}gl={i==0,i==2};\n'
    harness += builder
    harness += '\nstd::ofstream(std::to_string(i)+".vert")<<vs;std::ofstream(std::to_string(i)+".frag")<<fs;}}\n'
    env = {k.upper(): v for k, v in os.environ.items()} if os.name == 'nt' else os.environ.copy()
    with tempfile.TemporaryDirectory(prefix='metal-shader-') as directory:
        folder = Path(directory)
        cpp = folder/'build.cpp'; cpp.write_text(harness)
        exe = folder/('build.exe' if os.name == 'nt' else 'build')
        subprocess.run([args.cxx,'-x','c++','-std=c++20',str(cpp),'-o',str(exe)],env=env,check=True)
        subprocess.run([str(exe)],cwd=folder,env=env,check=True)
        for profile in range(3):
            subprocess.run([args.validator,'-l',str(folder/(str(profile)+'.vert')),str(folder/(str(profile)+'.frag'))],env=env,check=True)
    print('PASS production metal GLSL vertex/fragment compile and link: ES 100, compatibility 120, modern 130; no window')

if __name__ == '__main__':
    main()
