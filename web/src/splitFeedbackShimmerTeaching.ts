import type { Edge, Node } from '@xyflow/react';
import type { PatchNodeData } from './graph';

export type SplitShimmerLoopFocus = 'normal' | 'shifted' | 'shared';
export type SplitShimmerLoopKind = 'NORMAL RETURN' | 'SHIFTED RETURN' | 'MIXED / SHARED';

export const splitFeedbackShimmerTeaching = {
  title: 'FEEDBACK SHIMMER',
  method: 'REPEATED +12 CIRCULATION',
  normal: 'tank → normal gain → 67 ms return → recombine',
  shifted: 'tank → high-pass → pitch +12 → low-pass → shifted gain → 83 ms return',
  shared: 'recombine → shared tank → 149 ms delay → damping → split',
  circulation: [
    { pass: 'INPUT', frequency: '400 Hz', note: 'source enters shared tank' },
    { pass: 'PASS 1', frequency: '800 Hz', note: '+12 region / late 0 cents' },
    { pass: 'PASS 2', frequency: '1600 Hz', note: '+24 region / late 0 cents' },
  ],
  evidence: '+24 grows 40.28 dB from 1.0 s to 2.5 s; 48.32 dB stronger relative to +12 than the parallel reference.',
  boundary: 'Measured project-authored behavior, not a reconstruction of a proprietary shimmer algorithm. Higher passes are not required or implied.',
} as const;

const sharedNodes = new Set([
  'feedback-recombine', 'tank-entry', 'tank-diffusion-a', 'tank-delay',
  'tank-diffusion-b', 'tank-damping',
]);
const normalOnlyNodes = new Set(['normal-feedback', 'normal-feedback-delay']);
const shiftedOnlyNodes = new Set([
  'shifted-highpass-lowpass', 'shifted-highpass-invert', 'shifted-highpass-sum',
  'shifted-pitch', 'shifted-damping', 'shifted-feedback', 'shifted-feedback-delay',
]);
const sharedEdges = new Set([
  'feedback-tank', 'tank-diffusion-a', 'tank-delay', 'tank-diffusion-b', 'tank-damping',
]);
const normalOnlyEdges = new Set(['normal-feedback', 'normal-feedback-delay', 'normal-recombine']);
const shiftedOnlyEdges = new Set([
  'highpass-direct', 'highpass-lowpass', 'highpass-invert', 'highpass-subtract',
  'shifted-pitch', 'shifted-damping', 'shifted-feedback', 'shifted-feedback-delay', 'shifted-recombine',
]);

const focusNodes = (focus: SplitShimmerLoopFocus) => focus === 'shared'
  ? sharedNodes : new Set([...sharedNodes, ...(focus === 'normal' ? normalOnlyNodes : shiftedOnlyNodes)]);
const focusEdges = (focus: SplitShimmerLoopFocus) => focus === 'shared'
  ? sharedEdges : new Set([...sharedEdges, ...(focus === 'normal' ? normalOnlyEdges : shiftedOnlyEdges)]);

export function splitShimmerLoopKind(nodeIds: readonly string[]): SplitShimmerLoopKind {
  const normal = nodeIds.some((id) => normalOnlyNodes.has(id));
  const shifted = nodeIds.some((id) => shiftedOnlyNodes.has(id));
  if (normal && !shifted) return 'NORMAL RETURN';
  if (shifted && !normal) return 'SHIFTED RETURN';
  return 'MIXED / SHARED';
}

export function decorateSplitShimmerFocus(
  nodes: Node<PatchNodeData>[], edges: Edge[], focus: SplitShimmerLoopFocus,
): { nodes: Node<PatchNodeData>[]; edges: Edge[] } {
  const activeNodes = focusNodes(focus); const activeEdges = focusEdges(focus);
  const allNodes = new Set([...sharedNodes, ...normalOnlyNodes, ...shiftedOnlyNodes]);
  const allEdges = new Set([...sharedEdges, ...normalOnlyEdges, ...shiftedOnlyEdges]);
  const className = (current: string | undefined, active: boolean, related: boolean) => [
    current?.replace(/\bsplit-focus-(?:active|related)\b/g, '').trim(),
    active ? 'split-focus-active' : related ? 'split-focus-related' : '',
  ].filter(Boolean).join(' ');
  return {
    nodes: nodes.map((node) => ({ ...node, className: className(
      node.className, activeNodes.has(node.id), allNodes.has(node.id),
    ) })),
    edges: edges.map((edge) => ({ ...edge, className: className(
      edge.className, activeEdges.has(edge.id), allEdges.has(edge.id),
    ) })),
  };
}
