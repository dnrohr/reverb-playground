import type { Node, XYPosition } from '@xyflow/react';
import type { NodeRole, PatchNodeData, PatchParameter, PatchPort } from './graph';

export type ModuleType = 'stereo-input' | 'stereo-output' | 'gain' | 'sum' | 'delay' | 'allpass' | 'lowpass';
export interface ModuleDefinition { type: ModuleType; label: string; role: NodeRole; ports: PatchPort[]; parameters: PatchParameter[] }

const audioIn = (id = 'in'): PatchPort => ({ id, signal: 'audio', direction: 'input' });
const audioOut = (id = 'out'): PatchPort => ({ id, signal: 'audio', direction: 'output' });
const parameter = (id: string, value: number, unit: string, minimum: number, maximum: number, step: number): PatchParameter => ({ id, value, unit, minimum, maximum, step });

export const moduleDefinitions: ModuleDefinition[] = [
  { type: 'stereo-input', label: 'Stereo Input', role: 'io', ports: [audioOut('out-l'), audioOut('out-r')], parameters: [] },
  { type: 'stereo-output', label: 'Stereo Output', role: 'io', ports: [audioIn('in-l'), audioIn('in-r')], parameters: [] },
  { type: 'gain', label: 'Gain / Invert', role: 'routing', ports: [audioIn(), audioOut()], parameters: [parameter('gain', 1, 'linear', -1, 1, 0.001)] },
  { type: 'sum', label: 'Sum (+)', role: 'routing', ports: [audioIn('in-a'), audioIn('in-b'), audioOut()], parameters: [] },
  { type: 'delay', label: 'Delay', role: 'delay', ports: [audioIn(), audioOut()], parameters: [parameter('delay', 10, 'milliseconds', 0.1, 10000, 0.01)] },
  { type: 'allpass', label: 'Allpass', role: 'diffusion', ports: [audioIn(), audioOut()], parameters: [parameter('delay', 10, 'milliseconds', 0.1, 100, 0.01), parameter('coefficient', 0.5, 'unitless', -0.95, 0.95, 0.001)] },
  { type: 'lowpass', label: 'Low-pass', role: 'filter', ports: [audioIn(), audioOut()], parameters: [parameter('cutoff', 7000, 'hertz', 20, 20000, 1)] },
];

export const moduleByType = new Map(moduleDefinitions.map((definition) => [definition.type, definition]));

export function nextNodeId(type: ModuleType, nodes: Node<PatchNodeData>[]): string {
  const used = new Set(nodes.map((node) => node.id));
  let suffix = 1;
  while (used.has(`${type}-${suffix}`)) suffix++;
  return `${type}-${suffix}`;
}

export function createModuleNode(type: ModuleType, id: string, position: XYPosition): Node<PatchNodeData> {
  const definition = moduleByType.get(type);
  if (!definition) throw new Error(`Unsupported module type '${type}'`);
  return { id, type: 'patchNode', position: { ...position }, data: { label: definition.label, type, role: definition.role, ports: structuredClone(definition.ports), parameters: structuredClone(definition.parameters), runtimeBound: false } };
}
