import { describe, expect, it } from 'vitest';
import { gravityMeasuredReference } from './gravityReference';

describe('Gravity checked reference comparison', () => {
  it('loads the three measured envelopes and preserves their ordering', () => {
    const inverse = gravityMeasuredReference(-1);
    const bloom = gravityMeasuredReference(0);
    const forward = gravityMeasuredReference(1);
    expect(inverse.peakMilliseconds).toBeGreaterThan(bloom.peakMilliseconds + 200);
    expect(bloom.peakMilliseconds).toBeGreaterThan(forward.peakMilliseconds + 100);
    expect(inverse.earlyLateEnergyRatioDb).toBeLessThan(0);
    expect(forward.earlyLateEnergyRatioDb).toBeGreaterThan(bloom.earlyLateEnergyRatioDb);
    expect(inverse.path).toMatch(/^M 0\.00,/);
    expect(inverse.path.split(' L ')).toHaveLength(49);
  });

  it('states that checked measurements are not the current capture', () => {
    const reference = gravityMeasuredReference(0.4);
    expect(reference.state).toBe('FORWARD');
    expect(reference.description).toMatch(/Reference evidence, not the current live capture/);
    expect(reference.controlsDescription).toContain('gravity +1.00');
  });
});
