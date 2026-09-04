import type { Edge, Node } from '@xyflow/react';
import type { GraphState, PatchNodeData } from './graph';

export interface GraphClipboard { nodes: Node<PatchNodeData>[]; edges: Edge[] }

export function copySelectedGraph(state: GraphState): GraphClipboard | null {
  const nodes = state.nodes.filter((node) => node.selected && node.data.role !== 'io').map((node) => structuredClone(node));
  if (!nodes.length) return null;
  const selectedIds = new Set(nodes.map((node) => node.id));
  const groupMembers = new Map<string, string[]>();
  for (const node of state.nodes) if (node.data.presentationGroup) {
    const members = groupMembers.get(node.data.presentationGroup.id) ?? [];
    members.push(node.id); groupMembers.set(node.data.presentationGroup.id, members);
  }
  for (const node of nodes) {
    const group = node.data.presentationGroup;
    if (group && !groupMembers.get(group.id)?.every((id) => selectedIds.has(id))) delete node.data.presentationGroup;
    const instance = node.data.subpatchInstance;
    if (instance && !instance.memberNodeIds.every((id) => selectedIds.has(id))) {
      delete node.data.subpatchInstance;
      node.className = node.className?.replace(/\s*subpatch-member\b/g, '');
    }
    const hierarchy = node.data.hierarchyPresentation;
    if (hierarchy && !hierarchy.memberNodeIds.every((id) => selectedIds.has(id))) delete node.data.hierarchyPresentation;
  }
  const ids = new Set(nodes.map((node) => node.id));
  const edges = state.edges.filter((edge) => ids.has(edge.source) && ids.has(edge.target)).map((edge) => structuredClone(edge));
  return { nodes, edges };
}

function uniqueId(stem: string, used: Set<string>): string {
  let suffix = 1;
  let candidate = `${stem}-copy`;
  while (used.has(candidate)) candidate = `${stem}-copy-${++suffix}`;
  used.add(candidate);
  return candidate;
}

export function pasteGraph(state: GraphState, clipboard: GraphClipboard, offset = 40): GraphState {
  const nodeIds = new Set(state.nodes.map((node) => node.id));
  const edgeIds = new Set(state.edges.map((edge) => edge.id));
  const replacements = new Map<string, string>();
  const groupIds = new Set(state.nodes.flatMap((node) => node.data.presentationGroup ? [node.data.presentationGroup.id] : []));
  const groupReplacements = new Map<string, string>();
  const instanceIds = new Set(state.nodes.flatMap((node) => node.data.subpatchInstance ? [node.data.subpatchInstance.id] : []));
  const instanceReplacements = new Map<string, string>();
  const hierarchyIds = new Set(state.nodes.flatMap((node) => node.data.hierarchyPresentation ? [node.data.hierarchyPresentation.id] : []));
  const hierarchyReplacements = new Map<string, string>();
  const nodes = clipboard.nodes.map((source) => {
    const id = uniqueId(source.id, nodeIds);
    replacements.set(source.id, id);
    const data = { ...structuredClone(source.data), runtimeBound: false };
    if (data.presentationGroup) {
      let groupId = groupReplacements.get(data.presentationGroup.id);
      if (!groupId) { groupId = uniqueId(data.presentationGroup.id, groupIds); groupReplacements.set(data.presentationGroup.id, groupId); }
      data.presentationGroup = { ...data.presentationGroup, id: groupId };
    }
    return {
      ...structuredClone(source), id, position: { x: source.position.x + offset, y: source.position.y + offset },
      selected: true, data,
    };
  });
  for (const node of nodes) if (node.data.subpatchInstance) {
    const source = node.data.subpatchInstance;
    let instanceId = instanceReplacements.get(source.id);
    if (!instanceId) { instanceId = uniqueId(source.id, instanceIds); instanceReplacements.set(source.id, instanceId); }
    node.data.subpatchInstance = { ...source, id: instanceId,
      memberNodeIds: source.memberNodeIds.map((id) => replacements.get(id)!).filter(Boolean),
      ports: source.ports.map((port) => ({ ...port, nodeId: replacements.get(port.nodeId)! })).filter((port) => port.nodeId),
    };
  }
  for (const node of nodes) if (node.data.hierarchyPresentation
    && !hierarchyReplacements.has(node.data.hierarchyPresentation.id)) {
    hierarchyReplacements.set(node.data.hierarchyPresentation.id,
      uniqueId(node.data.hierarchyPresentation.id, hierarchyIds));
  }
  for (const node of nodes) if (node.data.hierarchyPresentation) {
    const source = node.data.hierarchyPresentation;
    const hierarchyId = hierarchyReplacements.get(source.id)!;
    node.data.hierarchyPresentation = {
      ...source,
      id: hierarchyId,
      name: `${source.name} copy`.slice(0, 64),
      position: { x: source.position.x + offset, y: source.position.y + offset },
      memberNodeIds: source.memberNodeIds.map((id) => replacements.get(id)!).filter(Boolean),
      ports: source.ports.map((port) => ({ ...port, targets: port.targets.map((target) => ({
        ...target, nodeId: replacements.get(target.nodeId)!,
      })).filter((target) => target.nodeId) })),
      ...(source.parentId ? { parentId: hierarchyReplacements.get(source.parentId) } : {}),
    };
  }
  const edges = clipboard.edges.map((source) => {
    const copy = structuredClone(source);
    const layout = copy.data?.layout as { waypoints?: Array<{ x: number; y: number }> } | undefined;
    if (layout?.waypoints) layout.waypoints = layout.waypoints.map((point) => ({ x: point.x + offset, y: point.y + offset }));
    return { ...copy, id: uniqueId(source.id, edgeIds), source: replacements.get(source.source)!, target: replacements.get(source.target)!, selected: true };
  });
  return {
    nodes: [...state.nodes.map((node) => ({ ...node, selected: false })), ...nodes],
    edges: [...state.edges.map((edge) => ({ ...edge, selected: false })), ...edges],
  };
}
