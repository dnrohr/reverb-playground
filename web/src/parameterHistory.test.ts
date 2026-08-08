import { describe, expect, it } from 'vitest';
import { commitParameterEdit, redoParameterEdit, undoParameterEdit, type ParameterHistory } from './parameterHistory';

describe('parameter edit history', () => {
  it('restores the exact before and after values through undo and redo', () => {
    const empty: ParameterHistory = { undo: [], redo: [] };
    const edit = { nodeId: 'tank-1', parameterId: 'delay', before: 8.91, after: 37.42 };
    const committed = commitParameterEdit(empty, edit);

    const undone = undoParameterEdit(committed);
    expect(undone.edit?.before).toBe(8.91);
    expect(undone.history).toEqual({ undo: [], redo: [edit] });

    const redone = redoParameterEdit(undone.history);
    expect(redone.edit?.after).toBe(37.42);
    expect(redone.history).toEqual({ undo: [edit], redo: [] });
  });

  it('does not record no-op edits and clears redo after a new edit', () => {
    const prior = { nodeId: 'filter', parameterId: 'cutoff', before: 6200, after: 9000 };
    const history: ParameterHistory = { undo: [], redo: [prior] };
    expect(commitParameterEdit(history, { ...prior, before: 6200, after: 6200 })).toBe(history);

    const next = { nodeId: 'sum', parameterId: 'gain', before: 0.5, after: 0.25 };
    expect(commitParameterEdit(history, next)).toEqual({ undo: [next], redo: [] });
  });
});
