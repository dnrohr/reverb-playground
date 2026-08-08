import type { Edge, Node } from '@xyflow/react';

export type SignalType = 'audio' | 'control';
export type PortDirection = 'input' | 'output';

export interface PatchPort {
  id: string;
  signal: SignalType;
  direction: PortDirection;
}

export interface PatchParameter {
  id: string;
  value: number;
  unit: string;
}

export interface PatchNodeData extends Record<string, unknown> {
  label: string;
  type: string;
  ports: PatchPort[];
  parameters: PatchParameter[];
  role: 'io' | 'routing' | 'filter' | 'diffusion' | 'tank' | 'tap';
}

export interface PatchNode {
  id: string;
  data: PatchNodeData;
  position: { x: number; y: number };
}

export interface PatchConnection {
  id: string;
  source: string;
  sourceHandle: string;
  target: string;
  targetHandle: string;
  signal: SignalType;
}

const audioInput = (id: string): PatchPort => ({ id, signal: 'audio', direction: 'input' });
const audioOutput = (id: string): PatchPort => ({ id, signal: 'audio', direction: 'output' });
const milliseconds = (value: number): PatchParameter => ({ id: 'delay', value, unit: 'milliseconds' });
const coefficient = (value: number): PatchParameter => ({ id: 'coefficient', value, unit: 'unitless' });

export const referenceNodes: PatchNode[] = [
  { id: 'input', position: { x: 0, y: 80 }, data: { label: 'Stereo Input', type: 'stereo-input', role: 'io', ports: [audioOutput('out-l'), audioOutput('out-r')], parameters: [] } },
  { id: 'sum', position: { x: 180, y: 80 }, data: { label: 'Mono Sum', type: 'sum', role: 'routing', ports: [audioInput('in-l'), audioInput('in-r'), audioOutput('out')], parameters: [] } },
  { id: 'input-filter', position: { x: 360, y: 80 }, data: { label: 'Input Low-pass', type: 'lowpass', role: 'filter', ports: [audioInput('in'), audioOutput('out')], parameters: [{ id: 'cutoff', value: 7000, unit: 'hertz' }] } },
  { id: 'diffuser-1', position: { x: 540, y: 80 }, data: { label: 'Diffuser 1', type: 'allpass', role: 'diffusion', ports: [audioInput('in'), audioOutput('out')], parameters: [milliseconds(4.31), coefficient(0.5)] } },
  { id: 'diffuser-2', position: { x: 720, y: 80 }, data: { label: 'Diffuser 2', type: 'allpass', role: 'diffusion', ports: [audioInput('in'), audioOutput('out')], parameters: [milliseconds(7.13), coefficient(0.5)] } },
  { id: 'tank-1', position: { x: 720, y: 260 }, data: { label: 'Tank 1', type: 'allpass', role: 'tank', ports: [audioInput('in'), audioOutput('out')], parameters: [milliseconds(13.73), coefficient(0.5)] } },
  { id: 'tank-2', position: { x: 540, y: 260 }, data: { label: 'Tank 2', type: 'allpass', role: 'tank', ports: [audioInput('in'), audioOutput('out')], parameters: [milliseconds(19.91), coefficient(-0.5)] } },
  { id: 'left-tap', position: { x: 360, y: 210 }, data: { label: 'Left Tap', type: 'allpass', role: 'tap', ports: [audioInput('in'), audioOutput('out')], parameters: [milliseconds(29.71), coefficient(0.5)] } },
  { id: 'right-tap', position: { x: 360, y: 330 }, data: { label: 'Right Tap', type: 'allpass', role: 'tap', ports: [audioInput('in'), audioOutput('out')], parameters: [milliseconds(37.11), coefficient(0.5)] } },
  { id: 'output', position: { x: 180, y: 260 }, data: { label: 'Stereo Output', type: 'stereo-output', role: 'io', ports: [audioInput('in-l'), audioInput('in-r')], parameters: [] } },
];

export const referenceConnections: PatchConnection[] = [
  { id: 'input-l-to-sum', source: 'input', sourceHandle: 'out-l', target: 'sum', targetHandle: 'in-l', signal: 'audio' },
  { id: 'input-r-to-sum', source: 'input', sourceHandle: 'out-r', target: 'sum', targetHandle: 'in-r', signal: 'audio' },
  { id: 'sum-to-filter', source: 'sum', sourceHandle: 'out', target: 'input-filter', targetHandle: 'in', signal: 'audio' },
  { id: 'filter-to-diffuser-1', source: 'input-filter', sourceHandle: 'out', target: 'diffuser-1', targetHandle: 'in', signal: 'audio' },
  { id: 'diffuser-1-to-diffuser-2', source: 'diffuser-1', sourceHandle: 'out', target: 'diffuser-2', targetHandle: 'in', signal: 'audio' },
  { id: 'diffuser-2-to-tank-1', source: 'diffuser-2', sourceHandle: 'out', target: 'tank-1', targetHandle: 'in', signal: 'audio' },
  { id: 'tank-1-to-tank-2', source: 'tank-1', sourceHandle: 'out', target: 'tank-2', targetHandle: 'in', signal: 'audio' },
  { id: 'tank-to-left', source: 'tank-2', sourceHandle: 'out', target: 'left-tap', targetHandle: 'in', signal: 'audio' },
  { id: 'tank-to-right', source: 'tank-2', sourceHandle: 'out', target: 'right-tap', targetHandle: 'in', signal: 'audio' },
  { id: 'left-to-output', source: 'left-tap', sourceHandle: 'out', target: 'output', targetHandle: 'in-l', signal: 'audio' },
  { id: 'right-to-output', source: 'right-tap', sourceHandle: 'out', target: 'output', targetHandle: 'in-r', signal: 'audio' },
];

export function createFlowModel(): { nodes: Node<PatchNodeData>[]; edges: Edge[] } {
  return {
    nodes: referenceNodes.map((node) => ({ ...node, type: 'patchNode', data: { ...node.data, ports: [...node.data.ports], parameters: [...node.data.parameters] } })),
    edges: referenceConnections.map((connection) => ({
      id: connection.id,
      source: connection.source,
      sourceHandle: connection.sourceHandle,
      target: connection.target,
      targetHandle: connection.targetHandle,
      type: 'smoothstep',
      className: `signal-edge signal-${connection.signal}`,
      data: { signal: connection.signal },
    })),
  };
}

export function deleteSelected(nodes: Node<PatchNodeData>[], edges: Edge[]): { nodes: Node<PatchNodeData>[]; edges: Edge[] } {
  const removedNodes = new Set(nodes.filter((node) => node.selected).map((node) => node.id));
  const removedEdges = new Set(edges.filter((edge) => edge.selected).map((edge) => edge.id));
  return {
    nodes: nodes.filter((node) => !removedNodes.has(node.id)),
    edges: edges.filter((edge) => !removedEdges.has(edge.id) && !removedNodes.has(edge.source) && !removedNodes.has(edge.target)),
  };
}
