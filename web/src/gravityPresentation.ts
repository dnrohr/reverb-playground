import type { MacroInspection } from './macroInspection';

export type GravityState = 'INVERSE' | 'BLOOM' | 'FORWARD';

export interface GravityEnvelopePrediction {
  state: GravityState;
  peakPosition: number;
  path: string;
  description: string;
}

const clampGravity = (value: number) => Math.min(1, Math.max(-1, Number.isFinite(value) ? value : 0));

export function gravityState(value: number): GravityState {
  const bounded = clampGravity(value);
  if (Math.abs(bounded) < 0.0005) return 'BLOOM';
  return bounded < 0 ? 'INVERSE' : 'FORWARD';
}

export function predictGravityEnvelope(value: number, pointCount = 49): GravityEnvelopePrediction {
  const bounded = clampGravity(value);
  const peakPosition = 0.48 - 0.35 * bounded;
  const count = Math.max(3, pointCount);
  const points = Array.from({ length: count }, (_, index) => {
    const time = index / (count - 1);
    const energy = time <= peakPosition
      ? 0.06 + 0.94 * Math.pow(time / peakPosition, 1.65)
      : Math.exp(-3.2 * (time - peakPosition) / (1 - peakPosition));
    return `${(time * 100).toFixed(2)},${(38 - energy * 32).toFixed(2)}`;
  });
  const state = gravityState(bounded);
  return {
    state,
    peakPosition,
    path: `M ${points.join(' L ')}`,
    description: `${state} design prediction; energy peak at ${Math.round(peakPosition * 100)}% of the displayed horizon. Not measured audio.`,
  };
}

export function gravityFocusNodeIds(inspection: MacroInspection): string[] {
  return [...new Set(inspection.nodeIds)];
}
