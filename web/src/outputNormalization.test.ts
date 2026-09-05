import { describe, expect, it } from 'vitest';
import { normalizedOutputPeak, outputNormalization } from './outputNormalization';

describe('impulse-response output normalization', () => {
  it('uses the shared stereo absolute peak and accounts for the current output gain', () => {
    const result = outputNormalization({ left: [0, -0.25], right: [0.5, 0] }, 2);
    expect(result).not.toBeNull();
    expect(result!.peak).toBe(0.5);
    expect(result!.gain).toBeCloseTo(2 * normalizedOutputPeak / 0.5, 12);
    expect(result!.limited).toBe(false);
  });

  it('caps extreme gain at 100x and refuses silent or invalid captures', () => {
    expect(outputNormalization({ left: [1e-6], right: [0] }, 1)).toEqual({ peak: 1e-6, gain: 100, limited: true });
    expect(outputNormalization({ left: [0], right: [0] }, 1)).toBeNull();
    expect(outputNormalization({ left: [Number.NaN], right: [0] }, 1)).toBeNull();
  });
});
