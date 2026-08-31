import { describe, expect, it } from 'vitest';
import type { Edge } from '@xyflow/react';
import type { GraphState, RuntimeSnapshot } from './graph';
import { createModuleNode } from './modules';
import { collapseGraphGroups, createGraphGroup, inspectGraphGroups, setGraphGroupCollapsed } from './graphGroups';
import { commitGraphEdit, documentGraphHash, emptyGraphHistory, redoGraphEdit, semanticGraphHash, undoGraphEdit } from './graphHistory';
import { copySelectedGraph, pasteGraph } from './graphClipboard';
import { parsePatchJson, writePatchJson } from './patchPersistence';
import { decorateFeedbackLoops, inspectFeedbackLoops } from './loopInspection';

const edge = (id: string, source: string, target: string, sourceHandle = 'out', targetHandle = 'in', signal = 'audio'): Edge => ({
  id, source, target, sourceHandle, targetHandle, className: `signal-edge signal-${signal}`, data: { signal },
});

function fixture(): GraphState {
  const input = createModuleNode('stereo-input', 'input', { x: 0, y: 0 });
  const delay = { ...createModuleNode('delay', 'delay', { x: 200, y: 0 }), selected: true };
  const allpass = { ...createModuleNode('allpass', 'allpass', { x: 400, y: 0 }), selected: true };
  const output = createModuleNode('stereo-output', 'output', { x: 650, y: 0 });
  return { nodes: [input, delay, allpass, output], edges: [
    edge('enter', 'input', 'delay', 'out-l'), edge('inside', 'delay', 'allpass'), edge('leave', 'allpass', 'output', 'out', 'in-l'),
  ] };
}

describe('collapsible graph groups', () => {
  it('projects one typed boundary port per crossing cable without changing authority', () => {
    const base = fixture(); const semantic = semanticGraphHash(base);
    const grouped = createGraphGroup(base, 'Diffusion pair');
    expect(semanticGraphHash(grouped)).toBe(semantic);
    const collapsed = setGraphGroupCollapsed(grouped, 'group-1', true);
    expect(semanticGraphHash(collapsed)).toBe(semantic);
    expect(documentGraphHash(collapsed)).not.toBe(documentGraphHash(grouped));
    const projected = collapseGraphGroups(collapsed.nodes, collapsed.edges);
    expect(projected.nodes.map((node) => node.id)).toEqual(['input', 'output', 'group-view-group-1']);
    expect(projected.nodes.at(-1)?.data.ports).toEqual([
      { id: 'input-enter', direction: 'input', signal: 'audio' }, { id: 'output-leave', direction: 'output', signal: 'audio' },
    ]);
    expect(projected.edges.map((item) => [item.id, item.source, item.target])).toEqual([
      ['enter', 'input', 'group-view-group-1'], ['leave', 'group-view-group-1', 'output'],
    ]);
    expect(collapsed.nodes).toHaveLength(4); expect(collapsed.edges).toHaveLength(3);
  });

  it('treats create and collapse as atomic undoable presentation edits', () => {
    const base = fixture(); const grouped = createGraphGroup(base, 'Tank');
    const createdHistory = commitGraphEdit(emptyGraphHistory(base), 'Create group', base, grouped);
    expect(inspectGraphGroups(undoGraphEdit(createdHistory).edit!.before.nodes)).toEqual([]);
    const collapsed = setGraphGroupCollapsed(grouped, 'group-1', true);
    const collapseHistory = commitGraphEdit(createdHistory, 'Collapse group', grouped, collapsed);
    const undone = undoGraphEdit(collapseHistory); expect(inspectGraphGroups(undone.edit!.before.nodes)[0]?.collapsed).toBe(false);
    expect(inspectGraphGroups(redoGraphEdit(undone.history).edit!.after.nodes)[0]?.collapsed).toBe(true);
  });

  it('keeps multiple group collapse states independent', () => {
    const base = fixture();
    const gain = createModuleNode('gain', 'gain', { x: 200, y: 180 });
    const lowpass = createModuleNode('lowpass', 'lowpass', { x: 400, y: 180 });
    let grouped = createGraphGroup({ nodes: [...base.nodes, gain, lowpass], edges: base.edges }, 'Upper pair');
    grouped = { ...grouped, nodes: grouped.nodes.map((node) => ({ ...node, selected: ['gain', 'lowpass'].includes(node.id) })) };
    grouped = createGraphGroup(grouped, 'Lower pair');
    grouped = setGraphGroupCollapsed(grouped, 'group-1', true);
    expect(inspectGraphGroups(grouped.nodes).map(({ id, collapsed }) => ({ id, collapsed }))).toEqual([
      { id: 'group-1', collapsed: true }, { id: 'group-2', collapsed: false },
    ]);
  });

  it('round trips groups while schema-v2 documents without groups migrate to none', () => {
    const reference: RuntimeSnapshot = { contractVersion: 2, engineId: 'barr-reference', sampleRate: 48000, nodes: [], connections: [], outsidePatch: [] };
    const grouped = setGraphGroupCollapsed(createGraphGroup(fixture(), 'Diffusion pair'), 'group-1', true);
    const json = writePatchJson(grouped.nodes, grouped.edges, { x: 3, y: 4, zoom: 0.8 });
    const loaded = parsePatchJson(json, reference);
    expect(inspectGraphGroups(loaded.nodes)).toEqual([{ id: 'group-1', name: 'Diffusion pair', collapsed: true, nodeIds: ['allpass', 'delay'] }]);
    expect(writePatchJson(loaded.nodes, loaded.edges, loaded.viewport)).toBe(json);
    const legacy = JSON.parse(json); delete legacy.layout.groups;
    expect(inspectGraphGroups(parsePatchJson(JSON.stringify(legacy), reference).nodes)).toEqual([]);
  });

  it('copies complete groups with fresh identity and strips partial group membership', () => {
    const grouped = createGraphGroup(fixture(), 'Diffusion pair');
    const selectedGroup = { ...grouped, nodes: grouped.nodes.map((node) => ({ ...node, selected: ['delay', 'allpass'].includes(node.id) })) };
    const complete = copySelectedGraph(selectedGroup)!; const pasted = pasteGraph(grouped, complete);
    const copiedGroup = inspectGraphGroups(pasted.nodes).find((group) => group.id !== 'group-1');
    expect(copiedGroup).toMatchObject({ name: 'Diffusion pair', nodeIds: ['allpass-copy', 'delay-copy'] });
    const partialState = { ...grouped, nodes: grouped.nodes.map((node) => ({ ...node, selected: node.id === 'delay' })) };
    expect(copySelectedGraph(partialState)?.nodes[0]?.data.presentationGroup).toBeUndefined();
  });

  it('rejects nesting and carries feedback highlighting across a collapsed boundary', () => {
    const grouped = createGraphGroup(fixture(), 'Diffusion pair');
    const selectedGrouped = { ...grouped, nodes: grouped.nodes.map((node) => ({ ...node, selected: ['delay', 'allpass'].includes(node.id) })) };
    expect(() => createGraphGroup(selectedGrouped, 'Nested')).toThrow('Nested groups are not supported');
    const feedback = { ...grouped, edges: [...grouped.edges, edge('return', 'allpass', 'delay')] };
    const inspection = inspectFeedbackLoops(feedback.nodes, feedback.edges, { nodeId: 'delay' });
    const decorated = decorateFeedbackLoops(feedback.nodes, feedback.edges, inspection, 0);
    const projected = collapseGraphGroups(setGraphGroupCollapsed({ nodes: decorated.nodes, edges: decorated.edges }, 'group-1', true).nodes, decorated.edges);
    expect(projected.nodes.find((node) => node.id === 'group-view-group-1')?.className).toContain('loop-active');
  });
});
