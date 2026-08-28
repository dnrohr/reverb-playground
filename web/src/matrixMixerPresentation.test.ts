import { describe, expect, it } from 'vitest';
import { collapseMatrixMixer, inspectMatrixMixer } from './matrixMixerPresentation';
import type { RuntimeSnapshot } from './graph';
import fourLineFdn from '../../tests/fixtures/four-line-fdn.rvp.json?raw';
import { parsePatchJson } from './patchPersistence';

const snapshot: RuntimeSnapshot = { contractVersion: 2, engineId: 'barr-reference', sampleRate: 48_000,
  nodes: [{ id: 'sum', type: 'sum', label: 'Sum', role: 'routing', position: { x: 0, y: 0 },
    ports: [{ id: 'in-a', signal: 'audio', direction: 'input' }, { id: 'in-b', signal: 'audio', direction: 'input' }, { id: 'out', signal: 'audio', direction: 'output' }], parameters: [] }],
  connections: [], outsidePatch: [] };

describe('Matrix Mixer compound presentation', () => {
  const graph = parsePatchJson(fourLineFdn, snapshot);

  it('recognizes all persisted coefficients and proves normalized energy', () => {
    const inspection = inspectMatrixMixer(graph.nodes)!;
    expect(inspection.coefficients).toEqual([
      [.5, .5, .5, .5], [.5, -.5, .5, -.5], [.5, .5, -.5, -.5], [.5, -.5, -.5, .5],
    ]);
    expect(inspection.rowEnergy).toEqual([1, 1, 1, 1]);
    expect(inspection.columnEnergy).toEqual([1, 1, 1, 1]);
    expect(inspection.orthogonal).toBe(true);
    expect(inspection.canCollapse).toBe(true);
  });

  it('collapses only presentation and expands to the exact original graph', () => {
    const inspection = inspectMatrixMixer(graph.nodes)!;
    const compact = collapseMatrixMixer(graph.nodes, graph.edges, inspection, true);
    expect(compact.nodes).toHaveLength(graph.nodes.length - 27);
    expect(compact.nodes.find((node) => node.id === 'matrix-mixer-view')?.data.ports).toHaveLength(8);
    expect(compact.edges.filter((edge) => edge.id.startsWith('matrix-view-'))).toHaveLength(8);
    expect(collapseMatrixMixer(graph.nodes, graph.edges, inspection, false)).toEqual({ nodes: graph.nodes, edges: graph.edges });
  });

  it('refuses compact presentation for an amplifying edit without normalizing it', () => {
    const edited = graph.nodes.map((node) => node.id === 'matrix-1-from-1'
      ? { ...node, data: { ...node.data, parameters: node.data.parameters.map((parameter) => parameter.id === 'gain' ? { ...parameter, value: 1 } : parameter) } } : node);
    const inspection = inspectMatrixMixer(edited)!;
    expect(inspection.canCollapse).toBe(false);
    expect(inspection.reason).toContain('no automatic normalization');
    expect(collapseMatrixMixer(edited, graph.edges, inspection, true).nodes).toBe(edited);
  });
});
