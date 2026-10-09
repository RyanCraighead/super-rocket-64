"""Material-only opt-in/undo, rejection, cancellation and preservation contracts."""
from pathlib import Path
import hashlib
import json
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
sys.path[:0] = [str(ROOT), str(ROOT / 'codex/windows')]
import car_appearance as appearance
import launcher as old


class AppearanceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='sr-appearance-')
        self.root = Path(self.temp.name)
        self.target = self.root / '.runtime/windows-octane'
        self.model = self.target / 'octane-model'
        self.model.mkdir(parents=True)
        self.active = self.model / 'materials'
        self.saved = self.model / 'materials.disabled'
        self.game = self.root / 'owned-game'
        cooked = self.game / 'TAGame/CookedPCConsole'
        cooked.mkdir(parents=True)
        for name, data in {'Body_Octane_SF.upk': b'owned body', 'Startup.upk': b'owned startup', 'wheel_sport80_SF.upk': b'unsupported wheel deliberately not needed'}.items():
            (cooked / name).write_bytes(data)
        self.profile = dict(schema='synthetic-only', body_geometry_sha256=self.sha(b'geometry'), startup_package_sha256=self.sha(b'owned startup'),
                            files={'body.uv': dict(size=2, sha256=self.sha(b'uv'))})
        self.patches = [patch.object(appearance.materials, 'PROFILE', self.profile), patch.object(appearance.materials, 'BODY_SHA', self.sha(b'owned body')),
                        patch('download_ueviewer.download'), patch.object(appearance.materials, 'export', side_effect=self.export),
                        patch('wizard_setup.discover', return_value=[str(self.game)]), patch.object(appearance.shutil, 'disk_usage', return_value=SimpleNamespace(free=1024**3))]
        self.mocks = [p.start() for p in self.patches]
        self.api = SimpleNamespace(ROOT=self.root, DATA_ROOT=self.root, old=old, profile=lambda character: self.target,
                                   check_assets=self.check_assets, cancel_check=lambda: None, require=old.require, verify_package=lambda: None)
        engine_hash = self.sha(b'synthetic compatible engine')
        (self.root / 'sm64coopdx.exe').write_bytes(b'synthetic compatible engine')
        capability = dict(schema=self.profile['schema'], profile_sha256=self.sha(json.dumps(self.profile, sort_keys=True, separators=(',', ':')).encode()), engine_sha256=engine_hash)
        (self.root / 'PACKAGE-MANIFEST.json').write_text(json.dumps(dict(files={'sm64coopdx.exe': dict(sha256=engine_hash)}, car_materials=capability)), encoding='utf-8')
        for name, data in {'body.bin': b'geometry', 'wheel.bin': b'wheel geometry', 'audio/car.pcm': b'my original sound',
                           '../save/sm64_save_file.bin': b'four slot progress', '../save/sm64config.txt': b'host save slot 3',
                           '../setup.json': b'my inventory', '../../controls/controls.cfg': b'custom controls',
                           '../../combined/save/sm64_save_file.bin': b'offline progress'}.items():
            p = self.model / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(data)
        self.before = self.snapshot()
        self.args = SimpleNamespace(action='appearance-apply', game=self.game)

    @staticmethod
    def sha(data):
        return hashlib.sha256(data).hexdigest()

    def snapshot(self):
        return {p: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.root.rglob('*') if p.is_file() and 'materials' not in p.parts and 'materials.disabled' not in p.parts}

    def tearDown(self):
        for path, identity in self.before.items():
            self.assertEqual((path.read_bytes(), path.stat().st_mtime_ns), identity, 'Unrelated file changed: ' + str(path))
        for p in reversed(self.patches):
            p.stop()
        self.temp.cleanup()

    def check_assets(self, character):
        self.assertEqual(character, 'octane')
        self.assertEqual((self.model / 'wheel.bin').read_bytes(), b'wheel geometry')

    def populate(self, folder):
        folder.mkdir()
        (folder / 'manifest.json').write_text(json.dumps(self.profile), encoding='utf-8')
        (folder / 'body.uv').write_bytes(b'uv')

    def export(self, game, viewer, stage, cancel):
        self.assertEqual(Path(game), self.game)
        self.populate(stage)

    def test_status_is_read_only_and_discovers_verified_source(self):
        self.args.game = None
        self.assertEqual(appearance.inspect(self.args, self.api)['source'], str(self.game))
        self.assertEqual(self.snapshot(), self.before)
        self.assertFalse(self.active.exists())
        self.mocks[2].assert_not_called()

    def test_upgrade_undo_reapply_exact_material_bytes_no_source_or_tool(self):
        self.assertTrue(appearance.change(self.args, self.api)['active'])
        original = {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.active.iterdir()}
        self.args.action = 'appearance-undo'
        self.args.game = None
        self.assertTrue(appearance.change(self.args, self.api)['reusable'])
        self.assertFalse(self.active.exists())
        self.args.action = 'appearance-apply'
        self.assertTrue(appearance.change(self.args, self.api)['active'])
        self.assertEqual({p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.active.iterdir()}, original)
        self.assertEqual(self.mocks[2].call_count, 1)
        self.assertEqual(self.mocks[3].call_count, 1)
        self.assertFalse((self.root / '.runtime/.seven-setup-lock').exists())
        self.assertEqual(list(self.model.glob('.appearance-*')), [])

    def test_existing_valid_materials_are_not_reextracted(self):
        self.populate(self.active)
        self.args.game = None
        self.assertTrue(appearance.change(self.args, self.api)['active'])
        self.mocks[2].assert_not_called()
        self.mocks[3].assert_not_called()

    def test_existing_materials_can_return_to_plain_appearance(self):
        self.populate(self.active)
        self.args.action = 'appearance-undo'
        self.assertTrue(appearance.change(self.args, self.api)['reusable'])
        self.assertFalse(self.active.exists())

    def test_missing_source_cannot_claim_success_or_download(self):
        self.args.game = None
        with self.assertRaisesRegex(ValueError, 'Choose your installed'):
            appearance.change(self.args, self.api)
        self.assertFalse(self.active.exists())
        self.mocks[2].assert_not_called()

    def test_unsupported_source_is_rejected_before_download(self):
        self.args.game = self.root / 'unsupported'
        with self.assertRaisesRegex(ValueError, 'version differs'):
            appearance.change(self.args, self.api)
        self.mocks[2].assert_not_called()

    def test_unsupported_discovery_is_not_silently_selected(self):
        self.args.game = None
        with patch('wizard_setup.discover', return_value=[str(self.root / 'unsupported')]):
            self.assertEqual(appearance.inspect(self.args, self.api)['source'], '')

    def test_low_space_fails_before_download(self):
        with patch.object(appearance.shutil, 'disk_usage', return_value=SimpleNamespace(free=1)):
            with self.assertRaisesRegex(ValueError, '256 MB'):
                appearance.change(self.args, self.api)
        self.mocks[2].assert_not_called()

    def test_failed_export_preserves_plain_appearance(self):
        self.mocks[3].side_effect = ValueError('export failed')
        with self.assertRaisesRegex(ValueError, 'export failed'):
            appearance.change(self.args, self.api)
        self.assertFalse(self.active.exists())
        self.assertEqual(list(self.model.glob('.appearance-*')), [])

    def test_failed_tool_preserves_plain_appearance(self):
        self.mocks[2].side_effect = OSError('download failed')
        with self.assertRaisesRegex(OSError, 'download failed'):
            appearance.change(self.args, self.api)
        self.mocks[3].assert_not_called()

    def test_cancel_after_export_prevents_publish(self):
        def export(*args):
            self.export(*args)
            self.api.cancel_check = lambda: (_ for _ in ()).throw(InterruptedError('cancelled'))
        self.mocks[3].side_effect = export
        with self.assertRaises(InterruptedError):
            appearance.change(self.args, self.api)
        self.assertFalse(self.active.exists())
        self.assertFalse(self.saved.exists())

    def test_cancel_undo_preserves_active_bytes(self):
        self.populate(self.active)
        self.args.action = 'appearance-undo'
        self.api.cancel_check = lambda: (_ for _ in ()).throw(InterruptedError('cancelled'))
        with self.assertRaises(InterruptedError):
            appearance.change(self.args, self.api)
        appearance.materials.validate(self.active)

    def test_game_lock_blocks_both_mutations(self):
        lock = self.target / 'save/.launch-lock'
        lock.mkdir()
        for action in ('appearance-apply', 'appearance-undo'):
            self.args.action = action
            with self.assertRaisesRegex(ValueError, 'Close the game normally'):
                appearance.change(self.args, self.api)
        self.mocks[2].assert_not_called()

    def test_game_race_after_export_does_not_publish(self):
        def export(*args):
            self.export(*args)
            (self.target / 'save/.launch-lock').mkdir()
        self.mocks[3].side_effect = export
        with self.assertRaisesRegex(ValueError, 'Close the game normally'):
            appearance.change(self.args, self.api)
        self.assertFalse(self.active.exists())

    def test_setup_lock_is_not_removed(self):
        lock = self.root / '.runtime/.seven-setup-lock'
        lock.mkdir()
        with self.assertRaisesRegex(ValueError, 'Another setup'):
            appearance.change(self.args, self.api)
        self.assertTrue(lock.is_dir())

    def test_engine_lock_is_respected(self):
        (self.root / '.runtime/.engine-setup-lock').mkdir()
        with self.assertRaisesRegex(ValueError, 'current setup'):
            appearance.change(self.args, self.api)

    def test_unknown_extra_material_file_is_preserved(self):
        self.populate(self.active)
        (self.active / 'user-note.txt').write_bytes(b'keep')
        self.args.action = 'appearance-undo'
        with self.assertRaisesRegex(ValueError, 'extra files'):
            appearance.change(self.args, self.api)
        self.assertEqual((self.active / 'user-note.txt').read_bytes(), b'keep')

    def test_invalid_existing_materials_are_never_replaced(self):
        self.populate(self.active)
        (self.active / 'body.uv').write_bytes(b'changed')
        self.assertFalse(appearance.inspect(self.args, self.api)['can_apply'])
        with self.assertRaises(ValueError):
            appearance.change(self.args, self.api)
        self.assertEqual((self.active / 'body.uv').read_bytes(), b'changed')

    def test_duplicate_archive_is_preserved(self):
        self.populate(self.active)
        self.populate(self.saved)
        with self.assertRaisesRegex(ValueError, 'both exist'):
            appearance.change(self.args, self.api)
        appearance.materials.validate(self.active)
        appearance.materials.validate(self.saved)

    def test_failed_atomic_rename_leaves_reusable_materials(self):
        self.populate(self.saved)
        with patch.object(Path, 'rename', side_effect=OSError('publish failed')):
            with self.assertRaisesRegex(OSError, 'publish failed'):
                appearance.change(self.args, self.api)
        appearance.materials.validate(self.saved)
        self.assertFalse(self.active.exists())

    def test_normal_migration_improves_existing_legacy_geometry_without_opt_in(self):
        self.args.action = 'appearance-migrate'
        self.args.game = None
        result = appearance.migrate(self.args, self.api)
        self.assertTrue(result['active'])
        self.assertFalse(result['needs_attention'])
        self.mocks[3].assert_called_once()

    def test_no_source_keeps_existing_car_and_requests_only_missing_prerequisite(self):
        self.args.game = None
        with patch('wizard_setup.discover', return_value=[]):
            result = appearance.migrate(self.args, self.api)
        self.assertTrue(result['needs_attention'])
        self.assertFalse(result['active'])
        self.mocks[2].assert_not_called()
        self.mocks[3].assert_not_called()

    def test_later_starts_reuse_successful_conversion_without_source_or_download(self):
        self.assertTrue(appearance.migrate(self.args, self.api)['active'])
        before = {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.active.iterdir()}
        self.mocks[2].reset_mock()
        self.mocks[3].reset_mock()
        self.args.game = None
        with patch('wizard_setup.discover', side_effect=AssertionError('Must reuse active materials')):
            for _ in range(3):
                result = appearance.migrate(self.args, self.api)
                self.assertTrue(result['active'])
                self.assertFalse(result['needs_attention'])
        self.assertEqual({p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in self.active.iterdir()}, before)
        self.mocks[2].assert_not_called()
        self.mocks[3].assert_not_called()

    def test_failed_automatic_conversion_keeps_previous_appearance(self):
        self.mocks[3].side_effect = ValueError('conversion failed')
        result = appearance.migrate(self.args, self.api)
        self.assertTrue(result['needs_attention'])
        self.assertIn('conversion failed', result['message'])
        self.assertFalse(self.active.exists())

    def test_automatic_migration_respects_explicit_revert(self):
        self.populate(self.saved)
        self.args.game = None
        result = appearance.migrate(self.args, self.api)
        self.assertTrue(result['reusable'])
        self.assertFalse(result['active'])
        self.assertFalse(result['needs_attention'])
        self.mocks[2].assert_not_called()
        self.mocks[3].assert_not_called()

    def test_normal_setup_also_preserves_explicit_revert(self):
        import seven_launcher
        self.populate(self.saved)
        with patch.object(seven_launcher, 'DATA_ROOT', self.root):
            seven_launcher.prepare_car_materials(SimpleNamespace(character='octane', game=self.game), self.target, self.root)
        self.assertFalse(self.active.exists())
        self.assertTrue(self.saved.is_dir())
        self.mocks[2].assert_not_called()
        self.mocks[3].assert_not_called()

    def test_customized_materials_are_kept_on_automatic_migration(self):
        self.populate(self.active)
        (self.active / 'body.uv').write_bytes(b'custom')
        self.assertTrue(appearance.migrate(self.args, self.api)['needs_attention'])
        self.assertEqual((self.active / 'body.uv').read_bytes(), b'custom')
        self.mocks[3].assert_not_called()

    def test_automatic_migration_defers_while_game_is_running(self):
        (self.target / 'save/.launch-lock').mkdir()
        result = appearance.migrate(self.args, self.api)
        self.assertTrue(result['deferred'])
        self.assertFalse(result['needs_attention'])
        self.mocks[3].assert_not_called()

    def test_incompatible_engine_does_not_activate_materials(self):
        original_read = appearance.materials.read
        def read(path, limit):
            data = original_read(path, limit)
            if Path(path).name == 'PACKAGE-MANIFEST.json':
                manifest = json.loads(data)
                manifest.pop('car_materials')
                return json.dumps(manifest).encode()
            return data
        with patch.object(appearance.materials, 'read', side_effect=read):
            result = appearance.migrate(self.args, self.api)
        self.assertTrue(result['needs_attention'])
        self.assertIn('compatible car renderer', result['message'])
        self.assertFalse(self.active.exists())
        self.mocks[3].assert_not_called()

    def test_changed_engine_before_activation_preserves_old_appearance(self):
        real_check = appearance.verify_engine
        calls = 0
        def check(api):
            nonlocal calls
            calls += 1
            if calls > 1:
                raise ValueError('engine changed')
            return real_check(api)
        with patch.object(appearance, 'verify_engine', side_effect=check):
            self.assertTrue(appearance.migrate(self.args, self.api)['needs_attention'])
        self.assertFalse(self.active.exists())


if __name__ == '__main__':
    unittest.main()
