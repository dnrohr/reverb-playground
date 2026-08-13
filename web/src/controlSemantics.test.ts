import { describe, expect, it } from 'vitest';
import { createModuleNode } from './modules';
import { curveMapControlValue, curveMappingRange, decorateControlPreview, lfoValue, mapControlValue, mappingRange } from './controlSemantics';

describe('control generators and mapping', () => {
  it('generates deterministic sine and triangle values from frequency and phase', () => {
    expect(lfoValue(0, 1, 0.25, 'sine')).toBeCloseTo(1);
    expect(lfoValue(0.25, 1, 0, 'triangle')).toBeCloseTo(0);
    expect(lfoValue(0.5, 1, 0, 'triangle')).toBeCloseTo(1);
  });

  it('maps scale offset and polarity and predicts the resulting range', () => {
    expect(mapControlValue(-1, 0.25, 0.5, 'bipolar')).toBeCloseTo(0.25);
    expect(mappingRange(0.25, 0.5, 'bipolar')).toEqual({ minimum: 0.25, maximum: 0.75 });
    expect(mappingRange(-2, 0.25, 'unipolar')).toEqual({ minimum: -1, maximum: 0.25 });
  });

  it('evaluates finite monotonic linear power and exponential curves', () => {
    let previousPower = -2;
    let previousExponential = -2;
    for (let index = 0; index <= 100; index++) {
      const input = -1 + index / 50;
      expect(curveMapControlValue(input, 'linear', 7, 4, 0.75, 0.1, 'bipolar', -1, 1))
        .toBeCloseTo(mapControlValue(input, 0.75, 0.1, 'bipolar'), 12);
      const power = curveMapControlValue(input, 'power', 0, 2.5, 0.75, 0.1, 'bipolar', -1, 1);
      const exponential = curveMapControlValue(input, 'exponential', 4, 1, 0.75, 0.1, 'bipolar', -1, 1);
      expect(Number.isFinite(power)).toBe(true);
      expect(Number.isFinite(exponential)).toBe(true);
      expect(power).toBeGreaterThanOrEqual(previousPower);
      expect(exponential).toBeGreaterThanOrEqual(previousExponential);
      previousPower = power;
      previousExponential = exponential;
    }
    expect(curveMappingRange('power', 0, 2, -2, 0.25, 'unipolar', -0.6, 0.8))
      .toEqual({ minimum: -0.6, maximum: 0.25 });
  });

  it('previews one LFO output through a mapper and every branched cable', () => {
    const lfo = createModuleNode('lfo', 'lfo-1', { x: 0, y: 0 });
    const mapper = createModuleNode('control-map', 'control-map-1', { x: 100, y: 0 });
    const gain = createModuleNode('gain', 'gain-1', { x: 200, y: 0 });
    const delay = createModuleNode('delay', 'delay-1', { x: 200, y: 100 });
    const edges = [
      { id: 'to-map', source: 'lfo-1', sourceHandle: 'out', target: 'control-map-1', targetHandle: 'in', data: { signal: 'control' } },
      { id: 'branch-a', source: 'control-map-1', sourceHandle: 'out', target: 'gain-1', targetHandle: 'gain-mod', data: { signal: 'control' } },
      { id: 'branch-b', source: 'control-map-1', sourceHandle: 'out', target: 'delay-1', targetHandle: 'delay-mod', data: { signal: 'control' } },
    ];
    const preview = decorateControlPreview([lfo, mapper, gain, delay], edges, 0.25);
    expect(preview.edges.map((edge) => edge.data?.controlValue)).toEqual([1, 1, 1]);
    expect(preview.nodes.find((node) => node.id === 'lfo-1')?.data.controlPreview?.label).toBe('SINE');
  });
});
