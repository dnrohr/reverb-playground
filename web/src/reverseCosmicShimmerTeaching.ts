import type { Edge, Node } from '@xyflow/react';
import type { PatchNodeData } from './graph';

export type ReverseCosmicFocus = 'rise' | 'grains' | 'feedback' | 'motion';

export const reverseCosmicShimmerTeaching = {
  title: 'REVERSE COSMIC SHIMMER',
  method: 'CAUSAL RISE + REVERSE GRAINS',
  rise: 'Three delayed taps rise into two diffusion stages before entering the shared tank. The first wet sample is measured near 261 ms and the response peaks near 721 ms.',
  grains: 'The high-passed tank signal enters two +12 st pitch blocks. Each block reads backward only inside an already buffered grain; neither can emit audio before its input and latency.',
  feedback: 'A dark normal return and two separately delayed, low-passed reverse-octave returns recombine before the tank. Their conservative combined gain keeps the four-second fixture finite and decaying.',
  motion: 'Independent slow LFOs move tank and unequal output allpasses. The paths stay related but non-identical; measured correlation is 0.17–0.48 with compatible mono energy.',
  evidence: 'Across 44.1 / 48 / 96 kHz: late-rise energy is about +6.3 dB, octave-to-fundamental balance grows more than 23 dB, and final energy falls more than 34 dB.',
  boundary: 'Project-authored behavioral synthesis. This is not a proprietary algorithm reconstruction and not true sample-order reverse reverb: only samples inside causal buffered grains run backward.',
} as const;

const groups: Record<ReverseCosmicFocus, { nodes: readonly string[]; edges: readonly string[] }> = {
  rise: {
    nodes: ['input', 'input-left-half', 'input-right-half', 'input-mono', 'rise-delay-early', 'rise-gain-early',
      'rise-delay-middle', 'rise-gain-middle', 'rise-delay-late', 'rise-gain-late', 'rise-sum-early',
      'rise-sum-complete', 'rise-diffusion-a', 'rise-diffusion-b', 'tank-entry'],
    edges: ['input-left', 'input-right', 'input-left-sum', 'input-right-sum', 'rise-early-delay', 'rise-early-gain',
      'rise-middle-delay', 'rise-middle-gain', 'rise-late-delay', 'rise-late-gain', 'rise-early-a', 'rise-early-b',
      'rise-complete-a', 'rise-complete-b', 'rise-diffuse-a', 'rise-diffuse-b', 'rise-tank'],
  },
  grains: {
    nodes: ['tank-damping', 'shift-highpass-lowpass', 'shift-highpass-invert', 'shift-highpass', 'reverse-pitch-left',
      'reverse-pitch-right', 'shift-damping-left', 'shift-damping-right', 'shimmer-output-left', 'shimmer-output-right',
      'left-output-sum', 'right-output-sum', 'output'],
    edges: ['highpass-direct', 'highpass-lowpass', 'highpass-invert', 'highpass-subtract', 'pitch-left', 'pitch-right',
      'pitch-damp-left', 'pitch-damp-right', 'shimmer-output-left', 'shimmer-output-right', 'left-output-shimmer',
      'right-output-shimmer', 'left-output', 'right-output'],
  },
  feedback: {
    nodes: ['tank-entry', 'tank-diffusion-a', 'tank-delay', 'tank-diffusion-b', 'tank-damping', 'normal-feedback',
      'normal-return-delay', 'shift-highpass-lowpass', 'shift-highpass-invert', 'shift-highpass', 'reverse-pitch-left',
      'reverse-pitch-right', 'shift-damping-left', 'shift-damping-right', 'shimmer-feedback-left',
      'shimmer-feedback-right', 'shimmer-return-left', 'shimmer-return-right', 'shimmer-return-sum', 'feedback-recombine'],
    edges: ['feedback-tank', 'tank-ap-a', 'tank-delay', 'tank-ap-b', 'tank-damping', 'normal-feedback', 'normal-delay',
      'normal-recombine', 'highpass-direct', 'highpass-lowpass', 'highpass-invert', 'highpass-subtract', 'pitch-left',
      'pitch-right', 'pitch-damp-left', 'pitch-damp-right', 'shimmer-gain-left', 'shimmer-gain-right',
      'shimmer-delay-left', 'shimmer-delay-right', 'shimmer-return-a', 'shimmer-return-b', 'shimmer-recombine'],
  },
  motion: {
    nodes: ['motion-left', 'motion-right', 'tank-diffusion-a', 'tank-diffusion-b', 'left-extraction',
      'right-extraction', 'right-extraction-b', 'wet-level', 'left-output-sum', 'right-output-sum', 'output'],
    edges: ['motion-tank-a', 'motion-tank-b', 'motion-output-left', 'motion-output-right', 'motion-output-right-b',
      'wet', 'wet-left', 'wet-right', 'wet-right-b', 'left-output-normal', 'right-output-normal', 'left-output', 'right-output'],
  },
};

export function decorateReverseCosmicFocus(nodes: Node<PatchNodeData>[], edges: Edge[], focus: ReverseCosmicFocus) {
  const activeNodes = new Set(groups[focus].nodes); const activeEdges = new Set(groups[focus].edges);
  const allNodes = new Set(Object.values(groups).flatMap((group) => [...group.nodes]));
  const allEdges = new Set(Object.values(groups).flatMap((group) => [...group.edges]));
  const className = (current: string | undefined, active: boolean, related: boolean) => [
    current?.replace(/\breverse-cosmic-focus-(?:active|related)\b/g, '').trim(),
    active ? 'reverse-cosmic-focus-active' : related ? 'reverse-cosmic-focus-related' : '',
  ].filter(Boolean).join(' ');
  return {
    nodes: nodes.map((node) => ({ ...node, className: className(node.className, activeNodes.has(node.id), allNodes.has(node.id)) })),
    edges: edges.map((edge) => ({ ...edge, className: className(edge.className, activeEdges.has(edge.id), allEdges.has(edge.id)) })),
  };
}
