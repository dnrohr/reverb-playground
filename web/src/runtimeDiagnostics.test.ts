import { describe, expect, it } from 'vitest';
import { formatBytes, parseRuntimeDiagnostics } from './runtimeDiagnostics';

const diagnostic = {
  formatVersion: 1, activeGraphRevision: 7,
  workloadEstimate: { basis: 'static-estimate', scalarOperationsPerSample: 48, scalarOperationsPerSecond: 2_304_000 },
  liveCpu: { basis: 'measured', processedBlocks: 100, loadPercent: 1.2, peakLoadPercent: 2.5 },
  delayMemory: { basis: 'prepared-allocation', lineCount: 6, bytes: 46084 },
  clipping: { basis: 'measured', samples: 3, blocks: 1 },
  mute: { manual: false, safetyLatched: true, active: true }, safetyEventCoherent: true,
  lastSafetyEvent: { generation: 2, kind: 'runaway', channel: 'left', sampleIndex: 4, graphRevision: 6 }, recoveryCount: 1,
};

describe('runtime diagnostics contract', () => {
  it('keeps estimates, measurements, prepared allocation, and safety revision explicit', () => {
    const parsed = parseRuntimeDiagnostics(JSON.stringify(diagnostic));
    expect(parsed.workloadEstimate.basis).toBe('static-estimate');
    expect(parsed.liveCpu.basis).toBe('measured');
    expect(parsed.delayMemory.basis).toBe('prepared-allocation');
    expect(parsed.lastSafetyEvent?.graphRevision).toBe(6);
    expect(parsed.activeGraphRevision).toBe(7);
    expect(formatBytes(46084)).toBe('45.0 KiB');
  });

  it('rejects inconsistent mute state, unsafe numbers, and incoherent event exposure', () => {
    expect(() => parseRuntimeDiagnostics({ ...diagnostic, mute: { manual: false, safetyLatched: false, active: true } })).toThrow(/inconsistent/);
    expect(() => parseRuntimeDiagnostics({ ...diagnostic, liveCpu: { ...diagnostic.liveCpu, loadPercent: Number.NaN } })).toThrow(/finite/);
    expect(() => parseRuntimeDiagnostics({ ...diagnostic, safetyEventCoherent: false })).toThrow(/must not be exposed/);
  });
});
