#!/usr/bin/env python3

import importlib.util
import json
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("qualify_hybrid_executor.py")
SPEC = importlib.util.spec_from_file_location("qualify_hybrid_executor", MODULE_PATH)
qualification = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(qualification)


class HybridExecutorQualificationTests(unittest.TestCase):
    def test_published_matrices_pass_every_m17_exit_gate(self):
        result = qualification.build_qualification(
            json.loads(qualification.BASELINE.read_text(encoding="utf-8")),
            json.loads(qualification.OPTIMIZED.read_text(encoding="utf-8")),
        )
        self.assertTrue(result["overallPassed"])
        self.assertEqual(result["gates"]["finiteOutputCases"], 75)
        self.assertEqual(result["gates"]["shimmerImprovementCases"], 20)
        self.assertEqual(result["gates"]["preparedMemoryNonRegressionCases"], 75)
        self.assertLessEqual(result["measurements"]["compileRegressionRatio"], 1.10)
        self.assertLessEqual(result["measurements"]["requestToActiveRegressionRatio"], 1.10)
        self.assertFalse(result["compileRegressionRationaleRequired"])
        self.assertTrue(result["gates"]["normalP95NonRegression"])
        self.assertTrue(result["gates"]["crossfadeP95NonRegression"])

    def test_missing_shimmer_fusion_fails_qualification(self):
        baseline = json.loads(qualification.BASELINE.read_text(encoding="utf-8"))
        optimized = json.loads(qualification.OPTIMIZED.read_text(encoding="utf-8"))
        for case in optimized["cases"]:
            if case["graphId"] == "reverse-cosmic-shimmer" and int(case["sampleRate"]) == 48_000:
                case["graph"]["fusedNodeCount"] = 0
        result = qualification.build_qualification(baseline, optimized)
        self.assertFalse(result["overallPassed"])
        self.assertLess(result["gates"]["shimmerImprovementCases"], 20)


if __name__ == "__main__":
    unittest.main()
