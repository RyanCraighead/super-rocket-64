"""Optional owned Rocket League event audio; no game audio is distributed.

Pinned banks -> pinned official vgmstream -> exact verified PCM WAV files.
Older geometry profiles remain playable when sound setup is absent/unavailable.
"""
from pathlib import Path
import hashlib
import io
import json
import os
import shutil
import struct
import subprocess
import tempfile
import time
import urllib.request
import uuid
import wave
import zipfile

PROFILE = json.loads(Path(__file__).with_name('rocket-audio-profile.json').read_text(encoding='utf-8'))
TOOL = json.loads(Path(__file__).with_name('vgmstream-r2117.json').read_text(encoding='utf-8'))
URL = 'https://github.com/vgmstream/vgmstream/releases/download/r2117/vgmstream-win64.zip'
ZIP_SHA = '6c4a8a3813864fefed081bbd337dbc0ad93bf88e0b92f5db98d7ab258b22dc6c'

def digest(data):
    return hashlib.sha256(data).hexdigest()

def read(path, limit):
    path = Path(path)
    if path.is_symlink() or getattr(path, 'is_junction', lambda: False)() or not path.is_file() or path.stat().st_size > limit:
        raise ValueError('Missing/invalid local audio file: ' + path.name)
    with path.open('rb') as f:
        data = f.read(limit + 1)
    if len(data) > limit:
        raise ValueError('Audio input exceeds size limit: ' + path.name)
    return data

def verify_tool(directory):
    directory = Path(directory)
    if directory.is_symlink() or getattr(directory, 'is_junction', lambda: False)():
        raise ValueError('Audio tool cache must be an ordinary directory')
    for name, sha in TOOL.items():
        if digest(read(directory / name, 32*1024*1024)) != sha:
            raise ValueError('Pinned audio decoder changed: ' + name)
    # Do not let unexpected side-loaded libraries enter this executable folder.
    if {p.name for p in directory.iterdir()} != set(TOOL):
        raise ValueError('Unexpected file in pinned audio decoder cache')
    return directory / 'vgmstream-cli.exe'

def provision(directory, cancel_check=lambda: None):
    directory = Path(directory)
    cancel_check()
    if directory.exists():
        try:
            return verify_tool(directory)
        except ValueError:
            directory.rename(directory.with_name(directory.name+'.invalid-'+uuid.uuid4().hex))
    directory.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.audio-tool-', dir=directory.parent) as tmp:
        request = urllib.request.Request(URL, headers={'User-Agent':'SuperRocket64-LocalAssetSetup/1'})
        with urllib.request.urlopen(request, timeout=10) as response:
            if not response.url.startswith(('https://github.com/', 'https://release-assets.githubusercontent.com/')):
                raise ValueError('Unexpected audio decoder download redirect')
            chunks=[];total=0
            while True:
                cancel_check();part=response.read(65536)
                if not part:break
                total+=len(part)
                if total>8*1024*1024:raise ValueError('Audio decoder archive too large')
                chunks.append(part)
        archive=b''.join(chunks)
        if digest(archive)!=ZIP_SHA:raise ValueError('Official audio decoder archive hash mismatch')
        stage=Path(tmp)/'verified';stage.mkdir()
        with zipfile.ZipFile(io.BytesIO(archive)) as z:
            if len(z.infolist())!=len(TOOL) or set(z.namelist())!=set(TOOL):
                raise ValueError('Unexpected audio decoder archive files')
            for name,sha in TOOL.items():
                cancel_check();entry=z.getinfo(name)
                if entry.file_size>32*1024*1024:raise ValueError('Audio decoder member too large')
                data=z.read(entry)
                if digest(data)!=sha:raise ValueError('Audio decoder member hash mismatch: '+name)
                (stage/name).write_bytes(data)
        verify_tool(stage);cancel_check();stage.rename(directory)
    return verify_tool(directory)

def media_index(data):
    """Bounded Wwise chunk/index reader; source version is separately pinned."""
    at=0;parts={}
    while at<len(data):
        if at+8>len(data):raise ValueError('Truncated audio bank header')
        tag=data[at:at+4];size=struct.unpack_from('<I',data,at+4)[0]
        if tag in parts or at+8+size>len(data):raise ValueError('Invalid audio bank chunk')
        parts[tag]=data[at+8:at+8+size];at+=8+size
    idx=parts.get(b'DIDX',b'');media=parts.get(b'DATA',b'')
    if not idx or len(idx)%12:raise ValueError('Missing/invalid audio bank media index')
    result={}
    for offset in range(0,len(idx),12):
        ident,start,size=struct.unpack_from('<III',idx,offset)
        if ident in result or not size or start+size>len(media):raise ValueError('Invalid audio bank media bounds')
        result[ident]=offset//12+1
    return result

def inspect_sources(game):
    cooked=Path(game)/'TAGame/CookedPCConsole';result={}
    for name,sha in PROFILE['banks'].items():
        data=read(cooked/name,16*1024*1024)
        if digest(data)!=sha:
            raise ValueError('Unsupported Rocket League sound-bank version: '+name+'. Verify the game or wait for an updated sound profile.')
        result[name]=media_index(data)
    return cooked,result

def validate(directory):
    directory=Path(directory)
    manifest=json.loads(read(directory/'manifest.json',65536))
    if manifest!=PROFILE:raise ValueError('Unsupported local car-audio manifest')
    for name,record in PROFILE['clips'].items():
        data=read(directory/(name+'.wav'),2*1024*1024)
        if len(data)!=record['size'] or digest(data)!=record['sha256']:
            raise ValueError('Car sound missing or changed: '+name+'.wav')
        with wave.open(io.BytesIO(data)) as wav:
            if (wav.getnchannels(),wav.getsampwidth(),wav.getframerate(),wav.getnframes()) != (
                record['channels'],record['sample_width'],record['rate'],record['frames']):
                raise ValueError('Invalid car sound format: '+name)
    return directory

def decode(command,cancel_check):
    with subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
            creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0)) as child:
        start=time.monotonic()
        try:
            while child.poll() is None:
                cancel_check()
                if time.monotonic()-start>30:raise ValueError('Audio decoder timed out; retry setup')
                time.sleep(.05)
            output=child.communicate()[0]
            if child.returncode:raise ValueError('Audio decoding failed; retry setup ('+str(child.returncode)+')')
            if len(output)>65536:raise ValueError('Unexpected audio decoder output')
        finally:
            if child.poll() is None:
                child.kill();child.wait(timeout=10)

def prepare(game,directory,cache,cancel_check=lambda: None):
    directory=Path(directory)
    cancel_check()
    if directory.exists():
        try:
            validate(directory)
            return 'verified existing car sounds'
        except (ValueError,OSError,KeyError,TypeError,wave.Error):
            pass
    if not game:
        return 'Car sounds unavailable. Select your Rocket League folder in Setup to add or repair them; gameplay remains ready.'
    # Validate supported banks before downloading or executing anything.
    cooked,indices=inspect_sources(game)
    if directory.resolve()==Path(game).resolve() or Path(game).resolve() in directory.resolve().parents:
        raise ValueError('Car audio output must be outside the installed game')
    decoder=provision(cache,cancel_check)
    directory.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.car-sounds-',dir=directory.parent) as tmp:
        stage=Path(tmp)/'audio';stage.mkdir()
        for name,record in PROFILE['clips'].items():
            cancel_check();index=indices[record['bank']].get(record['media_id'])
            if not index:raise ValueError('Expected car sound missing: '+name)
            decode([str(decoder),'-i','-s',str(index),'-o',str(stage/(name+'.wav')),str(cooked/record['bank'])],cancel_check)
        (stage/'manifest.json').write_text(json.dumps(PROFILE,indent=2)+'\n',encoding='utf-8')
        validate(stage);cancel_check()
        backup=None
        if directory.exists():
            backup=directory.with_name(directory.name+'.before-repair-'+uuid.uuid4().hex)
            directory.rename(backup)
        try:stage.rename(directory)
        except BaseException:
            if backup:backup.rename(directory)
            raise
    return 'car jump, flip and standard boost sounds extracted and verified'
