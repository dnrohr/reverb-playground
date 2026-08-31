import { describe, expect, it } from 'vitest';
import { copySelectedGraph, pasteGraph } from './graphClipboard';
import { createModuleNode } from './modules';
import { parsePatchJson, writePatchJson } from './patchPersistence';
import { detachSubpatchInstance, diffuseDelayDefinition, inspectSubpatchInstances, instantiateSubpatch, subpatchInstanceStatus } from './subpatches';
import type { RuntimeSnapshot } from './graph';

const reference: RuntimeSnapshot = { contractVersion: 2, engineId: 'barr-reference', sampleRate: 48_000, outsidePatch: [], nodes: [], connections: [] };
const base = () => ({ nodes: [createModuleNode('stereo-input', 'input', { x: 0, y: 0 }), createModuleNode('stereo-output', 'output', { x: 700, y: 0 })], edges: [] });

describe('reusable subpatch instances', () => {
  it('instantiates an explicit diffuser-delay branch as ordinary primitives', () => {
    const graph = instantiateSubpatch(base(), diffuseDelayDefinition, { x: 200, y: 100 });
    const instance = inspectSubpatchInstances(graph.nodes)[0]!;
    expect(graph.nodes.map((node) => node.data.type)).toEqual(['stereo-input', 'stereo-output', 'allpass', 'delay']);
    expect(graph.edges).toEqual([expect.objectContaining({ source: 'diffuse-delay-1-allpass', target: 'diffuse-delay-1-delay' })]);
    expect(instance.ports).toEqual([
      { id: 'in', signal: 'audio', direction: 'input', nodeId: 'diffuse-delay-1-allpass', portId: 'in' },
      { id: 'out', signal: 'audio', direction: 'output', nodeId: 'diffuse-delay-1-delay', portId: 'out' },
    ]);
    expect(graph.nodes.filter((node) => node.data.subpatchInstance).every((node) => node.data.runtimeBound === false)).toBe(true);
  });

  it('round trips pinned provenance and recovers safely when a definition is unavailable', () => {
    const graph = instantiateSubpatch(base(), diffuseDelayDefinition, { x: 200, y: 100 });
    const written = writePatchJson(graph.nodes, graph.edges, { x: 0, y: 0, zoom: 1 }); const loaded = parsePatchJson(written, reference);
    expect(writePatchJson(loaded.nodes, loaded.edges, loaded.viewport)).toBe(written);
    const instance = inspectSubpatchInstances(loaded.nodes)[0]!; expect(subpatchInstanceStatus(instance, loaded.nodes).kind).toBe('current');
    const missing = { ...instance, definitionId: 'missing.definition' }; expect(subpatchInstanceStatus(missing, loaded.nodes)).toMatchObject({ kind: 'missing', message: expect.stringMatching(/remain audible/) });
  });

  it('copies an instance with fresh member, cable, binding, and instance identities', () => {
    const graph = instantiateSubpatch(base(), diffuseDelayDefinition, { x: 200, y: 100 }); const copied = copySelectedGraph(graph)!;
    const pasted = pasteGraph(graph, copied); const instances = inspectSubpatchInstances(pasted.nodes);
    expect(instances).toHaveLength(2); expect(instances[1].id).toBe('diffuse-delay-1-copy');
    expect(instances[1].memberNodeIds.every((id) => id.includes('-copy'))).toBe(true);
    expect(instances[1].ports.every((port) => instances[1].memberNodeIds.includes(port.nodeId))).toBe(true);
    expect(new Set(pasted.edges.map((edge) => edge.id)).size).toBe(pasted.edges.length);
    const partial = { nodes: graph.nodes.map((node) => ({ ...node, selected: node.id.endsWith('-allpass') })), edges: graph.edges };
    const partialCopy = copySelectedGraph(partial)!; expect(partialCopy.nodes[0].data.subpatchInstance).toBeUndefined();
    expect(partialCopy.nodes[0].className).not.toContain('subpatch-member');
  });

  it('detaches provenance without changing primitives or cables', () => {
    const graph = instantiateSubpatch(base(), diffuseDelayDefinition, { x: 200, y: 100 }); const instance = inspectSubpatchInstances(graph.nodes)[0]!;
    const detached = detachSubpatchInstance(graph, instance.id);
    expect(detached.nodes.map(({ id, data }) => [id, data.type, data.parameters])).toEqual(graph.nodes.map(({ id, data }) => [id, data.type, data.parameters]));
    expect(detached.edges).toEqual(graph.edges); expect(inspectSubpatchInstances(detached.nodes)).toEqual([]);
  });
});
