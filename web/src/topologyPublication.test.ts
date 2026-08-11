import { describe, expect, it } from 'vitest';
import { createModuleNode } from './modules';
import { audibleGraphFingerprint, parseGraphPublicationResult } from './topologyPublication';

describe('topology publication bridge', () => {
  it('ignores layout and selection but changes for audible parameters and cables', () => {
    const input = createModuleNode('stereo-input', 'input', { x: 0, y: 0 });
    const output = createModuleNode('stereo-output', 'output', { x: 100, y: 0 });
    const base = audibleGraphFingerprint([input, output], []);
    expect(audibleGraphFingerprint([{ ...input, selected: true, position: { x: 99, y: 42 } }, output], [])).toBe(base);
    const gain = createModuleNode('gain', 'gain', { x: 50, y: 0 });
    const withGain = audibleGraphFingerprint([input, gain, output], []);
    gain.data.parameters[0].value = 0.25;
    expect(audibleGraphFingerprint([input, gain, output], [])).not.toBe(withGain);
    expect(audibleGraphFingerprint([input, output], [{ id: 'c', source: 'input', sourceHandle: 'out-l', target: 'output', targetHandle: 'in-l' }])).not.toBe(base);
  });

  it('accepts coherent native results and rejects contradictory ones', () => {
    expect(parseGraphPublicationResult('{"accepted":true,"revision":4,"error":""}').revision).toBe(4);
    expect(() => parseGraphPublicationResult({ accepted: true, revision: 0, error: '' })).toThrow(/inconsistent/);
    expect(() => parseGraphPublicationResult({ accepted: false, revision: 2, error: 'bad' })).toThrow(/inconsistent/);
  });
});
