"""Owned texture opt-in, cache preservation, invalid input and cancel tests."""
from pathlib import Path
import hashlib
import json
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from codex.windows import rocket_material_setup as materials


class MaterialSetup(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='sr64-material-test-')
        self.root = Path(self.temp.name)
        self.target = self.root / 'materials'
        self.profile = dict(schema='test-only', startup_package_sha256='unsupported-fixture', body_geometry_sha256=hashlib.sha256(b'geometry').hexdigest(),
                            files={'body.uv': dict(size=2, sha256=hashlib.sha256(b'uv').hexdigest())})
        self.mock = patch.object(materials, 'PROFILE', self.profile)
        self.mock.start()
        (self.root / 'body.bin').write_bytes(b'geometry')
        (self.root / 'controls.cfg').write_bytes(b'keep my bindings')
        (self.root / 'save.bin').write_bytes(b'keep my progress')

    def tearDown(self):
        self.assertEqual((self.root / 'body.bin').read_bytes(), b'geometry')
        self.assertEqual((self.root / 'controls.cfg').read_bytes(), b'keep my bindings')
        self.assertEqual((self.root / 'save.bin').read_bytes(), b'keep my progress')
        self.mock.stop()
        self.temp.cleanup()

    def populate(self, path):
        path.mkdir()
        (path / 'manifest.json').write_text(json.dumps(self.profile))
        (path / 'body.uv').write_bytes(b'uv')

    def export(self, game, viewer, stage, cancel_check):
        self.populate(stage)
        cancel_check()

    def test_verified_cache_needs_no_game_or_tool(self):
        self.populate(self.target)
        with patch.object(materials, 'export', side_effect=AssertionError('must not extract')):
            self.assertIn('verified', materials.prepare(None, self.target, None))

    def test_missing_game_does_not_write(self):
        with self.assertRaises(ValueError):
            materials.prepare(None, self.target, None)
        self.assertFalse(self.target.exists())

    def test_supported_upgrade_is_atomic_and_geometry_is_unchanged(self):
        with patch.object(materials, 'export', side_effect=self.export):
            self.assertIn('ready', materials.prepare(self.root, self.target, self.root))
        self.assertEqual(materials.validate(self.target), self.target)
        self.assertEqual(list(self.root.glob('.materials-*')), [])

    def test_corrupt_material_rejected(self):
        self.populate(self.target)
        (self.target / 'body.uv').write_bytes(b'bad')
        with self.assertRaises(ValueError):
            materials.validate(self.target)

    def test_manifest_version_rejected(self):
        self.populate(self.target)
        (self.target / 'manifest.json').write_text('{}')
        with self.assertRaises(ValueError):
            materials.validate(self.target)

    def test_failed_upgrade_keeps_existing_bytes(self):
        self.populate(self.target)
        (self.target / 'body.uv').write_bytes(b'old')
        with patch.object(materials, 'export', side_effect=ValueError('unsupported installed version')):
            with self.assertRaises(ValueError):
                materials.prepare(self.root, self.target, self.root)
        self.assertEqual((self.target / 'body.uv').read_bytes(), b'old')
        self.assertEqual(list(self.root.glob('materials.invalid-*')), [])

    def test_replaced_corrupt_material_kept_for_recovery(self):
        self.populate(self.target)
        (self.target / 'body.uv').write_bytes(b'old')
        with patch.object(materials, 'export', side_effect=self.export):
            materials.prepare(self.root, self.target, self.root)
        backup = list(self.root.glob('materials.invalid-*'))
        self.assertEqual(len(backup), 1)
        self.assertEqual((backup[0] / 'body.uv').read_bytes(), b'old')
        materials.validate(self.target)

    def test_cancel_before_publish_keeps_existing_materials(self):
        self.populate(self.target)
        (self.target / 'body.uv').write_bytes(b'old')
        calls = 0
        def cancel():
            nonlocal calls
            calls += 1
            if calls == 3:
                raise InterruptedError('cancelled')
        with patch.object(materials, 'export', side_effect=self.export):
            with self.assertRaises(InterruptedError):
                materials.prepare(self.root, self.target, self.root, cancel)
        self.assertEqual((self.target / 'body.uv').read_bytes(), b'old')
        self.assertEqual(list(self.root.glob('.materials-*')), [])

    def test_untrusted_paths_and_oversize_are_rejected(self):
        with self.assertRaises(ValueError):
            materials.read(self.root, 100)
        with self.assertRaises(ValueError):
            materials.read(self.root / 'body.bin', 3)

    def test_unknown_package_fails_before_aes_or_subprocess(self):
        with patch.object(materials, 'aes_ecb', side_effect=AssertionError('must not decrypt')):
            with self.assertRaises(ValueError):
                materials.repair_startup(b'unknown', b'unknown')


if __name__ == '__main__':
    unittest.main()
