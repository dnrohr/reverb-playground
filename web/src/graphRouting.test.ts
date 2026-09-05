import { describe, expect, it } from 'vitest';
import type { Edge, Node } from '@xyflow/react';
import { createModuleNode } from './modules';
import type { PatchNodeData } from './graph';
import { addCableWaypoint, alignSelectedNodes, arrangeGraphGroup, cableLayout, flipSelectedNodes, setRoutingPortal, traceCable, updateCableWaypoint } from './graphRouting';
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

function crossingCount(graphNodes: Node<PatchNodeData>[], graphEdges: Edge[]) {
  const byId = new Map(graphNodes.map((node) => [node.id, node]));
  const point = (nodeId: string, direction: 'input' | 'output') => {
    const node = byId.get(nodeId)!; const reversed = node.data.orientation === 'reverse';
    const right = direction === 'input' ? reversed : !reversed;
    return { x: node.position.x + (right ? (node.width ?? 183) : 0), y: node.position.y + (node.height ?? 105) / 2 };
  };
  const segments = graphEdges.map((edge) => [point(edge.source, 'output'), point(edge.target, 'input')] as const);
  const side = (a: { x: number; y: number }, b: { x: number; y: number }, c: { x: number; y: number }) =>
    (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
  let crossings = 0;
  for (let left = 0; left < segments.length; left += 1) for (let right = left + 1; right < segments.length; right += 1) {
    const [a, b] = segments[left]; const [c, d] = segments[right];
    if (side(a, b, c) * side(a, b, d) < 0 && side(c, d, a) * side(c, d, b) < 0) crossings += 1;
  }
  return crossings;
}

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
    expect(arrangeGraphGroup(grouped, 'g').nodes.map((node) => node.position)).toEqual([{ x: 10, y: 20 }, { x: 229, y: 20 }, { x: 10, y: 161 }]);
    expect(semanticGraphHash(arrangeGraphGroup(grouped, 'g'))).toBe(semanticGraphHash(grouped));
  });

  it('distributes mixed bounding boxes by equal free gaps while preserving outer edges', () => {
    const mixed = nodes.map((node, index) => ({ ...node, width: [100, 200, 80][index], position: { x: [0, 400, 600][index]!, y: 20 } }));
    const result = alignSelectedNodes({ nodes: mixed, edges }, 'horizontal').nodes;
    expect(result.map((node) => node.position.x)).toEqual([0, 250, 600]);
    expect([result[1].position.x - 100, result[2].position.x - (result[1].position.x + 200)]).toEqual([150, 150]);
    expect(() => alignSelectedNodes({ nodes: mixed.map((node, index) => ({ ...node, position: { x: index * 20, y: 0 } })), edges }, 'horizontal'))
      .toThrow(/need .* more graph pixels/);
  });

  it('refuses protected-neighbor collisions and flips presentation without semantic change', () => {
    const grouped = nodes.map((node) => ({ ...node, data: { ...node.data, presentationGroup: { id: 'g', name: 'Tank', collapsed: false } } }));
    const blocker = { ...createModuleNode('gain', 'blocker', { x: 180, y: 20 }), width: 500, height: 500 };
    expect(() => arrangeGraphGroup({ nodes: [...grouped, blocker], edges }, 'g')).toThrow(/protected neighboring blocks/);
    const base = { nodes, edges }; const flipped = flipSelectedNodes(base);
    expect(flipped.nodes.every((node) => node.data.orientation === 'reverse')).toBe(true);
    expect(semanticGraphHash(flipped)).toBe(semanticGraphHash(base));
    expect(documentGraphHash(flipped)).not.toBe(documentGraphHash(base));
    expect(flipSelectedNodes(flipped).nodes.every((node) => !node.data.orientation)).toBe(true);
  });

  it('packs representative group sizes without overlap and is idempotent', () => {
    for (let size = 2; size <= 16; size += 1) {
      const members = Array.from({ length: size }, (_, index) => ({
        ...createModuleNode(index % 2 ? 'delay' : 'gain', `member-${String(index).padStart(2, '0')}`, { x: 40 + index * 260, y: 60 + (index % 3) * 170 }),
        width: 150 + (index % 4) * 23,
        height: 92 + (index % 3) * 17,
        data: {
          ...createModuleNode(index % 2 ? 'delay' : 'gain', `member-${String(index).padStart(2, '0')}`, { x: 0, y: 0 }).data,
          presentationGroup: { id: 'representative', name: 'Representative', collapsed: false },
        },
      }));
      const arranged = arrangeGraphGroup({ nodes: [...members].reverse(), edges: [] }, 'representative');
      const byId = new Map(arranged.nodes.map((node) => [node.id, node.position]));
      const rerun = arrangeGraphGroup(arranged, 'representative');
      expect(new Map(rerun.nodes.map((node) => [node.id, node.position]))).toEqual(byId);
      for (let left = 0; left < arranged.nodes.length; left += 1) for (let right = left + 1; right < arranged.nodes.length; right += 1) {
        const a = arranged.nodes[left]; const b = arranged.nodes[right];
        const separated = a.position.x + (a.width ?? 183) <= b.position.x
          || b.position.x + (b.width ?? 183) <= a.position.x
          || a.position.y + (a.height ?? 105) <= b.position.y
          || b.position.y + (b.height ?? 105) <= a.position.y;
        expect(separated, `${size} members: ${a.id} overlaps ${b.id}`).toBe(true);
      }
    }
  });

  it('can remove a cable crossing by changing attachment geometry only', () => {
    const crossingNodes = [
      { ...createModuleNode('gain', 'source-a', { x: 0, y: 0 }), width: 80, height: 40, selected: true },
      { ...createModuleNode('gain', 'source-b', { x: 0, y: 240 }), width: 80, height: 40, selected: false },
      { ...createModuleNode('gain', 'target-a', { x: 0, y: 160 }), width: 80, height: 40, selected: true },
      { ...createModuleNode('gain', 'target-b', { x: 0, y: 80 }), width: 80, height: 40, selected: false },
    ];
    const crossingEdges: Edge[] = [
      { id: 'cross-a', source: 'source-a', target: 'target-a' },
      { id: 'cross-b', source: 'source-b', target: 'target-b' },
    ];
    const before = { nodes: crossingNodes, edges: crossingEdges }; const after = flipSelectedNodes(before);
    expect(crossingCount(before.nodes, crossingEdges)).toBe(1);
    expect(crossingCount(after.nodes, crossingEdges)).toBe(0);
    expect(semanticGraphHash(after)).toBe(semanticGraphHash(before));
  });

  it('traces complete directed source and output paths', () => {
    const upstream = traceCable({ nodes, edges }, 'bc', 'source'); const downstream = traceCable({ nodes, edges }, 'ab', 'output');
    expect(new Set(upstream.nodeIds)).toEqual(new Set(['a', 'b', 'c'])); expect(upstream.edges.every((edge) => edge.className?.includes('routing-trace-active'))).toBe(true);
    expect(new Set(downstream.nodeIds)).toEqual(new Set(['a', 'b', 'c']));
  });
});
