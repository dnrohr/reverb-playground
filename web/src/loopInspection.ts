import type { Edge, Node } from '@xyflow/react';
import type { PatchNodeData } from './graph';

export const feedbackLoopResultLimit = 64;
export const feedbackLoopTransitionBudget = 100_000;

export interface FeedbackElement {
  nodeId: string;
  label: string;
  parameter: string;
  value: number;
  unit: string;
}

export interface FeedbackLoop {
  id: string;
  nodeIds: string[];
  edgeIds: string[];
  nominalDelayMilliseconds: number;
  gainElements: FeedbackElement[];
  filters: FeedbackElement[];
}

export interface FeedbackLoopInspection {
  loops: FeedbackLoop[];
  truncated: boolean;
  exploredTransitions: number;
}

export type FeedbackSelection = { nodeId: string } | { edgeId: string };

export function decorateFeedbackLoops(
  nodes: Node<PatchNodeData>[], edges: Edge[], inspection: FeedbackLoopInspection | null, activeIndex: number,
): { nodes: Node<PatchNodeData>[]; edges: Edge[] } {
  const loop = inspection?.loops[activeIndex];
  const activeNodes = new Set(loop?.nodeIds ?? []); const activeEdges = new Set(loop?.edgeIds ?? []);
  const relatedNodes = new Set(inspection?.loops.flatMap((item) => item.nodeIds) ?? []);
  const relatedEdges = new Set(inspection?.loops.flatMap((item) => item.edgeIds) ?? []);
  const className = (current: string | undefined, active: boolean, related: boolean) => [
    current?.replace(/\bloop-(?:active|related)\b/g, '').trim(), active ? 'loop-active' : related ? 'loop-related' : '',
  ].filter(Boolean).join(' ');
  return {
    nodes: nodes.map((node) => ({ ...node, className: className(node.className, activeNodes.has(node.id), relatedNodes.has(node.id)) })),
    edges: edges.map((edge) => ({ ...edge, className: className(edge.className, activeEdges.has(edge.id), relatedEdges.has(edge.id)) })),
  };
}

function stronglyConnectedComponents(nodeIds: string[], edges: Edge[]): Map<string, Set<string>> {
  const adjacency = new Map(nodeIds.map((id) => [id, [] as string[]]));
  for (const edge of edges) adjacency.get(edge.source)?.push(edge.target);
  for (const targets of adjacency.values()) targets.sort();
  const indices = new Map<string, number>(); const lowLinks = new Map<string, number>();
  const stack: string[] = []; const onStack = new Set<string>(); const components: string[][] = []; let nextIndex = 0;
  const visit = (id: string) => {
    indices.set(id, nextIndex); lowLinks.set(id, nextIndex++); stack.push(id); onStack.add(id);
    for (const target of adjacency.get(id) ?? []) {
      if (!indices.has(target)) { visit(target); lowLinks.set(id, Math.min(lowLinks.get(id)!, lowLinks.get(target)!)); }
      else if (onStack.has(target)) lowLinks.set(id, Math.min(lowLinks.get(id)!, indices.get(target)!));
    }
    if (lowLinks.get(id) !== indices.get(id)) return;
    const component: string[] = [];
    while (stack.length) { const member = stack.pop()!; onStack.delete(member); component.push(member); if (member === id) break; }
    components.push(component);
  };
  for (const id of [...nodeIds].sort()) if (!indices.has(id)) visit(id);
  const result = new Map<string, Set<string>>();
  for (const component of components) { const members = new Set(component); for (const id of component) result.set(id, members); }
  return result;
}

function summarizeLoop(nodeIds: string[], edgeIds: string[], nodesById: Map<string, Node<PatchNodeData>>): FeedbackLoop {
  let nominalDelayMilliseconds = 0; const gainElements: FeedbackElement[] = []; const filters: FeedbackElement[] = [];
  for (const nodeId of nodeIds) {
    const node = nodesById.get(nodeId); if (!node) continue;
    const delay = node.data.parameters.find((parameter) => parameter.id === 'delay' && parameter.unit === 'milliseconds');
    if (delay && (node.data.type === 'delay' || node.data.type === 'allpass')) nominalDelayMilliseconds += delay.value;
    for (const parameter of node.data.parameters) {
      if (parameter.id === 'gain' || parameter.id === 'coefficient') gainElements.push({ nodeId, label: node.data.label, parameter: parameter.id, value: parameter.value, unit: parameter.unit });
      if (node.data.type === 'lowpass' && parameter.id === 'cutoff') filters.push({ nodeId, label: node.data.label, parameter: parameter.id, value: parameter.value, unit: parameter.unit });
    }
  }
  return { id: edgeIds.join('|'), nodeIds, edgeIds, nominalDelayMilliseconds, gainElements, filters };
}

export function inspectFeedbackLoops(
  nodes: Node<PatchNodeData>[], edges: Edge[], selection: FeedbackSelection,
  resultLimit = feedbackLoopResultLimit, transitionBudget = feedbackLoopTransitionBudget,
): FeedbackLoopInspection {
  const nodesById = new Map(nodes.map((node) => [node.id, node]));
  const edgesById = new Map(edges.map((edge) => [edge.id, edge]));
  const selectedEdge = 'edgeId' in selection ? edgesById.get(selection.edgeId) : undefined;
  const selectedNodeId = 'nodeId' in selection ? selection.nodeId : selectedEdge?.source;
  if (!selectedNodeId || !nodesById.has(selectedNodeId)) return { loops: [], truncated: false, exploredTransitions: 0 };

  const components = stronglyConnectedComponents([...nodesById.keys()], edges);
  const component = components.get(selectedNodeId) ?? new Set<string>();
  const selfLoop = edges.some((edge) => edge.source === selectedNodeId && edge.target === selectedNodeId);
  if (component.size < 2 && !selfLoop) return { loops: [], truncated: false, exploredTransitions: 0 };
  if (selectedEdge && (!component.has(selectedEdge.target) || !component.has(selectedEdge.source)))
    return { loops: [], truncated: false, exploredTransitions: 0 };

  const outgoing = new Map<string, Edge[]>();
  for (const edge of edges) if (component.has(edge.source) && component.has(edge.target)) {
    const list = outgoing.get(edge.source) ?? []; list.push(edge); outgoing.set(edge.source, list);
  }
  for (const list of outgoing.values()) list.sort((left, right) => left.id.localeCompare(right.id));
  const seeds = selectedEdge ? [selectedEdge] : (outgoing.get(selectedNodeId) ?? []);
  const loops: FeedbackLoop[] = []; const seen = new Set<string>(); let exploredTransitions = 0; let truncated = false;

  for (const seed of seeds) {
    if (loops.length >= resultLimit || exploredTransitions >= transitionBudget) { truncated = true; break; }
    if (seed.target === seed.source) {
      const summary = summarizeLoop([seed.source], [seed.id], nodesById); if (!seen.has(summary.id)) { seen.add(summary.id); loops.push(summary); }
      continue;
    }
    const visited = new Set<string>([seed.source, seed.target]);
    const pathNodes = [seed.source, seed.target]; const pathEdges = [seed.id];
    const search = (nodeId: string) => {
      for (const edge of outgoing.get(nodeId) ?? []) {
        if (exploredTransitions >= transitionBudget) { truncated = true; return; }
        exploredTransitions++;
        if (edge.target === seed.source) {
          const edgeIds = [...pathEdges, edge.id]; const id = edgeIds.join('|');
          if (!seen.has(id)) { seen.add(id); loops.push(summarizeLoop([...pathNodes], edgeIds, nodesById)); }
          if (loops.length >= resultLimit) { truncated = true; return; }
        } else if (!visited.has(edge.target)) {
          visited.add(edge.target); pathNodes.push(edge.target); pathEdges.push(edge.id);
          search(edge.target); pathEdges.pop(); pathNodes.pop(); visited.delete(edge.target);
          if (truncated) return;
        }
      }
    };
    search(seed.target);
    if (truncated) break;
  }
  loops.sort((left, right) => left.id.localeCompare(right.id));
  return { loops, truncated, exploredTransitions };
}
