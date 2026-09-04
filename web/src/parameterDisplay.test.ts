import { describe, expect, it } from 'vitest';
import { createModuleNode } from './modules';
import { acceptParameterPublication, resolveParameterDisplay, type ParameterUpdateSource } from './parameterDisplay';

describe('authoritative parameter display', () => {
  it.each(['pointer', 'numeric', 'control-modulation', 'matrix-edit', 'assisted-preview', 'factory-load',
    'patch-restore', 'host-restore', 'automation', 'native-snapshot', 'undo', 'redo', 'ab-promotion',
    'topology-publication'] as ParameterUpdateSource[])('accepts the newest %s publication without UI feedback', (source) => {
    const current = { revision: 8, source: 'pointer' as const, value: 0.25 };
    expect(acceptParameterPublication(current, { revision: 9, source, value: 0.5 })).toEqual({ revision: 9, source, value: 0.5 });
    expect(acceptParameterPublication(current, { revision: 7, source, value: 0.75 })).toBe(current);
  });

  it('distinguishes saved, live-modulated, preview, and pending topology values', () => {
    const gain = createModuleNode('gain', 'gain-1', { x: 0, y: 0 });
    const parameter = gain.data.parameters.find((item) => item.id === 'gain')!;
    parameter.value = 0.4;
    parameter.modulation = { portId: 'gain-mod', amount: 0.2, polarity: 'bipolar', clampMinimum: 0, clampMaximum: 1 };
    const preview = structuredClone(gain); preview.data.parameters.find((item) => item.id === 'gain')!.value = 0.7;
    const state = resolveParameterDisplay(gain, parameter, [{ id: 'control', source: 'macro', target: 'gain-1', targetHandle: 'gain-mod',
      data: { signal: 'control', controlValue: 0.5 } }], { nodes: [preview], edges: [] }, true);
    expect(state).toMatchObject({ savedBase: 0.4, liveValue: 0.5, previewValue: 0.7, pendingTopology: true, liveModulated: true });
    expect(state.label).toBe('SAVED BASE · LIVE MODULATED · AUDITION PREVIEW · PENDING TOPOLOGY');
  });
});
