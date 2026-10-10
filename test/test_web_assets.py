"""Verify that every embedded web asset round-trips through gzip."""

import gzip
import importlib.util
import re
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts/web-to-header.py"
spec = importlib.util.spec_from_file_location("web_to_header", SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class WebAssetTests(unittest.TestCase):
    def test_generated_assets_match_sources(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "web_files.h"
            module.build(ROOT / "data", output)
            header = output.read_text()
            arrays = re.findall(r"static const uint8_t web_asset_\d+\[\] PROGMEM = \{(.*?)\};", header, re.S)
            self.assertEqual(len(arrays), len(module.ASSETS))
            for array, name in zip(arrays, module.ASSETS):
                with self.subTest(name=name):
                    packed = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]{2})", array))
                    self.assertEqual(gzip.decompress(packed), (ROOT / "data" / name).read_bytes())


if __name__ == "__main__":
    unittest.main()
