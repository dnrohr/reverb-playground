import type { Edge, Node } from '@xyflow/react';
import type { PatchNodeData, PatchPort } from './graph';

export interface MatrixMixerInspection {
  coefficientNodeIds: string[];
  internalNodeIds: string[];
  coefficients: number[][];
  rowEnergy: number[];
  columnEnergy: number[];
  orthogonal: boolean;
  canCollapse: boolean;
  reason: string;
}

const coefficientId = /^matrix-([1-4])-from-([1-4])$/;
const internalSumId = /^matrix-[1-4]-(?:sum-a|sum-b|out)$/;

export function inspectMatrixMixer(nodes: Node<PatchNodeData>[]): MatrixMixerInspection | null {
  const coefficientNodes = nodes.filter((node) => coefficientId.test(node.id));
  const sumNodes = nodes.filter((node) => internalSumId.test(node.id));
  if (coefficientNodes.length !== 16 || sumNodes.length !== 12) return null;
  const coefficients = Array.from({ length: 4 }, () => Array(4).fill(Number.NaN));
  for (const node of coefficientNodes) {
    const match = coefficientId.exec(node.id)!;
    const value = node.data.parameters.find((parameter) => parameter.id === 'gain')?.value;
    if (typeof value !== 'number' || !Number.isFinite(value)) return null;
    coefficients[Number(match[1]) - 1]![Number(match[2]) - 1] = value;
  }
  const energy = (values: number[]) => values.reduce((total, value) => total + value * value, 0);
  const rowEnergy = coefficients.map(energy);
  const columnEnergy = [0, 1, 2, 3].map((column) => energy(coefficients.map((row) => row[column]!)));
  const dot = (a: number[], b: number[]) => a.reduce((total, value, index) => total + value * b[index]!, 0);
  const orthogonal = rowEnergy.every((value) => Math.abs(value - 1) <= 1e-9)
    && columnEnergy.every((value) => Math.abs(value - 1) <= 1e-9)
    && coefficients.every((row, index) => coefficients.every((other, otherIndex) => index === otherIndex || Math.abs(dot(row, other)) <= 1e-9));
  const amplifying = rowEnergy.some((value) => value > 1 + 1e-9) || columnEnergy.some((value) => value > 1 + 1e-9);
  return {
    coefficientNodeIds: coefficientNodes.map((node) => node.id),
    internalNodeIds: [...coefficientNodes, ...sumNodes].map((node) => node.id),
    coefficients,
    rowEnergy,
    columnEnergy,
    orthogonal,
    canCollapse: !amplifying,
    reason: amplifying ? 'Expanded: matrix row or column energy exceeds unity; no automatic normalization is applied.'
      : orthogonal ? 'Normalized orthogonal matrix; vector energy is preserved.'
        : 'Non-orthogonal but non-amplifying matrix; coefficients remain explicit.',
  };
}

const port = (id: string, direction: 'input' | 'output'): PatchPort => ({ id, signal: 'audio', direction });
export function collapseMatrixMixer(
  nodes: Node<PatchNodeData>[], edges: Edge[], inspection: MatrixMixerInspection | null, collapsed: boolean,
): { nodes: Node<PatchNodeData>[]; edges: Edge[] } {
  if (!inspection || !collapsed || !inspection.canCollapse) return { nodes, edges };
  const hidden = new Set(inspection.internalNodeIds);
  const compound: Node<PatchNodeData> = {
    id: 'matrix-mixer-view', type: 'patchNode', position: { x: 1_560, y: 0 }, className: 'matrix-mixer-compound',
    data: {
      label: 'Matrix Mixer 4×4', type: 'matrix-mixer', role: 'routing', runtimeBound: true,
      ports: [...[1, 2, 3, 4].map((index) => port(`in-${index}`, 'input')),
        ...[1, 2, 3, 4].map((index) => port(`out-${index}`, 'output'))],
      parameters: [],
    },
  };
  const visibleEdges = edges.filter((edge) => !hidden.has(edge.source) && !hidden.has(edge.target));
  for (let index = 1; index <= 4; index += 1) {
    visibleEdges.push({ id: `matrix-view-input-${index}`, source: `line-return-${index}`, sourceHandle: 'out',
      target: compound.id, targetHandle: `in-${index}`, data: { signal: 'audio' } });
    visibleEdges.push({ id: `matrix-view-output-${index}`, source: compound.id, sourceHandle: `out-${index}`,
      target: `line-entry-${index}`, targetHandle: 'in-b', data: { signal: 'audio' } });
  }
  return { nodes: [...nodes.filter((node) => !hidden.has(node.id)), compound], edges: visibleEdges };
}
