import { describe, expect, it } from 'vitest';
import type { Edge, Node } from '@xyflow/react';
import type { PatchNodeData } from './graph';
import { decorateFeedbackLoops, decorateRunawayFeedbackLoop, feedbackLoopTransitionBudget, inspectFeedbackLoops, inspectMostRelevantFeedbackLoop } from './loopInspection';
import { createModuleNode } from './modules';

const edge = (id: string, source: string, target: string): Edge => ({ id, source, target });
const gain = (id: string, value: number) => { const node = createModuleNode('gain', id, { x: 0, y: 0 }); node.data.parameters[0].value = value; return node; };

describe('feedback loop inspection', () => {
  it('finds nested loops containing a selected node and summarizes their elements', () => {
    const delay = createModuleNode('delay', 'delay', { x: 0, y: 0 }); delay.data.parameters[0].value = 12.5;
    const lowpass = createModuleNode('lowpass', 'filter', { x: 0, y: 0 }); lowpass.data.parameters[0].value = 6400;
    const nodes = [delay, gain('gain', -0.6), lowpass, createModuleNode('sum', 'sum', { x: 0, y: 0 })];
    const edges = [edge('a', 'sum', 'delay'), edge('b', 'delay', 'gain'), edge('c', 'gain', 'sum'), edge('d', 'gain', 'filter'), edge('e', 'filter', 'sum')];
    const result = inspectFeedbackLoops(nodes, edges, { nodeId: 'delay' });
    expect(result.loops.map((loop) => loop.edgeIds)).toEqual([['b', 'c', 'a'], ['b', 'd', 'e', 'a']]);
    expect(result.loops[0].nominalDelayMilliseconds).toBe(12.5);
    expect(result.loops[0].gainElements).toEqual([{ nodeId: 'gain', label: 'Gain', parameter: 'gain', value: -0.6, unit: 'linear' }]);
    expect(result.loops[1].filters).toEqual([{ nodeId: 'filter', label: 'Low-pass', parameter: 'cutoff', value: 6400, unit: 'hertz' }]);
  });

  it('distinguishes shared-edge loops and restricts cable selection to that edge', () => {
    const nodes = ['a', 'b', 'c', 'd'].map((id) => gain(id, 0.5));
    const edges = [edge('shared', 'a', 'b'), edge('bc', 'b', 'c'), edge('ca', 'c', 'a'), edge('bd', 'b', 'd'), edge('da', 'd', 'a')];
    const result = inspectFeedbackLoops(nodes, edges, { edgeId: 'shared' });
    expect(result.loops.map((loop) => loop.edgeIds)).toEqual([['shared', 'bc', 'ca'], ['shared', 'bd', 'da']]);
    expect(result.loops.every((loop) => loop.edgeIds.includes('shared'))).toBe(true);
    const before = structuredClone({ nodes, edges }); const decorated = decorateFeedbackLoops(nodes, edges, result, 0);
    expect(decorated.nodes.filter((node) => node.className === 'loop-active').map((node) => node.id)).toEqual(['a', 'b', 'c']);
    expect(decorated.nodes.find((node) => node.id === 'd')?.className).toBe('loop-related');
    expect(decorated.edges.filter((item) => item.className === 'loop-active').map((item) => item.id)).toEqual(['shared', 'bc', 'ca']);
    expect({ nodes, edges }).toEqual(before);
  });

  it('ranks and danger-highlights the most relevant delayed runaway loop', () => {
    const delayShort = createModuleNode('delay', 'delay-short', { x: 0, y: 0 }); delayShort.data.parameters[0].value = 10;
    const delayLong = createModuleNode('delay', 'delay-long', { x: 0, y: 0 }); delayLong.data.parameters[0].value = 80;
    const nodes = [delayShort, delayLong, gain('hot', 1.5), gain('quiet', 0.5), createModuleNode('sum', 'sum', { x: 0, y: 0 })];
    const edges = [
      edge('a', 'sum', 'delay-short'), edge('b', 'delay-short', 'hot'), edge('c', 'hot', 'sum'),
      edge('d', 'sum', 'delay-long'), edge('e', 'delay-long', 'quiet'), edge('f', 'quiet', 'sum'),
    ];
    const result = inspectMostRelevantFeedbackLoop(nodes, edges);
    expect(result.loops).toHaveLength(1);
    expect(result.loops[0]?.nodeIds).toContain('delay-short');
    const decorated = decorateRunawayFeedbackLoop(nodes, edges, result);
    expect(decorated.nodes.filter((node) => node.className?.includes('safety-loop-active')).length).toBeGreaterThan(0);
    expect(decorated.edges.filter((edge) => edge.className?.includes('safety-loop-active')).length).toBeGreaterThan(0);
    expect(result.exploredTransitions).toBeLessThanOrEqual(feedbackLoopTransitionBudget);
  });

  it('does not mutate the graph and ignores selections outside directed cycles', () => {
    const nodes = [gain('a', 1), gain('b', 1)]; const edges = [edge('ab', 'a', 'b')]; const before = structuredClone({ nodes, edges });
    expect(inspectFeedbackLoops(nodes, edges, { nodeId: 'a' }).loops).toEqual([]);
    expect({ nodes, edges }).toEqual(before);
  });

  it('stays bounded at the 256-node and 512-edge limit', () => {
    const nodes: Node<PatchNodeData>[] = Array.from({ length: 256 }, (_, index) => gain(`n${index}`, 0.5));
    const edges: Edge[] = [];
    for (let index = 0; index < 256; index++) {
      edges.push(edge(`ring-${index}`, `n${index}`, `n${(index + 1) % 256}`));
      edges.push(edge(`skip-${index}`, `n${index}`, `n${(index + 2) % 256}`));
    }
    const started = performance.now(); const result = inspectFeedbackLoops(nodes, edges, { nodeId: 'n0' }); const elapsed = performance.now() - started;
    expect(result.loops.length).toBeGreaterThan(0);
    expect(result.exploredTransitions).toBeLessThanOrEqual(feedbackLoopTransitionBudget);
    expect(result.truncated).toBe(true);
    expect(elapsed).toBeLessThan(250);
  });
});
