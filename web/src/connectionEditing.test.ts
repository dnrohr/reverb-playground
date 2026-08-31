import { describe, expect, it } from 'vitest';
import type { Connection } from '@xyflow/react';
import { connectGraph, decideConnection, insertSumForOccupiedInput, previewSumForOccupiedInput } from './connectionEditing';
import type { PatchNodeData } from './graph';
import { createModuleNode } from './modules';
import { commitGraphEdit, emptyGraphHistory, redoGraphEdit, undoGraphEdit } from './graphHistory';
import { copySelectedGraph, pasteGraph } from './graphClipboard';
import { writePatchJson } from './patchPersistence';
import { inspectFeedbackLoops } from './loopInspection';

const connection = (source: string, sourceHandle: string, target: string, targetHandle: string): Connection => ({ source, sourceHandle, target, targetHandle });

describe('typed connection editing', () => {
  const input = createModuleNode('stereo-input', 'input', { x: 0, y: 0 });
  const delay = createModuleNode('delay', 'delay', { x: 200, y: 0 });
  const allpass = createModuleNode('allpass', 'allpass', { x: 400, y: 0 });

  it('allows one output to branch to several inputs with stable typed edges', () => {
    let state = connectGraph({ nodes: [input, delay, allpass], edges: [] }, connection('input', 'out-l', 'delay', 'in'));
    state = connectGraph(state, connection('input', 'out-l', 'allpass', 'in'));
    expect(state.edges).toHaveLength(2); expect(new Set(state.edges.map((edge) => edge.source))).toEqual(new Set(['input']));
    expect(state.edges.every((edge) => edge.data?.signal === 'audio' && edge.interactionWidth === 24)).toBe(true);
  });

  it('branches one mapped control output to multiple parameter sockets', () => {
    const mapper = createModuleNode('control-map', 'mapper', { x: 0, y: 0 });
    let state = connectGraph({ nodes: [mapper, delay, allpass], edges: [] }, connection('mapper', 'out', 'delay', 'delay-mod'));
    state = connectGraph(state, connection('mapper', 'out', 'allpass', 'coefficient-mod'));
    expect(state.edges).toHaveLength(2);
    expect(state.edges.every((edge) => edge.data?.signal === 'control')).toBe(true);
  });

  it('rejects occupied inputs and can replace the existing cable', () => {
    const once = connectGraph({ nodes: [input, delay, allpass], edges: [] }, connection('input', 'out-l', 'delay', 'in'));
    expect(decideConnection(once.nodes, once.edges, connection('allpass', 'out', 'delay', 'in')).kind).toBe('occupied');
    expect(() => connectGraph(once, connection('allpass', 'out', 'delay', 'in'))).toThrow('Input is occupied');
    const replaced = connectGraph(once, connection('allpass', 'out', 'delay', 'in'), true);
    expect(replaced.edges).toHaveLength(1); expect(replaced.edges[0]?.source).toBe('allpass');
  });

  it('inserts an explicit Sum and rewires both sources atomically', () => {
    const once = connectGraph({ nodes: [input, delay, allpass], edges: [] }, connection('input', 'out-l', 'delay', 'in'));
    const result = insertSumForOccupiedInput(once, connection('allpass', 'out', 'delay', 'in'));
    expect(result.nodes.find((node) => node.id === 'sum-1')?.data.type).toBe('sum');
    expect(result.edges.map((edge) => `${edge.source}.${edge.sourceHandle}->${edge.target}.${edge.targetHandle}`)).toEqual([
      'input.out-l->sum-1.in-a', 'allpass.out->sum-1.in-b', 'sum-1.out->delay.in',
    ]);
    const history = commitGraphEdit(emptyGraphHistory(once), 'Insert +', once, result);
    expect(history.undo).toHaveLength(1);
    const undone = undoGraphEdit(history);
    expect(undone.edit?.before).toEqual(once);
    expect(undone.history.undo).toHaveLength(0);
  });

  it('previews exactly the graph confirmation commits without mutating the source', () => {
    const once = connectGraph({ nodes: [input, delay, allpass], edges: [] }, connection('input', 'out-l', 'delay', 'in'));
    const original = JSON.stringify(once);
    const requested = connection('allpass', 'out', 'delay', 'in');
    const preview = previewSumForOccupiedInput(once, requested);
    const committed = insertSumForOccupiedInput(once, requested);
    expect(JSON.stringify(once)).toBe(original);
    expect(preview.nodes.map(({ className: _className, draggable: _draggable, selectable: _selectable, deletable: _deletable, width: _width, height: _height, ...node }) => node)).toEqual(committed.nodes);
    expect(preview.edges.map(({ className, selectable: _selectable, deletable: _deletable, ...edge }) => ({ ...edge, className: className?.replace(' sum-insertion-preview', '') }))).toEqual(committed.edges);
    expect(preview.nodes.at(-1)).toMatchObject({ id: 'sum-1', draggable: false, selectable: false, deletable: false });
    expect(preview.edges.filter((edge) => edge.className?.includes('sum-insertion-preview'))).toHaveLength(3);
  });

  it('makes assisted insertion one deterministic undo and redo action', () => {
    const once = connectGraph({ nodes: [input, delay, allpass], edges: [] }, connection('input', 'out-l', 'delay', 'in'));
    const requested = connection('allpass', 'out', 'delay', 'in');
    const after = insertSumForOccupiedInput(once, requested);
    expect(insertSumForOccupiedInput(structuredClone(once), requested)).toEqual(after);
    const undone = undoGraphEdit(commitGraphEdit(emptyGraphHistory(once), 'Insert +', once, after));
    expect(undone.edit?.before).toEqual(once);
    const redone = redoGraphEdit(undone.history);
    expect(redone.edit?.after).toEqual(after);
  });

  it('persists and copies the inserted Sum as an ordinary visible primitive', () => {
    const once = connectGraph({ nodes: [input, delay, allpass], edges: [] }, connection('input', 'out-l', 'delay', 'in'));
    const after = insertSumForOccupiedInput(once, connection('allpass', 'out', 'delay', 'in'));
    const saved = JSON.parse(writePatchJson(after.nodes, after.edges, { x: 0, y: 0, zoom: 1 }));
    expect(saved.semantic.nodes.find((node: { id: string }) => node.id === 'sum-1')).toMatchObject({ type: 'sum' });
    expect(saved.layout.nodes.find((node: { nodeId: string }) => node.nodeId === 'sum-1')).toEqual({ nodeId: 'sum-1', x: -10, y: 110 });
    expect(saved.semantic.connections).toHaveLength(3);

    const selected = { nodes: after.nodes.map((node) => ({ ...node, selected: node.id === 'sum-1' })), edges: after.edges };
    const clipboard = copySelectedGraph(selected)!;
    expect(clipboard.nodes).toHaveLength(1);
    expect(clipboard.nodes[0]?.data.type).toBe('sum');
    expect(pasteGraph(selected, clipboard).nodes.at(-1)).toMatchObject({ id: 'sum-1-copy', data: { type: 'sum', runtimeBound: false } });
  });

  it('never offers an audio Sum preview for an occupied control input', () => {
    const mapper = createModuleNode('control-map', 'mapper', { x: 0, y: 0 });
    const lfo = createModuleNode('lfo', 'lfo', { x: 0, y: 150 });
    const once = connectGraph({ nodes: [mapper, lfo, delay], edges: [] }, connection('mapper', 'out', 'delay', 'delay-mod'));
    const requested = connection('lfo', 'out', 'delay', 'delay-mod');
    expect(decideConnection(once.nodes, once.edges, requested)).toMatchObject({ kind: 'occupied', signal: 'control' });
    expect(() => previewSumForOccupiedInput(once, requested)).toThrow('Automatic + insertion requires an occupied audio input');
  });

  it('keeps explicit delayed-feedback validation intact after assisted rewiring', () => {
    let feedback = connectGraph({ nodes: [input, delay, allpass], edges: [] }, connection('delay', 'out', 'allpass', 'in'));
    feedback = connectGraph(feedback, connection('allpass', 'out', 'delay', 'in'));
    const inserted = insertSumForOccupiedInput(feedback, connection('input', 'out-l', 'delay', 'in'));
    const inspection = inspectFeedbackLoops(inserted.nodes, inserted.edges, { nodeId: 'delay' });
    expect(inspection.loops).toHaveLength(1);
    expect(inspection.loops[0]?.nodeIds).toEqual(expect.arrayContaining(['delay', 'allpass', 'sum-1']));
  });

  it('rejects invalid direction and signal-type combinations', () => {
    expect(decideConnection([input, delay], [], connection('delay', 'in', 'input', 'out-l')).kind).toBe('invalid');
    const control = { ...delay, id: 'control', data: { ...delay.data, ports: [{ id: 'out', direction: 'output', signal: 'control' }] } as PatchNodeData };
    expect(decideConnection([control, delay], [], connection('control', 'out', 'delay', 'in'))).toEqual({ kind: 'invalid', message: 'control cannot connect to an audio input.' });
  });

  it('restores connection creation and deletion through structural history', () => {
    const before = { nodes: [input, delay], edges: [] };
    const connected = connectGraph(before, connection('input', 'out-l', 'delay', 'in'));
    const createUndo = undoGraphEdit(commitGraphEdit(emptyGraphHistory(), 'Create cable', before, connected));
    expect(createUndo.edit?.before.edges).toHaveLength(0);
    const deleted = { nodes: connected.nodes, edges: [] };
    const deleteUndo = undoGraphEdit(commitGraphEdit(emptyGraphHistory(), 'Delete cable', connected, deleted));
    expect(deleteUndo.edit?.before.edges).toEqual(connected.edges);
  });
});
