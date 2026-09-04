import { describe, expect, it } from 'vitest';
import { assessAssistedTuning, assistedTuningParameterKeys, createAssistedTuningPreview, supportsAssistedTuning } from './assistedTuning';
import { parsePatchJson, writePatchJson } from './patchPersistence';
import { commitGraphEdit, emptyGraphHistory, undoGraphEdit } from './graphHistory';
import type { RuntimeSnapshot } from './graph';
import fdn from '../../factory-patches/four-line-dense-room.rvp.json?raw';

const snapshot: RuntimeSnapshot = { contractVersion: 2, engineId: 'barr-reference', sampleRate: 48_000,
  nodes: [{ id: 'sum', type: 'sum', label: 'Sum', role: 'routing', position: { x: 0, y: 0 },
    ports: [{ id: 'in-a', signal: 'audio', direction: 'input' }, { id: 'in-b', signal: 'audio', direction: 'input' }, { id: 'out', signal: 'audio', direction: 'output' }], parameters: [] }],
  connections: [], outsidePatch: [] };

describe('non-destructive assisted tuning', () => {
  const graph = parsePatchJson(fdn, snapshot);
  const original = writePatchJson(graph.nodes, graph.edges, graph.viewport);

  it('recognizes the complete four-line topology', () => expect(supportsAssistedTuning(graph)).toBe(true));

  it.each(['smoother', 'less-metallic', 'wider', 'less-modulated'] as const)('%s creates a distinct preview without mutating baseline', (id) => {
    const preview = createAssistedTuningPreview(graph, id);
    expect(writePatchJson(graph.nodes, graph.edges, graph.viewport)).toBe(original);
    expect(writePatchJson(preview.nodes, preview.edges, graph.viewport)).not.toBe(original);
    expect(preview.nodes).not.toBe(graph.nodes);
  });

  it('recalculates decay gains with the smoother delay set', () => {
    const preview = createAssistedTuningPreview(graph, 'smoother');
    expect(preview.nodes.find((node) => node.id === 'line-delay-1')?.data.parameters[0]?.value).toBe(35.2);
    expect(preview.nodes.find((node) => node.id === 'line-return-1')?.data.parameters[0]?.value).toBeGreaterThan(0);
  });

  it('accept becomes one undoable edit whose undo restores the exact baseline', () => {
    const preview = createAssistedTuningPreview(graph, 'wider');
    const history = commitGraphEdit(emptyGraphHistory(graph), 'Accept wider tuning', graph, preview);
    expect(history.undo).toHaveLength(1);
    const undone = undoGraphEdit(history);
    expect(writePatchJson(undone.edit!.before.nodes, undone.edit!.before.edges, graph.viewport)).toBe(original);
  });

  it('declares exact disjoint parameter sets and accumulates compatible suggestions', () => {
    const smootherKeys = assistedTuningParameterKeys(graph, 'smoother');
    const darkerKeys = assistedTuningParameterKeys(graph, 'less-metallic');
    expect(smootherKeys).toHaveLength(8);
    expect(darkerKeys).toHaveLength(4);
    expect(smootherKeys.filter((key) => darkerKeys.includes(key))).toEqual([]);
    expect(assessAssistedTuning(graph, 'less-metallic', ['smoother'])).toMatchObject({ kind: 'compatible', overlap: [] });
    expect(assessAssistedTuning(graph, 'smoother', ['smoother'])).toMatchObject({ kind: 'replacement', overlap: smootherKeys });

    const smoother = createAssistedTuningPreview(graph, 'smoother');
    const cumulative = createAssistedTuningPreview(smoother, 'less-metallic');
    expect(cumulative.nodes.find((node) => node.id === 'line-delay-1')?.data.parameters[0]?.value).toBe(35.2);
    expect(cumulative.nodes.find((node) => node.id === 'line-damping-1')?.data.parameters[0]?.value)
      .toBeLessThan(graph.nodes.find((node) => node.id === 'line-damping-1')!.data.parameters[0]!.value);
  });

  it('creates a separate named undo step for each cumulative application', () => {
    const smoother = createAssistedTuningPreview(graph, 'smoother');
    let history = commitGraphEdit(emptyGraphHistory(graph), 'Apply tuning: smoother', graph, smoother);
    const cumulative = createAssistedTuningPreview(smoother, 'wider');
    history = commitGraphEdit(history, 'Apply tuning: wider', smoother, cumulative);
    expect(history.undo.map((edit) => edit.label)).toEqual(['Apply tuning: smoother', 'Apply tuning: wider']);
    expect(undoGraphEdit(history).edit?.before).toEqual(expect.objectContaining({ nodes: smoother.nodes }));
  });
});
