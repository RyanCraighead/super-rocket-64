"""Synthetic setup/cache/cancellation tests; no owned assets or downloads."""
import hashlib,io,json,struct,sys,tempfile,unittest,wave
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'windows'))
import rocket_audio_setup as audio

class AudioSetup(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        out=io.BytesIO()
        with wave.open(out,'wb') as w:w.setparams((2,2,48000,0,'NONE','not compressed'));w.writeframes(b'\0'*40)
        self.wav=out.getvalue();self.clip=dict(bank='test.bnk',media_id=123,size=len(self.wav),sha256=audio.digest(self.wav),channels=2,sample_width=2,rate=48000,frames=10)
        idx=struct.pack('<III',123,0,4)
        self.bank=b'DIDX'+struct.pack('<I',len(idx))+idx+b'DATA'+struct.pack('<I',4)+b'RIFF'
        self.profile=dict(schema='synthetic',banks={'test.bnk':audio.digest(self.bank)},clips={'jump':self.clip})
        self.p=patch.object(audio,'PROFILE',self.profile);self.p.start()
        self.game=self.root/'game';cooked=self.game/'TAGame/CookedPCConsole';cooked.mkdir(parents=True);(cooked/'test.bnk').write_bytes(self.bank)
        self.dest=self.root/'model/audio';self.cache=self.root/'tools/cache'
    def tearDown(self):self.p.stop();self.tmp.cleanup()
    def decoder(self,command,cancel):cancel();Path(command[command.index('-o')+1]).write_bytes(self.wav)
    def create(self):
        with patch.object(audio,'provision',return_value=self.root/'decoder'),patch.object(audio,'decode',side_effect=self.decoder):
            return audio.prepare(self.game,self.dest,self.cache)
    def test_runtime_profile_matches(self):
        import re
        profile=json.loads((Path(audio.__file__).with_name('rocket-audio-profile.json')).read_text())
        source=(Path(__file__).resolve().parents[3]/'src/pc/rocket_audio.cpp').read_text()
        records={n:(int(size),sha) for n,size,sha in re.findall(r'\{"([a-z_]+)",(\d+),"([a-f0-9]{64})"\}',source)}
        self.assertEqual(records,{n:(r['size'],r['sha256']) for n,r in profile['clips'].items()})
    def test_bounded_bank(self):
        self.assertEqual(audio.media_index(self.bank),{123:1})
        for data in (self.bank[:-1],self.bank+b'xx',self.bank+self.bank,self.bank.replace(struct.pack('<III',123,0,4),struct.pack('<III',123,50,4))):
            with self.assertRaises(ValueError):audio.media_index(data)
    def test_missing_sources_never_download(self):
        with patch.object(audio,'provision',side_effect=AssertionError('unexpected download')):
            self.assertIn('unavailable',audio.prepare(None,self.dest,self.cache))
            (self.game/'TAGame/CookedPCConsole/test.bnk').write_bytes(b'unsupported')
            with self.assertRaises(ValueError):audio.prepare(self.game,self.dest,self.cache)
    def test_verified_reuse_without_sources(self):
        self.create();before={p.name:(p.read_bytes(),p.stat().st_mtime_ns) for p in self.dest.iterdir()}
        with patch.object(audio,'provision',side_effect=AssertionError('unexpected download')),patch.object(audio,'decode',side_effect=AssertionError('unexpected decoder')):
            self.assertIn('existing',audio.prepare(None,self.dest,self.cache))
        self.assertEqual(before,{p.name:(p.read_bytes(),p.stat().st_mtime_ns) for p in self.dest.iterdir()})
    def test_missing_corrupt_manifest_and_wave(self):
        self.create();(self.dest/'jump.wav').write_bytes(b'broken')
        with self.assertRaises(ValueError):audio.validate(self.dest)
        self.assertIn('unavailable',audio.prepare(None,self.dest,self.cache))
        self.create();backups=list(self.dest.parent.glob('audio.before-repair-*'))
        self.assertEqual(len(backups),1);self.assertEqual((backups[0]/'jump.wav').read_bytes(),b'broken')
        (self.dest/'manifest.json').write_text('{}')
        with self.assertRaises(ValueError):audio.validate(self.dest)
    def test_cancel_and_partial_failure_preserve_previous(self):
        self.create();(self.dest/'jump.wav').write_bytes(b'old-invalid-preserved')
        for error in (InterruptedError('cancel'),ValueError('decoder failure')):
            with patch.object(audio,'provision',return_value='decoder'),patch.object(audio,'decode',side_effect=error):
                with self.assertRaises(type(error)):audio.prepare(self.game,self.dest,self.cache)
            self.assertEqual((self.dest/'jump.wav').read_bytes(),b'old-invalid-preserved')
            self.assertEqual(list(self.dest.parent.glob('.car-sounds-*')),[])
    def test_tool_cache_requires_exact_hashes_and_files(self):
        self.cache.mkdir(parents=True);(self.cache/'vgmstream-cli.exe').write_bytes(b'synthetic')
        with patch.object(audio,'TOOL',{'vgmstream-cli.exe':audio.digest(b'synthetic')}):
            self.assertEqual(audio.verify_tool(self.cache).name,'vgmstream-cli.exe')
            with patch.object(audio.urllib.request,'urlopen',side_effect=AssertionError('unexpected download')):audio.provision(self.cache)
            (self.cache/'unexpected.dll').write_bytes(b'x')
            with self.assertRaises(ValueError):audio.verify_tool(self.cache)
            (self.cache/'unexpected.dll').unlink();(self.cache/'vgmstream-cli.exe').write_bytes(b'tampered')
            with self.assertRaises(ValueError):audio.verify_tool(self.cache)
    def test_do_not_write_into_owned_game(self):
        with self.assertRaises(ValueError):audio.prepare(self.game,self.game/'audio',self.cache)

if __name__=='__main__':unittest.main()
