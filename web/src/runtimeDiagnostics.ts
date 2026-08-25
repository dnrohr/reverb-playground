export interface RuntimeDiagnostics {
  formatVersion: 1;
  activeGraphRevision: number;
  workloadEstimate: { basis: 'static-estimate'; scalarOperationsPerSample: number; scalarOperationsPerSecond: number };
  liveCpu: { basis: 'measured'; processedBlocks: number; loadPercent: number; peakLoadPercent: number };
  delayMemory: { basis: 'prepared-allocation'; lineCount: number; bytes: number };
  latency: {
    basis: 'compiled-active-graph'; samples: number; milliseconds: number; hostReportedSamples: number;
    outputPaths: { outputPort: string; samples: number; nodeIds: string[] }[];
    parallelJoins: { nodeId: string; minimumInputSamples: number; maximumInputSamples: number; uncompensatedSamples: number }[];
    compensationPolicy: string;
  };
  clipping: { basis: 'measured'; samples: number; blocks: number };
  mute: { manual: boolean; safetyLatched: boolean; active: boolean };
  safetyEventCoherent: boolean;
  lastSafetyEvent: null | { generation: number; kind: 'non-finite' | 'runaway'; channel: 'left' | 'right'; sampleIndex: number; graphRevision: number };
  recoveryCount: number;
  topologyPublication: {
    requestedRevision: number; pendingRevision: number; activeRevision: number; failedRevision: number;
    supersededRequests: number; completedCompilations: number; reclaimedRuntimes: number;
    crossfadeFromRevision: number; crossfadePositionSamples: number; crossfadeTotalSamples: number;
    completedCrossfades: number; lastCrossfadeFromRevision: number; lastCrossfadeToRevision: number;
    activeDelayLineCount: number; activeDelayMemoryBytes: number;
    failure: string;
  };
}

const record = (value: unknown, label: string): Record<string, unknown> => {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error(`${label} must be an object`);
  return value as Record<string, unknown>;
};
const finite = (value: unknown, label: string) => {
  if (typeof value !== 'number' || !Number.isFinite(value) || value < 0) throw new Error(`${label} must be finite and non-negative`);
  return value;
};
const count = (value: unknown, label: string) => {
  const result = finite(value, label);
  if (!Number.isSafeInteger(result)) throw new Error(`${label} must be an integer`);
  return result;
};
const flag = (value: unknown, label: string) => {
  if (typeof value !== 'boolean') throw new Error(`${label} must be boolean`);
  return value;
};
const text = (value: unknown, label: string) => {
  if (typeof value !== 'string') throw new Error(`${label} must be a string`);
  return value;
};

export function parseRuntimeDiagnostics(value: unknown): RuntimeDiagnostics {
  const root = record(typeof value === 'string' ? JSON.parse(value) as unknown : value, 'runtime diagnostics');
  if (root.formatVersion !== 1) throw new Error('unsupported runtime diagnostics format');
  const estimate = record(root.workloadEstimate, 'workload estimate');
  const cpu = record(root.liveCpu, 'live CPU');
  const memory = record(root.delayMemory, 'delay memory');
  const latency = record(root.latency, 'latency');
  const clipping = record(root.clipping, 'clipping');
  const mute = record(root.mute, 'mute');
  const topology = record(root.topologyPublication, 'topology publication');
  if (estimate.basis !== 'static-estimate' || cpu.basis !== 'measured' || memory.basis !== 'prepared-allocation' || latency.basis !== 'compiled-active-graph' || clipping.basis !== 'measured') throw new Error('diagnostic bases are invalid');
  if (!Array.isArray(latency.outputPaths) || !Array.isArray(latency.parallelJoins)) throw new Error('latency paths and joins must be arrays');
  const outputPaths = latency.outputPaths.map((item, index) => {
    const path = record(item, `latency output path ${index}`);
    if (!Array.isArray(path.nodeIds) || !path.nodeIds.every((id) => typeof id === 'string')) throw new Error(`latency output path ${index} node IDs must be strings`);
    return { outputPort: text(path.outputPort, 'latency output port'), samples: count(path.samples, 'latency output samples'), nodeIds: path.nodeIds as string[] };
  });
  const parallelJoins = latency.parallelJoins.map((item, index) => {
    const join = record(item, `latency join ${index}`);
    return { nodeId: text(join.nodeId, 'latency join node'), minimumInputSamples: count(join.minimumInputSamples, 'latency join minimum'), maximumInputSamples: count(join.maximumInputSamples, 'latency join maximum'), uncompensatedSamples: count(join.uncompensatedSamples, 'latency join difference') };
  });
  const coherent = flag(root.safetyEventCoherent, 'safety event coherence');
  let event: RuntimeDiagnostics['lastSafetyEvent'] = null;
  if (root.lastSafetyEvent !== null) {
    const raw = record(root.lastSafetyEvent, 'last safety event');
    if ((raw.kind !== 'non-finite' && raw.kind !== 'runaway') || (raw.channel !== 'left' && raw.channel !== 'right')) throw new Error('last safety event kind/channel is invalid');
    event = { generation: count(raw.generation, 'event generation'), kind: raw.kind, channel: raw.channel, sampleIndex: count(raw.sampleIndex, 'event sample'), graphRevision: count(raw.graphRevision, 'event revision') };
  }
  if (!coherent && event !== null) throw new Error('incoherent safety events must not be exposed');
  const parsedMute = { manual: flag(mute.manual, 'manual mute'), safetyLatched: flag(mute.safetyLatched, 'safety latch'), active: flag(mute.active, 'active mute') };
  if (parsedMute.active !== (parsedMute.manual || parsedMute.safetyLatched)) throw new Error('active mute is inconsistent');
  return {
    formatVersion: 1,
    activeGraphRevision: count(root.activeGraphRevision, 'active revision'),
    workloadEstimate: { basis: 'static-estimate', scalarOperationsPerSample: count(estimate.scalarOperationsPerSample, 'estimated operations/sample'), scalarOperationsPerSecond: finite(estimate.scalarOperationsPerSecond, 'estimated operations/second') },
    liveCpu: { basis: 'measured', processedBlocks: count(cpu.processedBlocks, 'processed blocks'), loadPercent: finite(cpu.loadPercent, 'live load'), peakLoadPercent: finite(cpu.peakLoadPercent, 'peak load') },
    delayMemory: { basis: 'prepared-allocation', lineCount: count(memory.lineCount, 'delay lines'), bytes: count(memory.bytes, 'delay bytes') },
    latency: { basis: 'compiled-active-graph', samples: count(latency.samples, 'graph latency samples'), milliseconds: finite(latency.milliseconds, 'graph latency milliseconds'), hostReportedSamples: count(latency.hostReportedSamples, 'host latency samples'), outputPaths, parallelJoins, compensationPolicy: text(latency.compensationPolicy, 'latency compensation policy') },
    clipping: { basis: 'measured', samples: count(clipping.samples, 'clipped samples'), blocks: count(clipping.blocks, 'clipped blocks') },
    mute: parsedMute,
    safetyEventCoherent: coherent,
    lastSafetyEvent: event,
    recoveryCount: count(root.recoveryCount, 'recovery count'),
    topologyPublication: {
      requestedRevision: count(topology.requestedRevision, 'requested topology revision'),
      pendingRevision: count(topology.pendingRevision, 'pending topology revision'),
      activeRevision: count(topology.activeRevision, 'active topology revision'),
      failedRevision: count(topology.failedRevision, 'failed topology revision'),
      supersededRequests: count(topology.supersededRequests, 'superseded topology requests'),
      completedCompilations: count(topology.completedCompilations, 'completed topology compilations'),
      reclaimedRuntimes: count(topology.reclaimedRuntimes, 'reclaimed topology runtimes'),
      crossfadeFromRevision: count(topology.crossfadeFromRevision, 'crossfade source revision'),
      crossfadePositionSamples: count(topology.crossfadePositionSamples, 'crossfade position'),
      crossfadeTotalSamples: count(topology.crossfadeTotalSamples, 'crossfade length'),
      completedCrossfades: count(topology.completedCrossfades, 'completed crossfades'),
      lastCrossfadeFromRevision: count(topology.lastCrossfadeFromRevision, 'last crossfade source'),
      lastCrossfadeToRevision: count(topology.lastCrossfadeToRevision, 'last crossfade target'),
      activeDelayLineCount: count(topology.activeDelayLineCount, 'active topology delay lines'),
      activeDelayMemoryBytes: count(topology.activeDelayMemoryBytes, 'active topology delay memory'),
      failure: typeof topology.failure === 'string' ? topology.failure : (() => { throw new Error('topology failure must be a string'); })(),
    },
  };
}

export function formatBytes(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KiB`;
  return `${(bytes / (1024 * 1024)).toFixed(2)} MiB`;
}
