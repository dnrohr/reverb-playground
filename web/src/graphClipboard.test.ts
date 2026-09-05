import { describe, expect, it } from 'vitest';
import type { GraphState } from './graph';
import { copySelectedGraph, pasteGraph } from './graphClipboard';
import { createModuleNode } from './modules';

describe('graph clipboard', () => {
  it('pastes new IDs while preserving parameters and internal connections', () => {
    const delay = { ...createModuleNode('delay', 'delay-1', { x: 10, y: 20 }), selected: true };
    delay.data.parameters[0].value = 37.25;
    delay.data.orientation = 'reverse';
    const gain = { ...createModuleNode('gain', 'gain-1', { x: 180, y: 20 }), selected: true };
    const graph: GraphState = {
      nodes: [delay, gain],
      edges: [{ id: 'delay-gain', source: delay.id, sourceHandle: 'out', target: gain.id, targetHandle: 'in' }],
    };
    const clipboard = copySelectedGraph(graph)!;
    const pasted = pasteGraph(graph, clipboard);
    expect(pasted.nodes.map((node) => node.id)).toEqual(['delay-1', 'gain-1', 'delay-1-copy', 'gain-1-copy']);
    expect(pasted.nodes[2].data.parameters[0].value).toBe(37.25);
    expect(pasted.nodes[2].data.orientation).toBe('reverse');
    expect(pasted.nodes[2].position).toEqual({ x: 50, y: 60 });
    expect(pasted.edges[1]).toMatchObject({ id: 'delay-gain-copy', source: 'delay-1-copy', target: 'gain-1-copy', sourceHandle: 'out', targetHandle: 'in' });
  });

  it('copies a Macro with its name and settings but assigns a fresh stable ID', () => {
    const macro = createModuleNode('macro', 'macro-gravity', { x: 10, y: 20 });
    macro.selected = true;
    macro.data.userName = 'Gravity';
    macro.data.parameters.find((parameter) => parameter.id === 'value')!.value = 0.42;
    const graph = { nodes: [macro], edges: [] };
    const pasted = pasteGraph(graph, copySelectedGraph(graph)!, 40);
    expect(pasted.nodes[1].id).toBe('macro-gravity-copy');
    expect(pasted.nodes[1].data.userName).toBe('Gravity');
    expect(pasted.nodes[1].data.parameters.find((parameter) => parameter.id === 'value')!.value).toBe(0.42);
  });

  it('copies only selected non-I/O nodes and their internal cables', () => {
    const input = { ...createModuleNode('stereo-input', 'input', { x: 0, y: 0 }), selected: true };
    const delay = { ...createModuleNode('delay', 'delay', { x: 100, y: 0 }), selected: true };
    const gain = createModuleNode('gain', 'gain', { x: 200, y: 0 });
    const clipboard = copySelectedGraph({ nodes: [input, delay, gain], edges: [
      { id: 'input-delay', source: 'input', target: 'delay' }, { id: 'delay-gain', source: 'delay', target: 'gain' },
    ] });
    expect(clipboard?.nodes.map((node) => node.id)).toEqual(['delay']);
    expect(clipboard?.edges).toEqual([]);
    expect(copySelectedGraph({ nodes: [input, gain], edges: [] })).toBeNull();
  });
});
