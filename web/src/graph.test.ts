import { describe, expect, it } from 'vitest';
import { createFlowModel, deleteSelected, parseRuntimeSnapshot, type RuntimeSnapshot } from './graph';

const snapshot: RuntimeSnapshot = {
  contractVersion: 2,
  engineId: 'barr-reference',
  sampleRate: 48000,
  nodes: [
    {
      id: 'tank-2', type: 'allpass', label: 'Tank 2', role: 'tank', position: { x: 20, y: 30 },
      ports: [
        { id: 'in', signal: 'audio', direction: 'input' },
        { id: 'delay-mod', signal: 'control', direction: 'input' },
        { id: 'coefficient-mod', signal: 'control', direction: 'input' },
        { id: 'out', signal: 'audio', direction: 'output' },
      ],
      parameters: [
        { id: 'delay', value: 19.91, unit: 'milliseconds', minimum: 0.1, maximum: 100, step: 0.01, modulation: { portId: 'delay-mod', amount: 2, polarity: 'bipolar', clampMinimum: 0.1, clampMaximum: 100 } },
        { id: 'coefficient', value: -0.5, unit: 'unitless', minimum: -0.95, maximum: 0.95, step: 0.001, modulation: { portId: 'coefficient-mod', amount: 0.25, polarity: 'bipolar', clampMinimum: -0.95, clampMaximum: 0.95 } },
      ],
    },
    {
      id: 'left-tap', type: 'allpass', label: 'Left Tap', role: 'tap', position: { x: 200, y: 30 },
      ports: [
        { id: 'in', signal: 'audio', direction: 'input' },
        { id: 'delay-mod', signal: 'control', direction: 'input' },
        { id: 'out', signal: 'audio', direction: 'output' },
      ],
      parameters: [{ id: 'delay', value: 29.71, unit: 'milliseconds', minimum: 0.1, maximum: 100, step: 0.01, modulation: { portId: 'delay-mod', amount: 2, polarity: 'bipolar', clampMinimum: 0.1, clampMaximum: 100 } }],
    },
  ],
  connections: [
    { id: 'tank-to-left', source: 'tank-2', sourcePort: 'out', target: 'left-tap', targetPort: 'in', signal: 'audio' },
  ],
  outsidePatch: [
    { id: 'master-audition-gain', purpose: 'audition output level' },
    { id: 'numerical-safety-guards', purpose: 'mute invalid output' },
  ],
};

describe('native runtime graph presentation model', () => {
  it('renders stable identities and live values from the native snapshot', () => {
    const parsed = parseRuntimeSnapshot(snapshot);
    const model = createFlowModel(parsed);
    expect(model.nodes.map((node) => node.id)).toEqual(['tank-2', 'left-tap']);
    expect(model.edges.map((edge) => edge.id)).toEqual(['tank-to-left']);
    expect(model.nodes[0]?.data.parameters[0]).toEqual({
      id: 'delay', value: 19.91, unit: 'milliseconds', minimum: 0.1, maximum: 100, step: 0.01,
      modulation: { portId: 'delay-mod', amount: 2, polarity: 'bipolar', clampMinimum: 0.1, clampMaximum: 100 },
    });
  });

  it('rejects contract and runtime identity mismatches before rendering', () => {
    expect(() => parseRuntimeSnapshot({ ...snapshot, contractVersion: 1 })).toThrow('unsupported contract version');
    expect(() => parseRuntimeSnapshot({ ...snapshot, engineId: 'different-engine' })).toThrow('unexpected engine identity');
    expect(() => parseRuntimeSnapshot({ ...snapshot, nodes: [...snapshot.nodes, snapshot.nodes[0]] })).toThrow('node IDs must be unique');
    expect(() => parseRuntimeSnapshot({
      ...snapshot,
      connections: [{ ...snapshot.connections[0], sourcePort: 'missing' }],
    })).toThrow('invalid source port');
    expect(() => parseRuntimeSnapshot({ ...snapshot, buildCommit: 'bad commit' })).toThrow('invalid build commit');
    expect(() => parseRuntimeSnapshot({ ...snapshot, restoredPatch: [] })).toThrow('invalid restored patch');
  });

  it('deletes selected nodes and incident connections from only the UI copy', () => {
    const model = createFlowModel(snapshot);
    const selected = model.nodes.map((node) => ({ ...node, selected: node.id === 'tank-2' }));
    const result = deleteSelected(selected, model.edges);
    expect(result.nodes.some((node) => node.id === 'tank-2')).toBe(false);
    expect(result.edges).toHaveLength(0);
    expect(createFlowModel(snapshot).nodes.some((node) => node.id === 'tank-2')).toBe(true);
  });

  it('marks audio cables with a semantic class independent of color', () => {
    const model = createFlowModel(snapshot);
    expect(model.edges.every((edge) => edge.className?.includes('signal-audio'))).toBe(true);
  });
});
