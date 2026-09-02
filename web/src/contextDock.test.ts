import { describe, expect, it } from 'vitest';
import appSource from './App.tsx?raw';
import patchPersistenceSource from './patchPersistence.ts?raw';
import {
  captureRevisionState,
  contextTabFor,
  emphasizeAnalyzeLayout,
  shouldPollRuntimeDiagnostics,
} from './contextDock';
import { arrangementPresentation, resolveWorkspaceLayout } from './workspaceLayout';

describe('unified context dock', () => {
  it('routes selections measurements diagnostics and teaching to one relevant tab', () => {
    expect(contextTabFor('node')).toBe('inspect');
    expect(contextTabFor('cable')).toBe('inspect');
    expect(contextTabFor('loop')).toBe('inspect');
    expect(contextTabFor('matrix')).toBe('analyze');
    expect(contextTabFor('measurement')).toBe('analyze');
    expect(contextTabFor('diagnostics')).toBe('analyze');
  });

  it('makes detailed polling dormant unless analysis or visible Energy needs it', () => {
    expect(shouldPollRuntimeDiagnostics('inspect', true, false)).toBe(false);
    expect(shouldPollRuntimeDiagnostics('learn', true, false)).toBe(false);
    expect(shouldPollRuntimeDiagnostics('analyze', false, false)).toBe(false);
    expect(shouldPollRuntimeDiagnostics('analyze', true, false)).toBe(true);
    expect(shouldPollRuntimeDiagnostics('inspect', false, true)).toBe(true);
  });

  it('marks revision-bound capture evidence current stale or unknown', () => {
    expect(captureRevisionState(7, 7)).toBe('CURRENT');
    expect(captureRevisionState(7, 8)).toBe('STALE');
    expect(captureRevisionState(0, 8)).toBe('UNKNOWN');
  });

  it('widens desktop analysis without reducing the canvas below 490 pixels', () => {
    for (const width of [900, 1200, 1536, 1920]) {
      const layout = emphasizeAnalyzeLayout(resolveWorkspaceLayout(width, arrangementPresentation('balanced')), true);
      expect(layout.canvasWidth).toBeGreaterThanOrEqual(490);
      expect(layout.moduleWidth + layout.canvasWidth + layout.contextWidth).toBe(width);
    }
    const narrow = resolveWorkspaceLayout(899, arrangementPresentation('learn'));
    expect(emphasizeAnalyzeLayout(narrow, true)).toBe(narrow);
  });

  it('keeps tab panels mounted for scroll retention and outside patch semantics', () => {
    expect(appSource).toContain('role="tablist"');
    expect(appSource).toContain('hidden={contextTab !== \'inspect\'}');
    expect(appSource).toContain('hidden={contextTab !== \'analyze\'}');
    expect(appSource).toContain('hidden={contextTab !== \'learn\'}');
    expect(patchPersistenceSource).not.toContain('contextTab');
    expect(appSource).toContain("learn: 'Guide'");
    expect(appSource).not.toContain('reverb-playground-teaching\', next');
  });
});
