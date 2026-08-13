import { describe, expect, it } from 'vitest';
import { createModuleNode } from './modules';
import { inspectMacroReachability } from './macroInspection';

describe('macro reachability inspection', () => {
  it('lists branched mapped destinations and their predicted ranges', () => {
    const macro = createModuleNode('macro', 'macro-1', { x: 0, y: 0 });
    const mapperA = createModuleNode('control-map', 'map-a', { x: 100, y: 0 });
    const mapperB = createModuleNode('control-map', 'map-b', { x: 100, y: 100 });
    const delay = createModuleNode('delay', 'delay-1', { x: 200, y: 0 });
    const allpass = createModuleNode('allpass', 'allpass-1', { x: 200, y: 100 });
    mapperA.data.parameters.find((parameter) => parameter.id === 'scale')!.value = 0.5;
    mapperB.data.parameters.find((parameter) => parameter.id === 'scale')!.value = -0.25;
    const edges = [
      { id: 'branch-a', source: macro.id, sourceHandle: 'out', target: mapperA.id, targetHandle: 'in', data: { signal: 'control' } },
      { id: 'branch-b', source: macro.id, sourceHandle: 'out', target: mapperB.id, targetHandle: 'in', data: { signal: 'control' } },
      { id: 'delay-map', source: mapperA.id, sourceHandle: 'out', target: delay.id, targetHandle: 'delay-mod', data: { signal: 'control' } },
      { id: 'allpass-map', source: mapperB.id, sourceHandle: 'out', target: allpass.id, targetHandle: 'coefficient-mod', data: { signal: 'control' } },
    ];
    const result = inspectMacroReachability(macro.id, [macro, mapperA, mapperB, delay, allpass], edges);
    expect(result.edgeIds).toEqual(['branch-a', 'branch-b', 'delay-map', 'allpass-map']);
    expect(result.destinations).toEqual([
      { nodeId: 'delay-1', parameterId: 'delay', minimum: 5, maximum: 15, unit: 'milliseconds' },
      { nodeId: 'allpass-1', parameterId: 'coefficient', minimum: 0.4375, maximum: 0.5625, unit: 'unitless' },
    ]);
  });
});
