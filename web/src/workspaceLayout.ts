export type WorkspaceArrangement = 'balanced' | 'create' | 'learn' | 'custom';
export type WorkspaceDock = 'modules' | 'context';

export interface WorkspacePresentation {
  arrangement: WorkspaceArrangement;
  modulesOpen: boolean;
  contextOpen: boolean;
  auditionOpen: boolean;
  narrowDock: WorkspaceDock;
}

export interface WorkspaceLayout {
  width: number;
  overlay: boolean;
  modulesVisible: boolean;
  contextVisible: boolean;
  moduleWidth: number;
  canvasWidth: number;
  contextWidth: number;
}

export const workspacePresentationStorageKey = 'reverb-playground-workspace-v1';
export const workspaceBreakpoint = 900;

export const defaultWorkspacePresentation: WorkspacePresentation = {
  arrangement: 'balanced',
  modulesOpen: true,
  contextOpen: true,
  auditionOpen: false,
  narrowDock: 'modules',
};

const clamp = (value: number, minimum: number, maximum: number) => Math.max(minimum, Math.min(maximum, value));

export function arrangementPresentation(arrangement: WorkspaceArrangement): WorkspacePresentation {
  if (arrangement === 'create') return { arrangement, modulesOpen: false, contextOpen: false, auditionOpen: false, narrowDock: 'modules' };
  if (arrangement === 'learn') return { arrangement, modulesOpen: false, contextOpen: true, auditionOpen: true, narrowDock: 'context' };
  if (arrangement === 'custom') return { ...defaultWorkspacePresentation, arrangement };
  return { ...defaultWorkspacePresentation };
}

export function toggleWorkspaceDock(presentation: WorkspacePresentation, dock: WorkspaceDock): WorkspacePresentation {
  if (dock === 'modules') return {
    ...presentation,
    arrangement: 'custom',
    modulesOpen: !presentation.modulesOpen,
    narrowDock: 'modules',
  };
  return {
    ...presentation,
    arrangement: 'custom',
    contextOpen: !presentation.contextOpen,
    narrowDock: 'context',
  };
}

export function resolveWorkspaceLayout(width: number, presentation: WorkspacePresentation): WorkspaceLayout {
  const logicalWidth = Math.max(0, Math.round(width));
  if (logicalWidth < workspaceBreakpoint) {
    const requestedDock = presentation.narrowDock === 'context' ? 'context' : 'modules';
    const modulesVisible = presentation.modulesOpen && (!presentation.contextOpen || requestedDock === 'modules');
    const contextVisible = presentation.contextOpen && (!presentation.modulesOpen || requestedDock === 'context');
    return {
      width: logicalWidth,
      overlay: true,
      modulesVisible,
      contextVisible,
      moduleWidth: modulesVisible ? Math.min(300, Math.max(240, logicalWidth - 56)) : 0,
      canvasWidth: logicalWidth,
      contextWidth: contextVisible ? Math.min(360, Math.max(280, logicalWidth - 56)) : 0,
    };
  }

  const moduleWidth = presentation.modulesOpen ? Math.round(clamp(logicalWidth * 0.16, 150, 202)) : 0;
  const contextMinimum = presentation.arrangement === 'learn' ? 260 : 190;
  const contextMaximum = presentation.arrangement === 'learn' ? 360 : 258;
  const contextRatio = presentation.arrangement === 'learn' ? 0.28 : 0.20;
  const contextWidth = presentation.contextOpen ? Math.round(clamp(logicalWidth * contextRatio, contextMinimum, contextMaximum)) : 0;
  return {
    width: logicalWidth,
    overlay: false,
    modulesVisible: presentation.modulesOpen,
    contextVisible: presentation.contextOpen,
    moduleWidth,
    canvasWidth: logicalWidth - moduleWidth - contextWidth,
    contextWidth,
  };
}

export function parseWorkspacePresentation(value: string | null): WorkspacePresentation {
  if (!value) return { ...defaultWorkspacePresentation };
  try {
    const parsed = JSON.parse(value) as Partial<WorkspacePresentation>;
    const arrangement = parsed.arrangement === 'create' || parsed.arrangement === 'learn' || parsed.arrangement === 'custom' ? parsed.arrangement : 'balanced';
    return {
      arrangement,
      modulesOpen: typeof parsed.modulesOpen === 'boolean' ? parsed.modulesOpen : defaultWorkspacePresentation.modulesOpen,
      contextOpen: typeof parsed.contextOpen === 'boolean' ? parsed.contextOpen : defaultWorkspacePresentation.contextOpen,
      auditionOpen: typeof parsed.auditionOpen === 'boolean' ? parsed.auditionOpen : defaultWorkspacePresentation.auditionOpen,
      narrowDock: parsed.narrowDock === 'context' ? 'context' : 'modules',
    };
  } catch {
    return { ...defaultWorkspacePresentation };
  }
}

export function workspaceGridColumns(layout: WorkspaceLayout): string {
  if (layout.overlay) return 'minmax(0, 1fr)';
  return `${layout.moduleWidth}px minmax(0, 1fr) ${layout.contextWidth}px`;
}
