import { describe, expect, it } from 'vitest';
import { collapseMatrixMixer, inspectMatrixMixer, inspectMatrixMixers } from './matrixMixerPresentation';
import type { RuntimeSnapshot } from './graph';
import fourLineFdn from '../../tests/fixtures/four-line-fdn.rvp.json?raw';
import { parsePatchJson } from './patchPersistence';
import { copySelectedGraph, pasteGraph } from './graphClipboard';
import { semanticGraphHash } from './graphHistory';
import { decorateFeedbackLoops, inspectFeedbackLoops } from './loopInspection';

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
    const decorated = graph.nodes.map((node) => inspection.memberNodeIds.includes(node.id) ? { ...node, className: 'energy-level-3 loop-active' } : node);
    const compact = collapseMatrixMixer(decorated, graph.edges, inspection, true);
    expect(compact.nodes).toHaveLength(graph.nodes.length - 27);
    const summary = compact.nodes.find((node) => node.id === 'compound-view-matrix-mixer')!;
    const members = new Set(inspection.memberNodeIds);
    const crossingCount = graph.edges.filter((edge) => members.has(edge.source) !== members.has(edge.target)).length;
    expect(crossingCount).toBe(20); expect(summary.data.ports).toHaveLength(crossingCount);
    expect(summary.data.runtimeBound).toBe(false);
    expect(summary.data.compoundPresentation).toMatchObject({ authoritativeNodeCount: 28, kind: 'matrix-mixer-4x4' });
    expect(summary.className).toContain('energy-level-3'); expect(summary.className).toContain('loop-active');
    expect(compact.edges.filter((edge) => edge.source === summary.id || edge.target === summary.id)).toHaveLength(crossingCount);
    expect(compact.edges.every((edge) => graph.edges.some((source) => source.id === edge.id))).toBe(true);
    expect(semanticGraphHash({ nodes: graph.nodes, edges: graph.edges })).toBe(semanticGraphHash(graph));
    expect(collapseMatrixMixer(graph.nodes, graph.edges, inspection, false)).toEqual({ nodes: graph.nodes, edges: graph.edges });
  });

  it('copies a collapsed summary as its exact primitives and recognizes the pasted compound independently', () => {
    const inspection = inspectMatrixMixer(graph.nodes)!; const members = new Set(inspection.memberNodeIds);
    const selected = { nodes: graph.nodes.map((node) => ({ ...node, selected: members.has(node.id) })), edges: graph.edges };
    const copied = copySelectedGraph(selected)!; expect(copied.nodes).toHaveLength(28);
    const pasted = pasteGraph(graph, copied); const compounds = inspectMatrixMixers(pasted.nodes);
    expect(compounds.map((compound) => compound.id)).toEqual(['matrix-mixer', 'matrix-mixer-copy']);
    expect(compounds[1].coefficients).toEqual(compounds[0].coefficients);
  });

  it('refuses compact presentation for an amplifying edit without normalizing it', () => {
    const edited = graph.nodes.map((node) => node.id === 'matrix-1-from-1'
      ? { ...node, data: { ...node.data, parameters: node.data.parameters.map((parameter) => parameter.id === 'gain' ? { ...parameter, value: 1 } : parameter) } } : node);
    const inspection = inspectMatrixMixer(edited)!;
    expect(inspection.canCollapse).toBe(false);
    expect(inspection.reason).toContain('no automatic normalization');
    expect(collapseMatrixMixer(edited, graph.edges, inspection, true).nodes).toBe(edited);
  });

  it('projects real feedback evidence to the summary without duplicating cable identity', () => {
    const inspection = inspectMatrixMixer(graph.nodes)!;
    const loop = inspectFeedbackLoops(graph.nodes, graph.edges, { nodeId: inspection.coefficientNodeIds[0] });
    expect(loop.loops.length).toBeGreaterThan(0);
    const decorated = decorateFeedbackLoops(graph.nodes, graph.edges, loop, 0);
    const compact = collapseMatrixMixer(decorated.nodes, decorated.edges, inspection, true);
    expect(compact.nodes.find((node) => node.id === 'compound-view-matrix-mixer')?.className).toContain('loop-active');
    expect(new Set(compact.edges.map((edge) => edge.id)).size).toBe(compact.edges.length);
  });
});
