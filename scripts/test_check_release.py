#!/usr/bin/env python3

import importlib.util
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("check_release.py")
SPEC = importlib.util.spec_from_file_location("check_release", MODULE_PATH)
check_release = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(check_release)


class ReleaseContractTests(unittest.TestCase):
    def test_repository_release_contract_is_complete(self):
        self.assertEqual(check_release.check_contract(), [])

    def test_missing_release_disclosures_are_reported(self):
        def altered_read(path: Path) -> str:
            text = path.read_text(encoding="utf-8")
            if path == check_release.NOTES:
                return text.replace("## Known alpha limitations", "## Deferred details")
            return text

        failures = check_release.check_contract(altered_read)
        self.assertTrue(any("Known alpha limitations" in failure for failure in failures))

    def test_missing_development_package_is_reported(self):
        def altered_read(path: Path) -> str:
            text = path.read_text(encoding="utf-8")
            if path == check_release.ROOT / ".github/workflows/verify.yml":
                return text.replace("windows-development-package:", "windows-package-disabled:")
            return text

        failures = check_release.check_contract(altered_read)
        self.assertTrue(any("windows-development-package:" in failure for failure in failures))


if __name__ == "__main__":
    unittest.main()
