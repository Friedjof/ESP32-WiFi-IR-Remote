"""Release checks for blank templates and sourced TV profiles."""

import json
import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROFILES = sorted((ROOT / "data/profiles").glob("*.json"))


class ProfileConfigurationTests(unittest.TestCase):
    def test_five_distinct_bundled_profiles(self):
        self.assertEqual(len(PROFILES), 5)
        self.assertEqual(len(PROFILES), len({path.stem for path in PROFILES}))

    def test_codes_and_layout(self):
        for path in PROFILES:
            with self.subTest(profile=path.stem):
                config = json.loads(path.read_text())
                self.assertEqual(config["id"], path.stem)
                self.assertFalse(config["receiver_enabled"])
                self.assertGreaterEqual(config["repeat_interval_ms"], 120)
                self.assertLessEqual(config["repeat_interval_ms"], 1000)
                self.assertTrue(config["pages"])
                visible = []
                for page in config["pages"]:
                    for group in page["groups"]:
                        self.assertGreaterEqual(group["columns"], 1)
                        self.assertLessEqual(group["columns"], 4)
                        visible.extend(button["command"] for button in group["buttons"])
                self.assertTrue(set(visible) <= set(config["commands"]))
                for command in config["commands"].values():
                    if path.stem in {"toshiba_tv", "media_player"}:
                        self.assertIsNone(command["code"])
                    else:
                        self.assertTrue(command["code"].startswith("0x"))
                if path.stem not in {"toshiba_tv", "media_player"}:
                    self.assertFalse(config["origin"]["target_verified"])
                    self.assertIn("Flipper-IRDB", config["origin"]["source_url"])

    def test_sourced_protocol_examples(self):
        expected = {
            "samsung_ue32f4000": ("SAMSUNG", "0xE0E040BF", 32, 38000),
            "lg_c1": ("NEC", "0x20DF10EF", 32, 38000),
            "sony_rmt_tx100d": ("SONY", "0xA90", 12, 40000),
        }
        for name, (protocol, code, bits, frequency) in expected.items():
            with self.subTest(profile=name):
                config = json.loads((ROOT / "data/profiles" / f"{name}.json").read_text())
                power = config["commands"]["power"]
                self.assertEqual((power["protocol"], power["code"], power["bits"], power["frequency"]),
                                 (protocol, code, bits, frequency))
        sony = json.loads((ROOT / "data/profiles/sony_rmt_tx100d.json").read_text())
        self.assertEqual(sony["commands"]["back"]["bits"], 15)

    def test_embedded_profiles_match_source_files(self):
        script = ROOT / "scripts/profiles-to-header.py"
        spec = importlib.util.spec_from_file_location("profiles_to_header", script)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "default_profiles.h"
            module.build(ROOT / "data/profiles", output)
            self.assertEqual(output.read_bytes(), (ROOT / "include/generated/default_profiles.h").read_bytes())


if __name__ == "__main__":
    unittest.main()
