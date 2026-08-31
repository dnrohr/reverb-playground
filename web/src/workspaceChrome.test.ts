import { describe, expect, it } from 'vitest';
import appSource from './App.tsx?raw';
import { editorCommandBarHeight, measurementBarHeight, webCanvasHeightGain } from './workspaceChrome';

describe('focused workspace chrome', () => {
  it('keeps one compact command row and moves measurement into the conditional bottom drawer', () => {
    expect(editorCommandBarHeight).toBe(48);
    expect(measurementBarHeight).toBe(50);
    expect(webCanvasHeightGain()).toBe(32);
    expect(appSource).toContain("const measurementDrawerVisible = !standaloneAvailable || workspacePresentation.auditionOpen");
    expect(appSource).toContain('gridTemplateRows: `${editorCommandBarHeight}px minmax(0, 1fr) ${measurementDrawerVisible ? measurementBarHeight : 0}px`');
    expect(appSource.indexOf('<MeasurementBar')).toBeGreaterThan(appSource.indexOf('className={`workspace'));
  });

  it('provides complete conventional menus without the retired permanent controls', () => {
    for (const menu of ['file', 'edit', 'view', 'help']) expect(appSource).toContain(`'${menu}'`);
    for (const command of [
      'SAVE PATCH', 'OPEN PATCH…', 'AUDIO DEVICE…', 'RESET PATCH',
      'UNDO', 'REDO', 'COPY', 'PASTE', 'DELETE SELECTION',
      'A/B SNAPSHOT COMPARISON', 'PROCESSING QUALITY', 'ENERGY', 'DIAGNOSTICS',
      'CONTEXTUAL LEARNING', 'KEITH BARR ARCHITECTURE NOTES',
    ]) expect(appSource).toContain(command);
    expect(appSource).not.toContain('className="factory-picker quality-picker"');
    expect(appSource).not.toContain('className="comparison-switch comparison-four"');
    expect(appSource).toContain('aria-label="A/B snapshot comparison"');
    expect(appSource).toContain("callNative('standaloneAuditionAvailable')");
    expect(appSource).toContain('standaloneAvailable ? <button');
  });

  it('keeps patch identity save state and the primary save action in the compact bar', () => {
    expect(appSource).toContain('className="patch-identity"');
    expect(appSource).toContain('isHistoryClean(graphHistory');
    expect(appSource).toContain('className="header-save"');
    expect(appSource).toContain('GRAPH ACTIVE');
  });
});
