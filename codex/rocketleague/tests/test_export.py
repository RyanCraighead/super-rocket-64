"""Synthetic package-layout tests. Contains no original game data."""
from pathlib import Path
import struct
import sys
import unittest
import tempfile
import hashlib
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from export_octane import compact_chunks, inspect_game, check_game

class ChunkLayout(unittest.TestCase):
    def sample(self):
        package=bytearray(100);package[20:24]=package[60:64]=bytes.fromhex('c1832a9e')
        plain=struct.pack('<I',2)+struct.pack('<QIQI',100,50,20,40)+bytes(12)+struct.pack('<QIQI',150,80,60,40)+bytes(12)
        return plain,package
    def test_compact_preserves_offsets_sizes_and_total_length(self):
        plain,package=self.sample();out=compact_chunks(plain,0,2,package)
        self.assertEqual(len(out),len(plain))
        self.assertEqual(struct.unpack_from('<QIQI',out,4),(100,50,20,40))
        self.assertEqual(struct.unpack_from('<QIQI',out,28),(150,80,60,40))
        self.assertEqual(package[20:24],bytes.fromhex('c1832a9e'))
    def test_fail_closed(self):
        plain,package=self.sample()
        for offset,count in [(-1,2),(0,0),(0,3),(2,2)]:
            with self.assertRaises(ValueError):compact_chunks(plain,offset,count,package)
        for change in [28,52,64]:
            data=bytearray(plain);data[change]=1
            with self.assertRaises(ValueError):compact_chunks(data,0,2,package)
        package[20]=0
        with self.assertRaises(ValueError):compact_chunks(plain,0,2,package)

class PackagePreflight(unittest.TestCase):
    def test_missing_package_identifies_file_and_recovery(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError, 'Body_Octane_SF.upk.*finish or verify'):
                check_game(Path(tmp))

    def test_unsupported_packages_report_both_hashes_without_writing(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            cooked = root / 'TAGame/CookedPCConsole'
            cooked.mkdir(parents=True)
            for name in ('Body_Octane_SF.upk', 'wheel_sport80_SF.upk'):
                (cooked / name).write_bytes(b'SYNTHETIC - NOT A PACKAGE')
            before = {p.relative_to(root).as_posix(): (p.read_bytes(), p.stat().st_mtime_ns)
                      for p in root.rglob('*') if p.is_file()}
            report = inspect_game(root)
            self.assertFalse(report['supported'])
            self.assertEqual(len(report['packages']), 2)
            with self.assertRaisesRegex(ValueError, 'Body_Octane_SF.upk.*wheel_sport80_SF.upk'):
                check_game(root)
            after = {p.relative_to(root).as_posix(): (p.read_bytes(), p.stat().st_mtime_ns)
                     for p in root.rglob('*') if p.is_file()}
            self.assertEqual(before, after)
            for record in report['packages'].values():
                self.assertEqual(record['sha256'], hashlib.sha256(b'SYNTHETIC - NOT A PACKAGE').hexdigest())

    def test_empty_package_is_rejected_before_tools(self):
        with tempfile.TemporaryDirectory() as tmp:
            cooked = Path(tmp) / 'TAGame/CookedPCConsole'
            cooked.mkdir(parents=True)
            (cooked / 'Body_Octane_SF.upk').touch()
            with self.assertRaisesRegex(ValueError, 'Invalid Rocket League package size'):
                inspect_game(Path(tmp))

if __name__=='__main__':unittest.main()
