import { describe, expect, it } from 'vitest';
import { gravityFocusNodeIds, gravityState, predictGravityEnvelope } from './gravityPresentation';

describe('Gravity presentation', () => {
  it('labels the bipolar endpoints and reliable center detent', () => {
    expect(gravityState(-1)).toBe('INVERSE');
    expect(gravityState(-0.2)).toBe('INVERSE');
    expect(gravityState(0)).toBe('BLOOM');
    expect(gravityState(0.0004)).toBe('BLOOM');
    expect(gravityState(0.2)).toBe('FORWARD');
    expect(gravityState(1)).toBe('FORWARD');
  });

  it('predicts ordered finite inverse, bloom, and forward peaks without claiming measurement', () => {
    const inverse = predictGravityEnvelope(-1);
    const bloom = predictGravityEnvelope(0);
    const forward = predictGravityEnvelope(1);
    expect(inverse.peakPosition).toBeGreaterThan(bloom.peakPosition);
    expect(bloom.peakPosition).toBeGreaterThan(forward.peakPosition);
    for (const prediction of [inverse, bloom, forward]) {
      expect(prediction.path).toMatch(/^M /);
      expect(prediction.path).not.toMatch(/NaN|Infinity/);
      expect(prediction.description).toMatch(/Not measured audio/);
    }
  });

  it('focuses the complete visible reachability set once', () => {
    expect(gravityFocusNodeIds({
      nodeIds: ['macro', 'map-a', 'delay', 'map-a'], edgeIds: [], destinations: [],
    })).toEqual(['macro', 'map-a', 'delay']);
  });
});
