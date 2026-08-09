import type { Edge, Node } from '@xyflow/react';
import type { GraphState, PatchNodeData } from './graph';

export interface GraphClipboard { nodes: Node<PatchNodeData>[]; edges: Edge[] }

export function copySelectedGraph(state: GraphState): GraphClipboard | null {
  const nodes = state.nodes.filter((node) => node.selected && node.data.role !== 'io').map((node) => structuredClone(node));
  if (!nodes.length) return null;
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
  const nodes = clipboard.nodes.map((source) => {
    const id = uniqueId(source.id, nodeIds);
    replacements.set(source.id, id);
    return {
      ...structuredClone(source), id, position: { x: source.position.x + offset, y: source.position.y + offset },
      selected: true, data: { ...structuredClone(source.data), runtimeBound: false },
    };
  });
  const edges = clipboard.edges.map((source) => ({
    ...structuredClone(source), id: uniqueId(source.id, edgeIds),
    source: replacements.get(source.source)!, target: replacements.get(source.target)!, selected: true,
  }));
  return {
    nodes: [...state.nodes.map((node) => ({ ...node, selected: false })), ...nodes],
    edges: [...state.edges.map((edge) => ({ ...edge, selected: false })), ...edges],
  };
}
