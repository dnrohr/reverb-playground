export type CaptureState = 'idle' | 'armed' | 'capturing' | 'complete';

export interface ImpulseCaptureStatus {
  state: CaptureState;
  generation: number;
  capturedFrames: number;
  capturedMilliseconds: number;
  maximumLengthMilliseconds: number;
  stopThresholdDb: number;
  muteLiveInput: boolean;
  impulseLevel: number;
}

export interface ImpulseCaptureResult {
  formatVersion: 1;
  generation: number;
  sampleRate: number;
  frameCount: number;
  maximumLengthMilliseconds: number;
  stopThresholdDb: number;
  muteLiveInput: boolean;
  impulseLevel: number;
  stopReason: 'threshold' | 'maximum-length';
  left: number[];
  right: number[];
}

function object(value: unknown): Record<string, unknown> {
  const parsed = typeof value === 'string' ? JSON.parse(value) as unknown : value;
  if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) throw new Error('Capture response is not an object');
  return parsed as Record<string, unknown>;
}

function finite(record: Record<string, unknown>, key: string) {
  const value = record[key];
  if (typeof value !== 'number' || !Number.isFinite(value)) throw new Error(`Capture ${key} is invalid`);
  return value;
}

export function parseImpulseCaptureStatus(value: unknown): ImpulseCaptureStatus {
  const record = object(value);
  const state = record.state;
  if (state !== 'idle' && state !== 'armed' && state !== 'capturing' && state !== 'complete') throw new Error('Capture state is invalid');
  if (typeof record.muteLiveInput !== 'boolean') throw new Error('Capture muteLiveInput is invalid');
  return {
    state,
    generation: finite(record, 'generation'),
    capturedFrames: finite(record, 'capturedFrames'),
    capturedMilliseconds: finite(record, 'capturedMilliseconds'),
    maximumLengthMilliseconds: finite(record, 'maximumLengthMilliseconds'),
    stopThresholdDb: finite(record, 'stopThresholdDb'),
    muteLiveInput: record.muteLiveInput,
    impulseLevel: finite(record, 'impulseLevel'),
  };
}

export function parseImpulseCaptureResult(value: unknown): ImpulseCaptureResult {
  const record = object(value);
  if (record.formatVersion !== 1) throw new Error('Unsupported capture format');
  if (record.stopReason !== 'threshold' && record.stopReason !== 'maximum-length') throw new Error('Capture stopReason is invalid');
  if (typeof record.muteLiveInput !== 'boolean' || !Array.isArray(record.left) || !Array.isArray(record.right)) throw new Error('Capture channel data is invalid');
  const frameCount = finite(record, 'frameCount');
  if (record.left.length !== frameCount || record.right.length !== frameCount || !record.left.every(Number.isFinite) || !record.right.every(Number.isFinite)) throw new Error('Capture channel lengths are invalid');
  return {
    formatVersion: 1,
    generation: finite(record, 'generation'),
    sampleRate: finite(record, 'sampleRate'),
    frameCount,
    maximumLengthMilliseconds: finite(record, 'maximumLengthMilliseconds'),
    stopThresholdDb: finite(record, 'stopThresholdDb'),
    muteLiveInput: record.muteLiveInput,
    impulseLevel: finite(record, 'impulseLevel'),
    stopReason: record.stopReason,
    left: record.left as number[],
    right: record.right as number[],
  };
}
