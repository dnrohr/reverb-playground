import type { GraphState } from './graph';
import { cloneGraph } from './graph';

export interface GraphEdit { label: string; before: GraphState; after: GraphState }
export interface GraphHistory { undo: GraphEdit[]; redo: GraphEdit[] }
export const emptyGraphHistory = (): GraphHistory => ({ undo: [], redo: [] });
export function commitGraphEdit(history: GraphHistory, label: string, before: GraphState, after: GraphState): GraphHistory {
  return { undo: [...history.undo, { label, before: cloneGraph(before), after: cloneGraph(after) }], redo: [] };
}
export function undoGraphEdit(history: GraphHistory) {
  const edit = history.undo.at(-1);
  return { edit, history: edit ? { undo: history.undo.slice(0, -1), redo: [edit, ...history.redo] } : history };
}
export function redoGraphEdit(history: GraphHistory) {
  const edit = history.redo[0];
  return { edit, history: edit ? { undo: [...history.undo, edit], redo: history.redo.slice(1) } : history };
}
