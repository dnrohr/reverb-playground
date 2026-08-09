import type { ImpulseCaptureResult } from './impulseCapture';

export type Rt60Refusal = 'no-energy' | 'tail-noise' | 'insufficient-range' | 'non-decaying';

export interface ResponseAnalysis {
  sampleRate: number;
  frameCount: number;
  peakLeft: number;
  peakRight: number;
  onsetFrame: number | null;
  lastActiveFrame: number | null;
  decayDb: Float64Array;
  rt60Seconds: number | null;
  rt60Refusal: Rt60Refusal | null;
}

export interface WaveformBucket {
  frame: number;
  minimum: number;
  maximum: number;
}

export interface DecayPoint { frame: number; decibels: number; }

export interface FrameWindow { start: number; end: number; }

export function frameWindow(frameCount: number, zoom: number, pan: number): FrameWindow {
  const boundedCount = Math.max(1, Math.floor(frameCount));
  const boundedZoom = Math.max(1, Math.min(256, zoom));
  const visible = Math.max(1, Math.ceil(boundedCount / boundedZoom));
  const maximumStart = Math.max(0, boundedCount - visible);
  const start = Math.round(maximumStart * Math.max(0, Math.min(1, pan)));
  return { start, end: Math.min(boundedCount, start + visible) };
}

export function waveformBuckets(samples: readonly number[], window: FrameWindow, bucketCount: number): WaveformBucket[] {
  const start = Math.max(0, Math.min(samples.length, Math.floor(window.start)));
  const end = Math.max(start, Math.min(samples.length, Math.ceil(window.end)));
  if (end <= start || bucketCount <= 0) return [];
  const count = Math.min(Math.floor(bucketCount), end - start);
  const result: WaveformBucket[] = [];
  for (let bucket = 0; bucket < count; bucket += 1) {
    const first = start + Math.floor(bucket * (end - start) / count);
    const next = start + Math.max(1, Math.floor((bucket + 1) * (end - start) / count));
    let minimum = Number.POSITIVE_INFINITY;
    let maximum = Number.NEGATIVE_INFINITY;
    for (let frame = first; frame < Math.min(end, next); frame += 1) {
      minimum = Math.min(minimum, samples[frame] ?? 0);
      maximum = Math.max(maximum, samples[frame] ?? 0);
    }
    result.push({ frame: first, minimum, maximum });
  }
  return result;
}

export function decayPoints(decayDb: Float64Array, window: FrameWindow, pointCount: number): DecayPoint[] {
  const start = Math.max(0, Math.min(decayDb.length, window.start));
  const end = Math.max(start, Math.min(decayDb.length, window.end));
  if (end <= start || pointCount <= 0) return [];
  const count = Math.min(pointCount, end - start);
  const result: DecayPoint[] = [];
  for (let point = 0; point < count; point += 1) {
    const frame = Math.min(end - 1, start + Math.floor(point * (end - start - 1) / Math.max(1, count - 1)));
    result.push({ frame, decibels: Number.isFinite(decayDb[frame]) ? decayDb[frame]! : -90 });
  }
  return result;
}

export function analyseResponse(capture: Pick<ImpulseCaptureResult, 'left' | 'right' | 'sampleRate' | 'frameCount'>, activeThreshold = 1e-6): ResponseAnalysis {
  if (capture.frameCount <= 0 || capture.left.length !== capture.frameCount || capture.right.length !== capture.frameCount || capture.sampleRate <= 0)
    throw new Error('Response analysis requires equal non-empty stereo channels and a positive sample rate');
  const decayDb = new Float64Array(capture.frameCount);
  let accumulatedEnergy = 0;
  let peakLeft = 0;
  let peakRight = 0;
  let onsetFrame: number | null = null;
  let lastActiveFrame: number | null = null;
  for (let frame = capture.frameCount - 1; frame >= 0; frame -= 1) {
    const left = capture.left[frame]!;
    const right = capture.right[frame]!;
    accumulatedEnergy += left * left + right * right;
    decayDb[frame] = accumulatedEnergy;
  }
  const totalEnergy = decayDb[0]!;
  for (let frame = 0; frame < capture.frameCount; frame += 1) {
    const left = Math.abs(capture.left[frame]!);
    const right = Math.abs(capture.right[frame]!);
    peakLeft = Math.max(peakLeft, left);
    peakRight = Math.max(peakRight, right);
    if (Math.max(left, right) > activeThreshold) {
      onsetFrame ??= frame;
      lastActiveFrame = frame;
    }
    decayDb[frame] = totalEnergy > 0 && decayDb[frame]! > 0 ? 10 * Math.log10(decayDb[frame]! / totalEnergy) : Number.NEGATIVE_INFINITY;
  }

  const peak = Math.max(peakLeft, peakRight);
  let rt60Seconds: number | null = null;
  let rt60Refusal: Rt60Refusal | null = null;
  if (totalEnergy <= 0 || peak <= 0) {
    rt60Refusal = 'no-energy';
  } else {
    const tailStart = Math.floor(capture.frameCount * .9);
    let tailEnergy = 0;
    for (let frame = tailStart; frame < capture.frameCount; frame += 1)
      tailEnergy += capture.left[frame]! ** 2 + capture.right[frame]! ** 2;
    const tailRms = Math.sqrt(tailEnergy / Math.max(1, (capture.frameCount - tailStart) * 2));
    if (tailRms > peak * 1e-4) {
      rt60Refusal = 'tail-noise';
    } else {
      let sumTime = 0, sumDb = 0, sumTimeSquared = 0, sumTimeDb = 0, count = 0;
      for (let frame = 0; frame < capture.frameCount; frame += 1) {
        const db = decayDb[frame]!;
        if (db <= -5 && db >= -35) {
          const time = frame / capture.sampleRate;
          sumTime += time; sumDb += db; sumTimeSquared += time * time; sumTimeDb += time * db; count += 1;
        }
      }
      if (count < 20) {
        rt60Refusal = 'insufficient-range';
      } else {
        const denominator = count * sumTimeSquared - sumTime * sumTime;
        const slope = denominator > Number.EPSILON ? (count * sumTimeDb - sumTime * sumDb) / denominator : 0;
        if (slope >= -1) rt60Refusal = 'non-decaying';
        else rt60Seconds = -60 / slope;
      }
    }
  }
  return { sampleRate: capture.sampleRate, frameCount: capture.frameCount, peakLeft, peakRight, onsetFrame, lastActiveFrame, decayDb, rt60Seconds, rt60Refusal };
}

export function rt60Explanation(reason: Rt60Refusal | null): string {
  if (reason === 'no-energy') return 'No response energy was captured, so decay time cannot be estimated.';
  if (reason === 'tail-noise') return 'The final 10% is above the permitted noise floor. Capture a longer or quieter tail.';
  if (reason === 'insufficient-range') return 'Fewer than 20 samples span the -5 to -35 dB fit range.';
  if (reason === 'non-decaying') return 'The fitted decay is flat or rising, so an RT60 extrapolation would be misleading.';
  return '';
}
