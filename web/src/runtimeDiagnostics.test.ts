import { describe, expect, it } from 'vitest';
import { formatBytes, parseRuntimeDiagnostics } from './runtimeDiagnostics';

const diagnostic = {
  formatVersion: 1, activeGraphRevision: 7,
  workloadEstimate: { basis: 'prepared-plan-estimate', scalarOperationsPerSample: 96, scalarOperationsPerSecond: 4_608_000, executionDomain: 'sample-wise', families: [{ family: 'pitch-shift', nodeCount: 2, estimatedScalarOperationsPerSample: 96 }] },
  liveCpu: { basis: 'measured', processedBlocks: 100, loadPercent: 1.2, peakLoadPercent: 2.5 },
  delayMemory: { basis: 'prepared-allocation', lineCount: 6, bytes: 46084 },
  latency: { basis: 'compiled-active-graph', samples: 17282, milliseconds: 360.04, hostReportedSamples: 17282, outputPaths: [{ outputPort: 'in-l', samples: 17282, nodeIds: ['input', 'pitch', 'output'] }], parallelJoins: [{ nodeId: 'wet-sum', minimumInputSamples: 0, maximumInputSamples: 17282, uncompensatedSamples: 17282 }], compensationPolicy: 'No hidden graph compensation.' },
  preparedGraph: { nodeCount: 12, connectionCount: 15, feedbackRegionCount: 1, blockWiseRegionCount: 2, sampleWiseRegionCount: 1, preparedStorageBytes: 48000, compileTiming: { validationMicroseconds: 10, schedulingMicroseconds: 20, preparationMicroseconds: 30, totalMicroseconds: 60, requestToActiveMicroseconds: 150 } },
  clipping: { basis: 'measured', samples: 3, blocks: 1 },
  mute: { manual: false, safetyLatched: true, active: true }, safetyEventCoherent: true,
  lastSafetyEvent: { generation: 2, kind: 'runaway', channel: 'left', sampleIndex: 4, graphRevision: 6 }, recoveryCount: 1,
  topologyPublication: { requestedRevision: 9, pendingRevision: 9, activeRevision: 8, failedRevision: 0, supersededRequests: 2, completedCompilations: 7, supersededCompilations: 1, lastSupersededCompileMicroseconds: 55, reclaimedRuntimes: 5, crossfadeFromRevision: 7, crossfadePositionSamples: 128, crossfadeTotalSamples: 480, completedCrossfades: 3, lastCrossfadeFromRevision: 6, lastCrossfadeToRevision: 7, activeDelayLineCount: 6, activeDelayMemoryBytes: 46084, failure: '' },
};

describe('runtime diagnostics contract', () => {
  it('keeps estimates, measurements, prepared allocation, and safety revision explicit', () => {
    const parsed = parseRuntimeDiagnostics(JSON.stringify(diagnostic));
    expect(parsed.workloadEstimate.basis).toBe('prepared-plan-estimate');
    expect(parsed.workloadEstimate.families[0].family).toBe('pitch-shift');
    expect(parsed.liveCpu.basis).toBe('measured');
    expect(parsed.delayMemory.basis).toBe('prepared-allocation');
    expect(parsed.latency.outputPaths[0].nodeIds).toContain('pitch');
    expect(parsed.latency.parallelJoins[0].uncompensatedSamples).toBe(17282);
    expect(parsed.preparedGraph.compileTiming.requestToActiveMicroseconds).toBe(150);
    expect(parsed.lastSafetyEvent?.graphRevision).toBe(6);
    expect(parsed.activeGraphRevision).toBe(7);
    expect(parsed.topologyPublication.pendingRevision).toBe(9);
    expect(parsed.topologyPublication.crossfadeTotalSamples).toBe(480);
    expect(formatBytes(46084)).toBe('45.0 KiB');
  });

  it('rejects inconsistent mute state, unsafe numbers, and incoherent event exposure', () => {
    expect(() => parseRuntimeDiagnostics({ ...diagnostic, mute: { manual: false, safetyLatched: false, active: true } })).toThrow(/inconsistent/);
    expect(() => parseRuntimeDiagnostics({ ...diagnostic, liveCpu: { ...diagnostic.liveCpu, loadPercent: Number.NaN } })).toThrow(/finite/);
    expect(() => parseRuntimeDiagnostics({ ...diagnostic, safetyEventCoherent: false })).toThrow(/must not be exposed/);
  });
});
