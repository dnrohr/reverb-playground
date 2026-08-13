import unittest

import check_build_identity


class BuildIdentityTests(unittest.TestCase):
    def test_reads_configured_commit(self):
        cache = "A:STRING=x\nREVERB_BUILD_COMMIT:STRING=0123456789ab\nB:BOOL=ON\n"
        self.assertEqual(check_build_identity.configured_commit(cache), "0123456789ab")

    def test_missing_or_wrong_type_is_rejected(self):
        self.assertIsNone(check_build_identity.configured_commit("A:STRING=x\n"))
        self.assertIsNone(check_build_identity.configured_commit("REVERB_BUILD_COMMIT:INTERNAL=abc\n"))


if __name__ == "__main__":
    unittest.main()
