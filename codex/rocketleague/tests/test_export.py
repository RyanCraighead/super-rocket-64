"""Synthetic package-layout tests. Contains no original game data."""
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from export_octane import compact_chunks

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

if __name__=='__main__':unittest.main()
