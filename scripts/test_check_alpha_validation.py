import unittest

import check_alpha_validation


class AlphaValidationChecks(unittest.TestCase):
    def complete_protocol(self):
        return "\n".join(check_alpha_validation.REQUIRED_PROTOCOL_PHRASES)

    def prepared_findings(self):
        return "anonymous IDs P[NN]\nKnown open P0 critical findings: **0**\nKnown open P1 high findings: **0**\n"

    def test_preparation_requires_full_journey_and_privacy_contract(self):
        self.assertEqual(
            check_alpha_validation.check_preparation(self.complete_protocol(), self.prepared_findings()),
            [],
        )
        failures = check_alpha_validation.check_preparation(
            self.complete_protocol().replace("Add visible modulation", ""),
            self.prepared_findings() + "Email address: someone@example.invalid\n",
        )
        self.assertTrue(any("modulation" in failure for failure in failures))
        self.assertTrue(any("personal-data" in failure for failure in failures))

    def test_release_requires_three_sessions_and_zero_blockers(self):
        complete = self.prepared_findings() + "Session: P01\nSession: P02\nSession: P03\n"
        self.assertEqual(check_alpha_validation.check_release(complete), [])
        failures = check_alpha_validation.check_release(
            complete.replace("Session: P03\n", "").replace("P1 high findings: **0**", "P1 high findings: **1**")
            + "Awaiting three non-implementer sessions\n"
        )
        self.assertEqual(len(failures), 3)


if __name__ == "__main__":
    unittest.main()
