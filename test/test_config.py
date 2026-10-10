"""Release checks for the shipped, deliberately unconfigured profiles."""

import json
import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROFILES = sorted((ROOT / "data/profiles").glob("*.json"))


class ProfileConfigurationTests(unittest.TestCase):
    def test_two_distinct_factory_profiles(self):
        self.assertGreaterEqual(len(PROFILES), 2)
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
                    self.assertIsNone(command["code"])

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
