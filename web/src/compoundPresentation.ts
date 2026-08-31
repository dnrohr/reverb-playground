import type { Edge, Node } from '@xyflow/react';
import type { PatchNodeData, PatchPort } from './graph';

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

const projectionTokens = ['loop-active', 'loop-related', 'safety-loop-active', 'routing-trace-active', 'split-focus-active', 'split-focus-related', 'reverse-cosmic-focus-active', 'reverse-cosmic-focus-related'];
function compoundClass(members: Node<PatchNodeData>[]) {
  const classes = new Set<string>(['compound-summary-boundary']); let energy = 0;
  for (const member of members) {
    for (const token of projectionTokens) if (member.className?.includes(token)) classes.add(token);
    const level = /\benergy-level-(\d+)\b/.exec(member.className ?? ''); if (level) energy = Math.max(energy, Number(level[1]));
  }
  classes.add(`energy-level-${energy}`); return [...classes].join(' ');
}
const boundaryPort = (edge: Edge, direction: 'input' | 'output'): PatchPort => ({ id: `${direction}-${edge.id}`, direction, signal: edge.data?.signal === 'control' ? 'control' : 'audio' });

export function projectCompoundPresentation(nodes: Node<PatchNodeData>[], edges: Edge[], presentation: CompoundPresentation, collapsed: boolean): CompoundProjection {
  if (!collapsed || !presentation.canCollapse) return { nodes, edges };
  const hidden = new Set(presentation.memberNodeIds); const members = nodes.filter((node) => hidden.has(node.id));
  if (members.length !== hidden.size) return { nodes, edges };
  const crossingIn = edges.filter((edge) => !hidden.has(edge.source) && hidden.has(edge.target));
  const crossingOut = edges.filter((edge) => hidden.has(edge.source) && !hidden.has(edge.target));
  const internalConnectionCount = edges.filter((edge) => hidden.has(edge.source) && hidden.has(edge.target)).length;
  const compound: Node<PatchNodeData> = {
    id: `compound-view-${presentation.id}`, type: 'patchNode', position: { x: Math.min(...members.map((node) => node.position.x)), y: Math.min(...members.map((node) => node.position.y)) },
    className: compoundClass(members), draggable: false, deletable: false, width: 210, height: Math.max(112, Math.max(crossingIn.length, crossingOut.length) * 28 + 66),
    data: { label: presentation.label, type: 'compound-summary', role: 'routing', runtimeBound: false, parameters: [],
      ports: [...crossingIn.map((edge) => boundaryPort(edge, 'input')), ...crossingOut.map((edge) => boundaryPort(edge, 'output'))],
      compoundPresentation: { id: presentation.id, kind: presentation.kind, memberNodeIds: presentation.memberNodeIds,
        authoritativeNodeCount: presentation.memberNodeIds.length, internalConnectionCount, summary: presentation.summary, learn: presentation.learn },
    },
  };
  return { nodes: [...nodes.filter((node) => !hidden.has(node.id)), compound], edges: edges.filter((edge) => !(hidden.has(edge.source) && hidden.has(edge.target))).map((edge) => {
    if (!hidden.has(edge.source) && hidden.has(edge.target)) return { ...edge, target: compound.id, targetHandle: `input-${edge.id}` };
    if (hidden.has(edge.source) && !hidden.has(edge.target)) return { ...edge, source: compound.id, sourceHandle: `output-${edge.id}` }; return edge;
  }) };
}

export function projectCompoundPresentations(nodes: Node<PatchNodeData>[], edges: Edge[], presentations: CompoundPresentation[], collapsedIds: ReadonlySet<string>): CompoundProjection {
  return presentations.reduce((graph, presentation) => projectCompoundPresentation(graph.nodes, graph.edges, presentation, collapsedIds.has(presentation.id)), { nodes, edges });
}

export const compoundMembers = (node: Node<PatchNodeData> | null) => new Set(node?.data.compoundPresentation?.memberNodeIds ?? []);
