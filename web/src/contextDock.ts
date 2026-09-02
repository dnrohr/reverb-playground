import type { WorkspaceLayout } from './workspaceLayout';

export type ContextTab = 'inspect' | 'analyze' | 'learn';
export type ContextIntent = 'node' | 'cable' | 'loop' | 'matrix' | 'measurement' | 'diagnostics';
export type EvidenceKind = 'EDITED' | 'MEASURED' | 'ESTIMATED' | 'PREDICTED' | 'DOCUMENTED' | 'COMPILED' | 'PREPARED';

export function contextTabFor(intent: ContextIntent): ContextTab {
  if (intent === 'node' || intent === 'cable' || intent === 'loop') return 'inspect';
  if (intent === 'matrix' || intent === 'measurement' || intent === 'diagnostics') return 'analyze';
  return 'analyze';
}

export function shouldPollRuntimeDiagnostics(tab: ContextTab, contextVisible: boolean, energyVisible: boolean): boolean {
  return energyVisible || (contextVisible && tab === 'analyze');
}

export function captureRevisionState(captureRevision: number, activeRevision: number): 'CURRENT' | 'STALE' | 'UNKNOWN' {
  if (captureRevision <= 0 || activeRevision <= 0) return 'UNKNOWN';
  return captureRevision === activeRevision ? 'CURRENT' : 'STALE';
}

export function emphasizeAnalyzeLayout(layout: WorkspaceLayout, active: boolean): WorkspaceLayout {
  if (!active || layout.overlay || !layout.contextVisible) return layout;
  const available = Math.max(layout.contextWidth, layout.width - layout.moduleWidth - 490);
  const contextWidth = Math.max(layout.contextWidth, Math.min(420, available));
  return { ...layout, contextWidth, canvasWidth: layout.width - layout.moduleWidth - contextWidth };
}
