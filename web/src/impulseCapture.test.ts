import { describe, expect, it } from 'vitest';
import { parseImpulseCaptureResult, parseImpulseCaptureStatus } from './impulseCapture';

describe('impulse capture bridge contract', () => {
  it('parses native status JSON and preserves visible bounds', () => {
    expect(parseImpulseCaptureStatus(JSON.stringify({ state: 'capturing', generation: 2, capturedFrames: 4800, capturedMilliseconds: 100, maximumLengthMilliseconds: 2000, stopThresholdDb: -80, muteLiveInput: true, impulseLevel: .1 }))).toEqual({ state: 'capturing', generation: 2, capturedFrames: 4800, capturedMilliseconds: 100, maximumLengthMilliseconds: 2000, stopThresholdDb: -80, muteLiveInput: true, impulseLevel: .1 });
  });

  it('requires finite equal-length stereo capture channels', () => {
    const valid = { formatVersion: 1, generation: 3, sampleRate: 48000, frameCount: 2, maximumLengthMilliseconds: 2000, stopThresholdDb: -80, muteLiveInput: true, impulseLevel: .1, stopReason: 'threshold', left: [0, .2], right: [0, -.1] };
    expect(parseImpulseCaptureResult(valid).frameCount).toBe(2);
    expect(() => parseImpulseCaptureResult({ ...valid, right: [0] })).toThrow(/lengths/);
    expect(() => parseImpulseCaptureResult({ ...valid, left: [0, Number.NaN] })).toThrow(/lengths/);
  });
});
