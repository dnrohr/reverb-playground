import { describe, expect, it } from 'vitest';
import { applyAuditionOverlay, toggleAuditionOverlay } from './auditionOverlay';
import { createModuleNode } from './modules';

const edge = (id: string, source: string, target: string, targetHandle = 'in') => ({ id, source, sourceHandle: 'out', target, targetHandle, data: { signal: 'audio' } });
const graph = () => ({ nodes: [
  createModuleNode('stereo-input', 'input', { x: 0, y: 0 }), createModuleNode('gain', 'gain', { x: 100, y: 0 }),
  createModuleNode('delay', 'delay', { x: 200, y: 0 }), createModuleNode('stereo-output', 'output', { x: 300, y: 0 }),
], edges: [edge('a', 'input', 'gain'), edge('b', 'gain', 'delay'), edge('c', 'delay', 'output', 'in-l')] });

describe('temporary audition overlays', () => {
  it('mutes and isolates without mutating the saved graph', () => {
    const saved = graph(); const original = structuredClone(saved);
    expect(applyAuditionOverlay(saved, { kind: 'mute', target: 'edge', id: 'b' }).graph.edges.map((item) => item.id)).toEqual(['a', 'c']);
    expect(applyAuditionOverlay(saved, { kind: 'isolate', target: 'node', id: 'gain' }).accepted).toBe(true);
    expect(saved).toEqual(original);
  });

  it('creates an explicit temporary bypass and discloses latency change', () => {
    const result = applyAuditionOverlay(graph(), { kind: 'bypass', target: 'node', id: 'gain' });
    expect(result).toMatchObject({ accepted: true, latencyChanged: true, message: expect.stringMatching(/latency/) });
    expect(result.graph.nodes.some((node) => node.id === 'gain')).toBe(false);
    expect(result.graph.edges).toContainEqual(expect.objectContaining({ source: 'input', target: 'delay', id: 'audition-bypass-gain-1' }));
  });

  it('refuses required I/O, control cables, ambiguous fan-in, and algebraic bypass', () => {
    expect(applyAuditionOverlay(graph(), { kind: 'mute', target: 'node', id: 'input' }).accepted).toBe(false);
    const control = graph(); control.edges[0]!.data.signal = 'control'; expect(applyAuditionOverlay(control, { kind: 'mute', target: 'edge', id: 'a' }).accepted).toBe(false);
    const ambiguous = graph(); ambiguous.edges.push(edge('extra', 'input', 'gain')); expect(applyAuditionOverlay(ambiguous, { kind: 'bypass', target: 'node', id: 'gain' }).accepted).toBe(false);
    const loop = graph(); loop.edges.push(edge('loop', 'delay', 'gain'));
    expect(applyAuditionOverlay(loop, { kind: 'bypass', target: 'node', id: 'delay' }).message).toMatch(/zero-delay/);
  });

  it('toggles the active operation off and switches another operation atomically', () => {
    const mute = { kind: 'mute', target: 'node', id: 'gain' } as const;
    const isolate = { kind: 'isolate', target: 'node', id: 'gain' } as const;
    expect(toggleAuditionOverlay(null, mute)).toEqual(mute);
    expect(toggleAuditionOverlay(mute, mute)).toBeNull();
    expect(toggleAuditionOverlay(mute, isolate)).toEqual(isolate);
    const original = graph();
    const preview = applyAuditionOverlay(original, mute).graph;
    expect(preview).not.toEqual(original);
    expect(applyAuditionOverlay(original, toggleAuditionOverlay(mute, mute)).graph).toBe(original);
  });
});
