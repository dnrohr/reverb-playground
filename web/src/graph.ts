import type { Edge, Node } from '@xyflow/react';

export type SignalType = 'audio' | 'control';
export type PortDirection = 'input' | 'output';
export type NodeRole = 'io' | 'routing' | 'filter' | 'delay' | 'diffusion' | 'pitch' | 'tank' | 'tap' | 'control';

export interface PatchPort {
  id: string;
  signal: SignalType;
  direction: PortDirection;
}

export interface PatchParameter {
  id: string;
  value: number;
  unit: string;
  minimum: number;
  maximum: number;
  step: number;
  modulation?: {
    portId: string;
    amount: number;
    polarity: 'unipolar' | 'bipolar';
    clampMinimum: number;
    clampMaximum: number;
  };
}

export interface PatchNodeData extends Record<string, unknown> {
  label: string;
  type: string;
  ports: PatchPort[];
  parameters: PatchParameter[];
  role: NodeRole;
  runtimeBound: boolean;
  userName?: string;
  presentation?: 'gravity';
  controlPreview?: { value: number; label: string };
  presentationGroup?: { id: string; name: string; collapsed: boolean };
  groupMemberIds?: string[];
}

export interface CableLayout {
  waypoints?: Array<{ x: number; y: number }>;
  portal?: { name: string };
}

export interface RuntimeNode {
  id: string;
  type: string;
  label: string;
  role: NodeRole;
  ports: PatchPort[];
  parameters: PatchParameter[];
  position: { x: number; y: number };
  name?: string;
  presentation?: 'gravity';
}

export interface RuntimeConnection {
  id: string;
  source: string;
  sourcePort: string;
  target: string;
  targetPort: string;
  signal: SignalType;
}

export interface RuntimeSnapshot {
  contractVersion: 2;
  engineId: 'barr-reference';
  sampleRate: number;
  nodes: RuntimeNode[];
  connections: RuntimeConnection[];
  outsidePatch: { id: string; purpose: string }[];
  productVersion?: string;
  buildCommit?: string;
  restoredPatch?: unknown;
}

const nodeRoles = new Set<NodeRole>(['io', 'routing', 'filter', 'delay', 'diffusion', 'pitch', 'tank', 'tap']);
const nodeTypes = new Set(['stereo-input', 'stereo-output', 'sum', 'lowpass', 'allpass']);

function requireCondition(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(`Runtime snapshot mismatch: ${message}`);
}

export function parseRuntimeSnapshot(input: unknown): RuntimeSnapshot {
  requireCondition(typeof input === 'object' && input !== null, 'payload is not an object');
  const snapshot = input as Partial<RuntimeSnapshot>;
  requireCondition(snapshot.contractVersion === 2, 'unsupported contract version');
  requireCondition(snapshot.engineId === 'barr-reference', 'unexpected engine identity');
  requireCondition(typeof snapshot.sampleRate === 'number' && Number.isFinite(snapshot.sampleRate), 'invalid sample rate');
  requireCondition(Array.isArray(snapshot.nodes) && snapshot.nodes.length > 0, 'nodes are missing');
  requireCondition(Array.isArray(snapshot.connections), 'connections are missing');
  requireCondition(Array.isArray(snapshot.outsidePatch), 'outside-patch processing is missing');
  requireCondition(snapshot.productVersion === undefined || (typeof snapshot.productVersion === 'string' && snapshot.productVersion.length > 0), 'invalid product version');
  requireCondition(snapshot.buildCommit === undefined || (typeof snapshot.buildCommit === 'string' && /^[0-9A-Za-z._-]+$/.test(snapshot.buildCommit)), 'invalid build commit');
  requireCondition(snapshot.restoredPatch === undefined || (typeof snapshot.restoredPatch === 'object' && snapshot.restoredPatch !== null && !Array.isArray(snapshot.restoredPatch)), 'invalid restored patch');

  const nodeIds = new Set<string>();
  const portsByNode = new Map<string, Map<string, PatchPort>>();
  for (const node of snapshot.nodes) {
    requireCondition(typeof node.id === 'string' && !nodeIds.has(node.id), 'node IDs must be unique');
    nodeIds.add(node.id);
    requireCondition(nodeTypes.has(node.type), `unsupported node type ${node.type}`);
    requireCondition(nodeRoles.has(node.role), `unsupported role ${node.role}`);
    requireCondition(Array.isArray(node.ports) && Array.isArray(node.parameters), `invalid node ${node.id}`);
    requireCondition(Number.isFinite(node.position?.x) && Number.isFinite(node.position?.y), `invalid layout for ${node.id}`);
    const ports = new Map<string, PatchPort>();
    for (const port of node.ports) {
      requireCondition(typeof port.id === 'string' && !ports.has(port.id), `port IDs must be unique on ${node.id}`);
      requireCondition(port.signal === 'audio' || port.signal === 'control', `invalid signal on ${node.id}.${port.id}`);
      requireCondition(port.direction === 'input' || port.direction === 'output', `invalid direction on ${node.id}.${port.id}`);
      ports.set(port.id, port);
    }
    portsByNode.set(node.id, ports);
    for (const parameter of node.parameters) {
      requireCondition(typeof parameter.id === 'string' && Number.isFinite(parameter.value), `invalid parameter on ${node.id}`);
      requireCondition(typeof parameter.unit === 'string' && parameter.unit.length > 0, `missing unit on ${node.id}.${parameter.id}`);
      requireCondition(Number.isFinite(parameter.minimum) && Number.isFinite(parameter.maximum) && parameter.minimum < parameter.maximum, `invalid range on ${node.id}.${parameter.id}`);
      requireCondition(Number.isFinite(parameter.step) && parameter.step > 0, `invalid step on ${node.id}.${parameter.id}`);
      const modulation = parameter.modulation;
      requireCondition(typeof modulation === 'object' && modulation !== null, `missing modulation mapping on ${node.id}.${parameter.id}`);
      requireCondition(typeof modulation.portId === 'string', `invalid modulation socket on ${node.id}.${parameter.id}`);
      const socket = ports.get(modulation.portId);
      requireCondition(socket?.signal === 'control' && socket.direction === 'input', `modulation socket is not a control input on ${node.id}.${parameter.id}`);
      requireCondition(Number.isFinite(modulation.amount), `invalid modulation amount on ${node.id}.${parameter.id}`);
      requireCondition(modulation.polarity === 'unipolar' || modulation.polarity === 'bipolar', `invalid modulation polarity on ${node.id}.${parameter.id}`);
      requireCondition(Number.isFinite(modulation.clampMinimum) && Number.isFinite(modulation.clampMaximum) && modulation.clampMinimum < modulation.clampMaximum, `invalid modulation clamp on ${node.id}.${parameter.id}`);
    }
  }
  for (const connection of snapshot.connections) {
    requireCondition(nodeIds.has(connection.source) && nodeIds.has(connection.target), `connection ${connection.id} references an unknown node`);
    requireCondition(connection.signal === 'audio' || connection.signal === 'control', `connection ${connection.id} has an invalid signal`);
    const sourcePort = portsByNode.get(connection.source)?.get(connection.sourcePort);
    const targetPort = portsByNode.get(connection.target)?.get(connection.targetPort);
    requireCondition(sourcePort?.direction === 'output' && sourcePort.signal === connection.signal, `connection ${connection.id} has an invalid source port`);
    requireCondition(targetPort?.direction === 'input' && targetPort.signal === connection.signal, `connection ${connection.id} has an invalid target port`);
  }
  return snapshot as RuntimeSnapshot;
}

export function createFlowModel(snapshot: RuntimeSnapshot): { nodes: Node<PatchNodeData>[]; edges: Edge[] } {
  return {
    nodes: snapshot.nodes.map((node) => ({
      id: node.id,
      type: 'patchNode',
      position: { ...node.position },
      data: {
        label: node.label,
        type: node.type,
        role: node.role,
        ports: node.ports.map((port) => ({ ...port })),
        parameters: node.parameters.map((parameter) => ({ ...parameter })),
        runtimeBound: true,
        ...(node.name ? { userName: node.name } : {}),
        ...(node.presentation ? { presentation: node.presentation } : {}),
      },
    })),
    edges: snapshot.connections.map((connection) => ({
      id: connection.id,
      source: connection.source,
      sourceHandle: connection.sourcePort,
      target: connection.target,
      targetHandle: connection.targetPort,
      type: 'smoothstep',
      className: `signal-edge signal-${connection.signal}`,
      data: { signal: connection.signal },
      interactionWidth: 24,
    })),
  };
}

export interface GraphState { nodes: Node<PatchNodeData>[]; edges: Edge[] }

export function cloneGraph(state: GraphState): GraphState {
  return structuredClone(state);
}

export function requiredIoError(nodes: Node<PatchNodeData>[]): string | null {
  const inputs = nodes.filter((node) => node.data.type === 'stereo-input').length;
  const outputs = nodes.filter((node) => node.data.type === 'stereo-output').length;
  if (inputs !== 1) return `Patch requires exactly one Stereo Input (found ${inputs}).`;
  if (outputs !== 1) return `Patch requires exactly one Stereo Output (found ${outputs}).`;
  return null;
}

export function deleteSelected(nodes: Node<PatchNodeData>[], edges: Edge[]): { nodes: Node<PatchNodeData>[]; edges: Edge[] } {
  const removedNodes = new Set(nodes.filter((node) => node.selected).map((node) => node.id));
  const removedEdges = new Set(edges.filter((edge) => edge.selected).map((edge) => edge.id));
  return {
    nodes: nodes.filter((node) => !removedNodes.has(node.id)),
    edges: edges.filter((edge) => !removedEdges.has(edge.id) && !removedNodes.has(edge.source) && !removedNodes.has(edge.target)),
  };
}
