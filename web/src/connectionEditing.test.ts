import { describe, expect, it } from 'vitest';
import type { Connection } from '@xyflow/react';
import { connectGraph, decideConnection, insertSumForOccupiedInput } from './connectionEditing';
import type { PatchNodeData } from './graph';
import { createModuleNode } from './modules';
import { commitGraphEdit, emptyGraphHistory, undoGraphEdit } from './graphHistory';

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
