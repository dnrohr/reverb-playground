import type { Edge, Node } from '@xyflow/react';

export type SignalType = 'audio' | 'control';
export type PortDirection = 'input' | 'output';
export type NodeRole = 'io' | 'routing' | 'filter' | 'diffusion' | 'tank' | 'tap';

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
  role: NodeRole;
}

export interface RuntimeNode {
  id: string;
  type: string;
  label: string;
  role: NodeRole;
  ports: PatchPort[];
  parameters: PatchParameter[];
  position: { x: number; y: number };
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
  contractVersion: 1;
  engineId: 'barr-reference';
  sampleRate: number;
  nodes: RuntimeNode[];
  connections: RuntimeConnection[];
  outsidePatch: { id: string; purpose: string }[];
}

const nodeRoles = new Set<NodeRole>(['io', 'routing', 'filter', 'diffusion', 'tank', 'tap']);
const nodeTypes = new Set(['stereo-input', 'stereo-output', 'sum', 'lowpass', 'allpass']);

function requireCondition(condition: unknown, message: string): asserts condition {
  if (!condition) throw new Error(`Runtime snapshot mismatch: ${message}`);
}

export function parseRuntimeSnapshot(input: unknown): RuntimeSnapshot {
  requireCondition(typeof input === 'object' && input !== null, 'payload is not an object');
  const snapshot = input as Partial<RuntimeSnapshot>;
  requireCondition(snapshot.contractVersion === 1, 'unsupported contract version');
  requireCondition(snapshot.engineId === 'barr-reference', 'unexpected engine identity');
  requireCondition(typeof snapshot.sampleRate === 'number' && Number.isFinite(snapshot.sampleRate), 'invalid sample rate');
  requireCondition(Array.isArray(snapshot.nodes) && snapshot.nodes.length > 0, 'nodes are missing');
  requireCondition(Array.isArray(snapshot.connections), 'connections are missing');
  requireCondition(Array.isArray(snapshot.outsidePatch), 'outside-patch processing is missing');

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
