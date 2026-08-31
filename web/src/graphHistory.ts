import type { Edge, Node } from '@xyflow/react';
import type { GraphState, PatchNodeData } from './graph';

export const graphHistoryLimit = 100;
export const graphHistoryMemoryLimitBytes = 8 * 1024 * 1024;

export interface GraphEdit { label: string; before: GraphState; after: GraphState; bytes: number }
export interface GraphHistory { undo: GraphEdit[]; redo: GraphEdit[]; cleanHash: string }

function snapshotNode(node: Node<PatchNodeData>): Node<PatchNodeData> {
  const copy = structuredClone(node);
  delete copy.selected;
  delete copy.dragging;
  return copy;
}

function snapshotEdge(edge: Edge): Edge {
  const copy = structuredClone(edge);
  delete copy.selected;
  return copy;
}

export function snapshotGraph(state: GraphState): GraphState {
  return { nodes: state.nodes.map(snapshotNode), edges: state.edges.map(snapshotEdge) };
}

function canonicalGraph(state: GraphState, includeLayout: boolean) {
  return {
    nodes: state.nodes.map((node) => ({
      id: node.id,
      type: node.data.type,
      userName: node.data.userName,
      presentation: node.data.presentation,
      ...(includeLayout ? {
        position: { x: node.position.x, y: node.position.y },
        presentationGroup: node.data.presentationGroup ? { ...node.data.presentationGroup } : undefined,
      } : {}),
      ports: node.data.ports.map(({ id, signal, direction }) => ({ id, signal, direction })),
      parameters: node.data.parameters.map(({ id, value, unit, modulation }) => ({ id, value, unit, modulation: modulation ? { ...modulation } : undefined })),
    })).sort((left, right) => left.id.localeCompare(right.id)),
    edges: state.edges.map((edge) => ({
      id: edge.id,
      source: edge.source,
      sourceHandle: edge.sourceHandle ?? '',
      target: edge.target,
      targetHandle: edge.targetHandle ?? '',
    })).sort((left, right) => left.id.localeCompare(right.id)),
  };
}

function fnv1a(text: string): string {
  let hash = 0x811c9dc5;
  for (let index = 0; index < text.length; index++) {
    hash ^= text.charCodeAt(index);
    hash = Math.imul(hash, 0x01000193);
  }
  return (hash >>> 0).toString(16).padStart(8, '0');
}

export function semanticGraphHash(state: GraphState): string {
  return fnv1a(JSON.stringify(canonicalGraph(state, false)));
}

export function documentGraphHash(state: GraphState): string {
  return fnv1a(JSON.stringify(canonicalGraph(state, true)));
}

export const emptyGraphHistory = (baseline: GraphState = { nodes: [], edges: [] }): GraphHistory => ({
  undo: [], redo: [], cleanHash: documentGraphHash(baseline),
});

export function commitGraphEdit(history: GraphHistory, label: string, before: GraphState, after: GraphState): GraphHistory {
  const beforeSnapshot = snapshotGraph(before);
  const afterSnapshot = snapshotGraph(after);
  if (documentGraphHash(beforeSnapshot) === documentGraphHash(afterSnapshot)) return history;
  const edit = { label, before: beforeSnapshot, after: afterSnapshot, bytes: new TextEncoder().encode(JSON.stringify({ label, before: beforeSnapshot, after: afterSnapshot })).byteLength };
  const undo = [...history.undo, edit].slice(-graphHistoryLimit);
  while (undo.length && undo.reduce((total, item) => total + item.bytes, 0) > graphHistoryMemoryLimitBytes) undo.shift();
  return {
    undo,
    redo: [],
    cleanHash: history.cleanHash,
  };
}

export function historyMemoryBytes(history: GraphHistory): number {
  return [...history.undo, ...history.redo].reduce((total, edit) => total + edit.bytes, 0);
}

export function undoGraphEdit(history: GraphHistory) {
  const edit = history.undo.at(-1);
  return { edit, history: edit ? { ...history, undo: history.undo.slice(0, -1), redo: [edit, ...history.redo] } : history };
}

export function redoGraphEdit(history: GraphHistory) {
  const edit = history.redo[0];
  return { edit, history: edit ? { ...history, undo: [...history.undo, edit], redo: history.redo.slice(1) } : history };
}

export function markHistoryClean(history: GraphHistory, state: GraphState): GraphHistory {
  return { ...history, cleanHash: documentGraphHash(state) };
}

export function isHistoryClean(history: GraphHistory, state: GraphState): boolean {
  return history.cleanHash === documentGraphHash(state);
}
