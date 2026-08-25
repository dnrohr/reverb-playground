import type { Edge, Node, Viewport } from '@xyflow/react';
import { requiredIoError, type PatchNodeData, type RuntimeSnapshot } from './graph';
import { createModuleNode, moduleByType, type ModuleType } from './modules';

export const patchSchemaVersion = 2 as const;
export const patchEngineVersion = '0.1';
interface SavedParameter { id: string; value: number; unit: string; modulation?: NonNullable<PatchNodeData['parameters'][number]['modulation']> }
interface SavedPatch { schemaVersion: 2; engineVersion: string; semantic: { nodes: Array<{ id: string; type: string; name?: string; presentation?: 'gravity'; ports: PatchNodeData['ports']; parameters: SavedParameter[] }>; connections: Array<{ id: string; from: { nodeId: string; portId: string }; to: { nodeId: string; portId: string } }> }; layout: { nodes: Array<{ nodeId: string; x: number; y: number }>; viewport: Viewport } }
export interface LoadedPatch { nodes: Node<PatchNodeData>[]; edges: Edge[]; viewport: Viewport; source: SavedPatch; warnings: string[] }
function fail(message: string): never { throw new Error(`Patch load rejected: ${message}`); }
function object(value: unknown, path: string): Record<string, unknown> { if (typeof value !== 'object' || value === null || Array.isArray(value)) fail(`${path} must be an object`); return value as Record<string, unknown>; }
function exactKeys(value: Record<string, unknown>, keys: string[], path: string, optional: string[] = []) { const expected = new Set([...keys, ...optional]); const unknown = Object.keys(value).find((key) => !expected.has(key)); if (unknown) fail(`${path} contains unknown field '${unknown}' (closed schemas reject future fields)`); for (const key of keys) if (!(key in value)) fail(`${path} is missing '${key}'`); }
function finite(value: unknown, path: string): number { if (typeof value !== 'number' || !Number.isFinite(value)) fail(`${path} must be a finite number`); return value; }
function parseModulation(value: unknown, expected: PatchNodeData['parameters'][number], path: string, sourceVersion: 1 | 2): PatchNodeData['parameters'][number]['modulation'] {
  if (sourceVersion === 1) {
    return expected.modulation ? { ...expected.modulation } : undefined;
  }
  if (!expected.modulation) {
    if (value !== undefined) fail(`${path} does not expose a modulation socket`);
    return undefined;
  }
  const mapping = object(value, `${path}.modulation`);
  exactKeys(mapping, ['portId', 'amount', 'polarity', 'clampMinimum', 'clampMaximum'], `${path}.modulation`);
  if (!expected.modulation || mapping.portId !== expected.modulation.portId) fail(`${path}.modulation references the wrong parameter socket`);
  if (mapping.polarity !== 'unipolar' && mapping.polarity !== 'bipolar') fail(`${path}.modulation has an invalid polarity`);
  const amount = finite(mapping.amount, `${path}.modulation.amount`);
  const legacyPitch = expected.id === 'semitones';
  const clampMinimum = legacyPitch ? Math.max(expected.minimum, finite(mapping.clampMinimum, `${path}.modulation.clampMinimum`)) : finite(mapping.clampMinimum, `${path}.modulation.clampMinimum`);
  const clampMaximum = legacyPitch ? Math.min(expected.maximum, finite(mapping.clampMaximum, `${path}.modulation.clampMaximum`)) : finite(mapping.clampMaximum, `${path}.modulation.clampMaximum`);
  if (clampMinimum < expected.minimum || clampMaximum > expected.maximum || clampMinimum >= clampMaximum) fail(`${path}.modulation clamp is outside the parameter range`);
  return { portId: expected.modulation.portId, amount, polarity: mapping.polarity as 'unipolar' | 'bipolar', clampMinimum, clampMaximum };
}

export function createSavedPatch(nodes: Node<PatchNodeData>[], edges: Edge[], viewport: Viewport): SavedPatch { return { schemaVersion: patchSchemaVersion, engineVersion: patchEngineVersion, semantic: { nodes: nodes.map((node) => ({ id: node.id, type: node.data.type, ...(node.data.userName ? { name: node.data.userName } : {}), ...(node.data.presentation ? { presentation: node.data.presentation } : {}), ports: node.data.ports.map((port) => ({ ...port })), parameters: node.data.parameters.map(({ id, value, unit, modulation }) => ({ id, value, unit, ...(modulation ? { modulation: { ...modulation } } : {}) })) })), connections: edges.map((edge) => ({ id: edge.id, from: { nodeId: edge.source, portId: String(edge.sourceHandle ?? '') }, to: { nodeId: edge.target, portId: String(edge.targetHandle ?? '') } })) }, layout: { nodes: nodes.map((node) => ({ nodeId: node.id, x: node.position.x, y: node.position.y })), viewport: { ...viewport } } }; }
export function writePatchJson(nodes: Node<PatchNodeData>[], edges: Edge[], viewport: Viewport): string { return `${JSON.stringify(createSavedPatch(nodes, edges, viewport), null, 2)}\n`; }

export function parsePatchJson(text: string, reference: RuntimeSnapshot): LoadedPatch {
  let raw: unknown; try { raw = JSON.parse(text); } catch (reason) { fail(`invalid JSON (${reason instanceof Error ? reason.message : 'parse failure'})`); }
  const root = object(raw, 'document'); exactKeys(root, ['schemaVersion', 'engineVersion', 'semantic', 'layout'], 'document');
  if (root.schemaVersion !== 1 && root.schemaVersion !== 2) fail(`unsupported schemaVersion '${String(root.schemaVersion)}'`); if (root.engineVersion !== patchEngineVersion) fail(`unsupported engineVersion '${String(root.engineVersion)}'`); const sourceVersion = root.schemaVersion;
  const semantic = object(root.semantic, 'semantic'); exactKeys(semantic, ['nodes', 'connections'], 'semantic'); if (!Array.isArray(semantic.nodes) || !Array.isArray(semantic.connections)) fail('semantic nodes/connections must be arrays');
  const savedNodeList = semantic.nodes as unknown[]; const savedConnectionList = semantic.connections as unknown[];
  const warnings: string[] = []; const referenceById = new Map(reference.nodes.map((node) => [node.id, node])); const ids = new Set<string>(); const nodes: Node<PatchNodeData>[] = [];
  for (const [index, unknownNode] of savedNodeList.entries()) {
    const saved = object(unknownNode, `semantic.nodes[${index}]`); exactKeys(saved, ['id', 'type', 'ports', 'parameters'], `semantic.nodes[${index}]`, ['name', 'presentation']);
    if (typeof saved.id !== 'string' || !saved.id || ids.has(saved.id)) fail(`node identity at index ${index} is invalid or duplicated`); ids.add(saved.id);
    if (typeof saved.type !== 'string') fail(`node '${saved.id}' has an invalid type`);
    const savedId = saved.id as string; const savedType = saved.type as string; const referenceNode = referenceById.get(savedId); const definition = moduleByType.get(savedType as ModuleType);
    if (savedType === 'macro' && (typeof saved.name !== 'string' || saved.name.length < 1 || saved.name.length > 64)) fail(`macro '${savedId}' requires a name of 1 through 64 characters`);
    if (saved.name !== undefined && (typeof saved.name !== 'string' || saved.name.length < 1 || saved.name.length > 64)) fail(`node '${savedId}' name must contain 1 through 64 characters`);
    if (saved.presentation !== undefined && (savedType !== 'macro' || saved.presentation !== 'gravity')) fail(`node '${savedId}' has an unsupported presentation designation`);
    if (referenceNode && referenceNode.type !== saved.type) fail(`node '${saved.id}' does not match the Barr reference`); if (!referenceNode && !definition) fail(`unsupported node type '${saved.type}'`);
    const expectedPorts = referenceNode?.ports ?? definition!.ports; const expectedParameters = referenceNode?.parameters ?? definition!.parameters; const savedPorts = saved.ports as unknown[]; const savedParameters = saved.parameters as unknown[];
    if (!Array.isArray(saved.ports) || (sourceVersion === 2 && saved.ports.length !== expectedPorts.length)) fail(`ports differ for node '${saved.id}'`);
    savedPorts.forEach((unknownPort, portIndex) => { const port = object(unknownPort, `node '${savedId}' port`); exactKeys(port, ['id', 'signal', 'direction'], `node '${savedId}' port`); const expected = expectedPorts.find((candidate) => candidate.id === port.id); if (!expected || port.signal !== expected.signal || port.direction !== expected.direction) fail(`port ${portIndex} differs for node '${savedId}'`); });
    const legacyCurveMapper = savedType === 'control-map' && savedParameters.length === 3 && expectedParameters.length === 8;
    const legacyPitchPhase = savedType === 'pitch-shift' && savedParameters.length === 4 && expectedParameters.length === 5;
    if (!Array.isArray(saved.parameters) || (saved.parameters.length !== expectedParameters.length && !legacyCurveMapper && !legacyPitchPhase)) fail(`parameters differ for node '${saved.id}'`);
    const values: PatchNodeData['parameters'] = savedParameters.map((unknownParameter, parameterIndex) => {
      const parameter = object(unknownParameter, `node '${savedId}' parameter`);
      const expected = expectedParameters[parameterIndex];
      exactKeys(parameter, sourceVersion === 1 || !expected?.modulation ? ['id', 'value', 'unit'] : ['id', 'value', 'unit', 'modulation'], `node '${savedId}' parameter`);
      let value = finite(parameter.value, `${savedId}.${String(parameter.id)}`);
      if (parameter.id !== expected?.id || parameter.unit !== expected.unit) fail(`parameter ${parameterIndex} differs for node '${savedId}'`);
      if (savedType === 'pitch-shift' && expected.id === 'semitones' && (value < expected.minimum || value > expected.maximum)) {
        const original = value; value = Math.min(expected.maximum, Math.max(expected.minimum, value));
        warnings.push(`Migrated Pitch Shift '${savedId}' semitones from ${original} to ${value} for the one-octave range.`);
      } else if (value < expected.minimum || value > expected.maximum) fail(`${savedId}.${expected.id} is outside ${expected.minimum}..${expected.maximum}`);
      return { ...expected, value, modulation: parseModulation(parameter.modulation, expected, `${savedId}.${expected.id}`, sourceVersion) };
    });
    if (legacyCurveMapper) values.push(...structuredClone(expectedParameters.slice(3)));
    if (legacyPitchPhase) values.push(structuredClone(expectedParameters[4]!));
    const base = referenceNode ? { id: savedId, type: 'patchNode', position: { x: 0, y: 0 }, data: { label: referenceNode.label, type: referenceNode.type, role: referenceNode.role, ports: structuredClone(referenceNode.ports), parameters: values, runtimeBound: true } } as Node<PatchNodeData> : createModuleNode(savedType as ModuleType, savedId, { x: 0, y: 0 });
    base.data.parameters = values; if (typeof saved.name === 'string') base.data.userName = saved.name;
    if (saved.presentation === 'gravity') base.data.presentation = 'gravity'; nodes.push(base);
  }
  const ioError = requiredIoError(nodes); if (ioError) fail(ioError);
  const ports = new Map(nodes.map((node) => [node.id, new Map(node.data.ports.map((port) => [port.id, port]))])); const edgeIds = new Set<string>(); const occupiedInputs = new Set<string>(); const edges: Edge[] = savedConnectionList.map((unknownConnection, index) => {
    const connection = object(unknownConnection, `semantic.connections[${index}]`); exactKeys(connection, ['id', 'from', 'to'], `semantic.connections[${index}]`); const from = object(connection.from, `connection.from`); const to = object(connection.to, `connection.to`); exactKeys(from, ['nodeId', 'portId'], 'connection.from'); exactKeys(to, ['nodeId', 'portId'], 'connection.to'); if (typeof connection.id !== 'string' || edgeIds.has(connection.id)) fail(`connection identity at index ${index} is invalid or duplicated`); const edgeId = connection.id as string; edgeIds.add(edgeId); const sourceNode = String(from.nodeId); const targetNode = String(to.nodeId); const targetKey = `${targetNode}.${String(to.portId)}`; const source = ports.get(sourceNode)?.get(String(from.portId)); const target = ports.get(targetNode)?.get(String(to.portId)); if (!source || source.direction !== 'output' || !target || target.direction !== 'input' || source.signal !== target.signal) fail(`connection '${edgeId}' has invalid endpoints`); if (occupiedInputs.has(targetKey)) fail(`input '${targetKey}' has more than one cable; insert Sum (+)`); occupiedInputs.add(targetKey); return { id: edgeId, source: sourceNode, sourceHandle: String(from.portId), target: targetNode, targetHandle: String(to.portId), type: 'smoothstep', className: `signal-edge signal-${source.signal}`, data: { signal: source.signal }, interactionWidth: 24 };
  });
  const layout = object(root.layout, 'layout'); exactKeys(layout, ['nodes', 'viewport'], 'layout'); if (!Array.isArray(layout.nodes) || layout.nodes.length !== nodes.length) fail('layout must position every node exactly once'); const layoutNodes = layout.nodes as unknown[]; const positions = new Map<string, {x:number;y:number}>(); layoutNodes.forEach((unknownPosition, index) => { const position = object(unknownPosition, `layout.nodes[${index}]`); exactKeys(position, ['nodeId', 'x', 'y'], `layout.nodes[${index}]`); if (typeof position.nodeId !== 'string' || !ids.has(position.nodeId) || positions.has(position.nodeId)) fail(`layout node identity at index ${index} is invalid or duplicated`); const nodeId = position.nodeId as string; positions.set(nodeId, { x: finite(position.x, `${nodeId}.x`), y: finite(position.y, `${nodeId}.y`) }); }); nodes.forEach((node) => { node.position = positions.get(node.id)!; });
  const rawViewport = object(layout.viewport, 'layout.viewport'); exactKeys(rawViewport, ['x', 'y', 'zoom'], 'layout.viewport'); const viewport = { x: finite(rawViewport.x, 'viewport.x'), y: finite(rawViewport.y, 'viewport.y'), zoom: finite(rawViewport.zoom, 'viewport.zoom') }; if (viewport.zoom <= 0) fail('viewport.zoom must be positive'); return { nodes, edges, viewport, source: createSavedPatch(nodes, edges, viewport), warnings };
}
