#!/usr/bin/env python3
"""Restore omitted upstream resources by exact hash; never obtain user ROMs.

Use a local upstream Git checkout with --from, or explicitly opt in to downloading
that upstream with --fetch-upstream. No downloaded program is executed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])

def restore(repo, manifest, check_only=False):
    commit = manifest['upstream_commit']
    git(repo, 'cat-file', '-e', commit + '^{commit}')
    count = 0
    for item in manifest['files']:
        relative = Path(item['path'])
        if relative.is_absolute() or '..' in relative.parts:
            raise ValueError('Unsafe manifest path')
        target = ROOT / relative
        if not target.resolve().is_relative_to(ROOT):
            raise ValueError('Resource destination escapes checkout')
        data = git(repo, 'show', commit + ':' + item['path'])
        blob = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        if len(data) != item['size'] or blob != item['git_blob_sha1'] or hashlib.sha256(data).hexdigest() != item['sha256']:
            raise ValueError('Upstream resource verification failed: ' + item['path'])
        if target.exists():
            if not target.is_file() or target.is_symlink() or target.read_bytes() != data:
                raise ValueError('Refusing to replace a different existing file: ' + item['path'])
        elif not check_only:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            target.chmod(int(item['mode'], 8) & 0o777)
        count += 1
    print(('Verified' if check_only else 'Restored/verified') + f' {count} pinned upstream resources. No ROMs were obtained and no game was started.')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--from', dest='source', type=Path, help='Existing upstream Git checkout containing the pinned commit')
    source.add_argument('--fetch-upstream', action='store_true', help='Download official upstream into a temporary Git checkout')
    parser.add_argument('--check-only', action='store_true', help='Verify source resources without copying them')
    args = parser.parse_args()
    manifest = json.loads((ROOT / 'UPSTREAM-RESOURCES.json').read_text())
    if args.source:
        restore(args.source, manifest, args.check_only)
    else:
        with tempfile.TemporaryDirectory(prefix='n64-upstream-') as directory:
            repo = Path(directory)
            subprocess.run(['git', 'init', '--quiet', str(repo)], check=True)
            subprocess.run(['git', '-C', str(repo), 'fetch', '--quiet', '--depth=1', manifest['upstream_url'], manifest['upstream_commit']], check=True)
            restore(repo, manifest, args.check_only)

if __name__ == '__main__':
    main()
