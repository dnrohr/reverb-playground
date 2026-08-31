import json
import tempfile
import unittest
from pathlib import Path

from audit_filter_mixer_need import audit, inspect_patch


class FilterMixerAuditTests(unittest.TestCase):
    def test_detects_structural_subtractive_highpass_and_explicit_mix(self):
        nodes = [
            {"id": "source", "type": "delay", "parameters": []},
            {"id": "low", "type": "lowpass", "parameters": []},
            {"id": "invert", "type": "gain", "parameters": [{"id": "gain", "value": -1}]},
            {"id": "subtract", "type": "sum", "parameters": []},
            {"id": "third", "type": "gain", "parameters": [{"id": "gain", "value": 0.5}]},
            {"id": "mix", "type": "sum", "parameters": []},
        ]
        edges = [("source", "low"), ("low", "invert"), ("source", "subtract"), ("invert", "subtract"), ("subtract", "mix"), ("third", "mix")]
        document = {"semantic": {"nodes": nodes, "connections": [
            {"id": str(index), "from": {"nodeId": source, "portId": "out"}, "to": {"nodeId": target, "portId": "in"}}
            for index, (source, target) in enumerate(edges)
        ]}}
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "fixture.rvp.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            result = inspect_patch(path)
        self.assertEqual(result["subtractiveHighpassCount"], 1)
        self.assertEqual(result["maximumExplicitMixInputs"], 3)
        self.assertEqual(result["terminalMixesWithThreeOrMoreInputs"], 1)

    def test_released_catalog_has_expected_scope(self):
        root = Path(__file__).resolve().parents[1]
        result = audit(root / "factory-patches")
        self.assertEqual(result["factoryCount"], 9)
        self.assertGreater(result["totals"]["lowpasses"], 0)
        self.assertGreater(result["totals"]["subtractiveHighpasses"], 0)
        self.assertEqual(result["totals"]["bandpassPrimitives"], 0)
        self.assertEqual(result["totals"]["fourByFourMatrixMixers"], 1)

    def test_published_measurement_matches_released_factories(self):
        root = Path(__file__).resolve().parents[1]
        published = json.loads((root / "artifacts/measurements/m27-filter-mixer-need.json").read_text(encoding="utf-8"))
        self.assertEqual(published, audit(root / "factory-patches"))


if __name__ == "__main__":
    unittest.main()
