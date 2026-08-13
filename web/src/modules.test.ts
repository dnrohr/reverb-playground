import { describe, expect, it } from 'vitest';
import { createModuleNode, moduleDefinitions, nextNodeId } from './modules';

describe('editable module library', () => {
  it('defines every M3.1 primitive with safe deterministic defaults', () => {
    expect(moduleDefinitions.map((item) => item.type)).toEqual(['stereo-input', 'stereo-output', 'gain', 'sum', 'delay', 'allpass', 'lowpass', 'macro', 'lfo', 'control-map', 'envelope-follower', 'hold-gate']);
    for (const definition of moduleDefinitions) {
      const node = createModuleNode(definition.type, `${definition.type}-1`, { x: 10, y: 20 });
      expect(node.data.label).toBeTruthy(); expect(node.data.runtimeBound).toBe(false);
      expect(node.data.parameters.every((parameter) => parameter.value >= parameter.minimum && parameter.value <= parameter.maximum)).toBe(true);
    }
  });

  it('defines Macro as one named normalized control source', () => {
    const macro = createModuleNode('macro', 'macro-1', { x: 0, y: 0 });
    expect(macro.data.userName).toBe('Macro');
    expect(macro.data.ports).toEqual([{ id: 'out', signal: 'control', direction: 'output' }]);
    expect(macro.data.parameters.map(({ id, value, unit }) => [id, value, unit])).toEqual([
      ['value', 0, 'normalized'], ['default-value', 0, 'normalized'], ['center-detent', 1, 'boolean'],
    ]);
  });

  it('makes follower and gate signal direction visible without hidden detection', () => {
    const follower = createModuleNode('envelope-follower', 'follower-1', { x: 0, y: 0 });
    const gate = createModuleNode('hold-gate', 'gate-1', { x: 0, y: 0 });
    expect(follower.data.ports.find((port) => port.id === 'in')?.signal).toBe('audio');
    expect(follower.data.ports.find((port) => port.id === 'out')?.signal).toBe('control');
    expect(gate.data.ports.find((port) => port.id === 'gate')?.signal).toBe('control');
    expect(gate.data.ports.filter((port) => port.signal === 'audio').map((port) => port.id)).toEqual(['in', 'out']);
    expect(follower.data.parameters.every((parameter) => parameter.modulation === undefined)).toBe(true);
    expect(gate.data.parameters.every((parameter) => parameter.modulation === undefined)).toBe(true);
  });

  it('allocates stable collision-free IDs', () => {
    const first = createModuleNode('delay', 'delay-1', { x: 0, y: 0 });
    expect(nextNodeId('delay', [first])).toBe('delay-2');
    expect(first.id).toBe('delay-1');
  });
});
