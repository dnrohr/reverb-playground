export interface ParameterEdit {
  nodeId: string;
  parameterId: string;
  before: number;
  after: number;
}

export interface ParameterHistory {
  undo: ParameterEdit[];
  redo: ParameterEdit[];
}

export function commitParameterEdit(history: ParameterHistory, edit: ParameterEdit): ParameterHistory {
  if (edit.before === edit.after) return history;
  return { undo: [...history.undo, edit], redo: [] };
}

export function undoParameterEdit(history: ParameterHistory): { history: ParameterHistory; edit?: ParameterEdit } {
  const edit = history.undo.at(-1);
  if (!edit) return { history };
  return {
    edit,
    history: { undo: history.undo.slice(0, -1), redo: [...history.redo, edit] },
  };
}

export function redoParameterEdit(history: ParameterHistory): { history: ParameterHistory; edit?: ParameterEdit } {
  const edit = history.redo.at(-1);
  if (!edit) return { history };
  return {
    edit,
    history: { undo: [...history.undo, edit], redo: history.redo.slice(0, -1) },
  };
}
