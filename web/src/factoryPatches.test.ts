import { describe, expect, it } from 'vitest';
import type { RuntimeSnapshot } from './graph';
import { comparisonPatchAfterSelection, comparisonPatchLabel, factoryPatches, loadFactoryPatch } from './factoryPatches';
import { parsePatchJson, writePatchJson } from './patchPersistence';
import { commitGraphEdit, emptyGraphHistory, redoGraphEdit, undoGraphEdit } from './graphHistory';
import { inspectFeedbackLoops } from './loopInspection';
import { decorateSplitShimmerFocus, splitShimmerLoopKind } from './splitFeedbackShimmerTeaching';
import factoryCatalogJson from '../../factory-patches/catalog.json?raw';

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
  'macro', 'lfo', 'control-map', 'envelope-follower', 'hold-gate',
  'pitch-shift',
]);

describe('factory patches', () => {
  it('matches the complete licensed and traceable factory catalog', () => {
    const catalog = JSON.parse(factoryCatalogJson) as {
      catalogVersion: number;
      patches: Array<{
        id: string;
        family: string;
        status: string;
        document: { kind: string; path: string; schemaVersion: number; engineVersion: string };
        license: { expression: string; file: string };
        provenance: { kind: string; source: string; description: string };
      }>;
    };
    expect(catalog.catalogVersion).toBe(1);
    expect(catalog.patches.map((patch) => patch.id)).toEqual(factoryPatches.map((patch) => patch.id));
    expect(catalog.patches.map((patch) => patch.family)).toEqual(['barr-reference', 'reverse-style', 'gated', 'modulated-reverse-style', 'gravity-diffusion', 'parallel-shimmer', 'feedback-shimmer']);
    for (const patch of catalog.patches) {
      expect(patch.status).toBe('complete');
      expect(['native-runtime', 'checked-in-json']).toContain(patch.document.kind);
      expect(patch.document.path).toMatch(/\.(cpp|json)$/);
      expect(patch.document.schemaVersion).toBe(2);
      expect(patch.document.engineVersion).toBe('0.1');
      expect(patch.license).toEqual({ expression: 'AGPL-3.0-only', file: 'LICENSE' });
      expect(patch.provenance.kind).toMatch(/^project-authored/);
      expect(patch.provenance.source).toMatch(/\.(cpp|mjs)$/);
      expect(patch.provenance.description.length).toBeGreaterThan(20);
    }
  });

  it('offers all complete reference, reverse, gated, and modulated-space designs', () => {
    expect(factoryPatches.map((patch) => patch.id)).toEqual([
      'barr-reference', 'causal-reverse-envelope', 'level-gated-room', 'modulated-cosmic-reverse', 'gravity-diffusion', 'safe-parallel-shimmer', 'split-feedback-shimmer',
    ]);
  });

  it.each(['causal-reverse-envelope', 'level-gated-room', 'modulated-cosmic-reverse', 'gravity-diffusion', 'safe-parallel-shimmer', 'split-feedback-shimmer'] as const)(
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

  it('ships Gravity Diffusion as an expanded editable instrument', () => {
    const gravity = loadFactoryPatch('gravity-diffusion', reference);
    expect(gravity.nodes).toHaveLength(58);
    expect(gravity.edges).toHaveLength(94);
    expect(gravity.nodes.filter((node) => node.data.type === 'macro')).toHaveLength(5);
    expect(gravity.nodes.filter((node) => node.data.type === 'control-map')).toHaveLength(8);
    expect(gravity.nodes.filter((node) => node.data.type === 'lfo')).toHaveLength(2);
    expect(gravity.nodes.find((node) => node.id === 'gravity')?.data.presentation).toBe('gravity');
    expect(gravity.nodes.find((node) => node.id === 'gravity')?.data.userName).toBe('Gravity');
    expect(gravity.nodes.every((node) => Number.isFinite(node.position.x) && Number.isFinite(node.position.y))).toBe(true);
  });

  it('exposes the cosmic reverse rise, delayed feedback, damping, and independent slow drift', () => {
    const cosmic = loadFactoryPatch('modulated-cosmic-reverse', reference);
    expect(cosmic.nodes.filter((node) => node.data.type === 'delay')).toHaveLength(4);
    expect(cosmic.nodes.filter((node) => node.data.type === 'lfo')).toHaveLength(2);
    expect(cosmic.nodes.filter((node) => node.data.type === 'lowpass')).toHaveLength(1);
    expect(cosmic.edges.filter((edge) => edge.data?.signal === 'control')).toHaveLength(4);
    expect(cosmic.edges.some((edge) => edge.source === 'feedback-0-58' && edge.target === 'tank-input')).toBe(true);
  });

  it('ships Safe Parallel Shimmer with named aligned normal and non-recirculating octave branches', () => {
    const shimmer = loadFactoryPatch('safe-parallel-shimmer', reference);
    expect(shimmer.nodes).toHaveLength(28);
    expect(shimmer.edges).toHaveLength(32);
    expect(shimmer.nodes.filter((node) => node.data.type === 'pitch-shift')).toHaveLength(1);
    expect(shimmer.nodes.find((node) => node.id === 'normal-latency-alignment')?.data.userName).toBe('Normal / aligned');
    expect(shimmer.nodes.find((node) => node.id === 'shimmer-pitch')?.data.userName).toBe('Octave / +12 st');
    expect(shimmer.nodes.find((node) => node.id === 'shimmer-level')?.data.userName).toBe('Shimmer level');
    expect(shimmer.edges.some((edge) => edge.source === 'shimmer-pitch' && edge.target === 'tank-entry')).toBe(false);
    expect(shimmer.edges.some((edge) => edge.source === 'shimmer-level' && edge.target === 'tank-entry')).toBe(false);
  });

  it('preserves a Safe Parallel Shimmer level edit through Undo Redo and schema save reload', () => {
    const original = loadFactoryPatch('safe-parallel-shimmer', reference);
    const edited = structuredClone(original);
    const shimmerLevel = edited.nodes.find((node) => node.id === 'shimmer-level')!;
    shimmerLevel.data.parameters.find((parameter) => parameter.id === 'gain')!.value = 0.17;
    const history = commitGraphEdit(emptyGraphHistory(original), 'Edit shimmer level', original, edited);
    const undone = undoGraphEdit(history);
    expect(undone.edit?.before.nodes.find((node) => node.id === 'shimmer-level')?.data.parameters[0].value).toBe(0.22);
    const redone = redoGraphEdit(undone.history);
    expect(redone.edit?.after.nodes.find((node) => node.id === 'shimmer-level')?.data.parameters[0].value).toBe(0.17);
    const saved = writePatchJson(redone.edit!.after.nodes, redone.edit!.after.edges, original.viewport);
    const restored = parsePatchJson(saved, reference);
    expect(restored.nodes.find((node) => node.id === 'shimmer-level')?.data.parameters[0].value).toBe(0.17);
    expect(writePatchJson(restored.nodes, restored.edges, restored.viewport)).toBe(saved);
  });

  it('ships Split-Feedback Shimmer with separately named complete return loops', () => {
    const shimmer = loadFactoryPatch('split-feedback-shimmer', reference);
    expect(shimmer.nodes).toHaveLength(25);
    expect(shimmer.edges).toHaveLength(29);
    expect(shimmer.nodes.find((node) => node.id === 'normal-feedback')?.data.userName).toBe('Normal feedback');
    expect(shimmer.nodes.find((node) => node.id === 'shifted-feedback')?.data.userName).toBe('Shifted feedback');
    expect(shimmer.nodes.find((node) => node.id === 'shifted-pitch')?.data.userName).toBe('Octave / +12 st');
    expect(shimmer.edges.some((edge) => edge.source === 'normal-feedback-delay' && edge.target === 'feedback-recombine')).toBe(true);
    expect(shimmer.edges.some((edge) => edge.source === 'shifted-feedback-delay' && edge.target === 'feedback-recombine')).toBe(true);
  });

  it('isolates the actual normal shifted and shared factory regions', () => {
    const shimmer = loadFactoryPatch('split-feedback-shimmer', reference);
    const normal = inspectFeedbackLoops(shimmer.nodes, shimmer.edges, { edgeId: 'normal-recombine' });
    const shifted = inspectFeedbackLoops(shimmer.nodes, shimmer.edges, { edgeId: 'shifted-recombine' });
    const shared = inspectFeedbackLoops(shimmer.nodes, shimmer.edges, { nodeId: 'tank-delay' });
    expect(normal.loops).toHaveLength(1);
    expect(shifted.loops).toHaveLength(2);
    expect(shared.loops).toHaveLength(3);
    expect(splitShimmerLoopKind(normal.loops[0]!.nodeIds)).toBe('NORMAL RETURN');
    expect(shifted.loops.every((loop) => splitShimmerLoopKind(loop.nodeIds) === 'SHIFTED RETURN')).toBe(true);
    expect(new Set(shared.loops.map((loop) => splitShimmerLoopKind(loop.nodeIds)))).toEqual(new Set(['NORMAL RETURN', 'SHIFTED RETURN']));
    const focused = decorateSplitShimmerFocus(shimmer.nodes, shimmer.edges, 'shared');
    expect(focused.nodes.find((node) => node.id === 'tank-delay')?.className).toContain('split-focus-active');
    expect(focused.nodes.find((node) => node.id === 'normal-feedback')?.className).toContain('split-focus-related');
    expect(focused.nodes.find((node) => node.id === 'shifted-feedback')?.className).toContain('split-focus-related');
  });

  it('preserves a Split-Feedback Shimmer edit through Undo Redo and schema save reload', () => {
    const original = loadFactoryPatch('split-feedback-shimmer', reference);
    const edited = structuredClone(original);
    edited.nodes.find((node) => node.id === 'shifted-feedback')!
      .data.parameters.find((parameter) => parameter.id === 'gain')!.value = 0.08;
    const history = commitGraphEdit(emptyGraphHistory(original), 'Edit shifted feedback', original, edited);
    const undone = undoGraphEdit(history);
    expect(undone.edit?.before.nodes.find((node) => node.id === 'shifted-feedback')?.data.parameters[0].value).toBe(0.10);
    const redone = redoGraphEdit(undone.history);
    expect(redone.edit?.after.nodes.find((node) => node.id === 'shifted-feedback')?.data.parameters[0].value).toBe(0.08);
    const saved = writePatchJson(redone.edit!.after.nodes, redone.edit!.after.edges, original.viewport);
    const restored = parsePatchJson(saved, reference);
    expect(restored.nodes.find((node) => node.id === 'shifted-feedback')?.data.parameters[0].value).toBe(0.08);
    expect(writePatchJson(restored.nodes, restored.edges, restored.viewport)).toBe(saved);
  });

  it('remembers the selected design while A is the Barr reference', () => {
    expect(comparisonPatchAfterSelection('level-gated-room', 'causal-reverse-envelope')).toBe('level-gated-room');
    expect(comparisonPatchAfterSelection('barr-reference', 'level-gated-room')).toBe('level-gated-room');
    expect(comparisonPatchAfterSelection('custom', 'level-gated-room')).toBe('level-gated-room');
    expect(comparisonPatchAfterSelection('modulated-cosmic-reverse', 'level-gated-room')).toBe('modulated-cosmic-reverse');
    expect(comparisonPatchAfterSelection('gravity-diffusion', 'modulated-cosmic-reverse')).toBe('gravity-diffusion');
    expect(comparisonPatchAfterSelection('barr-reference', 'gravity-diffusion')).toBe('gravity-diffusion');
    expect(comparisonPatchLabel('gravity-diffusion')).toBe('GRAVITY');
    expect(comparisonPatchAfterSelection('safe-parallel-shimmer', 'gravity-diffusion')).toBe('safe-parallel-shimmer');
    expect(comparisonPatchAfterSelection('barr-reference', 'safe-parallel-shimmer')).toBe('safe-parallel-shimmer');
    expect(comparisonPatchLabel('safe-parallel-shimmer')).toBe('PAR SHIMMER');
    expect(comparisonPatchAfterSelection('split-feedback-shimmer', 'safe-parallel-shimmer')).toBe('split-feedback-shimmer');
    expect(comparisonPatchAfterSelection('barr-reference', 'split-feedback-shimmer')).toBe('split-feedback-shimmer');
    expect(comparisonPatchLabel('split-feedback-shimmer')).toBe('FB SHIMMER');
  });
});
