import { describe, expect, it } from 'vitest';
import type { Edge } from '@xyflow/react';
import { createModuleNode } from './modules';
import { decorateSplitShimmerFocus, splitFeedbackShimmerTeaching, splitShimmerLoopKind } from './splitFeedbackShimmerTeaching';

describe('Split-Feedback Shimmer teaching contract', () => {
  it('ties visible circulation to checked +12 and +24 evidence without proprietary claims', () => {
    expect(splitFeedbackShimmerTeaching.title).toBe('FEEDBACK SHIMMER');
    expect(splitFeedbackShimmerTeaching.method).toContain('REPEATED +12');
    expect(splitFeedbackShimmerTeaching.circulation.map((step) => step.frequency)).toEqual(['400 Hz', '800 Hz', '1600 Hz']);
    expect(splitFeedbackShimmerTeaching.evidence).toContain('40.48 dB');
    expect(splitFeedbackShimmerTeaching.evidence).toContain('parallel reference');
    expect(splitFeedbackShimmerTeaching.boundary).toMatch(/not a reconstruction of a proprietary/);
  });

  it('classifies normal shifted and mixed loop paths', () => {
    expect(splitShimmerLoopKind(['tank-delay', 'normal-feedback'])).toBe('NORMAL RETURN');
    expect(splitShimmerLoopKind(['tank-delay', 'shifted-pitch', 'shifted-feedback'])).toBe('SHIFTED RETURN');
    expect(splitShimmerLoopKind(['tank-delay'])).toBe('MIXED / SHARED');
  });

  it('isolates normal shifted and shared focus without mutating the graph', () => {
    const ids = ['tank-entry', 'tank-delay', 'tank-damping', 'feedback-recombine', 'normal-feedback', 'normal-feedback-delay', 'shifted-pitch', 'shifted-feedback', 'shifted-feedback-delay', 'output'];
    const nodes = ids.map((id) => createModuleNode(id.includes('delay') ? 'delay' : 'gain', id, { x: 0, y: 0 }));
    const edges: Edge[] = [
      { id: 'feedback-tank', source: 'feedback-recombine', target: 'tank-entry' },
      { id: 'tank-delay', source: 'tank-entry', target: 'tank-delay' },
      { id: 'normal-feedback', source: 'tank-damping', target: 'normal-feedback' },
      { id: 'shifted-pitch', source: 'tank-damping', target: 'shifted-pitch' },
    ];
    const before = structuredClone({ nodes, edges });
    const normal = decorateSplitShimmerFocus(nodes, edges, 'normal');
    const shifted = decorateSplitShimmerFocus(nodes, edges, 'shifted');
    const shared = decorateSplitShimmerFocus(nodes, edges, 'shared');
    expect(normal.nodes.find((node) => node.id === 'normal-feedback')?.className).toContain('split-focus-active');
    expect(normal.nodes.find((node) => node.id === 'shifted-pitch')?.className).toContain('split-focus-related');
    expect(shifted.nodes.find((node) => node.id === 'shifted-pitch')?.className).toContain('split-focus-active');
    expect(shared.nodes.find((node) => node.id === 'tank-delay')?.className).toContain('split-focus-active');
    expect(shared.nodes.find((node) => node.id === 'normal-feedback')?.className).toContain('split-focus-related');
    expect(shared.nodes.find((node) => node.id === 'shifted-pitch')?.className).toContain('split-focus-related');
    expect({ nodes, edges }).toEqual(before);
  });
});
