"""Ensure the two factory remotes demonstrate different button layouts."""

import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class RemoteContractTests(unittest.TestCase):
    def test_layouts_have_different_buttons(self):
        profiles = [json.loads(path.read_text()) for path in sorted((ROOT / "data/profiles").glob("*.json"))]
        command_sets = []
        for profile in profiles:
            buttons = [button["command"] for page in profile["pages"] for group in page["groups"] for button in group["buttons"]]
            command_sets.append(set(buttons))
            self.assertEqual(len(buttons), len(set(buttons)))
        self.assertNotEqual(*command_sets[:2])


if __name__ == "__main__":
    unittest.main()
