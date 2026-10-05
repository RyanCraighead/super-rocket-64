#!/usr/bin/env python3
"""Inventory immutable tracked source blobs; never reads private working files.

Generate after committing source: python tools/source_manifest.py --write
Then commit only the manifest. Validate: python tools/source_manifest.py --check
The manifest excludes itself to avoid recursive hashing. Gitlinks are references,
not bundled content; restore and validate their pinned source independently.
"""
import argparse
import hashlib
import json
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
MANIFEST = "SOURCE-MANIFEST.json"


def git(*args):
    return subprocess.check_output(
        ["git", "-c", "safe.directory=" + ROOT.as_posix(), "-C", str(ROOT), *args]
    )


def inventory(revision):
    entries = []
    for raw in git("ls-tree", "-rz", "--full-tree", revision).split(b"\0"):
        if not raw:
            continue
        metadata, name = raw.split(b"\t", 1)
        mode, kind, oid = metadata.decode("ascii").split()
        path = name.decode("utf-8")
        if path != MANIFEST:
            entries.append((path, mode, kind, oid))
    return entries


def build(revision, previous):
    commit = git("rev-parse", revision + "^{commit}").decode().strip()
    old = {entry["path"]: entry for entry in previous.get("files", [])}
    files, links = [], []
    command = ["git", "-c", "safe.directory=" + ROOT.as_posix(), "-C", str(ROOT), "cat-file", "--batch"]
    process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    try:
        for path, mode, kind, oid in inventory(commit):
            if kind == "commit" and mode == "160000":
                links.append(dict(path=path, mode=mode, git_commit=oid))
                continue
            if kind != "blob":
                raise ValueError("Unexpected tracked object: " + path)
            process.stdin.write((oid + "\n").encode("ascii"))
            process.stdin.flush()
            header = process.stdout.readline().decode().split()
            if len(header) != 3 or header[:2] != [oid, "blob"]:
                raise ValueError("Invalid git object response: " + path)
            size = int(header[2])
            data = process.stdout.read(size)
            if len(data) != size or process.stdout.read(1) != b"\n":
                raise ValueError("Truncated git object: " + path)
            original = old.get(path, {})
            unchanged = original.get("git_blob_sha1") == oid
            files.append(dict(
                path=path, size=size, mode=mode, git_blob_sha1=oid,
                sha256=hashlib.sha256(data).hexdigest(), source_blob_sha1=oid,
                upstream_blob_sha1=original.get("upstream_blob_sha1") if unchanged else None,
                source_commit=original.get("source_commit", commit) if unchanged else commit,
            ))
        process.stdin.close()
        if process.wait() != 0:
            raise ValueError("git cat-file failed")
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        process.stdout.close()
    return dict(
        schema_version=2, source_commit=commit,
        source_tree=git("rev-parse", commit + "^{tree}").decode().strip(),
        upstream_commit=previous["upstream_commit"],
        scope="Tracked source blobs at source_commit, excluding SOURCE-MANIFEST.json itself. No working files, restored binary resources, supplied ROMs or decoded assets. Gitlinks are separate pinned references, not bundled files.",
        file_count_excluding_manifest=len(files), gitlink_count=len(links),
        gitlinks=links, files=files,
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--write", action="store_true")
    action.add_argument("--check", action="store_true")
    parser.add_argument("--revision", default="HEAD")
    args = parser.parse_args()
    target = ROOT / MANIFEST
    previous = json.loads(target.read_text(encoding="utf-8"))
    if args.write:
        result = build(args.revision, previous)
        target.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8", newline="\n")
    else:
        frozen = build(previous["source_commit"], previous)
        if frozen != previous:
            raise SystemExit("Manifest differs from its frozen source commit")
        current = build(args.revision, previous)
        if current["files"] != previous["files"] or current["gitlinks"] != previous["gitlinks"]:
            raise SystemExit("Current tracked source differs from manifest (excluding manifest itself)")
        result = previous
    print(f"Source manifest {'written' if args.write else 'verified'}: {len(result['files'])} blobs, {len(result['gitlinks'])} gitlinks; frozen {result['source_commit']}")


if __name__ == "__main__":
    main()
