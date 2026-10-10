"""Guard the researched hypotheses against accidental promotion to verified codes."""

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class CandidateCatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.profiles = {item['id']: item for item in json.loads(
            (ROOT / 'data/candidates.json').read_text())['profiles']}

    def test_every_candidate_is_explicitly_unverified_for_target(self):
        self.assertEqual(len(self.profiles), 5)
        for profile in self.profiles.values():
            self.assertNotEqual(profile['status'], 'verified_for_target')
            self.assertTrue(profile['source'])
            self.assertNotIn('factory_reset', profile['commands'])

    def test_flipper_nec_bit_order_is_distinct_from_user_values(self):
        flipper = self.profiles['toshiba_ct90325']['commands']
        supplied = self.profiles['toshiba_user_nec']['commands']
        self.assertEqual(flipper['power']['code'], '0x02FD48B7')
        self.assertEqual(flipper['volume_up']['code'], '0x02FD58A7')
        self.assertEqual(flipper['volume_down']['code'], '0x02FD7887')
        self.assertEqual(supplied['power']['code'], '0x40BF12ED')
        self.assertNotEqual(flipper['power']['code'], supplied['power']['code'])

    def test_rc5_address_and_36khz(self):
        profile = self.profiles['toshiba_ct8563']
        self.assertEqual(profile['frequency'], 36000)
        self.assertEqual(profile['commands']['volume_up']['code'], '0x050')
        self.assertEqual(profile['commands']['volume_up']['bits'], 12)

    def test_embedded_catalog_is_current(self):
        raw = json.dumps({'profiles': list(self.profiles.values())}, separators=(',', ':')).encode()
        header = (ROOT / 'include/generated/candidates.h').read_text()
        escaped = ''.join(f'\\x{byte:02x}' for byte in raw)
        embedded = ''.join(line.strip().strip('"') for line in header.splitlines()
                           if line.strip().startswith('"'))
        self.assertEqual(embedded, escaped)


if __name__ == '__main__':
    unittest.main()
