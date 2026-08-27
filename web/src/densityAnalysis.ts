import type { ImpulseCaptureResult } from './impulseCapture';

export interface DensityPoint {
  startFrame: number;
  echoDensity: number;
  activePeakCount: number;
  crestFactor: number;
  energyVariation: number;
  recurrence: number;
  recurrenceMilliseconds: number;
  spectralFlatness: number;
  stereoCorrelation: number;
}

export interface DensitySummary extends Omit<DensityPoint, 'startFrame' | 'activePeakCount'> {
  name: 'EARLY' | 'MIDDLE' | 'LATE';
  activePeaksPerSecond: number;
}

const epsilon = 1e-20;
const gaussianTailProbability = .31731050786291415;

function correlation(left: readonly number[], right: readonly number[], start: number, count: number) {
  let ll = 0, rr = 0, lr = 0;
  for (let index = start; index < start + count; index += 1) {
    const l = left[index] ?? 0, r = right[index] ?? 0;
    ll += l * l; rr += r * r; lr += l * r;
  }
  return ll > epsilon && rr > epsilon ? Math.max(-1, Math.min(1, lr / Math.sqrt(ll * rr))) : 0;
}

function spectrumFlatness(mono: readonly number[]) {
  const length = Math.min(256, mono.length);
  if (length < 8) return 0;
  let logSum = 0, sum = 0;
  for (let bin = 1; bin <= 32; bin += 1) {
    let real = 0, imaginary = 0;
    for (let n = 0; n < length; n += 1) {
      const window = .5 - .5 * Math.cos(2 * Math.PI * n / (length - 1));
      const phase = -2 * Math.PI * bin * n / length;
      real += mono[n]! * window * Math.cos(phase); imaginary += mono[n]! * window * Math.sin(phase);
    }
    const power = real * real + imaginary * imaginary + epsilon;
    logSum += Math.log(power); sum += power;
  }
  return Math.max(0, Math.min(1, Math.exp(logSum / 32) / (sum / 32)));
}

function measurePoint(capture: Pick<ImpulseCaptureResult, 'left' | 'right' | 'sampleRate'>, start: number, count: number): DensityPoint {
  const stride = Math.max(1, Math.floor(count / 640));
  const mono: number[] = [];
  let energy = 0, peak = 0;
  for (let frame = start; frame < start + count; frame += stride) {
    const value = .5 * ((capture.left[frame] ?? 0) + (capture.right[frame] ?? 0));
    mono.push(value); energy += value * value; peak = Math.max(peak, Math.abs(value));
  }
  const meanSquare = energy / mono.length, rms = Math.sqrt(meanSquare);
  let above = 0, peaks = 0;
  for (let index = 0; index < mono.length; index += 1) {
    if (Math.abs(mono[index]!) > rms) above += 1;
    if (index > 0 && index + 1 < mono.length && Math.abs(mono[index]!) > rms * .5
      && Math.abs(mono[index]!) >= Math.abs(mono[index - 1]!) && Math.abs(mono[index]!) > Math.abs(mono[index + 1]!)) peaks += 1;
  }
  const slices = new Array<number>(8).fill(0);
  mono.forEach((value, index) => { slices[Math.min(7, Math.floor(index * 8 / mono.length))]! += value * value; });
  const sliceMean = slices.reduce((sum, value) => sum + value, 0) / 8;
  const variation = sliceMean > epsilon ? Math.sqrt(slices.reduce((sum, value) => sum + (value - sliceMean) ** 2, 0) / 8) / sliceMean : 0;
  const decimatedRate = capture.sampleRate / stride;
  const minimumLag = Math.max(1, Math.floor(decimatedRate * .001));
  const maximumLag = Math.min(Math.floor(mono.length / 2), Math.floor(decimatedRate * .03));
  let recurrence = 0, recurrenceLag = 0;
  for (let lag = minimumLag; lag <= maximumLag; lag += 1) {
    let xx = 0, yy = 0, xy = 0;
    for (let index = lag; index < mono.length; index += 1) {
      xx += mono[index]! ** 2; yy += mono[index - lag]! ** 2; xy += mono[index]! * mono[index - lag]!;
    }
    const value = xx > epsilon && yy > epsilon ? Math.abs(xy) / Math.sqrt(xx * yy) : 0;
    if (value > recurrence) { recurrence = value; recurrenceLag = lag; }
  }
  return { startFrame: start, echoDensity: Math.max(0, Math.min(1, above / mono.length / gaussianTailProbability)),
    activePeakCount: peaks, crestFactor: rms > epsilon ? peak / rms : 0, energyVariation: variation,
    recurrence, recurrenceMilliseconds: 1000 * recurrenceLag / decimatedRate, spectralFlatness: spectrumFlatness(mono),
    stereoCorrelation: correlation(capture.left, capture.right, start, count) };
}

export function analyseDensity(capture: Pick<ImpulseCaptureResult, 'left' | 'right' | 'sampleRate' | 'frameCount'>) {
  if (capture.frameCount <= 0 || capture.left.length !== capture.frameCount || capture.right.length !== capture.frameCount || capture.sampleRate <= 0)
    throw new Error('Density analysis requires equal non-empty stereo channels and a positive sample rate');
  const windowFrames = Math.max(8, Math.round(capture.sampleRate * .04));
  const hopFrames = Math.max(1, Math.round(capture.sampleRate * .02));
  const points: DensityPoint[] = [];
  for (let start = 0; start + windowFrames <= capture.frameCount; start += hopFrames)
    points.push(measurePoint(capture, start, windowFrames));
  const summaries = (['EARLY', 'MIDDLE', 'LATE'] as const).map((name, region): DensitySummary => {
    const first = region * capture.frameCount / 3, last = (region + 1) * capture.frameCount / 3;
    const selected = points.filter((point) => point.startFrame + windowFrames > first && point.startFrame < last);
    const average = (read: (point: DensityPoint) => number) => selected.length ? selected.reduce((sum, point) => sum + read(point), 0) / selected.length : 0;
    return { name, echoDensity: average((point) => point.echoDensity), activePeaksPerSecond: average((point) => point.activePeakCount) / .04,
      crestFactor: average((point) => point.crestFactor), energyVariation: average((point) => point.energyVariation),
      recurrence: average((point) => point.recurrence), recurrenceMilliseconds: average((point) => point.recurrenceMilliseconds),
      spectralFlatness: average((point) => point.spectralFlatness), stereoCorrelation: average((point) => point.stereoCorrelation) };
  });
  return { points, summaries, windowFrames, hopFrames };
}
