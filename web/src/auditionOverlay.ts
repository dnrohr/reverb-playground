import type { Edge, Node } from '@xyflow/react';
import type { GraphState, PatchNodeData } from './graph';

export type AuditionOverlay =
  | { kind: 'mute' | 'isolate'; target: 'node' | 'edge'; id: string }
  | { kind: 'bypass'; target: 'node'; id: string };

export interface AuditionOverlayResult { accepted: boolean; graph: GraphState; message: string; latencyChanged: boolean }
const signal = (edge: Edge) => edge.data?.signal === 'control' ? 'control' : 'audio';

function reachable(start: string[], edges: Edge[], reverse = false) {
  const found = new Set(start); let changed = true;
  while (changed) { changed = false; for (const edge of edges) {
    const from = reverse ? edge.target : edge.source; const to = reverse ? edge.source : edge.target;
    if (found.has(from) && !found.has(to)) { found.add(to); changed = true; }
  } }
  return found;
}

function hasAlgebraicCycle(nodes: Node<PatchNodeData>[], edges: Edge[]) {
  const delayNodes = new Set(nodes.filter((node) => node.data.type === 'delay').map((node) => node.id));
  const adjacency = new Map<string, string[]>();
  for (const edge of edges) if (!delayNodes.has(edge.source) && !delayNodes.has(edge.target)) adjacency.set(edge.source, [...(adjacency.get(edge.source) ?? []), edge.target]);
  const visiting = new Set<string>(); const visited = new Set<string>();
  const visit = (id: string): boolean => { if (visiting.has(id)) return true; if (visited.has(id)) return false; visiting.add(id);
    for (const next of adjacency.get(id) ?? []) if (visit(next)) return true; visiting.delete(id); visited.add(id); return false; };
  return nodes.some((node) => !delayNodes.has(node.id) && visit(node.id));
}

export function applyAuditionOverlay(state: GraphState, overlay: AuditionOverlay | null): AuditionOverlayResult {
  if (!overlay) return { accepted: true, graph: state, message: 'No temporary audition overlay.', latencyChanged: false };
  if (overlay.target === 'edge') {
    const selected = state.edges.find((edge) => edge.id === overlay.id); if (!selected) return { accepted: false, graph: state, message: 'Selected cable no longer exists.', latencyChanged: false };
    if (signal(selected) !== 'audio') return { accepted: false, graph: state, message: 'Temporary audio audition applies only to mono audio cables.', latencyChanged: false };
    if (overlay.kind === 'mute') return { accepted: true, graph: { nodes: state.nodes, edges: state.edges.filter((edge) => edge.id !== selected.id) }, message: `Muted cable ${selected.id}.`, latencyChanged: false };
    const upstream = reachable([selected.source], state.edges, true); const downstream = reachable([selected.target], state.edges);
    const keepEdges = state.edges.filter((edge) => edge.id === selected.id
      || (upstream.has(edge.source) && upstream.has(edge.target))
      || (downstream.has(edge.source) && downstream.has(edge.target)));
    return { accepted: true, graph: { nodes: state.nodes, edges: keepEdges }, message: `Isolated branch through ${selected.id}.`, latencyChanged: false };
  }
  const node = state.nodes.find((candidate) => candidate.id === overlay.id); if (!node) return { accepted: false, graph: state, message: 'Selected block no longer exists.', latencyChanged: false };
  if (node.data.role === 'io') return { accepted: false, graph: state, message: 'Required Stereo I/O cannot be audition-muted, isolated, or bypassed.', latencyChanged: false };
  const hasAudioOutput = node.data.ports.some((port) => port.signal === 'audio' && port.direction === 'output');
  if (!hasAudioOutput) return { accepted: false, graph: state, message: 'Selected block has no mono audio output to audition.', latencyChanged: false };
  if (overlay.kind === 'mute') return { accepted: true, graph: { nodes: state.nodes, edges: state.edges.filter((edge) => edge.source !== node.id || signal(edge) !== 'audio') }, message: `Muted audio output of ${node.id}.`, latencyChanged: false };
  if (overlay.kind === 'isolate') {
    const upstream = reachable([node.id], state.edges, true); const downstream = reachable([node.id], state.edges);
    return { accepted: true, graph: { nodes: state.nodes, edges: state.edges.filter((edge) => (upstream.has(edge.source) && upstream.has(edge.target))
      || (downstream.has(edge.source) && downstream.has(edge.target))) }, message: `Isolated paths through ${node.id}.`, latencyChanged: false };
  }
  const incoming = state.edges.filter((edge) => edge.target === node.id && signal(edge) === 'audio'); const outgoing = state.edges.filter((edge) => edge.source === node.id && signal(edge) === 'audio');
  if (incoming.length !== 1 || !outgoing.length) return { accepted: false, graph: state, message: 'Bypass requires exactly one connected audio input and at least one audio output.', latencyChanged: false };
  const removed = new Set(state.edges.filter((edge) => edge.source === node.id || edge.target === node.id).map((edge) => edge.id)); const source = incoming[0]!;
  const replacements = outgoing.map((edge, index) => ({ ...edge, id: `audition-bypass-${node.id}-${index + 1}`, source: source.source, sourceHandle: source.sourceHandle,
    className: `${edge.className ?? ''} audition-overlay-edge`, data: { ...edge.data, auditionOverlay: true } }));
  const instanceId = node.data.subpatchInstance?.id; const groupId = node.data.presentationGroup?.id;
  const nextNodes = state.nodes.filter((candidate) => candidate.id !== node.id).map((candidate) => {
    if (instanceId && candidate.data.subpatchInstance?.id === instanceId) return { ...candidate,
      className: candidate.className?.replace(/\s*subpatch-member\b/g, ''), data: { ...candidate.data, subpatchInstance: undefined } };
    if (groupId && candidate.data.presentationGroup?.id === groupId) return { ...candidate, data: { ...candidate.data, presentationGroup: undefined } };
    return candidate;
  });
  const next = { nodes: nextNodes, edges: [...state.edges.filter((edge) => !removed.has(edge.id)), ...replacements] };
  if (hasAlgebraicCycle(next.nodes, next.edges)) return { accepted: false, graph: state, message: 'Bypass refused: removing this block would create a zero-delay algebraic cycle.', latencyChanged: false };
  return { accepted: true, graph: next, message: `Bypassed ${node.id}; compiled latency may change.`, latencyChanged: true };
}

export function decorateAuditionOverlay(state: GraphState, overlay: AuditionOverlay | null): GraphState {
  if (!overlay) return state; const token = `audition-${overlay.kind}`;
  return { nodes: state.nodes.map((node) => overlay.target === 'node' && node.id === overlay.id ? { ...node, className: `${node.className ?? ''} ${token}` } : node),
    edges: state.edges.map((edge) => overlay.target === 'edge' && edge.id === overlay.id ? { ...edge, className: `${edge.className ?? ''} ${token}` } : edge) };
}

export const auditionOverlayLabel = (overlay: AuditionOverlay | null) => !overlay ? null : `${overlay.kind.toUpperCase()} ${overlay.target.toUpperCase()} ${overlay.id}`;

export function toggleAuditionOverlay(current: AuditionOverlay | null, requested: AuditionOverlay): AuditionOverlay | null {
  return current?.kind === requested.kind && current.target === requested.target && current.id === requested.id ? null : requested;
}
