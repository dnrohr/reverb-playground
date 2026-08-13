import { describe, expect, it } from 'vitest';
import { architectureOverlay } from './architectureOverlay';

function fixture(kind: 'rise' | 'gate') {
  const sampleRate = 1_000;
  const frameCount = 500;
  const left = new Array<number>(frameCount).fill(0);
  const right = new Array<number>(frameCount).fill(0);
  if (kind === 'rise') {
    left.fill(.1, 40, 100); left.fill(.4, 100, 180); left.fill(1, 180, 200); left.fill(.001, 200, 260);
  } else {
    left.fill(1, 0, 20); left.fill(.2, 20, 150);
  }
  return { sampleRate, frameCount, left, right };
}

describe('architecture teaching overlays', () => {
  it('marks measured rise, late peak, and cutoff without claiming literal reversal', () => {
    const overlay = architectureOverlay(fixture('rise'), 'causal-reverse-envelope')!;
    expect(overlay.regions).toEqual([{ label: 'RISING ENERGY', startFrame: 40, endFrame: 185, tone: 'rise' }]);
    expect(overlay.markers[0]).toEqual({ label: 'LATE PEAK', frame: 185, tone: 'peak' });
    expect(overlay.explanation).toMatch(/does not reverse sample order/i);
  });

  it('teaches the cosmic design without claiming a proprietary reconstruction', () => {
    const overlay = architectureOverlay(fixture('rise'), 'modulated-cosmic-reverse')!;
    expect(overlay.title).toBe('Why this swells');
    expect(overlay.explanation).toMatch(/damped feedback space/i);
    expect(overlay.explanation).toMatch(/not a proprietary algorithm reconstruction/i);
  });

  it('marks gate, hold, release, and measured cutoff using visible millisecond controls', () => {
    const overlay = architectureOverlay(fixture('gate'), 'level-gated-room', {
      detectorReleaseMilliseconds: 20, holdMilliseconds: 100, releaseMilliseconds: 10,
    })!;
    expect(overlay.regions.map((region) => [region.label, region.startFrame, region.endFrame])).toEqual([
      ['GATE OPEN', 0, 20], ['HOLD', 20, 120], ['RELEASE', 120, 130],
    ]);
    expect(overlay.markers[0]?.label).toBe('CUTOFF');
    expect(overlay.explanation).toMatch(/retrigger/i);
  });

  it('does not invent architecture teaching for Barr, custom, or silent captures', () => {
    expect(architectureOverlay(fixture('rise'), 'barr-reference')).toBeNull();
    expect(architectureOverlay(fixture('rise'), 'custom')).toBeNull();
    const silent = { sampleRate: 1_000, frameCount: 10, left: new Array(10).fill(0), right: new Array(10).fill(0) };
    expect(architectureOverlay(silent, 'causal-reverse-envelope')).toBeNull();
  });
});
