import type { Edge, Node, Viewport, XYPosition } from '@xyflow/react';
import type {
  GraphState,
  HierarchyPortBinding,
  HierarchyPresentation,
  PatchNodeData,
  PatchPort,
} from './graph';

export interface CompoundPresentation {
  id: string;
  kind: string;
  label: string;
  memberNodeIds: string[];
  canCollapse: boolean;
  reason: string;
  summary: string;
  learn: string;
}

export interface CompoundProjection { nodes: Node<PatchNodeData>[]; edges: Edge[] }
export interface HierarchyProjection extends CompoundProjection { hierarchy: HierarchyPresentation }
// M29 supports one parent canvas and one opened nested canvas. Persisted parent
// links are reserved for a later recursive UI and are rejected beyond this
// deliberately explicit limit.
export const maximumHierarchyDepth = 1;

const projectionTokens = [
  'loop-active', 'loop-related', 'safety-loop-active', 'routing-trace-active',
  'split-focus-active', 'split-focus-related', 'reverse-cosmic-focus-active',
  'reverse-cosmic-focus-related',
];

function compoundClass(members: Node<PatchNodeData>[]) {
  const classes = new Set<string>(['compound-summary-boundary']); let energy = 0;
  for (const member of members) {
    for (const token of projectionTokens) if (member.className?.includes(token)) classes.add(token);
    const level = /\benergy-level-(\d+)\b/.exec(member.className ?? '');
    if (level) energy = Math.max(energy, Number(level[1]));
  }
  classes.add(`energy-level-${energy}`); return [...classes].join(' ');
}

const sameEndpoint = (edge: Edge, target: { nodeId: string; portId: string }, direction: 'input' | 'output') =>
  direction === 'input'
    ? edge.target === target.nodeId && edge.targetHandle === target.portId
    : edge.source === target.nodeId && edge.sourceHandle === target.portId;

export function hierarchyPortsForMatrix(id: string): HierarchyPortBinding[] {
  const suffix = id.slice('matrix-mixer'.length);
  const inputs: HierarchyPortBinding[] = [1, 2, 3, 4].map((column) => ({
    id: `in-${column}`, name: `IN ${column}`, signal: 'audio', direction: 'input',
    targets: [1, 2, 3, 4].map((row) => ({ nodeId: `matrix-${row}-from-${column}${suffix}`, portId: 'in' })),
  }));
  const outputs: HierarchyPortBinding[] = [1, 2, 3, 4].map((row) => ({
    id: `out-${row}`, name: `OUT ${row}`, signal: 'audio', direction: 'output',
    targets: [{ nodeId: `matrix-${row}-out${suffix}`, portId: 'out' }],
  }));
  return [...inputs, ...outputs];
}

export function hierarchyFromCompound(presentation: CompoundPresentation, nodes: Node<PatchNodeData>[]): HierarchyPresentation {
  const members = nodes.filter((node) => presentation.memberNodeIds.includes(node.id));
  const x = members.length ? Math.min(...members.map((node) => node.position.x)) : 0;
  const y = members.length ? Math.min(...members.map((node) => node.position.y)) : 0;
  return {
    id: presentation.id, kind: 'compound', name: presentation.label, collapsed: presentation.canCollapse,
    memberNodeIds: [...presentation.memberNodeIds], position: { x, y }, nestedViewport: { x: 0, y: 0, zoom: 0.72 },
    ports: hierarchyPortsForMatrix(presentation.id),
  };
}

export function hierarchyFromSubpatch(instance: NonNullable<PatchNodeData['subpatchInstance']>, nodes: Node<PatchNodeData>[]): HierarchyPresentation {
  const members = nodes.filter((node) => instance.memberNodeIds.includes(node.id));
  const x = members.length ? Math.min(...members.map((node) => node.position.x)) : 0;
  const y = members.length ? Math.min(...members.map((node) => node.position.y)) : 0;
  return {
    id: instance.id, kind: 'subpatch', name: instance.definitionName, collapsed: true,
    memberNodeIds: [...instance.memberNodeIds], position: { x, y }, nestedViewport: { x: 0, y: 0, zoom: 1 },
    ports: instance.ports.map((port) => ({ id: port.id, name: port.id.toUpperCase(), signal: port.signal,
      direction: port.direction, targets: [{ nodeId: port.nodeId, portId: port.portId }] })),
  };
}

function canonicalHierarchy(hierarchy: HierarchyPresentation) {
  return JSON.stringify({ ...hierarchy, memberNodeIds: [...hierarchy.memberNodeIds].sort(),
    ports: [...hierarchy.ports].map((port) => ({ ...port,
      targets: [...port.targets].sort((a, b) => `${a.nodeId}.${a.portId}`.localeCompare(`${b.nodeId}.${b.portId}`)),
    })).sort((a, b) => a.id.localeCompare(b.id)) });
}

export function inspectHierarchyPresentations(nodes: Node<PatchNodeData>[]): HierarchyPresentation[] {
  const found = new Map<string, HierarchyPresentation>();
  for (const node of nodes) {
    const hierarchy = node.data.hierarchyPresentation; if (!hierarchy) continue;
    const previous = found.get(hierarchy.id);
    if (previous && canonicalHierarchy(previous) !== canonicalHierarchy(hierarchy))
      throw new Error(`Hierarchy '${hierarchy.id}' has inconsistent member metadata.`);
    found.set(hierarchy.id, structuredClone(hierarchy));
  }
  return [...found.values()].sort((a, b) => a.id.localeCompare(b.id));
}

export function materializeHierarchyPresentations(state: GraphState, compounds: CompoundPresentation[]): GraphState {
  const existing = new Set(inspectHierarchyPresentations(state.nodes).map((item) => item.id));
  const additions: HierarchyPresentation[] = [];
  for (const compound of compounds)
    if (!existing.has(compound.id) && compound.canCollapse) additions.push(hierarchyFromCompound(compound, state.nodes));
  const subpatches = new Map<string, NonNullable<PatchNodeData['subpatchInstance']>>();
  for (const node of state.nodes) if (node.data.subpatchInstance && !subpatches.has(node.data.subpatchInstance.id))
    subpatches.set(node.data.subpatchInstance.id, node.data.subpatchInstance);
  for (const instance of subpatches.values()) if (!existing.has(instance.id)) additions.push(hierarchyFromSubpatch(instance, state.nodes));
  if (!additions.length) return state;
  const byMember = new Map<string, HierarchyPresentation>();
  for (const hierarchy of additions) for (const member of hierarchy.memberNodeIds) byMember.set(member, hierarchy);
  return { ...state, nodes: state.nodes.map((node) => {
    const hierarchy = byMember.get(node.id);
    return hierarchy ? { ...node, data: { ...node.data, hierarchyPresentation: structuredClone(hierarchy) } } : node;
  }) };
}

export function validateHierarchyPresentations(nodes: Node<PatchNodeData>[], edges: Edge[] = []): string[] {
  const errors: string[] = []; const ids = new Set(nodes.map((node) => node.id));
  const nodeById = new Map(nodes.map((node) => [node.id, node])); const owned = new Map<string, string>();
  let hierarchies: HierarchyPresentation[] = [];
  try { hierarchies = inspectHierarchyPresentations(nodes); }
  catch (reason) { return [reason instanceof Error ? reason.message : 'Hierarchy metadata is inconsistent.']; }
  const hierarchyIds = new Set(hierarchies.map((item) => item.id));
  for (const hierarchy of hierarchies) {
    const path = `layout.hierarchies['${hierarchy.id}']`;
    if (!hierarchy.id || !hierarchy.name.trim() || hierarchy.name.length > 64)
      errors.push(`${path} requires a stable ID and a name of 1 through 64 characters.`);
    if (hierarchy.memberNodeIds.length < 1) errors.push(`${path}.memberNodeIds must contain at least one primitive.`);
    for (const memberId of hierarchy.memberNodeIds) {
      if (!ids.has(memberId)) errors.push(`${path}.memberNodeIds references missing node '${memberId}'.`);
      if (nodeById.get(memberId)?.data.role === 'io') errors.push(`${path} cannot own I/O node '${memberId}'.`);
      const previous = owned.get(memberId);
      if (previous && previous !== hierarchy.id) errors.push(`${path} shares primitive '${memberId}' with '${previous}'.`);
      owned.set(memberId, hierarchy.id);
      if (nodeById.get(memberId)?.data.hierarchyPresentation?.id !== hierarchy.id)
        errors.push(`${path} is missing matching metadata on '${memberId}'.`);
    }
    const portIds = new Set<string>();
    const boundEndpoints = new Set<string>();
    for (const port of hierarchy.ports) {
      const portPath = `${path}.ports['${port.id}']`;
      if (!port.id || portIds.has(port.id)) errors.push(`${portPath} has an empty or duplicate stable port ID.`);
      portIds.add(port.id);
      if (!port.name.trim() || port.name.length > 32) errors.push(`${portPath}.name must contain 1 through 32 characters.`);
      if (!port.targets.length) errors.push(`${portPath}.targets must name at least one explicit internal port.`);
      for (const target of port.targets) {
        const endpointKey = `${port.direction}:${target.nodeId}.${target.portId}`;
        if (boundEndpoints.has(endpointKey)) errors.push(`${portPath} duplicates boundary target '${target.nodeId}.${target.portId}'.`);
        boundEndpoints.add(endpointKey);
        if (!hierarchy.memberNodeIds.includes(target.nodeId)) { errors.push(`${portPath} targets non-member '${target.nodeId}.${target.portId}'.`); continue; }
        const actual = nodeById.get(target.nodeId)?.data.ports.find((candidate) => candidate.id === target.portId);
        if (!actual || actual.signal !== port.signal || actual.direction !== port.direction)
          errors.push(`${portPath} does not match internal port '${target.nodeId}.${target.portId}'.`);
      }
      if (port.direction === 'input') {
        const sources = new Set(boundaryEdges(edges, hierarchy, port).map((edge) => `${edge.source}.${String(edge.sourceHandle ?? '')}`));
        if (sources.size > 1) errors.push(`${portPath} has divergent external sources; reconnect the parent proxy as one boundary operation.`);
      }
    }
    const members = new Set(hierarchy.memberNodeIds);
    for (const edge of edges) {
      const sourceInside = members.has(edge.source); const targetInside = members.has(edge.target);
      if (sourceInside === targetInside) continue;
      const direction = targetInside ? 'input' : 'output';
      const matched = hierarchy.ports.some((port) => port.direction === direction
        && port.targets.some((target) => sameEndpoint(edge, target, direction)));
      if (!matched) errors.push(`${path} has unmapped crossing connection '${edge.id}' at ${targetInside ? edge.target : edge.source}.${String(targetInside ? edge.targetHandle : edge.sourceHandle)}.`);
    }
    if (hierarchy.parentId && !hierarchyIds.has(hierarchy.parentId)) errors.push(`${path}.parentId references missing hierarchy '${hierarchy.parentId}'.`);
    if (![hierarchy.position.x, hierarchy.position.y, hierarchy.nestedViewport.x, hierarchy.nestedViewport.y,
      hierarchy.nestedViewport.zoom].every(Number.isFinite) || hierarchy.nestedViewport.zoom <= 0)
      errors.push(`${path} contains a non-finite position or invalid nested viewport.`);
  }
  for (const hierarchy of hierarchies) {
    const seen = new Set<string>(); let cursor: HierarchyPresentation | undefined = hierarchy; let depth = 0;
    while (cursor?.parentId) {
      if (seen.has(cursor.id)) { errors.push(`layout.hierarchies['${hierarchy.id}'] is recursive.`); break; }
      seen.add(cursor.id);
      if (seen.has(cursor.parentId)) { errors.push(`layout.hierarchies['${hierarchy.id}'] is recursive.`); break; }
      depth += 1;
      if (depth >= maximumHierarchyDepth) { errors.push(`layout.hierarchies['${hierarchy.id}'] exceeds nesting depth ${maximumHierarchyDepth}.`); break; }
      cursor = hierarchies.find((candidate) => candidate.id === cursor!.parentId);
    }
  }
  return [...new Set(errors)];
}

export function updateHierarchyPresentation(state: GraphState, hierarchyId: string,
  change: Partial<Pick<HierarchyPresentation, 'name' | 'collapsed' | 'position' | 'nestedViewport'>>): GraphState {
  let found = false;
  const nodes = state.nodes.map((node) => {
    if (node.data.hierarchyPresentation?.id !== hierarchyId) return node;
    found = true;
    return { ...node, data: { ...node.data,
      hierarchyPresentation: { ...node.data.hierarchyPresentation, ...structuredClone(change) } } };
  });
  if (!found) throw new Error(`Hierarchy '${hierarchyId}' does not exist.`);
  return { ...state, nodes };
}

export function deleteHierarchy(state: GraphState, hierarchyId: string): GraphState {
  const hierarchy = inspectHierarchyPresentations(state.nodes).find((item) => item.id === hierarchyId);
  if (!hierarchy) throw new Error(`Hierarchy '${hierarchyId}' does not exist.`);
  const removed = new Set(hierarchy.memberNodeIds);
  return { nodes: state.nodes.filter((node) => !removed.has(node.id)),
    edges: state.edges.filter((edge) => !removed.has(edge.source) && !removed.has(edge.target)) };
}

function boundaryEdges(edges: Edge[], hierarchy: HierarchyPresentation, port: HierarchyPortBinding) {
  const members = new Set(hierarchy.memberNodeIds);
  return edges.filter((edge) => port.targets.some((target) => sameEndpoint(edge, target, port.direction))
    && (port.direction === 'input' ? !members.has(edge.source) : !members.has(edge.target)));
}

const parentPort = (port: HierarchyPortBinding): PatchPort => ({ id: port.id, signal: port.signal, direction: port.direction });

export function projectHierarchyRoot(nodes: Node<PatchNodeData>[], edges: Edge[],
  hierarchies = inspectHierarchyPresentations(nodes)): CompoundProjection {
  let projectedNodes = [...nodes]; let projectedEdges = [...edges];
  for (const hierarchy of hierarchies.filter((item) => item.collapsed && !item.parentId)) {
    const hidden = new Set(hierarchy.memberNodeIds); const members = projectedNodes.filter((node) => hidden.has(node.id));
    if (members.length !== hidden.size) continue;
    const parent: Node<PatchNodeData> = {
      id: `hierarchy-view-${hierarchy.id}`, type: 'patchNode', position: { ...hierarchy.position },
      className: compoundClass(members), draggable: true, deletable: true, width: 232, height: 142,
      data: { label: hierarchy.name, type: 'compound-summary', role: 'routing', runtimeBound: false, parameters: [],
        ports: hierarchy.ports.map(parentPort), hierarchyPresentation: structuredClone(hierarchy),
        ...(members.find((node) => node.data.subpatchInstance)?.data.subpatchInstance
          ? { subpatchInstance: structuredClone(members.find((node) => node.data.subpatchInstance)!.data.subpatchInstance) }
          : {}),
        compoundPresentation: { id: hierarchy.id, kind: hierarchy.kind, memberNodeIds: hierarchy.memberNodeIds,
          authoritativeNodeCount: hierarchy.memberNodeIds.length,
          internalConnectionCount: edges.filter((edge) => hidden.has(edge.source) && hidden.has(edge.target)).length,
          summary: hierarchy.kind === 'compound'
            ? 'Four explicit inputs feed 16 signed gains and 12 sums, producing four explicit outputs.'
            : 'A version-pinned reusable view over ordinary editable primitives.',
          learn: 'Open the nested schematic to inspect and edit the same authoritative primitives used by audio, save, export, and diagnostics.' } },
    };
    const internalIds = new Set(projectedEdges.filter((edge) => hidden.has(edge.source) && hidden.has(edge.target)).map((edge) => edge.id));
    const crossingIds = new Set<string>(); const projectedBoundary: Edge[] = [];
    for (const port of hierarchy.ports) {
      const crossings = boundaryEdges(edges, hierarchy, port); crossings.forEach((edge) => crossingIds.add(edge.id));
      if (port.direction === 'input' && crossings.length) {
        const sources = new Map<string, Edge[]>();
        for (const edge of crossings) { const key = `${edge.source}\n${edge.sourceHandle ?? ''}`;
          const group = sources.get(key) ?? []; group.push(edge); sources.set(key, group); }
        for (const [key, grouped] of sources) {
          const [source, sourceHandle] = key.split('\n');
          projectedBoundary.push({ ...grouped[0]!, id: `hierarchy-edge-${hierarchy.id}-${port.id}-${source}-${sourceHandle}`,
            source, sourceHandle, target: parent.id, targetHandle: port.id,
            data: { ...grouped[0]!.data, hierarchyId: hierarchy.id, hierarchyPortId: port.id,
              hierarchyEdgeIds: grouped.map((edge) => edge.id) } });
        }
      } else if (port.direction === 'output') for (const edge of crossings) projectedBoundary.push({ ...edge,
        source: parent.id, sourceHandle: port.id,
        data: { ...edge.data, hierarchyId: hierarchy.id, hierarchyPortId: port.id, hierarchyEdgeIds: [edge.id] } });
    }
    projectedNodes = [...projectedNodes.filter((node) => !hidden.has(node.id)), parent];
    projectedEdges = [...projectedEdges.filter((edge) => !internalIds.has(edge.id) && !crossingIds.has(edge.id)), ...projectedBoundary];
  }
  return { nodes: projectedNodes, edges: projectedEdges };
}

function boundaryNode(hierarchy: HierarchyPresentation, port: HierarchyPortBinding, position: XYPosition): Node<PatchNodeData> {
  const visibleDirection = port.direction === 'input' ? 'output' : 'input';
  return { id: `hierarchy-boundary-${hierarchy.id}-${port.id}`, type: 'patchNode', position,
    draggable: false, deletable: false, width: 112, height: 74,
    className: `hierarchy-boundary-node hierarchy-boundary-${port.direction}`,
    data: { label: port.name, type: 'hierarchy-boundary', role: 'io', runtimeBound: false, parameters: [],
      ports: [{ id: port.id, signal: port.signal, direction: visibleDirection }],
      hierarchyBoundary: { hierarchyId: hierarchy.id, portId: port.id, name: port.name,
        direction: port.direction, targets: structuredClone(port.targets) } } };
}

export function projectNestedHierarchy(nodes: Node<PatchNodeData>[], edges: Edge[], hierarchy: HierarchyPresentation): HierarchyProjection {
  const members = new Set(hierarchy.memberNodeIds); const memberNodes = nodes.filter((node) => members.has(node.id));
  const minX = memberNodes.length ? Math.min(...memberNodes.map((node) => node.position.x)) : 0;
  const maxX = memberNodes.length ? Math.max(...memberNodes.map((node) => node.position.x)) : 0;
  const minY = memberNodes.length ? Math.min(...memberNodes.map((node) => node.position.y)) : 0;
  const inputs = hierarchy.ports.filter((port) => port.direction === 'input');
  const outputs = hierarchy.ports.filter((port) => port.direction === 'output');
  const boundaryNodes = [...inputs.map((port, index) => boundaryNode(hierarchy, port, { x: minX - 250, y: minY + index * 150 })),
    ...outputs.map((port, index) => boundaryNode(hierarchy, port, { x: maxX + 250, y: minY + index * 150 }))];
  const nestedEdges: Edge[] = edges.flatMap((edge) => {
    const sourceInside = members.has(edge.source); const targetInside = members.has(edge.target);
    if (sourceInside && targetInside) return [{ ...edge }];
    if (!sourceInside && targetInside) {
      const port = inputs.find((candidate) => candidate.targets.some((target) => sameEndpoint(edge, target, 'input')));
      return port ? [{ ...edge, source: `hierarchy-boundary-${hierarchy.id}-${port.id}`, sourceHandle: port.id,
        data: { ...edge.data, hierarchyId: hierarchy.id, hierarchyPortId: port.id, hierarchyEdgeIds: [edge.id] } }] : [];
    }
    if (sourceInside && !targetInside) {
      const port = outputs.find((candidate) => candidate.targets.some((target) => sameEndpoint(edge, target, 'output')));
      return port ? [{ ...edge, target: `hierarchy-boundary-${hierarchy.id}-${port.id}`, targetHandle: port.id,
        data: { ...edge.data, hierarchyId: hierarchy.id, hierarchyPortId: port.id, hierarchyEdgeIds: [edge.id] } }] : [];
    }
    return [];
  });
  return { nodes: [...memberNodes, ...boundaryNodes], edges: nestedEdges, hierarchy };
}

export const hierarchyForViewNode = (node: Node<PatchNodeData> | null) => node?.data.hierarchyPresentation ?? null;
export const compoundMembers = (node: Node<PatchNodeData> | null) => new Set(
  node?.data.hierarchyPresentation?.memberNodeIds ?? node?.data.compoundPresentation?.memberNodeIds ?? []);
export function hierarchyActualEdgeIds(edge: Edge): string[] {
  const ids = edge.data?.hierarchyEdgeIds;
  return Array.isArray(ids) && ids.every((id) => typeof id === 'string') ? ids as string[] : [edge.id];
}

export function expandHierarchyConnection(
  visibleNodes: Node<PatchNodeData>[],
  authoritativeEdges: Edge[],
  connection: { source: string | null; sourceHandle: string | null; target: string | null; targetHandle: string | null },
) {
  if (!connection.source || !connection.target || !connection.sourceHandle || !connection.targetHandle) return [];
  const sourceNode = visibleNodes.find((node) => node.id === connection.source);
  const targetNode = visibleNodes.find((node) => node.id === connection.target);
  let sources = [{ nodeId: connection.source, portId: connection.sourceHandle }];
  let targets = [{ nodeId: connection.target, portId: connection.targetHandle }];
  const sourceHierarchy = sourceNode?.data.hierarchyPresentation;
  if (sourceHierarchy) {
    const port = sourceHierarchy.ports.find((candidate) => candidate.id === connection.sourceHandle && candidate.direction === 'output');
    sources = port?.targets.length ? [port.targets[0]!] : [];
  } else if (sourceNode?.data.hierarchyBoundary?.direction === 'input') {
    const boundary = sourceNode.data.hierarchyBoundary;
    const existing = authoritativeEdges.find((edge) => boundary.targets.some((target) => sameEndpoint(edge, target, 'input')));
    sources = existing ? [{ nodeId: existing.source, portId: String(existing.sourceHandle ?? '') }] : [];
  }
  const targetHierarchy = targetNode?.data.hierarchyPresentation;
  if (targetHierarchy) {
    const port = targetHierarchy.ports.find((candidate) => candidate.id === connection.targetHandle && candidate.direction === 'input');
    targets = port?.targets ?? [];
  } else if (targetNode?.data.hierarchyBoundary?.direction === 'output') {
    const boundary = targetNode.data.hierarchyBoundary;
    const existing = authoritativeEdges.find((edge) => boundary.targets.some((target) => sameEndpoint(edge, target, 'output')));
    targets = existing ? [{ nodeId: existing.target, portId: String(existing.targetHandle ?? '') }] : [];
  }
  return sources.flatMap((source) => targets.map((target) => ({
    source: source.nodeId, sourceHandle: source.portId, target: target.nodeId, targetHandle: target.portId,
  })));
}
export function renameHierarchy(state: GraphState, hierarchyId: string, name: string): GraphState {
  const clean = name.trim(); if (!clean || clean.length > 64) throw new Error('Compound names must contain 1 through 64 characters.');
  return updateHierarchyPresentation(state, hierarchyId, { name: clean });
}
export const setHierarchyViewport = (state: GraphState, hierarchyId: string, viewport: Viewport) =>
  updateHierarchyPresentation(state, hierarchyId, { nestedViewport: { ...viewport } });

// Compatibility wrappers retained for focused M26 tests and downstream modules.
export function projectCompoundPresentation(nodes: Node<PatchNodeData>[], edges: Edge[], presentation: CompoundPresentation, collapsed: boolean): CompoundProjection {
  if (!collapsed || !presentation.canCollapse) return { nodes, edges };
  const state = materializeHierarchyPresentations({ nodes, edges }, [presentation]);
  const hierarchy = inspectHierarchyPresentations(state.nodes).find((item) => item.id === presentation.id);
  return hierarchy ? projectHierarchyRoot(state.nodes, state.edges, [{ ...hierarchy, collapsed: true }]) : state;
}
export function projectCompoundPresentations(nodes: Node<PatchNodeData>[], edges: Edge[], presentations: CompoundPresentation[], collapsedIds: ReadonlySet<string>): CompoundProjection {
  const state = materializeHierarchyPresentations({ nodes, edges }, presentations);
  const hierarchies = inspectHierarchyPresentations(state.nodes).map((hierarchy) => ({ ...hierarchy, collapsed: collapsedIds.has(hierarchy.id) }));
  return projectHierarchyRoot(state.nodes, state.edges, hierarchies);
}
