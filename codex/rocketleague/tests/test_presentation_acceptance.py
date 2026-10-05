"""Synthetic verifier negatives; these records are NOT native gameplay evidence."""
import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from verify_network_motion import verify_presentation_handoff


class PresentationAcceptance(unittest.TestCase):
    def test_requires_sent_delivered_and_drawn_same_presentation(self):
        pose = {'sequence': 12, 'active': 2}
        verify_presentation_handoff({12: pose}, [pose], [pose])
        for sent, accepted, drawn in (
            ({}, [pose], [pose]),
            ({12: pose}, [], [pose]),
            ({12: pose}, [pose], []),
            ({12: pose}, [pose], [{'sequence': 12, 'active': 1}]),
            ({12: pose}, [pose], [{'sequence': 13, 'active': 2}]),
            ({12: pose}, [{'sequence': 13, 'active': 2}], [pose]),
        ):
            with self.subTest(sent=sent, accepted=accepted, drawn=drawn):
                with self.assertRaises(AssertionError):
                    verify_presentation_handoff(sent, accepted, drawn)


if __name__ == '__main__':
    unittest.main()
