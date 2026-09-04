import { describe, expect, it } from 'vitest';
import type { RuntimeSnapshot } from './graph';
import { copySelectedGraph, pasteGraph } from './graphClipboard';
import { documentGraphHash, semanticGraphHash } from './graphHistory';
import { inspectMatrixMixer } from './matrixMixerPresentation';
import { parsePatchJson, writePatchJson } from './patchPersistence';
import {
  deleteHierarchy,
  expandHierarchyConnection,
  inspectHierarchyPresentations,
  materializeHierarchyPresentations,
  projectHierarchyRoot,
  renameHierarchy,
  setHierarchyViewport,
  updateHierarchyPresentation,
  validateHierarchyPresentations,
} from './compoundPresentation';
import fourLineFdn from '../../tests/fixtures/four-line-fdn.rvp.json?raw';

const snapshot: RuntimeSnapshot = { contractVersion: 2, engineId: 'barr-reference', sampleRate: 48_000,
  nodes: [{ id: 'sum', type: 'sum', label: 'Sum', role: 'routing', position: { x: 0, y: 0 },
    ports: [{ id: 'in-a', signal: 'audio', direction: 'input' }, { id: 'in-b', signal: 'audio', direction: 'input' },
      { id: 'out', signal: 'audio', direction: 'output' }], parameters: [] }],
  connections: [], outsidePatch: [] };

function matrixGraph() {
  const graph = parsePatchJson(fourLineFdn, snapshot);
  return materializeHierarchyPresentations(graph, [inspectMatrixMixer(graph.nodes)!]);
}

describe('hierarchical compound contract', () => {
  it('persists parent position, nested viewport, ports, and exact semantic graph identity', () => {
    const graph = renameHierarchy(matrixGraph(), 'matrix-mixer', 'Late field matrix');
    const hierarchy = inspectHierarchyPresentations(graph.nodes)[0]!;
    hierarchy.nestedViewport = { x: -40, y: 22, zoom: .81 };
    const nodes = graph.nodes.map((node) => node.data.hierarchyPresentation?.id === hierarchy.id
      ? { ...node, data: { ...node.data, hierarchyPresentation: structuredClone(hierarchy) } } : node);
    const semantic = semanticGraphHash({ nodes, edges: graph.edges });
    const json = writePatchJson(nodes, graph.edges, { x: 3, y: 4, zoom: .9 });
    const saved = JSON.parse(json);
    expect(saved.layout.hierarchies[0]).toMatchObject({ id: 'matrix-mixer', name: 'Late field matrix',
      nestedViewport: { x: -40, y: 22, zoom: .81 } });
    const restored = parsePatchJson(json, snapshot);
    expect(semanticGraphHash(restored)).toBe(semantic);
    expect(writePatchJson(restored.nodes, restored.edges, restored.viewport)).toBe(json);
  });

  it('treats nested pan and zoom like the root viewport while tracking parent movement', () => {
    const graph = matrixGraph();
    const documentHash = documentGraphHash(graph);
    const viewed = setHierarchyViewport(graph, 'matrix-mixer', { x: -120, y: 48, zoom: .63 });
    expect(documentGraphHash(viewed)).toBe(documentHash);
    const moved = updateHierarchyPresentation(viewed, 'matrix-mixer', { position: { x: 880, y: 360 } });
    expect(documentGraphHash(moved)).not.toBe(documentHash);
  });

  it('maps one four-way Matrix input proxy to the four explicit gain cables', () => {
    const graph = matrixGraph();
    const root = projectHierarchyRoot(graph.nodes, graph.edges);
    const parent = root.nodes.find((node) => node.id === 'hierarchy-view-matrix-mixer')!;
    const translated = expandHierarchyConnection(root.nodes, graph.edges, {
      source: 'line-return-1', sourceHandle: 'out', target: parent.id, targetHandle: 'in-1',
    });
    expect(translated).toEqual([1, 2, 3, 4].map((row) => ({ source: 'line-return-1', sourceHandle: 'out',
      target: `matrix-${row}-from-1`, targetHandle: 'in' })));
  });

  it('copies a whole parent with fresh hierarchy, primitive, and boundary target IDs', () => {
    const graph = matrixGraph(); const hierarchy = inspectHierarchyPresentations(graph.nodes)[0]!;
    const members = new Set(hierarchy.memberNodeIds);
    const clipboard = copySelectedGraph({ nodes: graph.nodes.map((node) => ({ ...node, selected: members.has(node.id) })), edges: graph.edges })!;
    const pasted = pasteGraph(graph, clipboard);
    const copy = inspectHierarchyPresentations(pasted.nodes).find((item) => item.id !== hierarchy.id)!;
    expect(copy.id).toBe('matrix-mixer-copy');
    expect(copy.memberNodeIds).toHaveLength(28);
    expect(copy.ports.flatMap((port) => port.targets).every((target) => copy.memberNodeIds.includes(target.nodeId))).toBe(true);
    expect(validateHierarchyPresentations(pasted.nodes, pasted.edges)).toEqual([]);
  });

  it('atomically deletes the parent-owned primitives and rejects dangling or recursive maps', () => {
    const graph = matrixGraph(); const before = graph.nodes.length;
    const removed = deleteHierarchy(graph, 'matrix-mixer');
    expect(removed.nodes).toHaveLength(before - 28);
    expect(removed.edges.some((edge) => edge.source.startsWith('matrix-') || edge.target.startsWith('matrix-'))).toBe(false);

    const broken = structuredClone(graph.nodes);
    for (const node of broken) if (node.data.hierarchyPresentation?.id === 'matrix-mixer') {
      node.data.hierarchyPresentation.ports[0]!.targets[0]!.portId = 'missing';
      node.data.hierarchyPresentation.parentId = 'matrix-mixer';
    }
    const errors = validateHierarchyPresentations(broken, graph.edges).join(' ');
    expect(errors).toContain("ports['in-1']");
    expect(errors).toContain('recursive');
    expect(() => writePatchJson(broken, graph.edges, { x: 0, y: 0, zoom: 1 })).toThrow(/layout\.hierarchies/);
  });
});
