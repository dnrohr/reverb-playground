import { describe, expect, it } from 'vitest';
import { createModuleNode } from './modules';
import { decorateEnergy } from './energyDecoration';

describe('energy decoration', () => {
  it('decorates presentation copies by source energy without changing graph semantics', () => {
    const source = createModuleNode('gain', 'source', { x: 0, y: 0 });
    source.className = 'loop-active';
    const target = createModuleNode('delay', 'target', { x: 1, y: 1 });
    const edge = { id: 'cable', source: 'source', target: 'target', className: 'signal-audio' };
    const result = decorateEnergy([source, target], [edge], { source: .61 });
    expect(result.nodes[0]?.className).toBe('loop-active energy-level-4');
    expect(result.edges[0]?.className).toBe('signal-audio energy-level-4');
    expect(source.className).toBe('loop-active');
    expect(edge.className).toBe('signal-audio');
  });
});
