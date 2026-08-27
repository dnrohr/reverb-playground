import { describe, expect, it } from 'vitest';
import { analyseDensity } from './densityAnalysis';

function fixture(dense: boolean) {
  const sampleRate = 48_000, frameCount = sampleRate;
  const left = new Array<number>(frameCount).fill(0), right = new Array<number>(frameCount).fill(0);
  if (!dense) [40, 113, 229, 401, 677, 887].forEach((ms) => { left[Math.round(ms * 48)] = .7; right[Math.round(ms * 48)] = .3; });
  else for (let frame = 0; frame < frameCount; frame += 1) {
    const value = Math.sin(frame * .731) * Math.sin(frame * .117) * .2;
    left[frame] = value; right[frame] = Math.sin(frame * .619) * .2;
  }
  return { left, right, sampleRate, frameCount };
}

describe('density inspector analysis', () => {
  it('separates sparse repeats from a dense field and emits three named summaries', () => {
    const sparse = analyseDensity(fixture(false)), dense = analyseDensity(fixture(true));
    expect(dense.summaries.map((summary) => summary.name)).toEqual(['EARLY', 'MIDDLE', 'LATE']);
    expect(dense.summaries[1]!.echoDensity).toBeGreaterThan(sparse.summaries[1]!.echoDensity + .4);
    expect(dense.summaries[1]!.activePeaksPerSecond).toBeGreaterThan(sparse.summaries[1]!.activePeaksPerSecond * 10);
    expect(dense.summaries[1]!.crestFactor).toBeLessThan(sparse.summaries[1]!.crestFactor);
  });

  it('rejects malformed captures and keeps every visible metric finite', () => {
    expect(() => analyseDensity({ left: [], right: [], sampleRate: 48_000, frameCount: 0 })).toThrow(/non-empty/);
    const result = analyseDensity(fixture(true));
    for (const point of result.points) for (const value of Object.values(point)) expect(Number.isFinite(value)).toBe(true);
  });
});
