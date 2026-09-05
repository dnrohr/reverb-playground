import type { Edge, Node, Viewport } from '@xyflow/react';
import { requiredIoError, type CableLayout, type HierarchyPresentation, type PatchNodeData, type RuntimeSnapshot } from './graph';
import { createModuleNode, moduleByType, type ModuleType } from './modules';
import { inspectGraphGroups } from './graphGroups';
import { inspectSubpatchInstances, type SubpatchInstance } from './subpatches';
import { inspectHierarchyPresentations, validateHierarchyPresentations } from './compoundPresentation';

export const patchSchemaVersion = 2 as const;
export const patchEngineVersion = '0.1';
export type QualityPolicy = 'draft' | 'normal' | 'high';
interface SavedParameter { id: string; value: number; unit: string; modulation?: NonNullable<PatchNodeData['parameters'][number]['modulation']> }
interface SavedPatch { schemaVersion: 2; engineVersion: string; qualityPolicy: QualityPolicy; semantic: { nodes: Array<{ id: string; type: string; name?: string; presentation?: 'gravity'; ports: PatchNodeData['ports']; parameters: SavedParameter[] }>; connections: Array<{ id: string; from: { nodeId: string; portId: string }; to: { nodeId: string; portId: string } }> }; layout: { nodes: Array<{ nodeId: string; x: number; y: number; orientation?: 'reverse' }>; viewport: Viewport; groups?: Array<{ id: string; name: string; collapsed: boolean; nodeIds: string[] }>; cables?: Array<{ edgeId: string; waypoints?: Array<{ x: number; y: number }>; portal?: { name: string } }>; subpatches?: SubpatchInstance[]; hierarchies?: HierarchyPresentation[] } }
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

export function createSavedPatch(nodes: Node<PatchNodeData>[], edges: Edge[], viewport: Viewport, qualityPolicy: QualityPolicy = 'normal'): SavedPatch {
  const hierarchyErrors = validateHierarchyPresentations(nodes, edges);
  if (hierarchyErrors.length) fail(hierarchyErrors[0]!);
  const groups = inspectGraphGroups(nodes);
  const cables = edges.flatMap((edge) => edge.data?.layout ? [{ edgeId: edge.id, ...(structuredClone(edge.data.layout) as CableLayout) }] : []);
  const subpatches = inspectSubpatchInstances(nodes);
  const hierarchies = inspectHierarchyPresentations(nodes);
  return { schemaVersion: patchSchemaVersion, engineVersion: patchEngineVersion, qualityPolicy,
    semantic: { nodes: nodes.map((node) => ({ id: node.id, type: node.data.type,
      ...(node.data.userName ? { name: node.data.userName } : {}), ...(node.data.presentation ? { presentation: node.data.presentation } : {}),
      ports: node.data.ports.map((port) => ({ ...port })), parameters: node.data.parameters.map(({ id, value, unit, modulation }) =>
        ({ id, value, unit, ...(modulation ? { modulation: { ...modulation } } : {}) })) })),
      connections: edges.map((edge) => ({ id: edge.id, from: { nodeId: edge.source, portId: String(edge.sourceHandle ?? '') },
        to: { nodeId: edge.target, portId: String(edge.targetHandle ?? '') } })) },
    layout: { nodes: nodes.map((node) => ({ nodeId: node.id, x: node.position.x, y: node.position.y,
      ...(node.data.orientation ? { orientation: node.data.orientation } : {}) })), viewport: { ...viewport },
      ...(groups.length ? { groups } : {}), ...(cables.length ? { cables } : {}), ...(subpatches.length ? { subpatches } : {}),
      ...(hierarchies.length ? { hierarchies } : {}) } };
}
export function writePatchJson(nodes: Node<PatchNodeData>[], edges: Edge[], viewport: Viewport, qualityPolicy: QualityPolicy = 'normal'): string { return `${JSON.stringify(createSavedPatch(nodes, edges, viewport, qualityPolicy), null, 2)}\n`; }

export function parsePatchJson(text: string, reference: RuntimeSnapshot): LoadedPatch {
  let raw: unknown; try { raw = JSON.parse(text); } catch (reason) { fail(`invalid JSON (${reason instanceof Error ? reason.message : 'parse failure'})`); }
  const root = object(raw, 'document'); exactKeys(root, ['schemaVersion', 'engineVersion', 'semantic', 'layout'], 'document', ['qualityPolicy']);
  if (root.schemaVersion !== 1 && root.schemaVersion !== 2) fail(`unsupported schemaVersion '${String(root.schemaVersion)}'`); if (root.engineVersion !== patchEngineVersion) fail(`unsupported engineVersion '${String(root.engineVersion)}'`); const sourceVersion = root.schemaVersion;
  const qualityPolicy = root.qualityPolicy ?? 'normal'; if (qualityPolicy !== 'draft' && qualityPolicy !== 'normal' && qualityPolicy !== 'high') fail(`unsupported qualityPolicy '${String(qualityPolicy)}'`);
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
  const layout = object(root.layout, 'layout'); exactKeys(layout, ['nodes', 'viewport'], 'layout', ['groups', 'cables', 'subpatches', 'hierarchies']); if (!Array.isArray(layout.nodes) || layout.nodes.length !== nodes.length) fail('layout must position every node exactly once'); const layoutNodes = layout.nodes as unknown[]; const positions = new Map<string, {x:number;y:number;orientation?:'reverse'}>(); layoutNodes.forEach((unknownPosition, index) => { const position = object(unknownPosition, `layout.nodes[${index}]`); exactKeys(position, ['nodeId', 'x', 'y'], `layout.nodes[${index}]`, ['orientation']); if (typeof position.nodeId !== 'string' || !ids.has(position.nodeId) || positions.has(position.nodeId)) fail(`layout node identity at index ${index} is invalid or duplicated`); if (position.orientation !== undefined && position.orientation !== 'reverse') fail(`layout node '${String(position.nodeId)}' has invalid orientation`); const nodeId = position.nodeId as string; positions.set(nodeId, { x: finite(position.x, `${nodeId}.x`), y: finite(position.y, `${nodeId}.y`), ...(position.orientation === 'reverse' ? { orientation: 'reverse' as const } : {}) }); }); nodes.forEach((node) => { const position = positions.get(node.id)!; node.position = { x: position.x, y: position.y }; if (position.orientation) node.data.orientation = position.orientation; });
  const groups = layout.groups ?? []; if (!Array.isArray(groups)) fail('layout.groups must be an array');
  const groupIds = new Set<string>(); const groupedNodes = new Set<string>();
  for (const [index, unknownGroup] of groups.entries()) {
    const group = object(unknownGroup, `layout.groups[${index}]`); exactKeys(group, ['id', 'name', 'collapsed', 'nodeIds'], `layout.groups[${index}]`);
    if (typeof group.id !== 'string' || !group.id || groupIds.has(group.id)) fail(`group identity at index ${index} is invalid or duplicated`);
    if (typeof group.name !== 'string' || group.name.trim().length < 1 || group.name.length > 64) fail(`group '${String(group.id)}' name must contain 1 through 64 characters`);
    if (typeof group.collapsed !== 'boolean' || !Array.isArray(group.nodeIds) || group.nodeIds.length < 2) fail(`group '${String(group.id)}' must contain at least two nodes`);
    groupIds.add(group.id); const memberIds = group.nodeIds as unknown[];
    for (const memberId of memberIds) {
      if (typeof memberId !== 'string' || !ids.has(memberId) || groupedNodes.has(memberId)) fail(`group '${group.id}' contains an invalid, duplicated, or nested node`);
      const member = nodes.find((node) => node.id === memberId)!; if (member.data.role === 'io') fail(`group '${group.id}' cannot contain I/O node '${memberId}'`);
      groupedNodes.add(memberId); member.data.presentationGroup = { id: group.id, name: group.name.trim(), collapsed: group.collapsed };
    }
  }
  const cables = layout.cables ?? []; if (!Array.isArray(cables)) fail('layout.cables must be an array'); const routedEdges = new Set<string>();
  for (const [index, unknownCable] of cables.entries()) {
    const cable = object(unknownCable, `layout.cables[${index}]`); exactKeys(cable, ['edgeId'], `layout.cables[${index}]`, ['waypoints', 'portal']);
    if (typeof cable.edgeId !== 'string' || !edgeIds.has(cable.edgeId) || routedEdges.has(cable.edgeId)) fail(`routed cable identity at index ${index} is invalid or duplicated`);
    const cableLayout: CableLayout = {}; routedEdges.add(cable.edgeId);
    if (cable.waypoints !== undefined) {
      if (!Array.isArray(cable.waypoints) || cable.waypoints.length > 32) fail(`cable '${cable.edgeId}' has invalid waypoints`);
      cableLayout.waypoints = cable.waypoints.map((unknownPoint, pointIndex) => { const point = object(unknownPoint, `layout.cables[${index}].waypoints[${pointIndex}]`); exactKeys(point, ['x', 'y'], 'waypoint'); return { x: finite(point.x, 'waypoint.x'), y: finite(point.y, 'waypoint.y') }; });
    }
    if (cable.portal !== undefined) { const portal = object(cable.portal, `layout.cables[${index}].portal`); exactKeys(portal, ['name'], 'portal'); if (typeof portal.name !== 'string' || !portal.name.trim() || portal.name.length > 32) fail(`cable '${cable.edgeId}' has an invalid portal name`); cableLayout.portal = { name: portal.name.trim() }; }
    if (!cableLayout.waypoints?.length && !cableLayout.portal) fail(`cable '${cable.edgeId}' has empty routing metadata`);
    const edge = edges.find((candidate) => candidate.id === cable.edgeId)!; edge.data = { ...edge.data, layout: cableLayout };
  }
  const subpatches = layout.subpatches ?? []; if (!Array.isArray(subpatches)) fail('layout.subpatches must be an array'); const subpatchIds = new Set<string>(); const subpatchMembers = new Set<string>();
  for (const [index, unknownSubpatch] of subpatches.entries()) {
    const saved = object(unknownSubpatch, `layout.subpatches[${index}]`); exactKeys(saved, ['id', 'definitionId', 'definitionVersion', 'definitionName', 'memberNodeIds', 'ports'], `layout.subpatches[${index}]`);
    if (typeof saved.id !== 'string' || !saved.id || subpatchIds.has(saved.id)) fail(`subpatch identity at index ${index} is invalid or duplicated`);
    if (typeof saved.definitionId !== 'string' || !saved.definitionId || typeof saved.definitionName !== 'string' || !saved.definitionName || saved.definitionName.length > 64) fail(`subpatch '${saved.id}' has invalid definition identity`);
    if (!Number.isInteger(saved.definitionVersion) || Number(saved.definitionVersion) < 1) fail(`subpatch '${saved.id}' has invalid definition version`);
    if (!Array.isArray(saved.memberNodeIds) || !saved.memberNodeIds.length || !Array.isArray(saved.ports) || !saved.ports.length) fail(`subpatch '${saved.id}' requires members and explicit ports`);
    const memberNodeIds = saved.memberNodeIds as unknown[]; const memberSet = new Set<string>();
    for (const memberId of memberNodeIds) { const member = nodes.find((node) => node.id === memberId); if (typeof memberId !== 'string' || !member || member.data.role === 'io' || memberSet.has(memberId) || subpatchMembers.has(memberId)) fail(`subpatch '${saved.id}' has invalid, duplicated, shared, or I/O member '${String(memberId)}'`); memberSet.add(memberId); subpatchMembers.add(memberId); }
    const bindingIds = new Set<string>(); const bindings = (saved.ports as unknown[]).map((unknownPort, portIndex) => { const port = object(unknownPort, `subpatch '${saved.id}' port ${portIndex}`); exactKeys(port, ['id', 'signal', 'direction', 'nodeId', 'portId'], `subpatch '${saved.id}' port ${portIndex}`); if (typeof port.id !== 'string' || !port.id || bindingIds.has(port.id) || (port.signal !== 'audio' && port.signal !== 'control') || (port.direction !== 'input' && port.direction !== 'output') || typeof port.nodeId !== 'string' || !memberSet.has(port.nodeId) || typeof port.portId !== 'string') fail(`subpatch '${saved.id}' has invalid or duplicated explicit port ${portIndex}`); bindingIds.add(port.id); const actual = nodes.find((node) => node.id === port.nodeId)?.data.ports.find((candidate) => candidate.id === port.portId); if (!actual || actual.signal !== port.signal || actual.direction !== port.direction) fail(`subpatch '${saved.id}' port '${port.id}' does not match its primitive endpoint`); return { id: port.id, signal: port.signal, direction: port.direction, nodeId: port.nodeId, portId: port.portId } as SubpatchInstance['ports'][number]; });
    subpatchIds.add(saved.id); const metadata: SubpatchInstance = { id: saved.id, definitionId: saved.definitionId, definitionVersion: Number(saved.definitionVersion), definitionName: saved.definitionName, memberNodeIds: [...memberSet], ports: bindings };
    nodes.forEach((node) => { if (memberSet.has(node.id)) { node.className = `${node.className ?? ''} subpatch-member`; node.data.subpatchInstance = structuredClone(metadata); } });
  }
  const hierarchies = layout.hierarchies ?? [];
  if (!Array.isArray(hierarchies)) fail('layout.hierarchies must be an array');
  const hierarchyIds = new Set<string>(); const hierarchyMembers = new Set<string>();
  for (const [index, unknownHierarchy] of hierarchies.entries()) {
    const saved = object(unknownHierarchy, `layout.hierarchies[${index}]`);
    exactKeys(saved, ['id', 'kind', 'name', 'collapsed', 'memberNodeIds', 'position', 'nestedViewport', 'ports'],
      `layout.hierarchies[${index}]`, ['parentId', 'orientation']);
    if (typeof saved.id !== 'string' || !saved.id || hierarchyIds.has(saved.id)) fail(`hierarchy identity at index ${index} is invalid or duplicated`);
    if (saved.kind !== 'compound' && saved.kind !== 'subpatch') fail(`hierarchy '${String(saved.id)}' has unsupported kind '${String(saved.kind)}'`);
    if (typeof saved.name !== 'string' || !saved.name.trim() || saved.name.length > 64) fail(`hierarchy '${saved.id}' name must contain 1 through 64 characters`);
    if (typeof saved.collapsed !== 'boolean' || !Array.isArray(saved.memberNodeIds) || !saved.memberNodeIds.length)
      fail(`hierarchy '${saved.id}' requires collapsed state and member primitives`);
    if (saved.parentId !== undefined && (typeof saved.parentId !== 'string' || !saved.parentId)) fail(`hierarchy '${saved.id}' has an invalid parentId`);
    if (saved.orientation !== undefined && saved.orientation !== 'reverse') fail(`hierarchy '${saved.id}' has invalid orientation`);
    const position = object(saved.position, `hierarchy '${saved.id}'.position`);
    exactKeys(position, ['x', 'y'], `hierarchy '${saved.id}'.position`);
    const nestedViewport = object(saved.nestedViewport, `hierarchy '${saved.id}'.nestedViewport`);
    exactKeys(nestedViewport, ['x', 'y', 'zoom'], `hierarchy '${saved.id}'.nestedViewport`);
    const memberNodeIds = (saved.memberNodeIds as unknown[]).map((memberId) => {
      if (typeof memberId !== 'string' || !ids.has(memberId) || hierarchyMembers.has(memberId))
        fail(`hierarchy '${saved.id}' has invalid, duplicated, or shared member '${String(memberId)}'`);
      hierarchyMembers.add(memberId); return memberId;
    });
    if (!Array.isArray(saved.ports) || !saved.ports.length) fail(`hierarchy '${saved.id}' requires explicit boundary ports`);
    const portIds = new Set<string>();
    const hierarchyPorts = (saved.ports as unknown[]).map((unknownPort, portIndex) => {
      const port = object(unknownPort, `hierarchy '${saved.id}' port ${portIndex}`);
      exactKeys(port, ['id', 'name', 'signal', 'direction', 'targets'], `hierarchy '${saved.id}' port ${portIndex}`);
      if (typeof port.id !== 'string' || !port.id || portIds.has(port.id) || typeof port.name !== 'string' || !port.name.trim()
        || port.name.length > 32 || (port.signal !== 'audio' && port.signal !== 'control')
        || (port.direction !== 'input' && port.direction !== 'output') || !Array.isArray(port.targets) || !port.targets.length)
        fail(`hierarchy '${saved.id}' has invalid boundary port ${portIndex}`);
      portIds.add(port.id);
      const targets = (port.targets as unknown[]).map((unknownTarget, targetIndex) => {
        const target = object(unknownTarget, `hierarchy '${saved.id}' port '${port.id}' target ${targetIndex}`);
        exactKeys(target, ['nodeId', 'portId'], `hierarchy '${saved.id}' port '${port.id}' target ${targetIndex}`);
        if (typeof target.nodeId !== 'string' || !memberNodeIds.includes(target.nodeId) || typeof target.portId !== 'string')
          fail(`hierarchy '${saved.id}' port '${port.id}' has a dangling target`);
        const actual = nodes.find((node) => node.id === target.nodeId)?.data.ports.find((candidate) => candidate.id === target.portId);
        if (!actual || actual.signal !== port.signal || actual.direction !== port.direction)
          fail(`hierarchy '${saved.id}' port '${port.id}' does not match '${target.nodeId}.${target.portId}'`);
        return { nodeId: target.nodeId, portId: target.portId };
      });
      return { id: port.id, name: port.name.trim(), signal: port.signal, direction: port.direction, targets } as HierarchyPresentation['ports'][number];
    });
    hierarchyIds.add(saved.id);
    const metadata: HierarchyPresentation = { id: saved.id, kind: saved.kind, name: saved.name.trim(), collapsed: saved.collapsed,
      memberNodeIds, position: { x: finite(position.x, `hierarchy '${saved.id}'.position.x`), y: finite(position.y, `hierarchy '${saved.id}'.position.y`) },
      nestedViewport: { x: finite(nestedViewport.x, `hierarchy '${saved.id}'.nestedViewport.x`),
        y: finite(nestedViewport.y, `hierarchy '${saved.id}'.nestedViewport.y`),
        zoom: finite(nestedViewport.zoom, `hierarchy '${saved.id}'.nestedViewport.zoom`) },
      ports: hierarchyPorts, ...(saved.parentId ? { parentId: saved.parentId as string } : {}) };
    if (saved.orientation === 'reverse') metadata.orientation = 'reverse';
    if (metadata.nestedViewport.zoom <= 0) fail(`hierarchy '${saved.id}' nested viewport zoom must be positive`);
    nodes.forEach((node) => { if (memberNodeIds.includes(node.id)) node.data.hierarchyPresentation = structuredClone(metadata); });
  }
  const hierarchyErrors = validateHierarchyPresentations(nodes, edges);
  if (hierarchyErrors.length) fail(hierarchyErrors[0]!);
  const rawViewport = object(layout.viewport, 'layout.viewport'); exactKeys(rawViewport, ['x', 'y', 'zoom'], 'layout.viewport'); const viewport = { x: finite(rawViewport.x, 'viewport.x'), y: finite(rawViewport.y, 'viewport.y'), zoom: finite(rawViewport.zoom, 'viewport.zoom') }; if (viewport.zoom <= 0) fail('viewport.zoom must be positive'); return { nodes, edges, viewport, source: createSavedPatch(nodes, edges, viewport, qualityPolicy), warnings };
}
