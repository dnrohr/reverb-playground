import type { Connection, Edge, Node } from '@xyflow/react';
import type { GraphState, PatchNodeData, SignalType } from './graph';
import { createModuleNode, nextNodeId } from './modules';

export type ConnectionDecision =
  | { kind: 'valid'; signal: SignalType }
  | { kind: 'occupied'; signal: SignalType; existing: Edge }
  | { kind: 'invalid'; message: string };

function port(nodes: Node<PatchNodeData>[], nodeId: string | null, portId: string | null) {
  return nodes.find((node) => node.id === nodeId)?.data.ports.find((candidate) => candidate.id === portId);
}

export function decideConnection(nodes: Node<PatchNodeData>[], edges: Edge[], connection: Connection): ConnectionDecision {
  const source = port(nodes, connection.source, connection.sourceHandle);
  const target = port(nodes, connection.target, connection.targetHandle);
  if (!source || !target) return { kind: 'invalid', message: 'Connect an existing output port to an existing input port.' };
  if (source.direction !== 'output' || target.direction !== 'input') return { kind: 'invalid', message: 'Cables must run from an output to an input.' };
  if (source.signal !== target.signal) return { kind: 'invalid', message: `${source.signal} cannot connect to an ${target.signal} input.` };
  const existing = edges.find((edge) => edge.target === connection.target && edge.targetHandle === connection.targetHandle);
  return existing ? { kind: 'occupied', signal: source.signal, existing } : { kind: 'valid', signal: source.signal };
}

export function nextEdgeId(connection: Connection, edges: Edge[]): string {
  const stem = `${connection.source}-${connection.sourceHandle}-to-${connection.target}-${connection.targetHandle}`;
  const used = new Set(edges.map((edge) => edge.id));
  if (!used.has(stem)) return stem;
  let suffix = 2; while (used.has(`${stem}-${suffix}`)) suffix++;
  return `${stem}-${suffix}`;
}

export function createTypedEdge(connection: Connection, signal: SignalType, edges: Edge[]): Edge {
  return {
    id: nextEdgeId(connection, edges), source: connection.source!, sourceHandle: connection.sourceHandle,
    target: connection.target!, targetHandle: connection.targetHandle, type: 'smoothstep',
    className: `signal-edge signal-${signal}`, data: { signal }, interactionWidth: 24,
  };
}

export function connectGraph(state: GraphState, connection: Connection, replaceOccupied = false): GraphState {
  const decision = decideConnection(state.nodes, state.edges, connection);
  if (decision.kind === 'invalid') throw new Error(decision.message);
  if (decision.kind === 'occupied' && !replaceOccupied) throw new Error('Input is occupied. Replace its cable or insert +.');
  const edges = decision.kind === 'occupied' ? state.edges.filter((edge) => edge.id !== decision.existing.id) : state.edges;
  return { nodes: state.nodes, edges: [...edges, createTypedEdge(connection, decision.signal, edges)] };
}

export function insertSumForOccupiedInput(state: GraphState, connection: Connection): GraphState {
  const decision = decideConnection(state.nodes, state.edges, connection);
  if (decision.kind !== 'occupied' || decision.signal !== 'audio') throw new Error('Automatic + insertion requires an occupied audio input.');
  const target = state.nodes.find((node) => node.id === connection.target)!;
  const sumId = nextNodeId('sum', state.nodes);
  const sum = createModuleNode('sum', sumId, { x: target.position.x - 210, y: target.position.y + 110 });
  const remaining = state.edges.filter((edge) => edge.id !== decision.existing.id);
  const first: Connection = { source: decision.existing.source, sourceHandle: decision.existing.sourceHandle ?? null, target: sumId, targetHandle: 'in-a' };
  const second: Connection = { source: connection.source, sourceHandle: connection.sourceHandle, target: sumId, targetHandle: 'in-b' };
  const output: Connection = { source: sumId, sourceHandle: 'out', target: connection.target, targetHandle: connection.targetHandle };
  let edges = [...remaining, createTypedEdge(first, 'audio', remaining)];
  edges = [...edges, createTypedEdge(second, 'audio', edges)];
  edges = [...edges, createTypedEdge(output, 'audio', edges)];
  return { nodes: [...state.nodes, sum], edges };
}
