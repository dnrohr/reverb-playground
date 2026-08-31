import { describe, expect, it } from 'vitest';
import type { Edge } from '@xyflow/react';
import { createModuleNode } from './modules';
import { addCableWaypoint, alignSelectedNodes, arrangeGraphGroup, cableLayout, setRoutingPortal, traceCable, updateCableWaypoint } from './graphRouting';
import { documentGraphHash, semanticGraphHash } from './graphHistory';

const nodes = [
  { ...createModuleNode('gain', 'a', { x: 10, y: 20 }), selected: true },
  { ...createModuleNode('allpass', 'b', { x: 300, y: 90 }), selected: true },
  { ...createModuleNode('delay', 'c', { x: 600, y: 210 }), selected: true },
];
const edges: Edge[] = [
  { id: 'ab', source: 'a', sourceHandle: 'out', target: 'b', targetHandle: 'in', data: { signal: 'audio' } },
  { id: 'bc', source: 'b', sourceHandle: 'out', target: 'c', targetHandle: 'in', data: { signal: 'audio' } },
];

describe('large graph routing and layout', () => {
  it('stores waypoint and paired portal presentation without semantic changes', () => {
    const base = { nodes, edges }; const semantic = semanticGraphHash(base);
    let edited = addCableWaypoint(base, 'ab'); edited = updateCableWaypoint(edited, 'ab', 0, { x: 123, y: 456 }); edited = setRoutingPortal(edited, 'ab', 'TANK RETURN');
    expect(cableLayout(edited.edges[0])).toEqual({ waypoints: [{ x: 123, y: 456 }], portal: { name: 'TANK RETURN' } });
    expect(semanticGraphHash(edited)).toBe(semantic); expect(documentGraphHash(edited)).not.toBe(documentGraphHash(base));
  });

  it('aligns, distributes, and arranges groups deterministically', () => {
    expect(alignSelectedNodes({ nodes, edges }, 'left').nodes.map((node) => node.position.x)).toEqual([10, 10, 10]);
    expect(alignSelectedNodes({ nodes, edges }, 'horizontal').nodes.map((node) => node.position.x)).toEqual([10, 305, 600]);
    const grouped = { nodes: nodes.map((node) => ({ ...node, data: { ...node.data, presentationGroup: { id: 'g', name: 'Tank', collapsed: false } } })), edges };
    expect(arrangeGraphGroup(grouped, 'g').nodes.map((node) => node.position)).toEqual([{ x: 10, y: 20 }, { x: 220, y: 20 }, { x: 10, y: 170 }]);
    expect(semanticGraphHash(arrangeGraphGroup(grouped, 'g'))).toBe(semanticGraphHash(grouped));
  });

  it('traces complete directed source and output paths', () => {
    const upstream = traceCable({ nodes, edges }, 'bc', 'source'); const downstream = traceCable({ nodes, edges }, 'ab', 'output');
    expect(new Set(upstream.nodeIds)).toEqual(new Set(['a', 'b', 'c'])); expect(upstream.edges.every((edge) => edge.className?.includes('routing-trace-active'))).toBe(true);
    expect(new Set(downstream.nodeIds)).toEqual(new Set(['a', 'b', 'c']));
  });
});
