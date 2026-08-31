import type { Edge, Node } from '@xyflow/react';
import type { PatchNodeData } from './graph';
import { projectCompoundPresentation, type CompoundPresentation } from './compoundPresentation';

export interface MatrixMixerInspection extends CompoundPresentation {
  coefficientNodeIds: string[];
  internalNodeIds: string[];
  coefficients: number[][];
  rowEnergy: number[];
  columnEnergy: number[];
  orthogonal: boolean;
}

const coefficientId = /^matrix-([1-4])-from-([1-4])(?<suffix>-copy\d*)?$/;
const internalSumId = /^matrix-[1-4]-(?:sum-a|sum-b|out)(?<suffix>-copy\d*)?$/;

export function inspectMatrixMixers(nodes: Node<PatchNodeData>[]): MatrixMixerInspection[] {
  const suffixes = new Set<string>();
  for (const node of nodes) { const match = coefficientId.exec(node.id); if (match) suffixes.add(match.groups?.suffix ?? ''); }
  const results: MatrixMixerInspection[] = [];
  for (const suffix of [...suffixes].sort()) {
    const matchesSuffix = (match: RegExpExecArray | null) => match && (match.groups?.suffix ?? '') === suffix;
    const coefficientNodes = nodes.filter((node) => matchesSuffix(coefficientId.exec(node.id)));
    const sumNodes = nodes.filter((node) => matchesSuffix(internalSumId.exec(node.id)));
    if (coefficientNodes.length !== 16 || sumNodes.length !== 12) continue;
    const coefficients = Array.from({ length: 4 }, () => Array(4).fill(Number.NaN));
    for (const node of coefficientNodes) {
      const match = coefficientId.exec(node.id)!; const value = node.data.parameters.find((parameter) => parameter.id === 'gain')?.value;
      if (typeof value !== 'number' || !Number.isFinite(value)) continue;
      coefficients[Number(match[1]) - 1]![Number(match[2]) - 1] = value;
    }
    if (coefficients.flat().some((value) => !Number.isFinite(value))) continue;
    const energy = (values: number[]) => values.reduce((total, value) => total + value * value, 0);
    const rowEnergy = coefficients.map(energy); const columnEnergy = [0, 1, 2, 3].map((column) => energy(coefficients.map((row) => row[column]!)));
    const dot = (a: number[], b: number[]) => a.reduce((total, value, index) => total + value * b[index]!, 0);
    const orthogonal = rowEnergy.every((value) => Math.abs(value - 1) <= 1e-9) && columnEnergy.every((value) => Math.abs(value - 1) <= 1e-9)
      && coefficients.every((row, index) => coefficients.every((other, otherIndex) => index === otherIndex || Math.abs(dot(row, other)) <= 1e-9));
    const amplifying = rowEnergy.some((value) => value > 1 + 1e-9) || columnEnergy.some((value) => value > 1 + 1e-9);
    const reason = amplifying ? 'Expanded: matrix row or column energy exceeds unity; no automatic normalization is applied.'
      : orthogonal ? 'Normalized orthogonal matrix; vector energy is preserved.' : 'Non-orthogonal but non-amplifying matrix; coefficients remain explicit.';
    const internalNodeIds = [...coefficientNodes, ...sumNodes].map((node) => node.id).sort();
    results.push({ id: `matrix-mixer${suffix}`, kind: 'matrix-mixer-4x4', label: `Matrix Mixer 4×4${suffix ? ' copy' : ''}`,
      memberNodeIds: internalNodeIds, coefficientNodeIds: coefficientNodes.map((node) => node.id).sort(), internalNodeIds,
      coefficients, rowEnergy, columnEnergy, orthogonal, canCollapse: !amplifying, reason,
      summary: 'Four inputs are multiplied by 16 visible signed gains and recombined by 12 explicit sums.',
      learn: 'This compact boundary is a navigation view of an ordinary 4×4 matrix. Expand it to inspect every coefficient, sum, feedback path, and executable primitive.',
    });
  }
  return results;
}

export function inspectMatrixMixer(nodes: Node<PatchNodeData>[]): MatrixMixerInspection | null {
  const inspections = inspectMatrixMixers(nodes); return inspections.find((inspection) => inspection.id === 'matrix-mixer') ?? inspections[0] ?? null;
}

export function collapseMatrixMixer(nodes: Node<PatchNodeData>[], edges: Edge[], inspection: MatrixMixerInspection | null, collapsed: boolean) {
  return inspection ? projectCompoundPresentation(nodes, edges, inspection, collapsed) : { nodes, edges };
}
