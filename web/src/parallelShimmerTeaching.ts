export type ParallelShimmerBranch = 'tank' | 'normal' | 'octave' | 'recombine' | 'other';

export const parallelShimmerTeaching = {
  title: 'PARALLEL SHIMMER',
  method: 'ONE PITCH PASS',
  normal: 'tank → 360.01 ms alignment → level',
  octave: 'high-pass → pitch +12 st → damping → diffusion',
  contrast: 'Both paths recombine after the tank. Unlike classic feedback shimmer, the octave path cannot return through Pitch Shift—so it adds a stable halo, not a +24 / +36 octave staircase.',
} as const;

export function parallelShimmerBranch(nodeId?: string): ParallelShimmerBranch {
  if (!nodeId) return 'other';
  if (nodeId === 'parallel-wet-sum' || nodeId === 'wet-balance'
      || nodeId.endsWith('-extraction') || nodeId === 'output') return 'recombine';
  if (nodeId.startsWith('normal-')) return 'normal';
  if (nodeId.startsWith('shimmer-')) return 'octave';
  if (nodeId.startsWith('tank-') || nodeId === 'reverb-decay' || nodeId === 'feedback-delay') return 'tank';
  return 'other';
}
