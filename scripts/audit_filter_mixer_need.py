#!/usr/bin/env python3
"""Measure filter and multi-input mixing patterns in released factory patches."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def _parameter(node: dict, parameter_id: str) -> float | None:
    for parameter in node.get("parameters", []):
        if parameter.get("id") == parameter_id:
            value = parameter.get("value")
            return float(value) if isinstance(value, (int, float)) else None
    return None


def inspect_patch(path: Path) -> dict:
    document = json.loads(path.read_text(encoding="utf-8"))
    semantic = document["semantic"]
    nodes = {node["id"]: node for node in semantic["nodes"]}
    incoming: dict[str, list[str]] = {node_id: [] for node_id in nodes}
    outgoing: dict[str, list[str]] = {node_id: [] for node_id in nodes}
    for connection in semantic["connections"]:
        source = connection["from"]["nodeId"]
        target = connection["to"]["nodeId"]
        incoming[target].append(source)
        outgoing[source].append(target)

    highpasses: list[dict] = []
    for lowpass_id, lowpass in nodes.items():
        if lowpass.get("type") != "lowpass" or len(incoming[lowpass_id]) != 1:
            continue
        source_id = incoming[lowpass_id][0]
        for gain_id in outgoing[lowpass_id]:
            gain = nodes[gain_id]
            if gain.get("type") != "gain" or _parameter(gain, "gain") != -1.0:
                continue
            for sum_id in outgoing[gain_id]:
                if nodes[sum_id].get("type") == "sum" and source_id in incoming[sum_id]:
                    highpasses.append({"source": source_id, "lowpass": lowpass_id, "invert": gain_id, "sum": sum_id})

    def sum_leaf_count(node_id: str, visiting: set[str] | None = None) -> int:
        visiting = set() if visiting is None else visiting
        if node_id in visiting:
            return 0
        if nodes[node_id].get("type") != "sum":
            return 1
        return sum(sum_leaf_count(source, visiting | {node_id}) for source in incoming[node_id])

    sums = [node_id for node_id, node in nodes.items() if node.get("type") == "sum"]
    terminal_sums = [node_id for node_id in sums if not any(nodes[target].get("type") == "sum" for target in outgoing[node_id])]
    mix_inputs = [sum_leaf_count(node_id) for node_id in terminal_sums]
    matrix_coefficients = [node_id for node_id in nodes if node_id.startswith("matrix-") and "-from-" in node_id and nodes[node_id].get("type") == "gain"]
    matrix_sums = [node_id for node_id in nodes if node_id.startswith("matrix-") and nodes[node_id].get("type") == "sum"]

    return {
        "factory": path.stem.removesuffix(".rvp"),
        "nodeCount": len(nodes),
        "lowpassCount": sum(node.get("type") == "lowpass" for node in nodes.values()),
        "subtractiveHighpassCount": len(highpasses),
        "subtractiveHighpassMembers": highpasses,
        "bandpassPrimitiveCount": sum(node.get("type") == "bandpass" for node in nodes.values()),
        "sumCount": len(sums),
        "terminalMixesWithThreeOrMoreInputs": sum(count >= 3 for count in mix_inputs),
        "maximumExplicitMixInputs": max(mix_inputs, default=0),
        "matrixMixer4x4": len(matrix_coefficients) == 16 and len(matrix_sums) == 12,
    }


def audit(directory: Path) -> dict:
    patches = [inspect_patch(path) for path in sorted(directory.glob("*.rvp.json"))]
    return {
        "schemaVersion": 1,
        "basis": "released schema-v2 factory patch graphs; presentation metadata excluded",
        "factoryCount": len(patches),
        "totals": {
            "nodes": sum(patch["nodeCount"] for patch in patches),
            "lowpasses": sum(patch["lowpassCount"] for patch in patches),
            "subtractiveHighpasses": sum(patch["subtractiveHighpassCount"] for patch in patches),
            "bandpassPrimitives": sum(patch["bandpassPrimitiveCount"] for patch in patches),
            "sums": sum(patch["sumCount"] for patch in patches),
            "terminalMixesWithThreeOrMoreInputs": sum(patch["terminalMixesWithThreeOrMoreInputs"] for patch in patches),
            "fourByFourMatrixMixers": sum(patch["matrixMixer4x4"] for patch in patches),
        },
        "patches": patches,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("factory_directory", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = audit(args.factory_directory)
    rendered = json.dumps(result, indent=2) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8", newline="\n")
    else:
        print(rendered, end="")


if __name__ == "__main__":
    main()
