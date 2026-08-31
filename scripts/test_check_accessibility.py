import unittest

import check_accessibility


class AccessibilityContractTests(unittest.TestCase):
    def test_wcag_reference_ratios(self):
        self.assertAlmostEqual(check_accessibility.contrast_ratio("#000000", "#ffffff"), 21.0)
        self.assertAlmostEqual(check_accessibility.contrast_ratio("#777777", "#ffffff"), 4.478, places=3)

    def test_current_contract_passes(self):
        root = check_accessibility.ROOT
        failures = check_accessibility.check_contract(
            (root / "web/src/styles.css").read_text(encoding="utf-8"),
            (root / "web/src/App.tsx").read_text(encoding="utf-8"),
            (root / "src/ui/Source/EditorShell.cpp").read_text(encoding="utf-8"),
            (root / "src/app/PluginEditor.cpp").read_text(encoding="utf-8"),
        )
        self.assertEqual(failures, [])

    def test_missing_non_color_and_reduced_motion_contracts_fail(self):
        root = check_accessibility.ROOT
        styles = (root / "web/src/styles.css").read_text(encoding="utf-8").replace(
            "transition-duration: .001ms !important", "transition-duration: 1s"
        )
        app = (root / "web/src/App.tsx").read_text(encoding="utf-8").replace("AUDIO / SOLID", "AUDIO")
        failures = check_accessibility.check_contract(
            styles,
            app,
            (root / "src/ui/Source/EditorShell.cpp").read_text(encoding="utf-8"),
            (root / "src/app/PluginEditor.cpp").read_text(encoding="utf-8"),
        )
        self.assertTrue(any("reduced-motion" in failure for failure in failures))
        self.assertTrue(any("AUDIO / SOLID" in failure for failure in failures))

    def test_missing_native_to_web_focus_handoff_fails(self):
        root = check_accessibility.ROOT
        shell = (root / "src/ui/Source/EditorShell.cpp").read_text(encoding="utf-8").replace(
            "browser_->setWantsKeyboardFocus(true);", "browser_->setWantsKeyboardFocus(false);"
        )
        failures = check_accessibility.check_contract(
            (root / "web/src/styles.css").read_text(encoding="utf-8"),
            (root / "web/src/App.tsx").read_text(encoding="utf-8"),
            shell,
            (root / "src/app/PluginEditor.cpp").read_text(encoding="utf-8"),
        )
        self.assertTrue(any("setWantsKeyboardFocus" in failure for failure in failures))


if __name__ == "__main__":
    unittest.main()
