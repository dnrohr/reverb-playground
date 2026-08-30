import { describe, expect, it } from 'vitest';
import {
  arrangementPresentation,
  defaultWorkspacePresentation,
  parseWorkspacePresentation,
  resolveWorkspaceLayout,
  toggleWorkspaceDock,
  workspaceGridColumns,
  workspacePresentationStorageKey,
  type WorkspaceDock,
  type WorkspacePresentation,
} from './workspaceLayout';
import patchPersistenceSource from './patchPersistence.ts?raw';

const widths = [640, 720, 899, 900, 1200, 1536, 1920];
const scales = [1, 1.25, 1.5];
const representativeStates = ['empty', 'loaded', 'looping', 'exporting', 'selected', 'safety-latched'] as const;

const domainState = (state: typeof representativeStates[number]) => ({
  graph: state === 'empty' ? { nodes: [], edges: [] } : { nodes: [{ id: 'delay-1' }], edges: [{ id: 'cable-1' }] },
  selection: state === 'selected' ? 'delay-1' : null,
  transport: { loaded: state !== 'empty', playing: state === 'looping', frame: 24000 },
  loop: { enabled: state === 'looping', start: 1200, end: 48000 },
  gains: { wet: 0.72, dry: 0.31 },
  analysis: { generation: 4, points: [0, -6, -12] },
  export: { running: state === 'exporting', progress: 0.42 },
  safety: { latched: state === 'safety-latched' },
});

describe('responsive workspace layout', () => {
  it('defines distinct balanced, creation, and learning arrangements', () => {
    expect(arrangementPresentation('balanced')).toMatchObject({ modulesOpen: true, contextOpen: true, auditionOpen: false });
    expect(arrangementPresentation('create')).toMatchObject({ modulesOpen: false, contextOpen: false, auditionOpen: false });
    expect(arrangementPresentation('learn')).toMatchObject({ modulesOpen: false, contextOpen: true, auditionOpen: true });
    expect(resolveWorkspaceLayout(1200, arrangementPresentation('balanced')).canvasWidth).toBe(768);
    expect(resolveWorkspaceLayout(1200, arrangementPresentation('create')).canvasWidth).toBe(1200);
    expect(resolveWorkspaceLayout(1200, arrangementPresentation('learn')).contextWidth).toBe(336);
  });

  it('covers every dock combination and representative state at every required width and scale', () => {
    for (const width of widths) for (const scale of scales) for (const state of representativeStates)
      for (const modulesOpen of [false, true]) for (const contextOpen of [false, true]) for (const narrowDock of ['modules', 'context'] as WorkspaceDock[]) {
        const domain = domainState(state);
        const before = JSON.stringify(domain);
        const presentation: WorkspacePresentation = { arrangement: 'balanced', modulesOpen, contextOpen, auditionOpen: state === 'loaded' || state === 'looping' || state === 'exporting', narrowDock };
        const layout = resolveWorkspaceLayout(width, presentation);
        expect(layout.canvasWidth, `${width}px ${scale}x ${state}`).toBeGreaterThanOrEqual(width < 900 ? width : 490);
        expect(layout.moduleWidth + layout.canvasWidth + layout.contextWidth, `${width}px ${scale}x ${state}`).toBe(layout.overlay ? width + layout.moduleWidth + layout.contextWidth : width);
        expect(layout.modulesVisible && layout.contextVisible && layout.overlay, `${width}px ${scale}x ${state}`).toBe(false);
        expect(layout.canvasWidth * scale).toBeGreaterThanOrEqual((width < 900 ? width : 490) * scale);
        expect(workspaceGridColumns(layout)).not.toContain('undefined');
        expect(JSON.stringify(domain), `${width}px ${scale}x ${state} domain state`).toBe(before);
      }
  });

  it('keeps presentation state separate, restorable, and tolerant of invalid saved preferences', () => {
    const changed = toggleWorkspaceDock(toggleWorkspaceDock(defaultWorkspacePresentation, 'modules'), 'context');
    expect(changed.arrangement).toBe('custom');
    expect(parseWorkspacePresentation(JSON.stringify(changed))).toEqual(changed);
    expect(parseWorkspacePresentation('{broken')).toEqual(defaultWorkspacePresentation);
    expect(parseWorkspacePresentation('{"arrangement":"future"}')).toEqual(defaultWorkspacePresentation);
    expect(patchPersistenceSource).not.toContain(workspacePresentationStorageKey);
  });

  it('uses the most recently requested dock as the sole narrow overlay', () => {
    const both = { ...defaultWorkspacePresentation, modulesOpen: true, contextOpen: true };
    expect(resolveWorkspaceLayout(899, { ...both, narrowDock: 'modules' })).toMatchObject({ modulesVisible: true, contextVisible: false });
    expect(resolveWorkspaceLayout(899, { ...both, narrowDock: 'context' })).toMatchObject({ modulesVisible: false, contextVisible: true });
    expect(resolveWorkspaceLayout(900, both)).toMatchObject({ modulesVisible: true, contextVisible: true, overlay: false });
  });
});
