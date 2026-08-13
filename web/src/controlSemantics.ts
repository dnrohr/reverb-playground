import type { Edge, Node } from '@xyflow/react';
import type { PatchNodeData } from './graph';

const clamp = (value: number, minimum: number, maximum: number) => Math.min(maximum, Math.max(minimum, value));
const parameter = (node: Node<PatchNodeData>, id: string, fallback: number) => node.data.parameters.find((item) => item.id === id)?.value ?? fallback;

export type ControlPolarity = 'unipolar' | 'bipolar';
export type ControlCurveFamily = 'linear' | 'power' | 'exponential';

export function lfoValue(timeSeconds: number, frequency: number, phaseCycles: number, waveform: 'sine' | 'triangle'): number {
  const phase = ((timeSeconds * clamp(frequency, 0, 500) + phaseCycles) % 1 + 1) % 1;
  return waveform === 'sine' ? Math.sin(phase * Math.PI * 2) : 1 - 4 * Math.abs(phase - 0.5);
}

export function mapControlValue(input: number, scale: number, offset: number, polarity: ControlPolarity): number {
  const normalized = clamp(Number.isFinite(input) ? input : 0, polarity === 'bipolar' ? -1 : 0, 1);
  return clamp(normalized * scale + offset, -1, 1);
}

export function curveMapControlValue(
  input: number, family: ControlCurveFamily, curveAmount: number, exponent: number,
  scale: number, offset: number, polarity: ControlPolarity, clampMinimum: number, clampMaximum: number,
): number {
  const normalized = clamp(Number.isFinite(input) ? input : 0, polarity === 'bipolar' ? -1 : 0, 1);
  let shaped = normalized;
  if (family === 'power') {
    const safeExponent = clamp(Number.isFinite(exponent) ? exponent : 1, 0.1, 8);
    shaped = polarity === 'bipolar' ? Math.sign(normalized) * Math.abs(normalized) ** safeExponent : normalized ** safeExponent;
  } else if (family === 'exponential') {
    const amount = clamp(Number.isFinite(curveAmount) ? curveAmount : 0, -8, 8);
    const unitInput = polarity === 'bipolar' ? (normalized + 1) / 2 : normalized;
    const unitOutput = Math.abs(amount) < 1e-9 ? unitInput : Math.expm1(amount * unitInput) / Math.expm1(amount);
    shaped = polarity === 'bipolar' ? unitOutput * 2 - 1 : unitOutput;
  }
  const minimum = Number.isFinite(clampMinimum) ? clampMinimum : -1;
  const maximum = Number.isFinite(clampMaximum) ? clampMaximum : 1;
  if (minimum >= maximum) return clamp(Number.isFinite(offset) ? offset : 0, -1, 1);
  return clamp(shaped * (Number.isFinite(scale) ? scale : 0) + (Number.isFinite(offset) ? offset : 0), minimum, maximum);
}

export function mappingRange(scale: number, offset: number, polarity: ControlPolarity): { minimum: number; maximum: number } {
  const first = mapControlValue(polarity === 'bipolar' ? -1 : 0, scale, offset, polarity);
  const second = mapControlValue(1, scale, offset, polarity);
  return { minimum: Math.min(first, second), maximum: Math.max(first, second) };
}

export function curveMappingRange(
  family: ControlCurveFamily, curveAmount: number, exponent: number, scale: number,
  offset: number, polarity: ControlPolarity, clampMinimum: number, clampMaximum: number,
): { minimum: number; maximum: number } {
  const lower = polarity === 'bipolar' ? -1 : 0;
  const first = curveMapControlValue(lower, family, curveAmount, exponent, scale, offset, polarity, clampMinimum, clampMaximum);
  const second = curveMapControlValue(1, family, curveAmount, exponent, scale, offset, polarity, clampMinimum, clampMaximum);
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
    if (node?.data.type === 'macro') {
      value = clamp(parameter(node, 'value', 0), -1, 1);
    } else if (node?.data.type === 'lfo') {
      value = lfoValue(timeSeconds, parameter(node, 'frequency', 1), parameter(node, 'phase', 0), parameter(node, 'waveform', 0) >= 0.5 ? 'triangle' : 'sine');
    } else if (node?.data.type === 'control-map') {
      const source = incoming.get(`${nodeId}.in`);
      const input = source ? outputFor(source.source) : 0;
      const familyValue = parameter(node, 'curve-family', 0);
      const family: ControlCurveFamily = familyValue >= 1.5 ? 'exponential' : familyValue >= 0.5 ? 'power' : 'linear';
      value = curveMapControlValue(
        input, family, parameter(node, 'curve-amount', 0), parameter(node, 'exponent', 1),
        parameter(node, 'scale', 1), parameter(node, 'offset', 0),
        parameter(node, 'polarity', 1) >= 0.5 ? 'bipolar' : 'unipolar',
        parameter(node, 'clamp-min', -1), parameter(node, 'clamp-max', 1),
      );
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
          label: node.data.type === 'macro' ? 'MACRO'
            : node.data.type === 'lfo' ? (parameter(node, 'waveform', 0) >= 0.5 ? 'TRI' : 'SINE')
            : parameter(node, 'curve-family', 0) >= 1.5 ? 'EXP' : parameter(node, 'curve-family', 0) >= 0.5 ? 'POWER' : 'LINEAR',
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
