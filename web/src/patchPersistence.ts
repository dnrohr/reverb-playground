import type { Edge, Node, Viewport } from '@xyflow/react';
import { createFlowModel, type PatchNodeData, type RuntimeSnapshot } from './graph';

export const patchSchemaVersion = 1 as const;
export const patchEngineVersion = '0.1';

interface SavedPatch {
  schemaVersion: 1;
  engineVersion: string;
  semantic: {
    nodes: Array<{
      id: string;
      type: string;
      ports: PatchNodeData['ports'];
      parameters: Array<{ id: string; value: number; unit: string }>;
    }>;
    connections: Array<{
      id: string;
      from: { nodeId: string; portId: string };
      to: { nodeId: string; portId: string };
    }>;
  };
  layout: {
    nodes: Array<{ nodeId: string; x: number; y: number }>;
    viewport: Viewport;
  };
}

export interface LoadedPatch {
  nodes: Node<PatchNodeData>[];
  edges: Edge[];
  viewport: Viewport;
  source: SavedPatch;
}

function fail(message: string): never {
  throw new Error(`Patch load rejected: ${message}`);
}

function object(value: unknown, path: string): Record<string, unknown> {
  if (typeof value !== 'object' || value === null || Array.isArray(value)) fail(`${path} must be an object`);
  return value as Record<string, unknown>;
}

function exactKeys(value: Record<string, unknown>, keys: string[], path: string) {
  const expected = new Set(keys);
  const unknown = Object.keys(value).filter((key) => !expected.has(key));
  if (unknown.length) fail(`${path} contains unknown field '${unknown[0]}' (schema v1 rejects future fields)`);
  for (const key of keys) if (!(key in value)) fail(`${path} is missing '${key}'`);
}

function finite(value: unknown, path: string): number {
  if (typeof value !== 'number' || !Number.isFinite(value)) fail(`${path} must be a finite number`);
  return value;
}

export function createSavedPatch(
  nodes: Node<PatchNodeData>[], edges: Edge[], viewport: Viewport,
): SavedPatch {
  return {
    schemaVersion: patchSchemaVersion,
    engineVersion: patchEngineVersion,
    semantic: {
      nodes: nodes.map((node) => ({
        id: node.id,
        type: node.data.type,
        ports: node.data.ports.map((port) => ({ ...port })),
        parameters: node.data.parameters.map(({ id, value, unit }) => ({ id, value, unit })),
      })),
      connections: edges.map((edge) => ({
        id: edge.id,
        from: { nodeId: edge.source, portId: String(edge.sourceHandle ?? '') },
        to: { nodeId: edge.target, portId: String(edge.targetHandle ?? '') },
      })),
    },
    layout: {
      nodes: nodes.map((node) => ({ nodeId: node.id, x: node.position.x, y: node.position.y })),
      viewport: { ...viewport },
    },
  };
}

export function writePatchJson(nodes: Node<PatchNodeData>[], edges: Edge[], viewport: Viewport): string {
  return `${JSON.stringify(createSavedPatch(nodes, edges, viewport), null, 2)}\n`;
}

export function parsePatchJson(text: string, reference: RuntimeSnapshot): LoadedPatch {
  let raw: unknown;
  try { raw = JSON.parse(text); } catch (reason) {
    fail(`invalid JSON (${reason instanceof Error ? reason.message : 'parse failure'})`);
  }
  const root = object(raw, 'document');
  exactKeys(root, ['schemaVersion', 'engineVersion', 'semantic', 'layout'], 'document');
  if (root.schemaVersion !== patchSchemaVersion) fail(`unsupported schemaVersion '${String(root.schemaVersion)}'`);
  if (root.engineVersion !== patchEngineVersion) fail(`unsupported engineVersion '${String(root.engineVersion)}'`);

  const semantic = object(root.semantic, 'semantic');
  exactKeys(semantic, ['nodes', 'connections'], 'semantic');
  if (!Array.isArray(semantic.nodes) || !Array.isArray(semantic.connections)) fail('semantic nodes/connections must be arrays');
  if (semantic.nodes.length !== reference.nodes.length) fail(`expected ${reference.nodes.length} reference nodes`);
  if (semantic.connections.length !== reference.connections.length) fail(`expected ${reference.connections.length} reference connections`);

  const savedNodes = new Map<string, { parameters: Array<{ id: string; value: number; unit: string }> }>();
  for (const [index, unknownNode] of semantic.nodes.entries()) {
    const node = object(unknownNode, `semantic.nodes[${index}]`);
    exactKeys(node, ['id', 'type', 'ports', 'parameters'], `semantic.nodes[${index}]`);
    if (typeof node.id !== 'string' || savedNodes.has(node.id)) fail(`node identity at index ${index} is invalid or duplicated`);
    const expected = reference.nodes.find((candidate) => candidate.id === node.id);
    if (!expected || node.type !== expected.type) fail(`node '${node.id}' does not match the Barr reference`);
    if (!Array.isArray(node.ports) || node.ports.length !== expected.ports.length) fail(`ports differ for node '${node.id}'`);
    node.ports.forEach((unknownPort, portIndex) => {
      const port = object(unknownPort, `node '${node.id}' port`);
      exactKeys(port, ['id', 'signal', 'direction'], `node '${node.id}' port`);
      const expectedPort = expected.ports[portIndex];
      if (port.id !== expectedPort?.id || port.signal !== expectedPort.signal || port.direction !== expectedPort.direction)
        fail(`port ${portIndex} differs for node '${node.id}'`);
    });
    if (!Array.isArray(node.parameters) || node.parameters.length !== expected.parameters.length)
      fail(`parameters differ for node '${node.id}'`);
    const parameters = node.parameters.map((unknownParameter, parameterIndex) => {
      const parameter = object(unknownParameter, `node '${node.id}' parameter`);
      exactKeys(parameter, ['id', 'value', 'unit'], `node '${node.id}' parameter`);
      const expectedParameter = expected.parameters[parameterIndex];
      const value = finite(parameter.value, `${node.id}.${String(parameter.id)}`);
      if (parameter.id !== expectedParameter?.id || parameter.unit !== expectedParameter.unit)
        fail(`parameter ${parameterIndex} differs for node '${node.id}'`);
      if (value < expectedParameter.minimum || value > expectedParameter.maximum)
        fail(`${node.id}.${expectedParameter.id} is outside ${expectedParameter.minimum}..${expectedParameter.maximum}`);
      return { id: expectedParameter.id, value, unit: expectedParameter.unit };
    });
    savedNodes.set(node.id, { parameters });
  }

  for (const [index, unknownConnection] of semantic.connections.entries()) {
    const connection = object(unknownConnection, `semantic.connections[${index}]`);
    exactKeys(connection, ['id', 'from', 'to'], `semantic.connections[${index}]`);
    const expected = reference.connections[index];
    const from = object(connection.from, `connection '${String(connection.id)}'.from`);
    const to = object(connection.to, `connection '${String(connection.id)}'.to`);
    exactKeys(from, ['nodeId', 'portId'], `connection '${String(connection.id)}'.from`);
    exactKeys(to, ['nodeId', 'portId'], `connection '${String(connection.id)}'.to`);
    if (connection.id !== expected?.id || from.nodeId !== expected.source || from.portId !== expected.sourcePort
      || to.nodeId !== expected.target || to.portId !== expected.targetPort)
      fail(`connection at index ${index} differs from the Barr reference`);
  }

  const layout = object(root.layout, 'layout');
  exactKeys(layout, ['nodes', 'viewport'], 'layout');
  if (!Array.isArray(layout.nodes) || layout.nodes.length !== reference.nodes.length) fail('layout must position every reference node exactly once');
  const positions = new Map<string, { x: number; y: number }>();
  layout.nodes.forEach((unknownPosition, index) => {
    const position = object(unknownPosition, `layout.nodes[${index}]`);
    exactKeys(position, ['nodeId', 'x', 'y'], `layout.nodes[${index}]`);
    if (typeof position.nodeId !== 'string' || positions.has(position.nodeId)
      || !reference.nodes.some((node) => node.id === position.nodeId)) fail(`layout node identity at index ${index} is invalid or duplicated`);
    positions.set(position.nodeId, { x: finite(position.x, `${position.nodeId}.x`), y: finite(position.y, `${position.nodeId}.y`) });
  });
  const rawViewport = object(layout.viewport, 'layout.viewport');
  exactKeys(rawViewport, ['x', 'y', 'zoom'], 'layout.viewport');
  const viewport = { x: finite(rawViewport.x, 'viewport.x'), y: finite(rawViewport.y, 'viewport.y'), zoom: finite(rawViewport.zoom, 'viewport.zoom') };
  if (viewport.zoom <= 0) fail('viewport.zoom must be positive');

  const flow = createFlowModel(reference);
  const nodes = flow.nodes.map((node) => ({
    ...node,
    position: { ...positions.get(node.id)! },
    data: {
      ...node.data,
      parameters: node.data.parameters.map((parameter) => {
        const saved = savedNodes.get(node.id)?.parameters.find((candidate) => candidate.id === parameter.id);
        return { ...parameter, value: saved?.value ?? parameter.value };
      }),
    },
  }));
  return { nodes, edges: flow.edges, viewport, source: raw as SavedPatch };
}
