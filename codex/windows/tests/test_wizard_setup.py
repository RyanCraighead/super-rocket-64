"""Installer inspection tests: real validators, isolated files, no extraction."""
import contextlib
import io
import json
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import seven_launcher as api
import wizard_setup as wizard


class WizardTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='srwiz-')
        self.root = Path(self.temp.name)
        self.old_root = api.DATA_ROOT
        api.DATA_ROOT = self.root / 'data'

    def tearDown(self):
        api.DATA_ROOT = self.old_root
        self.temp.cleanup()

    def args(self, **changes):
        values = dict(action='preflight', character='octane', sm64=None, rom=None, game=None)
        values.update(changes)
        return SimpleNamespace(**values)

    def test_empty_status_is_not_playable_and_does_not_create_data(self):
        with patch.object(wizard, 'discover', return_value=[]):
            report = wizard.status(api)
        self.assertEqual(report['ready'], [])
        self.assertFalse(report['playable'])
        self.assertFalse(report['sm64'])
        self.assertFalse(api.DATA_ROOT.exists())

    def test_missing_sm64_is_actionable_before_extraction(self):
        with self.assertRaisesRegex(ValueError, 'original SM64 US ROM'):
            wizard.preflight(self.args(), api)
        self.assertFalse(api.DATA_ROOT.exists())

    def test_invalid_actual_rom_formats_are_rejected(self):
        invalid = self.root / 'invalid.z64'
        invalid.write_bytes(b'not a supported original game')
        for character in ('octane', 'link', 'bomberman', 'banjo', 'spiderman', 'tony'):
            with self.subTest(character=character):
                if character == 'octane':
                    args = self.args(sm64=invalid)
                    with self.assertRaises((ValueError, OSError)):
                        wizard.preflight(args, api)
                else:
                    with patch.object(wizard, 'status', return_value=dict(sm64=True, ready=[])):
                        with self.assertRaises((ValueError, OSError)):
                            wizard.preflight(self.args(character=character, rom=invalid), api)
        self.assertFalse(api.DATA_ROOT.exists())

    def test_missing_rocket_league_and_unknown_hashes_do_not_provision(self):
        game = self.root / 'Epic Games/rocketleague'
        with patch.object(wizard, 'status', return_value=dict(sm64=True, ready=[])):
            with self.assertRaisesRegex(ValueError, 'Rocket League installation folder'):
                wizard.preflight(self.args(), api)
            with self.assertRaisesRegex(ValueError, 'Missing Rocket League file'):
                wizard.preflight(self.args(game=game), api)
            cooked = game / 'TAGame/CookedPCConsole'; cooked.mkdir(parents=True)
            (cooked / 'Body_Octane_SF.upk').write_bytes(b'unsupported body')
            (cooked / 'wheel_sport80_SF.upk').write_bytes(b'unsupported wheel')
            with self.assertRaisesRegex(ValueError, 'Unsupported Rocket League package version'):
                wizard.preflight(self.args(game=game), api)
        self.assertFalse(api.DATA_ROOT.exists())

    def test_completed_profile_reuses_sources_and_checks_storage(self):
        with patch.object(wizard, 'status', return_value=dict(sm64=True, ready=['octane', 'link'])):
            self.assertTrue(wizard.preflight(self.args(), api)['valid'])
            self.assertTrue(wizard.preflight(self.args(character='link'), api)['valid'])
            with patch.object(wizard.shutil, 'disk_usage', return_value=SimpleNamespace(free=1)):
                with self.assertRaisesRegex(ValueError, 'Free at least'):
                    wizard.preflight(self.args(), api)
        self.assertFalse(api.DATA_ROOT.exists())

    def test_failure_protocol_is_structured_without_traceback(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertEqual(wizard.run(self.args(), api), 0)
        result = json.loads(output.getvalue())
        self.assertFalse(result['valid'])
        self.assertIn('SM64', result['message'])
        self.assertNotIn('Traceback', output.getvalue())

    def test_cached_car_optional_upgrade_does_not_require_supported_wheel(self):
        game = self.root / 'Games/rocketleague'
        (game / 'TAGame/CookedPCConsole').mkdir(parents=True)
        (game / 'TAGame/CookedPCConsole/wheel_sport80_SF.upk').write_bytes(b'new wheel version')
        with patch.object(wizard, 'status', return_value=dict(sm64=True, ready=['octane'])):
            with patch('codex.rocketleague.tools.export_octane.check_game', side_effect=AssertionError('cached geometry must not re-extract')):
                result = wizard.preflight(self.args(game=game), api)
                self.assertTrue(result['valid'])
                self.assertIn('Optional sounds and materials', result['message'])
            with self.assertRaisesRegex(ValueError, 'Rocket League installation folder'):
                wizard.preflight(self.args(game=self.root / 'missing'), api)
        self.assertFalse(api.DATA_ROOT.exists())

    def test_epic_discovery_reads_local_manifest_only(self):
        game = self.root / 'Games/rocketleague'
        (game / 'TAGame/CookedPCConsole').mkdir(parents=True)
        manifests = self.root / 'Epic/EpicGamesLauncher/Data/Manifests'; manifests.mkdir(parents=True)
        (manifests / 'valid.item').write_text(json.dumps(dict(DisplayName='Rocket League', InstallLocation=str(game))))
        (manifests / 'bad.item').write_text('invalid JSON')
        (manifests / 'unrelated.item').write_text(json.dumps(dict(DisplayName='Other game', InstallLocation=str(game))))
        with patch.dict(os.environ, {'PROGRAMDATA': str(self.root)}):
            results = wizard.discover()
        self.assertEqual(results.count(str(game)), 1)
        self.assertFalse(api.DATA_ROOT.exists())


if __name__ == '__main__':
    unittest.main()
