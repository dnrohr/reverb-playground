import { describe, expect, it } from 'vitest';
import type { RuntimeSnapshot } from './graph';
import { factoryPatches, loadFactoryPatch } from './factoryPatches';
import { parsePatchJson, writePatchJson } from './patchPersistence';

const reference: RuntimeSnapshot = {
  contractVersion: 2,
  engineId: 'barr-reference',
  sampleRate: 48_000,
  nodes: [],
  connections: [],
  outsidePatch: [],
};

const visibleTypes = new Set([
  'stereo-input', 'stereo-output', 'sum', 'gain', 'delay', 'allpass', 'lowpass',
  'envelope-follower', 'hold-gate',
]);

describe('factory patches', () => {
  it('offers the Barr reference, causal reverse envelope, and level-gated room', () => {
    expect(factoryPatches.map((patch) => patch.id)).toEqual([
      'barr-reference', 'causal-reverse-envelope', 'level-gated-room',
    ]);
  });

  it.each(['causal-reverse-envelope', 'level-gated-room'] as const)(
    'loads %s using only visible editable primitives and round trips schema v2',
    (id) => {
      const loaded = loadFactoryPatch(id, reference);
      expect(loaded.nodes.every((node) => visibleTypes.has(node.data.type))).toBe(true);
      expect(loaded.nodes.every((node) => node.data.runtimeBound === false)).toBe(true);
      expect(loaded.nodes.filter((node) => node.data.type === 'stereo-input')).toHaveLength(1);
      expect(loaded.nodes.filter((node) => node.data.type === 'stereo-output')).toHaveLength(1);
      const written = writePatchJson(loaded.nodes, loaded.edges, loaded.viewport);
      const roundTrip = parsePatchJson(written, reference);
      expect(writePatchJson(roundTrip.nodes, roundTrip.edges, roundTrip.viewport)).toBe(written);
    },
  );

  it('keeps reverse and gated construction honest in their visible modules', () => {
    const reverse = loadFactoryPatch('causal-reverse-envelope', reference);
    const gated = loadFactoryPatch('level-gated-room', reference);
    expect(reverse.nodes.filter((node) => node.data.type === 'delay')).toHaveLength(3);
    expect(gated.nodes.filter((node) => node.data.type === 'envelope-follower')).toHaveLength(1);
    expect(gated.nodes.filter((node) => node.data.type === 'hold-gate')).toHaveLength(2);
    expect(reverse.nodes.map((node) => node.id)).not.toEqual(gated.nodes.map((node) => node.id));
  });
});
