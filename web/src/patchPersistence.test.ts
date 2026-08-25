import { describe, expect, it } from 'vitest';
import { createFlowModel, type RuntimeSnapshot } from './graph';
import { parsePatchJson, writePatchJson } from './patchPersistence';
import { createModuleNode } from './modules';
import { semanticGraphHash } from './graphHistory';
import sharedEdgeLoopFixture from '../../artifacts/ui/m4-1-feedback-loop-highlighting/shared-edge-loop-fixture.rvp.json?raw';
import schemaV1MigrationFixture from '../../tests/fixtures/patches/valid/schema-v1-migration.json?raw';

const reference: RuntimeSnapshot = {
  contractVersion: 2, engineId: 'barr-reference', sampleRate: 48000, outsidePatch: [],
  nodes: [{
    id: 'sum', type: 'sum', label: 'Mono Sum', role: 'routing', position: { x: 1, y: 2 },
    ports: [
      { id: 'in', signal: 'audio', direction: 'input' },
      { id: 'gain-mod', signal: 'control', direction: 'input' },
      { id: 'out', signal: 'audio', direction: 'output' },
    ],
    parameters: [{ id: 'gain', value: 0.5, unit: 'linear', minimum: 0, maximum: 1, step: 0.001, modulation: { portId: 'gain-mod', amount: 0.5, polarity: 'bipolar', clampMinimum: 0, clampMaximum: 1 } }],
  }],
  connections: [],
};

describe('patch persistence', () => {
  it('migrates every released schema version to deterministic schema v2', () => {
    const migrated = parsePatchJson(schemaV1MigrationFixture, reference);
    const gain = migrated.nodes.find((node) => node.id === 'legacy-gain')!;
    expect(gain.data.parameters).toEqual([
      expect.objectContaining({
        id: 'gain', value: 0.375, unit: 'linear',
        modulation: { portId: 'gain-mod', amount: 0.5, polarity: 'bipolar', clampMinimum: -1, clampMaximum: 1 },
      }),
    ]);
    expect(gain.data.ports.map((port) => port.id)).toEqual(['in', 'gain-mod', 'out']);
    expect(migrated.viewport).toEqual({ x: 12, y: -8, zoom: 0.875 });
    const versionTwo = writePatchJson(migrated.nodes, migrated.edges, migrated.viewport);
    expect(JSON.parse(versionTwo).schemaVersion).toBe(2);
    const roundTrip = parsePatchJson(versionTwo, reference);
    expect(writePatchJson(roundTrip.nodes, roundTrip.edges, roundTrip.viewport)).toBe(versionTwo);
  });

  it('round trips semantics, exact parameter values, positions, and viewport deterministically', () => {
    const flow = createFlowModel(reference);
    flow.nodes.push(createModuleNode('stereo-input', 'stereo-input-1', { x: -200, y: 0 }), createModuleNode('stereo-output', 'stereo-output-1', { x: 200, y: 0 }));
    flow.nodes[0].data.parameters[0].value = 0.371;
    flow.nodes[0].data.parameters[0].modulation = { portId: 'gain-mod', amount: -0.275, polarity: 'unipolar', clampMinimum: 0.1, clampMaximum: 0.9 };
    flow.nodes[0].position = { x: -42.25, y: 319.75 };
    const viewport = { x: 17.5, y: -88.25, zoom: 1.375 };
    const semanticBeforeSave = semanticGraphHash(flow);
    const written = writePatchJson(flow.nodes, flow.edges, viewport);
    expect(semanticGraphHash(flow)).toBe(semanticBeforeSave);
    const loaded = parsePatchJson(written, reference);

    expect(loaded.nodes[0].data.parameters[0].value).toBe(0.371);
    expect(loaded.nodes[0].data.parameters[0].modulation).toEqual({ portId: 'gain-mod', amount: -0.275, polarity: 'unipolar', clampMinimum: 0.1, clampMaximum: 0.9 });
    expect(loaded.nodes[0].position).toEqual(flow.nodes[0].position);
    expect(loaded.viewport).toEqual(viewport);
    expect(writePatchJson(loaded.nodes, loaded.edges, loaded.viewport)).toBe(written);
  });

  it('migrates legacy Pitch Shift values into one octave with a visible warning', () => {
    const flow = createFlowModel(reference);
    flow.nodes.push(createModuleNode('stereo-input', 'stereo-input-1', { x: -200, y: 0 }), createModuleNode('pitch-shift', 'pitch-shift-1', { x: 0, y: 0 }), createModuleNode('stereo-output', 'stereo-output-1', { x: 200, y: 0 }));
    const saved = JSON.parse(writePatchJson(flow.nodes, flow.edges, { x: 0, y: 0, zoom: 1 }));
    const pitch = saved.semantic.nodes.find((node: { type: string }) => node.type === 'pitch-shift');
    pitch.parameters[0].value = -24;
    pitch.parameters[0].modulation.clampMinimum = -24;
    pitch.parameters[0].modulation.clampMaximum = 24;
    const loaded = parsePatchJson(JSON.stringify(saved), reference);
    const migrated = loaded.nodes.find((node) => node.id === 'pitch-shift-1')!.data.parameters[0];
    expect(migrated).toMatchObject({ value: -12, minimum: -12, maximum: 12, modulation: { clampMinimum: -12, clampMaximum: 12 } });
    expect(loaded.warnings).toEqual([expect.stringMatching(/one-octave range/)]);
  });

  it('rejects invalid and future documents before returning replacement state', () => {
    const flow = createFlowModel(reference);
    flow.nodes.push(createModuleNode('stereo-input', 'stereo-input-1', { x: -200, y: 0 }), createModuleNode('stereo-output', 'stereo-output-1', { x: 200, y: 0 }));
    const untouched = structuredClone(flow);
    const valid = JSON.parse(writePatchJson(flow.nodes, flow.edges, { x: 0, y: 0, zoom: 1 })) as Record<string, unknown>;
    expect(() => parsePatchJson('{ bad json', reference)).toThrow(/invalid JSON/);
    expect(() => parsePatchJson(JSON.stringify({ ...valid, schemaVersion: 3 }), reference)).toThrow(/unsupported schemaVersion/);
    expect(() => parsePatchJson(JSON.stringify({ ...valid, futureField: true }), reference)).toThrow(/unknown field 'futureField'/);

    const semantic = valid.semantic as { nodes: Array<{ parameters: Array<{ value: number }> }> };
    semantic.nodes[0].parameters[0].value = 2;
    expect(() => parsePatchJson(JSON.stringify(valid), reference)).toThrow(/outside 0\.\.1/);
    expect(flow).toEqual(untouched);

    semantic.nodes[0].parameters[0] = { ...semantic.nodes[0].parameters[0], futureUnitHint: 'percent' } as never;
    expect(() => parsePatchJson(JSON.stringify(valid), reference)).toThrow(/unknown field 'futureUnitHint'/);
  });

  it('round trips created primitives and rejects missing required I/O', () => {
    const flow = createFlowModel(reference);
    flow.nodes.push(createModuleNode('stereo-input', 'stereo-input-1', { x: -200, y: 0 }), createModuleNode('delay', 'delay-1', { x: 100, y: 20 }), createModuleNode('stereo-output', 'stereo-output-1', { x: 300, y: 0 }));
    const json = writePatchJson(flow.nodes, flow.edges, { x: 0, y: 0, zoom: 1 });
    const loaded = parsePatchJson(json, reference);
    expect(loaded.nodes.find((node) => node.id === 'delay-1')?.data.runtimeBound).toBe(false);
    const withoutInput = flow.nodes.filter((node) => node.data.type !== 'stereo-input');
    expect(() => parsePatchJson(writePatchJson(withoutInput, [], { x: 0, y: 0, zoom: 1 }), reference)).toThrow(/exactly one Stereo Input/);
  });

  it('round trips LFO and mapping blocks with a branched control output exactly', () => {
    const input = createModuleNode('stereo-input', 'stereo-input-1', { x: 0, y: 0 });
    const output = createModuleNode('stereo-output', 'stereo-output-1', { x: 600, y: 0 });
    const lfo = createModuleNode('lfo', 'lfo-1', { x: 100, y: 200 });
    const mapper = createModuleNode('control-map', 'control-map-1', { x: 300, y: 200 });
    const gain = createModuleNode('gain', 'gain-1', { x: 500, y: 150 });
    const delay = createModuleNode('delay', 'delay-1', { x: 500, y: 280 });
    lfo.data.parameters.find((parameter) => parameter.id === 'frequency')!.value = 0.37;
    lfo.data.parameters.find((parameter) => parameter.id === 'waveform')!.value = 1;
    mapper.data.parameters.find((parameter) => parameter.id === 'scale')!.value = -0.4;
    mapper.data.parameters.find((parameter) => parameter.id === 'offset')!.value = 0.2;
    mapper.data.parameters.find((parameter) => parameter.id === 'curve-family')!.value = 2;
    mapper.data.parameters.find((parameter) => parameter.id === 'curve-amount')!.value = -3.5;
    mapper.data.parameters.find((parameter) => parameter.id === 'exponent')!.value = 2.25;
    mapper.data.parameters.find((parameter) => parameter.id === 'clamp-min')!.value = -0.7;
    mapper.data.parameters.find((parameter) => parameter.id === 'clamp-max')!.value = 0.85;
    const edges = [
      { id: 'lfo-map', source: lfo.id, sourceHandle: 'out', target: mapper.id, targetHandle: 'in' },
      { id: 'map-gain', source: mapper.id, sourceHandle: 'out', target: gain.id, targetHandle: 'gain-mod' },
      { id: 'map-delay', source: mapper.id, sourceHandle: 'out', target: delay.id, targetHandle: 'delay-mod' },
    ];
    const written = writePatchJson([input, output, lfo, mapper, gain, delay], edges, { x: 3, y: 4, zoom: 0.8 });
    const loaded = parsePatchJson(written, reference);
    expect(loaded.edges.filter((edge) => edge.source === mapper.id)).toHaveLength(2);
    expect(loaded.nodes.find((node) => node.id === lfo.id)?.data.parameters.find((parameter) => parameter.id === 'waveform')?.value).toBe(1);
    expect(loaded.nodes.find((node) => node.id === mapper.id)?.data.parameters.map((parameter) => [parameter.id, parameter.value]).slice(3)).toEqual([
      ['curve-family', 2], ['curve-amount', -3.5], ['exponent', 2.25], ['clamp-min', -0.7], ['clamp-max', 0.85],
    ]);
    expect(writePatchJson(loaded.nodes, loaded.edges, loaded.viewport)).toBe(written);
  });

  it('round trips a named Macro with stable IDs and all exposed settings', () => {
    const input = createModuleNode('stereo-input', 'stereo-input-1', { x: 0, y: 0 });
    const output = createModuleNode('stereo-output', 'stereo-output-1', { x: 700, y: 0 });
    const macro = createModuleNode('macro', 'macro-gravity', { x: 100, y: 220 });
    const mapper = createModuleNode('control-map', 'gravity-time-map', { x: 330, y: 220 });
    const delay = createModuleNode('delay', 'gravity-delay', { x: 560, y: 180 });
    macro.data.userName = 'Gravity';
    macro.data.presentation = 'gravity';
    macro.data.parameters.find((parameter) => parameter.id === 'value')!.value = 0.375;
    macro.data.parameters.find((parameter) => parameter.id === 'default-value')!.value = -0.125;
    macro.data.parameters.find((parameter) => parameter.id === 'center-detent')!.value = 1;
    const edges = [
      { id: 'gravity-map', source: macro.id, sourceHandle: 'out', target: mapper.id, targetHandle: 'in' },
      { id: 'gravity-delay-time', source: mapper.id, sourceHandle: 'out', target: delay.id, targetHandle: 'delay-mod' },
    ];
    const written = writePatchJson([input, output, macro, mapper, delay], edges, { x: 9, y: -4, zoom: 0.9 });
    const loaded = parsePatchJson(written, reference);
    const restored = loaded.nodes.find((node) => node.id === 'macro-gravity')!;
    expect(restored.data.userName).toBe('Gravity');
    expect(restored.data.presentation).toBe('gravity');
    expect(restored.data.parameters.map(({ id, value }) => [id, value])).toEqual([
      ['value', 0.375], ['default-value', -0.125], ['center-detent', 1],
    ]);
    expect(loaded.edges.map((edge) => edge.id)).toEqual(['gravity-map', 'gravity-delay-time']);
    expect(writePatchJson(loaded.nodes, loaded.edges, loaded.viewport)).toBe(written);
  });

  it('rejects unsupported or non-Macro presentation designations', () => {
    const input = createModuleNode('stereo-input', 'stereo-input-1', { x: 0, y: 0 });
    const output = createModuleNode('stereo-output', 'stereo-output-1', { x: 400, y: 0 });
    const macro = createModuleNode('macro', 'macro-1', { x: 200, y: 100 });
    macro.data.presentation = 'gravity';
    const raw = JSON.parse(writePatchJson([input, macro, output], [], { x: 0, y: 0, zoom: 1 })) as {
      semantic: { nodes: Array<Record<string, unknown>> };
    };
    raw.semantic.nodes.find((node) => node.id === 'macro-1')!.presentation = 'hidden-special';
    expect(() => parsePatchJson(JSON.stringify(raw), reference)).toThrow(/unsupported presentation/);
    raw.semantic.nodes.find((node) => node.id === 'macro-1')!.presentation = undefined;
    raw.semantic.nodes.find((node) => node.id === 'stereo-input-1')!.presentation = 'gravity';
    expect(() => parsePatchJson(JSON.stringify(raw), reference)).toThrow(/unsupported presentation/);
  });

  it('upgrades legacy schema-v2 Scale / Offset nodes to a linear Curve Mapper', () => {
    const input = createModuleNode('stereo-input', 'stereo-input-1', { x: 0, y: 0 });
    const output = createModuleNode('stereo-output', 'stereo-output-1', { x: 600, y: 0 });
    const mapper = createModuleNode('control-map', 'control-map-1', { x: 300, y: 200 });
    const raw = JSON.parse(writePatchJson([input, mapper, output], [], { x: 0, y: 0, zoom: 1 })) as {
      semantic: { nodes: Array<{ type: string; parameters: unknown[] }> };
    };
    raw.semantic.nodes.find((node) => node.type === 'control-map')!.parameters.splice(3);
    const loaded = parsePatchJson(JSON.stringify(raw), reference);
    const values = loaded.nodes.find((node) => node.id === mapper.id)!.data.parameters;
    expect(values.find((parameter) => parameter.id === 'curve-family')?.value).toBe(0);
    expect(values.find((parameter) => parameter.id === 'exponent')?.value).toBe(1);
    expect(values.find((parameter) => parameter.id === 'clamp-min')?.value).toBe(-1);
    expect(values.find((parameter) => parameter.id === 'clamp-max')?.value).toBe(1);
  });

  it('round trips follower and gate base controls without inventing modulation sockets', () => {
    const input = createModuleNode('stereo-input', 'stereo-input-1', { x: 0, y: 0 });
    const follower = createModuleNode('envelope-follower', 'envelope-follower-1', { x: 180, y: 180 });
    const gate = createModuleNode('hold-gate', 'hold-gate-1', { x: 380, y: 0 });
    const output = createModuleNode('stereo-output', 'stereo-output-1', { x: 600, y: 0 });
    follower.data.parameters.find((parameter) => parameter.id === 'attack')!.value = 7.5;
    follower.data.parameters.find((parameter) => parameter.id === 'release')!.value = 321;
    gate.data.parameters.find((parameter) => parameter.id === 'threshold')!.value = 0.63;
    gate.data.parameters.find((parameter) => parameter.id === 'attack')!.value = 4.2;
    gate.data.parameters.find((parameter) => parameter.id === 'hold')!.value = 280;
    gate.data.parameters.find((parameter) => parameter.id === 'release')!.value = 35;
    const edges = [
      { id: 'detect', source: input.id, sourceHandle: 'out-l', target: follower.id, targetHandle: 'in' },
      { id: 'gate-control', source: follower.id, sourceHandle: 'out', target: gate.id, targetHandle: 'gate' },
      { id: 'audio', source: input.id, sourceHandle: 'out-r', target: gate.id, targetHandle: 'in' },
      { id: 'left', source: gate.id, sourceHandle: 'out', target: output.id, targetHandle: 'in-l' },
      { id: 'right', source: input.id, sourceHandle: 'out-r', target: output.id, targetHandle: 'in-r' },
    ];
    const written = writePatchJson([input, follower, gate, output], edges, { x: 12, y: -8, zoom: 0.9 });
    const loaded = parsePatchJson(written, reference);
    const loadedFollower = loaded.nodes.find((node) => node.id === follower.id)!;
    const loadedGate = loaded.nodes.find((node) => node.id === gate.id)!;
    expect(loadedFollower.data.ports.map((port) => port.id)).toEqual(['in', 'out']);
    expect(loadedGate.data.ports.map((port) => port.id)).toEqual(['in', 'gate', 'out']);
    expect(loadedFollower.data.parameters.map((parameter) => [parameter.id, parameter.value, parameter.modulation])).toEqual([
      ['attack', 7.5, undefined], ['release', 321, undefined],
    ]);
    expect(loadedGate.data.parameters.map((parameter) => [parameter.id, parameter.value, parameter.modulation])).toEqual([
      ['threshold', 0.63, undefined], ['attack', 4.2, undefined], ['hold', 280, undefined], ['release', 35, undefined],
    ]);
    expect(writePatchJson(loaded.nodes, loaded.edges, loaded.viewport)).toBe(written);
  });

  it('round trips every Pitch Shift field and its explicit mono connections', () => {
    const input = createModuleNode('stereo-input', 'stereo-input-1', { x: 0, y: 0 });
    const pitch = createModuleNode('pitch-shift', 'pitch-shift-1', { x: 240, y: 40 });
    const output = createModuleNode('stereo-output', 'stereo-output-1', { x: 520, y: 0 });
    const values = new Map([['semitones', -7.25], ['grain', 93.4], ['overlap', 0.72], ['direction', 1], ['phase', 0.373]]);
    pitch.data.parameters.forEach((parameter) => { parameter.value = values.get(parameter.id) ?? parameter.value; });
    pitch.data.parameters.find((parameter) => parameter.id === 'semitones')!.modulation!.amount = 5.5;
    const edges = [
      { id: 'into-pitch', source: input.id, sourceHandle: 'out-l', target: pitch.id, targetHandle: 'in' },
      { id: 'pitch-left', source: pitch.id, sourceHandle: 'out', target: output.id, targetHandle: 'in-l' },
      { id: 'right', source: input.id, sourceHandle: 'out-r', target: output.id, targetHandle: 'in-r' },
    ];
    const written = writePatchJson([input, pitch, output], edges, { x: -12, y: 9, zoom: 0.85 });
    const loaded = parsePatchJson(written, reference);
    const restored = loaded.nodes.find((node) => node.id === pitch.id)!;
    expect(restored.data.parameters.map(({ id, value, unit }) => [id, value, unit])).toEqual([
      ['semitones', -7.25, 'semitones'], ['grain', 93.4, 'milliseconds'],
      ['overlap', 0.72, 'normalized'], ['direction', 1, 'direction'],
      ['phase', 0.373, 'cycles'],
    ]);
    expect(restored.data.parameters[0].modulation?.amount).toBe(5.5);
    expect(writePatchJson(loaded.nodes, loaded.edges, loaded.viewport)).toBe(written);

    const legacy = JSON.parse(written) as { semantic: { nodes: Array<{ type: string; parameters: Array<{ id: string }> }> } };
    const legacyPitch = legacy.semantic.nodes.find((node) => node.type === 'pitch-shift')!;
    legacyPitch.parameters = legacyPitch.parameters.filter((parameter) => parameter.id !== 'phase');
    const migrated = parsePatchJson(JSON.stringify(legacy), reference);
    expect(migrated.nodes.find((node) => node.id === pitch.id)?.data.parameters.at(-1)).toMatchObject({ id: 'phase', value: 0, unit: 'cycles' });
  });

  it('rejects persisted graphs with multiple cables on one input', () => {
    const input = createModuleNode('stereo-input', 'stereo-input-1', { x: 0, y: 0 });
    const delay = createModuleNode('delay', 'delay-1', { x: 200, y: 0 });
    const output = createModuleNode('stereo-output', 'stereo-output-1', { x: 400, y: 0 });
    const edges = [
      { id: 'left', source: input.id, sourceHandle: 'out-l', target: delay.id, targetHandle: 'in' },
      { id: 'right', source: input.id, sourceHandle: 'out-r', target: delay.id, targetHandle: 'in' },
    ];
    expect(() => parsePatchJson(writePatchJson([input, delay, output], edges, { x: 0, y: 0, zoom: 1 }), reference)).toThrow(/more than one cable; insert Sum/);
  });

  it('loads the M4.1 shared-edge loop evidence fixture through schema v1', () => {
    const loaded = parsePatchJson(sharedEdgeLoopFixture, reference);
    expect(loaded.nodes).toHaveLength(8);
    expect(loaded.edges).toHaveLength(10);
    expect(loaded.edges.find((edge) => edge.id === 'return-root')).toMatchObject({ source: 'return-sum', target: 'root-sum' });
  });
});
