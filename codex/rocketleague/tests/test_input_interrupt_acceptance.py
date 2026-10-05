"""Synthetic reader regressions, not native input acceptance."""
import json
from pathlib import Path
import tempfile
import unittest
from analyze_gameplay import analyze_interrupts


def samples():
    return [dict(ms=ms,focus=1,console=int(500<=ms<2000),jump=int(ms>=6350),
                 boost=96 if ms>=6500 else 100,
                 raw_jump=int(750<=ms<3000 or 4500<=ms<6000 or 6300<=ms<6450),
                 raw_boost=int(750<=ms<3000 or 4500<=ms<6000 or 6500<=ms<6700),
                 requested_jump=int(6300<=ms<6450),requested_boost=int(6500<=ms<6700))
            for ms in range(0,8001,50)]


class InterruptionEvidence(unittest.TestCase):
    def check(self, rows):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'stderr.log'
            path.write_text('ROCKET_INPUT_QA_DETACH\nROCKET_INPUT_QA_RECONNECT_HELD\nROCKET_INPUT_QA_END\n'+
                            '\n'.join('ROCKET_INPUT_QA_STATE '+json.dumps(s) for s in rows))
            return analyze_interrupts(path)

    def test_combined_filter_neutralizes_held_device_buttons(self):
        self.assertEqual(self.check(samples())['schema'],'rocket-gamepad-interruption-observation-v2')

    def test_held_device_evidence_is_required(self):
        for field in ('raw_jump','raw_boost'):
            rows=samples();rows[100][field]=0
            with self.subTest(field=field),self.assertRaises(AssertionError):self.check(rows)

    def test_filtered_request_leaks_are_rejected(self):
        for ms in (1500,2500,5000):
            for field in ('requested_jump','requested_boost'):
                rows=samples();rows[ms//50][field]=1
                with self.subTest(ms=ms,field=field),self.assertRaises(AssertionError):self.check(rows)

    def test_native_effect_leaks_are_rejected(self):
        for field,value in (('jump',1),('boost',99)):
            rows=samples();rows[100][field]=value
            with self.subTest(field=field),self.assertRaises(AssertionError):self.check(rows)

    def test_real_release_and_repress_required(self):
        for field in ('raw_jump','requested_jump','jump','raw_boost','requested_boost'):
            rows=samples()
            for row in rows:
                if row['ms']>=6300:row[field]=0
            with self.subTest(field=field),self.assertRaises(AssertionError):self.check(rows)

    def test_legacy_missing_raw_telemetry_is_incomplete(self):
        rows=samples()
        for row in rows:row.pop('raw_jump')
        with self.assertRaises(KeyError):self.check(rows)


if __name__=='__main__':unittest.main()
