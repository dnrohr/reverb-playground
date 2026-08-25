import { describe, expect, it } from 'vitest';
import { parallelShimmerBranch, parallelShimmerTeaching } from './parallelShimmerTeaching';

describe('Safe Parallel Shimmer teaching contract', () => {
  it('calls the design parallel shimmer and contrasts one pitch pass with classic feedback shimmer', () => {
    expect(parallelShimmerTeaching.title).toBe('PARALLEL SHIMMER');
    expect(parallelShimmerTeaching.method).toBe('ONE PITCH PASS');
    expect(parallelShimmerTeaching.octave).toContain('pitch +12 st');
    expect(parallelShimmerTeaching.contrast).toMatch(/Unlike classic feedback shimmer/);
    expect(parallelShimmerTeaching.contrast).toMatch(/cannot return through Pitch Shift/);
    expect(parallelShimmerTeaching.contrast).toMatch(/not a \+24 \/ \+36 octave staircase/);
  });

  it('identifies normal octave tank and recombination selections', () => {
    expect(parallelShimmerTeaching.normal).toContain('360.01 ms alignment');
    expect(parallelShimmerBranch('normal-level')).toBe('normal');
    expect(parallelShimmerBranch('shimmer-pitch')).toBe('octave');
    expect(parallelShimmerBranch('reverb-decay')).toBe('tank');
    expect(parallelShimmerBranch('parallel-wet-sum')).toBe('recombine');
    expect(parallelShimmerBranch('input')).toBe('other');
  });
});
