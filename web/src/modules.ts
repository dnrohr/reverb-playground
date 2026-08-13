import type { Node, XYPosition } from '@xyflow/react';
import type { NodeRole, PatchNodeData, PatchParameter, PatchPort } from './graph';

export type ModuleType = 'stereo-input' | 'stereo-output' | 'gain' | 'sum' | 'delay' | 'allpass' | 'lowpass' | 'macro' | 'lfo' | 'control-map' | 'envelope-follower' | 'hold-gate';
export interface ModuleDefinition { type: ModuleType; label: string; role: NodeRole; ports: PatchPort[]; parameters: PatchParameter[] }

const audioIn = (id = 'in'): PatchPort => ({ id, signal: 'audio', direction: 'input' });
const audioOut = (id = 'out'): PatchPort => ({ id, signal: 'audio', direction: 'output' });
const controlIn = (id: string): PatchPort => ({ id, signal: 'control', direction: 'input' });
const controlOut = (id = 'out'): PatchPort => ({ id, signal: 'control', direction: 'output' });
const parameter = (id: string, value: number, unit: string, minimum: number, maximum: number, step: number, amount: number): PatchParameter => ({
  id, value, unit, minimum, maximum, step,
  modulation: { portId: `${id}-mod`, amount, polarity: 'bipolar', clampMinimum: minimum, clampMaximum: maximum },
});
const controlParameter = (id: string, value: number, unit: string, minimum: number, maximum: number, step: number, amount: number): PatchParameter => ({
  ...parameter(id, value, unit, minimum, maximum, step, amount),
  modulation: { portId: `${id}-mod`, amount, polarity: 'bipolar', clampMinimum: minimum, clampMaximum: maximum },
});
const staticParameter = (id: string, value: number, unit: string, minimum: number, maximum: number, step: number): PatchParameter => ({
  id, value, unit, minimum, maximum, step,
});

export const moduleDefinitions: ModuleDefinition[] = [
  { type: 'stereo-input', label: 'Stereo Input', role: 'io', ports: [audioOut('out-l'), audioOut('out-r')], parameters: [] },
  { type: 'stereo-output', label: 'Stereo Output', role: 'io', ports: [audioIn('in-l'), audioIn('in-r')], parameters: [] },
  { type: 'gain', label: 'Gain / Invert', role: 'routing', ports: [audioIn(), controlIn('gain-mod'), audioOut()], parameters: [parameter('gain', 1, 'linear', -1, 1, 0.001, 0.5)] },
  { type: 'sum', label: 'Sum (+)', role: 'routing', ports: [audioIn('in-a'), audioIn('in-b'), audioOut()], parameters: [] },
  { type: 'delay', label: 'Delay', role: 'delay', ports: [audioIn(), controlIn('delay-mod'), audioOut()], parameters: [parameter('delay', 10, 'milliseconds', 0.1, 10000, 0.01, 10)] },
  { type: 'allpass', label: 'Allpass', role: 'diffusion', ports: [audioIn(), controlIn('delay-mod'), controlIn('coefficient-mod'), audioOut()], parameters: [parameter('delay', 10, 'milliseconds', 0.1, 100, 0.01, 2), parameter('coefficient', 0.5, 'unitless', -0.95, 0.95, 0.001, 0.25)] },
  { type: 'lowpass', label: 'Low-pass', role: 'filter', ports: [audioIn(), controlIn('cutoff-mod'), audioOut()], parameters: [parameter('cutoff', 7000, 'hertz', 20, 20000, 1, 5000)] },
  { type: 'macro', label: 'Macro', role: 'control', ports: [controlOut()], parameters: [staticParameter('value', 0, 'normalized', -1, 1, 0.001), staticParameter('default-value', 0, 'normalized', -1, 1, 0.001), staticParameter('center-detent', 1, 'boolean', 0, 1, 1)] },
  { type: 'lfo', label: 'LFO', role: 'control', ports: [controlIn('frequency-mod'), controlIn('phase-mod'), controlIn('waveform-mod'), controlIn('run-mode-mod'), controlOut()], parameters: [controlParameter('frequency', 1, 'hertz', 0.01, 100, 0.01, 1), controlParameter('phase', 0, 'cycles', 0, 0.999, 0.001, 0.25), controlParameter('waveform', 0, 'waveform', 0, 1, 1, 1), controlParameter('run-mode', 0, 'run-mode', 0, 1, 1, 1)] },
  { type: 'control-map', label: 'Curve Mapper', role: 'control', ports: [controlIn('in'), controlIn('scale-mod'), controlIn('offset-mod'), controlIn('polarity-mod'), controlOut()], parameters: [controlParameter('scale', 1, 'linear', -4, 4, 0.01, 1), controlParameter('offset', 0, 'unitless', -1, 1, 0.01, 0.5), controlParameter('polarity', 1, 'polarity', 0, 1, 1, 1), staticParameter('curve-family', 0, 'curve-family', 0, 2, 1), staticParameter('curve-amount', 0, 'unitless', -8, 8, 0.01), staticParameter('exponent', 1, 'unitless', 0.1, 8, 0.01), staticParameter('clamp-min', -1, 'unitless', -1, 1, 0.01), staticParameter('clamp-max', 1, 'unitless', -1, 1, 0.01)] },
  { type: 'envelope-follower', label: 'Envelope Follower', role: 'control', ports: [audioIn(), controlOut()], parameters: [staticParameter('attack', 5, 'milliseconds', 0.1, 500, 0.1), staticParameter('release', 100, 'milliseconds', 1, 5000, 1)] },
  { type: 'hold-gate', label: 'Hold Gate', role: 'routing', ports: [audioIn(), controlIn('gate'), audioOut()], parameters: [staticParameter('threshold', 0.5, 'unitless', 0, 1, 0.01), staticParameter('attack', 2, 'milliseconds', 0.1, 100, 0.1), staticParameter('hold', 250, 'milliseconds', 1, 2000, 1), staticParameter('release', 20, 'milliseconds', 0.1, 1000, 0.1)] },
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
  return { id, type: 'patchNode', position: { ...position }, data: { label: definition.label, type, role: definition.role, ports: structuredClone(definition.ports), parameters: structuredClone(definition.parameters), runtimeBound: false, ...(type === 'macro' ? { userName: 'Macro' } : {}) } };
}
