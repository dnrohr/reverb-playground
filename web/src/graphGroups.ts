import type { Edge, Node } from '@xyflow/react';
import type { GraphState, PatchNodeData, PatchPort, SignalType } from './graph';

export interface GraphGroupSummary { id: string; name: string; collapsed: boolean; nodeIds: string[] }

export function inspectGraphGroups(nodes: Node<PatchNodeData>[]): GraphGroupSummary[] {
  const groups = new Map<string, GraphGroupSummary>();
  for (const node of nodes) {
    const group = node.data.presentationGroup;
    if (!group) continue;
    const summary = groups.get(group.id) ?? { ...group, nodeIds: [] };
    if (summary.name !== group.name || summary.collapsed !== group.collapsed) throw new Error(`Group '${group.id}' has inconsistent member metadata.`);
    summary.nodeIds.push(node.id); groups.set(group.id, summary);
  }
  return [...groups.values()].map((group) => ({ ...group, nodeIds: group.nodeIds.sort() })).sort((a, b) => a.id.localeCompare(b.id));
}

function nextGroupId(nodes: Node<PatchNodeData>[]) {
  const used = new Set(nodes.flatMap((node) => node.data.presentationGroup ? [node.data.presentationGroup.id] : []));
  let index = 1; while (used.has(`group-${index}`)) index += 1; return `group-${index}`;
}

export function createGraphGroup(state: GraphState, name: string): GraphState {
  const selected = state.nodes.filter((node) => node.selected && node.data.role !== 'io');
  if (selected.length < 2) throw new Error('Select at least two non-I/O blocks to create a group.');
  if (selected.some((node) => node.data.presentationGroup)) throw new Error('Nested groups are not supported. Ungroup the selected blocks first.');
  const cleanName = name.trim();
  if (!cleanName || cleanName.length > 64) throw new Error('Group names must contain 1 through 64 characters.');
  const id = nextGroupId(state.nodes); const members = new Set(selected.map((node) => node.id));
  return { ...state, nodes: state.nodes.map((node) => members.has(node.id) ? {
    ...node, selected: false, data: { ...node.data, presentationGroup: { id, name: cleanName, collapsed: false } },
  } : { ...node, selected: false }) };
}

export function setGraphGroupCollapsed(state: GraphState, groupId: string, collapsed: boolean): GraphState {
  let found = false;
  const nodes = state.nodes.map((node) => node.data.presentationGroup?.id === groupId ? (found = true, {
    ...node, data: { ...node.data, presentationGroup: { ...node.data.presentationGroup, collapsed } },
  }) : node);
  if (!found) throw new Error(`Group '${groupId}' does not exist.`);
  return { ...state, nodes };
}

export function renameGraphGroup(state: GraphState, groupId: string, name: string): GraphState {
  const cleanName = name.trim(); if (!cleanName || cleanName.length > 64) throw new Error('Group names must contain 1 through 64 characters.');
  return { ...state, nodes: state.nodes.map((node) => node.data.presentationGroup?.id === groupId ? {
    ...node, data: { ...node.data, presentationGroup: { ...node.data.presentationGroup, name: cleanName } },
  } : node) };
}

export function removeGraphGroup(state: GraphState, groupId: string): GraphState {
  return { ...state, nodes: state.nodes.map((node) => {
    if (node.data.presentationGroup?.id !== groupId) return node;
    const data = { ...node.data }; delete data.presentationGroup; return { ...node, data };
  }) };
}

const boundaryPort = (edge: Edge, direction: 'input' | 'output'): PatchPort => ({
  id: `${direction}-${edge.id}`, direction, signal: (edge.data?.signal === 'control' ? 'control' : 'audio') as SignalType,
});

export function collapseGraphGroups(nodes: Node<PatchNodeData>[], edges: Edge[]): { nodes: Node<PatchNodeData>[]; edges: Edge[] } {
  const groups = inspectGraphGroups(nodes);
  const expanded = new Map(groups.filter((group) => !group.collapsed).flatMap((group) => group.nodeIds.map((id) => [id, group] as const)));
  let projectedNodes = nodes.map((node) => expanded.has(node.id) ? {
    ...node, className: `${node.className ?? ''} graph-group-member`.trim(),
  } : node); let projectedEdges = edges.map((edge) => ({ ...edge }));
  for (const group of groups.filter((candidate) => candidate.collapsed)) {
    const hidden = new Set(group.nodeIds); const members = nodes.filter((node) => hidden.has(node.id));
    const crossingIn = edges.filter((edge) => !hidden.has(edge.source) && hidden.has(edge.target));
    const crossingOut = edges.filter((edge) => hidden.has(edge.source) && !hidden.has(edge.target));
    const x = Math.min(...members.map((node) => node.position.x)); const y = Math.min(...members.map((node) => node.position.y));
    const active = members.some((node) => node.className?.includes('loop-active'));
    const related = members.some((node) => node.className?.includes('loop-related'));
    const compound: Node<PatchNodeData> = {
      id: `group-view-${group.id}`, type: 'patchNode', position: { x, y },
      className: `graph-group-boundary${active ? ' loop-active' : related ? ' loop-related' : ''}`,
      width: 190, height: Math.max(93, Math.max(crossingIn.length, crossingOut.length) * 28 + 54),
      draggable: false, deletable: false,
      data: { label: group.name, type: 'graph-group', role: 'routing', runtimeBound: false, parameters: [],
        ports: [...crossingIn.map((edge) => boundaryPort(edge, 'input')), ...crossingOut.map((edge) => boundaryPort(edge, 'output'))],
        presentationGroup: { id: group.id, name: group.name, collapsed: true }, groupMemberIds: group.nodeIds },
    };
    projectedNodes = [...projectedNodes.filter((node) => !hidden.has(node.id)), compound];
    projectedEdges = projectedEdges.filter((edge) => !(hidden.has(edge.source) && hidden.has(edge.target))).map((edge) => {
      if (!hidden.has(edge.source) && hidden.has(edge.target)) return { ...edge, target: compound.id, targetHandle: `input-${edge.id}` };
      if (hidden.has(edge.source) && !hidden.has(edge.target)) return { ...edge, source: compound.id, sourceHandle: `output-${edge.id}` };
      return edge;
    });
  }
  return { nodes: projectedNodes, edges: projectedEdges };
}
