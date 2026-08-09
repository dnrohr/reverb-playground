export interface EnergyTelemetryFrame {
  formatVersion: 1;
  enabled: boolean;
  coherent: boolean;
  generation: number;
  observedSampleValues: number;
  nodes: Array<{ nodeId: string; rms: number }>;
}

export type EnergyLevels = Record<string, number>;

const object = (value: unknown, label: string): Record<string, unknown> => {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new Error(`${label} must be an object`);
  return value as Record<string, unknown>;
};

export function parseEnergyTelemetry(value: unknown): EnergyTelemetryFrame {
  const parsed = typeof value === 'string' ? JSON.parse(value) as unknown : value;
  const root = object(parsed, 'energy telemetry');
  if (root.formatVersion !== 1 || typeof root.enabled !== 'boolean' || typeof root.coherent !== 'boolean')
    throw new Error('energy telemetry header is invalid');
  if (!Number.isSafeInteger(root.generation) || Number(root.generation) < 0 || !Number.isSafeInteger(root.observedSampleValues) || Number(root.observedSampleValues) < 0)
    throw new Error('energy telemetry counters are invalid');
  if (!Array.isArray(root.nodes)) throw new Error('energy telemetry nodes must be an array');
  const seen = new Set<string>();
  const nodes = root.nodes.map((item, index) => {
    const node = object(item, `energy telemetry node ${index}`);
    if (typeof node.nodeId !== 'string' || !node.nodeId || seen.has(node.nodeId)) throw new Error(`energy telemetry node ${index} has an invalid identity`);
    if (typeof node.rms !== 'number' || !Number.isFinite(node.rms) || node.rms < 0) throw new Error(`energy telemetry node ${node.nodeId} has invalid RMS`);
    seen.add(node.nodeId);
    return { nodeId: node.nodeId, rms: node.rms };
  });
  return { formatVersion: 1, enabled: root.enabled, coherent: root.coherent, generation: Number(root.generation), observedSampleValues: Number(root.observedSampleValues), nodes };
}

export function rmsToLevel(rms: number): number {
  if (!Number.isFinite(rms) || rms <= 0) return 0;
  return Math.max(0, Math.min(1, (20 * Math.log10(rms) + 72) / 72));
}

export function smoothEnergy(previous: EnergyLevels, frame: EnergyTelemetryFrame, elapsedMilliseconds: number): EnergyLevels {
  if (!frame.enabled || !frame.coherent) return frame.enabled ? previous : {};
  const targets = new Map(frame.nodes.map((node) => [node.nodeId, rmsToLevel(node.rms)]));
  const result: EnergyLevels = {};
  for (const id of new Set([...Object.keys(previous), ...targets.keys()])) {
    const before = previous[id] ?? 0;
    const target = targets.get(id) ?? 0;
    const timeConstant = target > before ? 42 : 260;
    const amount = 1 - Math.exp(-Math.max(0, elapsedMilliseconds) / timeConstant);
    const next = before + (target - before) * amount;
    if (next > .001) result[id] = next;
  }
  return result;
}

export function energyLevelClass(value: number): string {
  return `energy-level-${Math.max(0, Math.min(5, Math.ceil(value * 5)))}`;
}

export function shouldRunEnergyTelemetry(enabled: boolean, reducedMotion: boolean): boolean {
  return enabled && !reducedMotion;
}
