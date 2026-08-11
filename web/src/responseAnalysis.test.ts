import { describe, expect, it } from 'vitest';
import { analyseResponse, decayPoints, frameWindow, rt60Explanation, waveformBuckets } from './responseAnalysis';

function exponential(rt60Seconds: number) {
  const sampleRate = 48_000;
  const frameCount = 96_000;
  const left = new Array<number>(frameCount).fill(0);
  const right = new Array<number>(frameCount).fill(0);
  for (let frame = 240; frame < frameCount; frame += 1) {
    const value = 10 ** (-3 * ((frame - 240) / sampleRate) / rt60Seconds);
    left[frame] = value; right[frame] = value * .5;
  }
  return { left, right, sampleRate, frameCount };
}

describe('stereo response analysis', () => {
  it('matches the tested synthetic T30 RT60 fixture', () => {
    const analysis = analyseResponse(exponential(.75));
    expect(analysis.onsetFrame).toBe(240);
    expect(analysis.peakLeft).toBe(1);
    expect(analysis.peakRight).toBe(.5);
    expect(analysis.rt60Refusal).toBeNull();
    expect(analysis.rt60Seconds).toBeCloseTo(.75, 2);
  });

  it('explains insufficient range and excessive tail noise', () => {
    const impulse = { left: [1, 0, 0, 0], right: [0, 0, 0, 0], sampleRate: 48_000, frameCount: 4 };
    expect(analyseResponse(impulse).rt60Refusal).toBe('insufficient-range');
    const noisy = exponential(.75); noisy.left.fill(.001, noisy.frameCount * .9); noisy.right.fill(-.001, noisy.frameCount * .9);
    const noisyAnalysis = analyseResponse(noisy);
    expect(noisyAnalysis.rt60Refusal).toBe('tail-noise');
    expect(rt60Explanation(noisyAnalysis.rt60Refusal)).toMatch(/longer or quieter/);
    expect(rt60Explanation('abrupt-cutoff')).toMatch(/level gate.*misleading/i);
  });

  it('preserves extrema while decimating a full tail and bounds zoom/pan', () => {
    const samples = [0, .2, 1, -.8, .1, 0, -.4, .3];
    const full = frameWindow(samples.length, 1, 0);
    expect(waveformBuckets(samples, full, 2)).toEqual([
      { frame: 0, minimum: -.8, maximum: 1 }, { frame: 4, minimum: -.4, maximum: .3 },
    ]);
    expect(frameWindow(1_000, 10, .5)).toEqual({ start: 450, end: 550 });
    expect(frameWindow(1_000, 999, 1)).toEqual({ start: 996, end: 1_000 });
    const points = decayPoints(new Float64Array([0, -5, -20, -40]), { start: 0, end: 4 }, 3);
    expect(points).toEqual([{ frame: 0, decibels: 0 }, { frame: 1, decibels: -5 }, { frame: 3, decibels: -40 }]);
  });
});
