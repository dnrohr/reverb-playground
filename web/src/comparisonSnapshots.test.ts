import { describe, expect, it } from 'vitest';
import type { GraphState } from './graph';
import { captureComparisonSnapshot, compareSnapshots, comparisonGainLinear, comparisonMatch } from './comparisonSnapshots';

const graph = (gain: number): GraphState => ({ nodes: [{ id: 'gain', type: 'patchNode', position: { x: 0, y: 0 }, data: {
  label: 'Gain', type: 'gain', role: 'routing', runtimeBound: false,
  ports: [{ id: 'in', signal: 'audio', direction: 'input' }, { id: 'out', signal: 'audio', direction: 'output' }],
  parameters: [{ id: 'gain', value: gain, unit: 'linear', minimum: -1, maximum: 1, step: .001 }],
} }], edges: [] });
const capture = (value: number, generation = 1) => ({ formatVersion: 1 as const, generation, sampleRate: 48_000, frameCount: 128,
  maximumLengthMilliseconds: 2_000, stopThresholdDb: -80, muteLiveInput: true, impulseLevel: .1, stopReason: 'maximum-length' as const,
  left: Array(128).fill(value), right: Array(128).fill(value) });
const snap = (slot: 'A' | 'B', value: number, latency: number, sample: number) => captureComparisonSnapshot({ slot, label: slot,
  graph: graph(value), viewport: { x: 0, y: 0, zoom: 1 }, qualityPolicy: 'normal', wetGain: .5, dryGain: 0,
  latencySamples: latency, capture: capture(sample, slot === 'A' ? 1 : 2), captureGraphHash: captureComparisonSnapshot({ slot, label: slot,
    graph: graph(value), viewport: { x: 0, y: 0, zoom: 1 }, qualityPolicy: 'normal', wetGain: .5, dryGain: 0,
    latencySamples: latency, capture: null, captureGraphHash: null }).graphHash });

describe('comparison snapshots', () => {
  it('captures exact independent graphs and reports block gain and latency changes', () => {
    const source = graph(.25); const baseline = captureComparisonSnapshot({ slot: 'A', label: 'A', graph: source,
      viewport: { x: 0, y: 0, zoom: 1 }, qualityPolicy: 'normal', wetGain: .5, dryGain: 0,
      latencySamples: 64, capture: null, captureGraphHash: null });
    source.nodes[0].data.parameters[0].value = .9;
    expect(baseline.graph.nodes[0].data.parameters[0].value).toBe(.25);
    const a = snap('A', .25, 64, .2); const b = snap('B', .75, 160, .1);
    expect(compareSnapshots(a, b)).toMatchObject({ changedBlocks: ['gain'], gainChanges: ['gain 0.250 → 0.750'], latencyDeltaSamples: 96 });
  });
  it('attenuates only the louder deterministic probe and never boosts either snapshot', () => {
    const match = comparisonMatch(snap('A', .25, 64, .2), snap('B', .75, 160, .1));
    expect(match.accepted).toBe(true);
    if (!match.accepted) return;
    expect(match.adjustmentA).toBeCloseTo(-6.0206, 3); expect(match.adjustmentB).toBeCloseTo(0, 6);
    expect(comparisonGainLinear(match.adjustmentA)).toBeCloseTo(.5, 6);
  });
  it('refuses missing, silent, clipped, and insufficient evidence', () => {
    const noProbe = captureComparisonSnapshot({ slot: 'A', label: 'A', graph: graph(1), viewport: { x: 0, y: 0, zoom: 1 },
      qualityPolicy: 'normal', wetGain: .5, dryGain: 0, latencySamples: 0, capture: null, captureGraphHash: null });
    expect(comparisonMatch(noProbe, snap('B', 1, 0, .1))).toMatchObject({ accepted: false, reason: expect.stringMatching(/impulse/i) });
    for (const value of [0, 1]) {
      const bad = captureComparisonSnapshot({ ...noProbe, slot: 'A', graph: graph(1), capture: capture(value), captureGraphHash: noProbe.graphHash });
      expect(bad.probe?.accepted).toBe(false);
    }
  });
});
