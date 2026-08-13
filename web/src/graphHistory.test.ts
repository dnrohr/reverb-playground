import { describe, expect, it } from 'vitest';
import { deleteSelected, requiredIoError, type GraphState } from './graph';
import {
  commitGraphEdit, documentGraphHash, emptyGraphHistory, graphHistoryLimit, graphHistoryMemoryLimitBytes,
  historyMemoryBytes, isHistoryClean, markHistoryClean, redoGraphEdit, semanticGraphHash, undoGraphEdit,
} from './graphHistory';
import { createModuleNode } from './modules';

function baseGraph(): GraphState {
  return {
    nodes: [
      createModuleNode('stereo-input', 'input', { x: 0, y: 0 }),
      createModuleNode('delay', 'delay', { x: 100, y: 0 }),
      createModuleNode('stereo-output', 'output', { x: 200, y: 0 }),
    ],
    edges: [],
  };
}

describe('unified graph history', () => {
  it('returns semantic hashes to every prior value across mixed edits', () => {
    const states = [baseGraph()];
    const moved = structuredClone(states.at(-1)!); moved.nodes[1].position.x = 140; states.push(moved);
    const parameter = structuredClone(states.at(-1)!); parameter.nodes[1].data.parameters[0].value = 27.5; states.push(parameter);
    const modulation = structuredClone(states.at(-1)!); modulation.nodes[1].data.parameters[0].modulation!.amount = 8; modulation.nodes[1].data.parameters[0].modulation!.polarity = 'unipolar'; states.push(modulation);
    const connected = structuredClone(states.at(-1)!); connected.edges.push({ id: 'cable', source: 'input', sourceHandle: 'out-l', target: 'delay', targetHandle: 'in' }); states.push(connected);
    const added = structuredClone(states.at(-1)!); added.nodes.push(createModuleNode('gain', 'gain', { x: 80, y: 100 })); states.push(added);

    let history = emptyGraphHistory(states[0]);
    for (let index = 1; index < states.length; index++) history = commitGraphEdit(history, `edit ${index}`, states[index - 1], states[index]);
    for (let index = states.length - 2; index >= 0; index--) {
      const result = undoGraphEdit(history); expect(semanticGraphHash(result.edit!.before)).toBe(semanticGraphHash(states[index])); history = result.history;
    }
    for (let index = 1; index < states.length; index++) {
      const result = redoGraphEdit(history); expect(semanticGraphHash(result.edit!.after)).toBe(semanticGraphHash(states[index])); history = result.history;
    }
  });

  it('deletes a node and incident cables atomically and restores both with one undo', () => {
    const before = baseGraph(); before.nodes[1].selected = true;
    before.edges = [{ id: 'e1', source: 'input', sourceHandle: 'out-l', target: 'delay', targetHandle: 'in' }];
    const after = deleteSelected(before.nodes, before.edges);
    const result = undoGraphEdit(commitGraphEdit(emptyGraphHistory(before), 'Delete selection', before, after));
    expect(result.edit?.before.nodes.map((node) => node.id)).toContain('delay');
    expect(result.edit?.before.edges).toHaveLength(1);
  });

  it('records and restores Macro names as semantic edits', () => {
    const before = baseGraph();
    const macro = createModuleNode('macro', 'macro-gravity', { x: 80, y: 160 });
    before.nodes.push(macro);
    const after = structuredClone(before);
    after.nodes.find((node) => node.id === macro.id)!.data.userName = 'Gravity';
    expect(semanticGraphHash(after)).not.toBe(semanticGraphHash(before));
    const undone = undoGraphEdit(commitGraphEdit(emptyGraphHistory(before), 'Rename Macro', before, after));
    expect(undone.edit!.before.nodes.find((node) => node.id === macro.id)!.data.userName).toBe('Macro');
    const redone = redoGraphEdit(undone.history);
    expect(redone.edit!.after.nodes.find((node) => node.id === macro.id)!.data.userName).toBe('Gravity');
  });

  it('bounds history and retains a clean-state marker without clearing undo on save', () => {
    let state = baseGraph(); let history = emptyGraphHistory(state);
    expect(isHistoryClean(history, state)).toBe(true);
    for (let index = 0; index < graphHistoryLimit + 9; index++) {
      const next = structuredClone(state); next.nodes[1].position.x++;
      history = commitGraphEdit(history, 'Move', state, next); state = next;
    }
    expect(history.undo).toHaveLength(graphHistoryLimit);
    expect(historyMemoryBytes(history)).toBeLessThanOrEqual(graphHistoryMemoryLimitBytes);
    expect(isHistoryClean(history, state)).toBe(false);
    const saved = markHistoryClean(history, state);
    expect(saved.undo).toHaveLength(graphHistoryLimit);
    expect(isHistoryClean(saved, state)).toBe(true);
    const undone = undoGraphEdit(saved);
    expect(isHistoryClean(undone.history, undone.edit!.before)).toBe(false);
    const redone = redoGraphEdit(undone.history);
    expect(documentGraphHash(redone.edit!.after)).toBe(saved.cleanHash);
    expect(isHistoryClean(redone.history, redone.edit!.after)).toBe(true);
  });

  it('drops oldest snapshots before exceeding the documented memory limit', () => {
    let state = baseGraph(); state.nodes[1].data.label = 'x'.repeat(1_100_000);
    let history = emptyGraphHistory(state);
    for (let index = 0; index < 5; index++) {
      const next = structuredClone(state); next.nodes[1].data.parameters[0].value += 1;
      history = commitGraphEdit(history, 'Large edit', state, next); state = next;
    }
    expect(history.undo.length).toBeLessThan(5);
    expect(historyMemoryBytes(history)).toBeLessThanOrEqual(graphHistoryMemoryLimitBytes);
  });

  it('enforces exactly one stereo input and output', () => {
    const input = createModuleNode('stereo-input', 'input', { x: 0, y: 0 }); const output = createModuleNode('stereo-output', 'output', { x: 0, y: 0 });
    expect(requiredIoError([input, output])).toBeNull(); expect(requiredIoError([output])).toMatch('exactly one Stereo Input'); expect(requiredIoError([input, output, output])).toMatch('exactly one Stereo Output');
  });
});
