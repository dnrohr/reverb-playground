import type { Edge, Node } from '@xyflow/react';
import type { CableLayout, GraphState, PatchNodeData } from './graph';

export type AlignmentCommand = 'left' | 'top' | 'horizontal' | 'vertical';
export type TraceDirection = 'source' | 'output';

export interface NodeBounds { x: number; y: number; width: number; height: number }
const defaultNodeWidth = 183;
const defaultNodeHeight = 105;
const boundsFor = (node: Node<PatchNodeData>): NodeBounds => ({
  x: node.position.x, y: node.position.y,
  width: node.measured?.width ?? node.width ?? defaultNodeWidth,
  height: node.measured?.height ?? node.height ?? defaultNodeHeight,
});
const intersects = (a: NodeBounds, b: NodeBounds, clearance = 24) =>
  a.x < b.x + b.width + clearance && a.x + a.width + clearance > b.x
  && a.y < b.y + b.height + clearance && a.y + a.height + clearance > b.y;

export const cableLayout = (edge: Edge): CableLayout => (edge.data?.layout ?? {}) as CableLayout;

const withLayout = (edge: Edge, layout: CableLayout): Edge => ({
  ...edge, data: { ...edge.data, ...(Object.keys(layout).length ? { layout } : {}) },
});

export function addCableWaypoint(state: GraphState, edgeId: string): GraphState {
  const source = state.edges.find((edge) => edge.id === edgeId);
  if (!source) throw new Error(`Cable '${edgeId}' does not exist.`);
  const from = state.nodes.find((node) => node.id === source.source)?.position;
  const to = state.nodes.find((node) => node.id === source.target)?.position;
  if (!from || !to) throw new Error(`Cable '${edgeId}' has missing endpoints.`);
  const layout = cableLayout(source); const previous = layout.waypoints ?? [];
  const anchors = [from, ...previous, to]; let longest = 0; let insertAt = 0;
  for (let index = 0; index < anchors.length - 1; index += 1) {
    const length = Math.hypot(anchors[index + 1].x - anchors[index].x, anchors[index + 1].y - anchors[index].y);
    if (length > longest) { longest = length; insertAt = index; }
  }
  const a = anchors[insertAt]; const b = anchors[insertAt + 1];
  const waypoint = { x: Math.round((a.x + b.x) / 2), y: Math.round((a.y + b.y) / 2) };
  const waypoints = [...previous]; waypoints.splice(insertAt, 0, waypoint);
  return { ...state, edges: state.edges.map((edge) => edge.id === edgeId ? withLayout(edge, { ...layout, waypoints }) : edge) };
}

export function updateCableWaypoint(state: GraphState, edgeId: string, index: number, point: { x: number; y: number }): GraphState {
  if (!Number.isFinite(point.x) || !Number.isFinite(point.y)) throw new Error('Waypoint coordinates must be finite.');
  return { ...state, edges: state.edges.map((edge) => {
    if (edge.id !== edgeId) return edge; const layout = cableLayout(edge); const waypoints = [...(layout.waypoints ?? [])];
    if (!waypoints[index]) throw new Error(`Waypoint ${index + 1} does not exist.`);
    waypoints[index] = { x: point.x, y: point.y }; return withLayout(edge, { ...layout, waypoints });
  }) };
}

export function clearCableWaypoints(state: GraphState, edgeId: string): GraphState {
  return { ...state, edges: state.edges.map((edge) => {
    if (edge.id !== edgeId) return edge; const { waypoints: _discard, ...layout } = cableLayout(edge); return withLayout(edge, layout);
  }) };
}

export function setRoutingPortal(state: GraphState, edgeId: string, name?: string): GraphState {
  const clean = name?.trim();
  if (clean !== undefined && (!clean || clean.length > 32)) throw new Error('Portal names must contain 1 through 32 characters.');
  let found = false;
  const edges = state.edges.map((edge) => {
    if (edge.id !== edgeId) return edge; found = true; const layout = cableLayout(edge);
    if (clean === undefined) { const { portal: _discard, ...rest } = layout; return withLayout(edge, rest); }
    return withLayout(edge, { ...layout, portal: { name: clean } });
  });
  if (!found) throw new Error(`Cable '${edgeId}' does not exist.`);
  return { ...state, edges };
}

export function alignSelectedNodes(state: GraphState, command: AlignmentCommand): GraphState {
  const selected = state.nodes.filter((node) => node.selected && node.data.role !== 'io');
  const required = command === 'horizontal' || command === 'vertical' ? 3 : 2;
  if (selected.length < required) throw new Error(`${command === 'horizontal' || command === 'vertical' ? 'Distribute' : 'Align'} requires ${required} selected non-I/O blocks.`);
  const positions = new Map(selected.map((node) => [node.id, { ...node.position }]));
  if (command === 'left' || command === 'top') {
    const value = Math.min(...selected.map((node) => command === 'left' ? boundsFor(node).x : boundsFor(node).y));
    for (const node of selected) positions.set(node.id, { ...node.position, [command === 'left' ? 'x' : 'y']: value });
  } else {
    const axis = command === 'horizontal' ? 'x' : 'y';
    const size = command === 'horizontal' ? 'width' : 'height';
    const ordered = [...selected].sort((a, b) => boundsFor(a)[axis] - boundsFor(b)[axis] || a.id.localeCompare(b.id));
    const first = boundsFor(ordered[0]); const last = boundsFor(ordered.at(-1)!);
    const start = first[axis]; const end = last[axis] + last[size];
    const totalSize = ordered.reduce((sum, node) => sum + boundsFor(node)[size], 0);
    const gap = (end - start - totalSize) / (ordered.length - 1);
    if (gap < 0) throw new Error(`Cannot distribute ${command}: selected bounding boxes need ${Math.ceil(-gap * (ordered.length - 1))} more graph pixels.`);
    let cursor = start;
    ordered.forEach((node) => {
      positions.set(node.id, { ...node.position, [axis]: cursor });
      cursor += boundsFor(node)[size] + gap;
    });
  }
  return { ...state, nodes: state.nodes.map((node) => positions.has(node.id) ? { ...node, position: positions.get(node.id)! } : node) };
}

export function arrangeGraphGroup(state: GraphState, groupId: string): GraphState {
  const members = state.nodes.filter((node) => node.data.presentationGroup?.id === groupId);
  if (members.length < 2) throw new Error(`Group '${groupId}' does not exist.`);
  if (members.some((node) => node.data.presentationGroup?.collapsed)) throw new Error('Expand the group before arranging its members.');
  const ordered = [...members].sort((a, b) => a.id.localeCompare(b.id));
  const origin = { x: Math.min(...members.map((node) => node.position.x)), y: Math.min(...members.map((node) => node.position.y)) };
  const protectedBounds = state.nodes.filter((node) => !members.includes(node)).map(boundsFor);
  const spacing = 36;
  const candidates = Array.from({ length: ordered.length }, (_, index) => index + 1).map((columns) => {
    const rows = Math.ceil(ordered.length / columns);
    const columnWidths = Array.from({ length: columns }, () => 0);
    const rowHeights = Array.from({ length: rows }, () => 0);
    ordered.forEach((node, index) => {
      const bounds = boundsFor(node); const column = index % columns; const row = Math.floor(index / columns);
      columnWidths[column] = Math.max(columnWidths[column]!, bounds.width);
      rowHeights[row] = Math.max(rowHeights[row]!, bounds.height);
    });
    const xOffsets = columnWidths.map((_, index) => columnWidths.slice(0, index).reduce((sum, value) => sum + value + spacing, 0));
    const yOffsets = rowHeights.map((_, index) => rowHeights.slice(0, index).reduce((sum, value) => sum + value + spacing, 0));
    const positions = new Map<string, { x: number; y: number }>();
    ordered.forEach((node, index) => positions.set(node.id, { x: origin.x + xOffsets[index % columns]!, y: origin.y + yOffsets[Math.floor(index / columns)]! }));
    const placed = ordered.map((node) => ({ ...boundsFor(node), ...positions.get(node.id)! }));
    const collision = placed.some((bounds) => protectedBounds.some((other) => intersects(bounds, other)));
    const width = columnWidths.reduce((sum, value) => sum + value, 0) + spacing * (columns - 1);
    const height = rowHeights.reduce((sum, value) => sum + value, 0) + spacing * (rows - 1);
    return { columns, positions, collision, score: Math.abs(Math.log(Math.max(width, 1) / Math.max(height, 1))) };
  }).filter((candidate) => !candidate.collision).sort((a, b) => a.score - b.score || a.columns - b.columns);
  const positions = candidates[0]?.positions;
  if (!positions) throw new Error('No bounded group grid fits without colliding with protected neighboring blocks. Move the group anchor and retry.');
  return { ...state, nodes: state.nodes.map((node) => positions.has(node.id) ? { ...node, position: positions.get(node.id)! } : node) };
}

export function flipSelectedNodes(state: GraphState): GraphState {
  const selected = state.nodes.filter((node) => node.selected && node.data.role !== 'io'
    && node.data.type !== 'graph-group' && !node.data.hierarchyBoundary);
  if (!selected.length) throw new Error('Select at least one eligible non-I/O block to flip.');
  const ids = new Set(selected.map((node) => node.id));
  return { ...state, nodes: state.nodes.map((node) => ids.has(node.id) ? {
    ...node, data: { ...node.data, ...(node.data.orientation === 'reverse' ? { orientation: undefined } : { orientation: 'reverse' as const }) },
  } : node) };
}

const appendClass = (current: string | undefined, active: boolean) => [current?.replace(/\brouting-trace-(?:active|related)\b/g, '').trim(), active ? 'routing-trace-active' : ''].filter(Boolean).join(' ');

export function traceCable(state: GraphState, edgeId: string, direction: TraceDirection): { nodes: Node<PatchNodeData>[]; edges: Edge[]; nodeIds: string[] } {
  const selected = state.edges.find((edge) => edge.id === edgeId); if (!selected) return { ...state, nodeIds: [] };
  const visitedNodes = new Set<string>(); const visitedEdges = new Set<string>();
  const queue = [direction === 'source' ? selected.source : selected.target];
  while (queue.length) {
    const nodeId = queue.shift()!; if (visitedNodes.has(nodeId)) continue; visitedNodes.add(nodeId);
    for (const edge of state.edges) {
      const follows = direction === 'source' ? edge.target === nodeId : edge.source === nodeId;
      if (!follows) continue; visitedEdges.add(edge.id); queue.push(direction === 'source' ? edge.source : edge.target);
    }
  }
  visitedEdges.add(edgeId); visitedNodes.add(selected.source); visitedNodes.add(selected.target);
  return {
    nodes: state.nodes.map((node) => ({ ...node, className: appendClass(node.className, visitedNodes.has(node.id)) })),
    edges: state.edges.map((edge) => ({ ...edge, className: appendClass(edge.className, visitedEdges.has(edge.id)) })),
    nodeIds: [...visitedNodes],
  };
}
