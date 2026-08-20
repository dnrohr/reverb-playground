import { describe, expect, it } from 'vitest';
import type { RuntimeSnapshot } from './graph';
import { loadFactoryPatch } from './factoryPatches';
import { decorateReverseCosmicFocus, reverseCosmicShimmerTeaching } from './reverseCosmicShimmerTeaching';

const snapshot: RuntimeSnapshot = { contractVersion: 2, engineId: 'barr-reference', sampleRate: 48_000, nodes: [], connections: [], outsidePatch: [] };

describe('Reverse Cosmic Shimmer teaching', () => {
  it('states the causal and authorship boundaries with measured evidence', () => {
    expect(reverseCosmicShimmerTeaching.grains).toMatch(/backward only inside an already buffered grain/);
    expect(reverseCosmicShimmerTeaching.boundary).toMatch(/not true sample-order reverse reverb/);
    expect(reverseCosmicShimmerTeaching.boundary).toMatch(/Project-authored behavioral synthesis/);
    expect(reverseCosmicShimmerTeaching.evidence).toContain('more than 23 dB');
  });

  it('focuses each principal path without changing the factory graph', () => {
    const graph = loadFactoryPatch('reverse-cosmic-shimmer', snapshot);
    const original = structuredClone(graph);
    const rise = decorateReverseCosmicFocus(graph.nodes, graph.edges, 'rise');
    const grains = decorateReverseCosmicFocus(graph.nodes, graph.edges, 'grains');
    const feedback = decorateReverseCosmicFocus(graph.nodes, graph.edges, 'feedback');
    const motion = decorateReverseCosmicFocus(graph.nodes, graph.edges, 'motion');
    expect(rise.nodes.find((node) => node.id === 'rise-delay-late')?.className).toContain('reverse-cosmic-focus-active');
    expect(grains.nodes.find((node) => node.id === 'reverse-pitch-right')?.className).toContain('reverse-cosmic-focus-active');
    expect(feedback.edges.find((edge) => edge.id === 'shimmer-recombine')?.className).toContain('reverse-cosmic-focus-active');
    expect(motion.edges.find((edge) => edge.id === 'motion-output-right-b')?.className).toContain('reverse-cosmic-focus-active');
    expect(graph).toEqual(original);
  });
});
