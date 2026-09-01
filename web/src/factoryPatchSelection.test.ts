import { describe, expect, it } from 'vitest';
import appSource from './App.tsx?raw';

describe('factory patch selection', () => {
  it('clears transient graph-action status before identifying the new patch', () => {
    const selection = appSource.slice(
      appSource.indexOf('const selectFactoryPatch = useCallback'),
      appSource.indexOf('const copySelection = useCallback'),
    );

    expect(selection).toContain('setGraphStatus(null)');
    expect(selection.indexOf('setGraphStatus(null)')).toBeLessThan(selection.indexOf('setActivePatchId(id)'));
  });
});
