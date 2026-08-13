import unittest

import check_documentation


class DocumentationContractTests(unittest.TestCase):
    def valid_inputs(self):
        module_source = "\n".join(
            f"{{ type: '{name}', label: 'Label', role: 'io', ports: [], parameters: [] }},"
            for name in ("input", "delay")
        )
        reference = "\n".join(
            ["<!-- module: input -->", "<!-- module: delay -->"]
            + [f"<!-- visualization: {name} -->" for name in check_documentation.REQUIRED_VISUALIZATIONS]
        )
        development = "\n".join(check_documentation.CANONICAL_COMMANDS)
        tutorial = " ".join(
            ("Barr Reference", "Trigger Impulse", "Capture impulse", "Diagnostics", "Save Patch", "Load Patch")
        )
        gravity = "\n".join(check_documentation.REQUIRED_GRAVITY_PHRASES)
        return module_source, reference, development, tutorial, gravity

    def test_accepts_complete_contract(self):
        self.assertEqual(check_documentation.check_contract(*self.valid_inputs()), [])

    def test_reports_new_undocumented_module(self):
        source, reference, development, tutorial, gravity = self.valid_inputs()
        source += "\n{ type: 'new-module', label: 'New', role: 'io', ports: [], parameters: [] },"
        failures = check_documentation.check_contract(source, reference, development, tutorial, gravity)
        self.assertTrue(any("new-module" in failure for failure in failures))

    def test_reports_missing_visualization_command_and_tutorial_action(self):
        source, reference, development, tutorial, gravity = self.valid_inputs()
        reference = reference.replace("<!-- visualization: diagnostics -->", "")
        development = development.replace("pnpm --dir web test", "")
        tutorial = tutorial.replace("Save Patch", "")
        failures = check_documentation.check_contract(source, reference, development, tutorial, gravity)
        self.assertTrue(any("diagnostics" in failure for failure in failures))
        self.assertTrue(any("pnpm --dir web test" in failure for failure in failures))
        self.assertTrue(any("Save Patch" in failure for failure in failures))

    def test_reports_incomplete_gravity_contract(self):
        source, reference, development, tutorial, gravity = self.valid_inputs()
        gravity = gravity.replace("timeToPeakMs", "")
        failures = check_documentation.check_contract(
            source, reference, development, tutorial, gravity
        )
        self.assertTrue(any("timeToPeakMs" in failure for failure in failures))


if __name__ == "__main__":
    unittest.main()
