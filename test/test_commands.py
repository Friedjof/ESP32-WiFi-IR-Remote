"""Check bundled remote button layouts."""

import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class RemoteContractTests(unittest.TestCase):
    def test_layouts_have_different_buttons(self):
        profiles = [json.loads(path.read_text()) for path in sorted((ROOT / "data/profiles").glob("*.json"))]
        for profile in profiles:
            buttons = [button["command"] for page in profile["pages"] for group in page["groups"] for button in group["buttons"]]
            self.assertEqual(len(buttons), len(set(buttons)))
        profiles_by_id = {profile["id"]: profile for profile in profiles}
        def buttons(profile_id):
            return {button["command"] for page in profiles_by_id[profile_id]["pages"]
                    for group in page["groups"] for button in group["buttons"]}
        self.assertNotEqual(buttons("toshiba_tv"), buttons("media_player"))
        self.assertIn("digit_0", buttons("samsung_ue32f4000"))
        self.assertIn("digit_0", buttons("lg_c1"))
        self.assertIn("digit_0", buttons("sony_rmt_tx100d"))


if __name__ == "__main__":
    unittest.main()
