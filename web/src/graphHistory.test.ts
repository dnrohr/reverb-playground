import { describe, expect, it } from 'vitest';
import { deleteSelected, requiredIoError, type GraphState } from './graph';
import { commitGraphEdit, emptyGraphHistory, undoGraphEdit } from './graphHistory';
import { createModuleNode } from './modules';

describe('structural graph edits', () => {
  it('deletes a node and its incident cables atomically and restores both with undo', () => {
    const input = createModuleNode('stereo-input', 'stereo-input-1', { x: 0, y: 0 });
    const delay = { ...createModuleNode('delay', 'delay-1', { x: 1, y: 0 }), selected: true };
    const output = createModuleNode('stereo-output', 'stereo-output-1', { x: 2, y: 0 });
    const before: GraphState = { nodes: [input, delay, output], edges: [{ id: 'e1', source: input.id, sourceHandle: 'out-l', target: delay.id, targetHandle: 'in' }] };
    const after = deleteSelected(before.nodes, before.edges); expect(after.nodes.map((node) => node.id)).not.toContain('delay-1'); expect(after.edges).toHaveLength(0);
    const result = undoGraphEdit(commitGraphEdit(emptyGraphHistory(), 'Delete selection', before, after));
    expect(result.edit?.before).toEqual(before);
  });

  it('enforces exactly one stereo input and output', () => {
    const input = createModuleNode('stereo-input', 'input', { x: 0, y: 0 }); const output = createModuleNode('stereo-output', 'output', { x: 0, y: 0 });
    expect(requiredIoError([input, output])).toBeNull(); expect(requiredIoError([output])).toMatch('exactly one Stereo Input'); expect(requiredIoError([input, output, output])).toMatch('exactly one Stereo Output');
  });
});
