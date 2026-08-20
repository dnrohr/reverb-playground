import { renderToStaticMarkup } from 'react-dom/server';
import { describe, expect, it } from 'vitest';
import { deleteSelected, type GraphState } from './graph';
import { copySelectedGraph, pasteGraph } from './graphClipboard';
import { commitGraphEdit, emptyGraphHistory, redoGraphEdit, undoGraphEdit } from './graphHistory';
import { connectGraph } from './connectionEditing';
import { createModuleNode } from './modules';
import { PitchShiftVisualization } from './PitchShiftVisualization';

describe('visible Pitch Shift block', () => {
  it('connects as a mono audio processor with typed cables', () => {
    const input = createModuleNode('stereo-input', 'input', { x: 0, y: 0 });
    const pitch = createModuleNode('pitch-shift', 'pitch-shift-1', { x: 240, y: 0 });
    const output = createModuleNode('stereo-output', 'output', { x: 480, y: 0 });
    let state: GraphState = { nodes: [input, pitch, output], edges: [] };
    state = connectGraph(state, { source: 'input', sourceHandle: 'out-l', target: 'pitch-shift-1', targetHandle: 'in' });
    state = connectGraph(state, { source: 'pitch-shift-1', sourceHandle: 'out', target: 'output', targetHandle: 'in-l' });
    expect(state.edges.map((edge) => edge.data?.signal)).toEqual(['audio', 'audio']);
    expect(state.edges.map((edge) => `${edge.sourceHandle}->${edge.targetHandle}`)).toEqual(['out-l->in', 'out->in-l']);
  });

  it('supports copy, delete, undo, and redo without losing controls', () => {
    const pitch = createModuleNode('pitch-shift', 'pitch-shift-1', { x: 100, y: 80 });
    pitch.selected = true;
    pitch.data.parameters.find((parameter) => parameter.id === 'direction')!.value = 1;
    const before: GraphState = { nodes: [pitch], edges: [] };
    const pasted = pasteGraph(before, copySelectedGraph(before)!);
    expect(pasted.nodes[1].id).toBe('pitch-shift-1-copy');
    expect(pasted.nodes[1].data.parameters.find((parameter) => parameter.id === 'direction')?.value).toBe(1);

    pasted.nodes[1].selected = true;
    const deleted = deleteSelected(pasted.nodes, pasted.edges);
    const history = commitGraphEdit(emptyGraphHistory(pasted), 'Delete Pitch Shift', pasted, deleted);
    const undone = undoGraphEdit(history);
    expect(undone.edit?.before.nodes.some((node) => node.id === 'pitch-shift-1-copy')).toBe(true);
    const redone = redoGraphEdit(undone.history);
    expect(redone.edit?.after.nodes.some((node) => node.id === 'pitch-shift-1-copy')).toBe(false);
  });

  it('labels animation as illustrative and supplies a static reduced-motion state', () => {
    const parameters = createModuleNode('pitch-shift', 'pitch-shift-1', { x: 0, y: 0 }).data.parameters;
    parameters.find((parameter) => parameter.id === 'direction')!.value = 1;
    const markup = renderToStaticMarkup(<PitchShiftVisualization parameters={parameters} reducedMotion sampleRate={48_000} />);
    expect(markup).toContain('Illustrative dual grain phase, reverse');
    expect(markup).toContain('is-reduced');
    expect(markup).toContain('28,802 samples');
    expect(markup).toContain('not measured audio or sample-accurate');
    expect(markup).toContain('not a fixed-Hz frequency shift or moving-Delay Doppler effect');
    expect(markup).toContain('not the whole reverb');
  });
});
