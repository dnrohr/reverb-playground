import type { Edge, Node } from '@xyflow/react';
import type { PatchNodeData } from './graph';

const clamp = (value: number, minimum: number, maximum: number) => Math.min(maximum, Math.max(minimum, value));
const parameter = (node: Node<PatchNodeData>, id: string, fallback: number) => node.data.parameters.find((item) => item.id === id)?.value ?? fallback;

export type ControlPolarity = 'unipolar' | 'bipolar';

export function lfoValue(timeSeconds: number, frequency: number, phaseCycles: number, waveform: 'sine' | 'triangle'): number {
  const phase = ((timeSeconds * clamp(frequency, 0, 500) + phaseCycles) % 1 + 1) % 1;
  return waveform === 'sine' ? Math.sin(phase * Math.PI * 2) : 1 - 4 * Math.abs(phase - 0.5);
}

export function mapControlValue(input: number, scale: number, offset: number, polarity: ControlPolarity): number {
  const normalized = clamp(Number.isFinite(input) ? input : 0, polarity === 'bipolar' ? -1 : 0, 1);
  return clamp(normalized * scale + offset, -1, 1);
}

export function mappingRange(scale: number, offset: number, polarity: ControlPolarity): { minimum: number; maximum: number } {
  const first = mapControlValue(polarity === 'bipolar' ? -1 : 0, scale, offset, polarity);
  const second = mapControlValue(1, scale, offset, polarity);
  return { minimum: Math.min(first, second), maximum: Math.max(first, second) };
}

export interface ControlPreview { nodes: Node<PatchNodeData>[]; edges: Edge[] }

export function decorateControlPreview(nodes: Node<PatchNodeData>[], edges: Edge[], timeSeconds: number): ControlPreview {
  const byId = new Map(nodes.map((node) => [node.id, node]));
  const incoming = new Map(edges.filter((edge) => edge.data?.signal === 'control').map((edge) => [`${edge.target}.${edge.targetHandle}`, edge]));
  const memo = new Map<string, number>();
  const visiting = new Set<string>();
  const outputFor = (nodeId: string): number => {
    if (memo.has(nodeId)) return memo.get(nodeId)!;
    if (visiting.has(nodeId)) return 0;
    visiting.add(nodeId);
    const node = byId.get(nodeId);
    let value = 0;
    if (node?.data.type === 'lfo') {
      value = lfoValue(timeSeconds, parameter(node, 'frequency', 1), parameter(node, 'phase', 0), parameter(node, 'waveform', 0) >= 0.5 ? 'triangle' : 'sine');
    } else if (node?.data.type === 'control-map') {
      const source = incoming.get(`${nodeId}.in`);
      const input = source ? outputFor(source.source) : 0;
      value = mapControlValue(input, parameter(node, 'scale', 1), parameter(node, 'offset', 0), parameter(node, 'polarity', 1) >= 0.5 ? 'bipolar' : 'unipolar');
    }
    visiting.delete(nodeId);
    memo.set(nodeId, value);
    return value;
  };

  for (const node of nodes) if (node.data.role === 'control') outputFor(node.id);
  return {
    nodes: nodes.map((node) => node.data.role !== 'control' ? node : {
      ...node,
      data: {
        ...node.data,
        controlPreview: {
          value: memo.get(node.id) ?? 0,
          label: node.data.type === 'lfo' ? (parameter(node, 'waveform', 0) >= 0.5 ? 'TRI' : 'SINE') : 'MAP',
        },
      },
      className: `${node.className ?? ''} control-preview-node`.trim(),
    }),
    edges: edges.map((edge) => edge.data?.signal !== 'control' ? edge : {
      ...edge,
      className: `${edge.className ?? ''} control-live`.trim(),
      data: { ...edge.data, controlValue: outputFor(edge.source) },
    }),
  };
}
