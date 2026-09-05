import type { ImpulseCaptureResult } from './impulseCapture';

export const outputGainMaximum = 100;
export const normalizedOutputPeak = 10 ** (-0.1 / 20);

export interface OutputNormalization {
  peak: number;
  gain: number;
  limited: boolean;
}

export function outputNormalization(capture: Pick<ImpulseCaptureResult, 'left' | 'right'>,
  currentGain: number): OutputNormalization | null {
  let peak = 0;
  for (const sample of capture.left) peak = Math.max(peak, Math.abs(sample));
  for (const sample of capture.right) peak = Math.max(peak, Math.abs(sample));
  if (!Number.isFinite(peak) || peak <= 1e-12 || !Number.isFinite(currentGain) || currentGain < 0) return null;
  const requested = currentGain * normalizedOutputPeak / peak;
  const gain = Math.min(outputGainMaximum, Math.max(0, requested));
  return { peak, gain, limited: requested > outputGainMaximum };
}
