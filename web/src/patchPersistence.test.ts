import { describe, expect, it } from 'vitest';
import { createFlowModel, type RuntimeSnapshot } from './graph';
import { parsePatchJson, writePatchJson } from './patchPersistence';
import { createModuleNode } from './modules';

const reference: RuntimeSnapshot = {
  contractVersion: 1, engineId: 'barr-reference', sampleRate: 48000, outsidePatch: [],
  nodes: [{
    id: 'sum', type: 'sum', label: 'Mono Sum', role: 'routing', position: { x: 1, y: 2 },
    ports: [
      { id: 'in', signal: 'audio', direction: 'input' },
      { id: 'out', signal: 'audio', direction: 'output' },
    ],
    parameters: [{ id: 'gain', value: 0.5, unit: 'linear', minimum: 0, maximum: 1, step: 0.001 }],
  }],
  connections: [],
};

describe('patch persistence', () => {
  it('round trips semantics, exact parameter values, positions, and viewport deterministically', () => {
    const flow = createFlowModel(reference);
    flow.nodes.push(createModuleNode('stereo-input', 'stereo-input-1', { x: -200, y: 0 }), createModuleNode('stereo-output', 'stereo-output-1', { x: 200, y: 0 }));
    flow.nodes[0].data.parameters[0].value = 0.371;
    flow.nodes[0].position = { x: -42.25, y: 319.75 };
    const viewport = { x: 17.5, y: -88.25, zoom: 1.375 };
    const written = writePatchJson(flow.nodes, flow.edges, viewport);
    const loaded = parsePatchJson(written, reference);

    expect(loaded.nodes[0].data.parameters[0].value).toBe(0.371);
    expect(loaded.nodes[0].position).toEqual(flow.nodes[0].position);
    expect(loaded.viewport).toEqual(viewport);
    expect(writePatchJson(loaded.nodes, loaded.edges, loaded.viewport)).toBe(written);
  });

  it('rejects invalid and future documents before returning replacement state', () => {
    const flow = createFlowModel(reference);
    flow.nodes.push(createModuleNode('stereo-input', 'stereo-input-1', { x: -200, y: 0 }), createModuleNode('stereo-output', 'stereo-output-1', { x: 200, y: 0 }));
    const untouched = structuredClone(flow);
    const valid = JSON.parse(writePatchJson(flow.nodes, flow.edges, { x: 0, y: 0, zoom: 1 })) as Record<string, unknown>;
    expect(() => parsePatchJson('{ bad json', reference)).toThrow(/invalid JSON/);
    expect(() => parsePatchJson(JSON.stringify({ ...valid, schemaVersion: 2 }), reference)).toThrow(/unsupported schemaVersion/);
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
});
