import { gravityState, type GravityState } from './gravityPresentation';
import inverseJson from '../../artifacts/audio/m9-4-gravity-references/gravity-inverse-48k-measurements.json?raw';
import bloomJson from '../../artifacts/audio/m9-4-gravity-references/gravity-bloom-48k-measurements.json?raw';
import forwardJson from '../../artifacts/audio/m9-4-gravity-references/gravity-forward-48k-measurements.json?raw';

interface ReferenceDocument {
  referenceId: 'inverse' | 'bloom' | 'forward';
  comparisonHorizonMs: number;
  controls: Record<'gravity' | 'size' | 'feedback' | 'damping' | 'modulation', number>;
  matched: { onsetFrame: number; timeToPeakMs: number; earlyLateEnergyRatioDb: number };
  matchedEnvelope: Array<[number, number]>;
}

export interface GravityMeasuredReference {
  state: GravityState;
  label: string;
  path: string;
  peakMilliseconds: number;
  earlyLateEnergyRatioDb: number;
  controlsDescription: string;
  description: string;
}

const documents = new Map<GravityState, ReferenceDocument>([
  ['INVERSE', JSON.parse(inverseJson) as ReferenceDocument],
  ['BLOOM', JSON.parse(bloomJson) as ReferenceDocument],
  ['FORWARD', JSON.parse(forwardJson) as ReferenceDocument],
]);

export function gravityMeasuredReference(value: number): GravityMeasuredReference {
  const state = gravityState(value);
  const document = documents.get(state)!;
  const points = document.matchedEnvelope.map(([time, energy]) =>
    `${(time * 100).toFixed(2)},${(38 - Math.max(0, Math.min(1, energy)) * 32).toFixed(2)}`);
  const controlsDescription = Object.entries(document.controls)
    .map(([name, setting]) => `${name} ${setting >= 0 ? '+' : ''}${setting.toFixed(2)}`).join(', ');
  return {
    state,
    label: `MEASURED ${state} REFERENCE`,
    path: `M ${points.join(' L ')}`,
    peakMilliseconds: document.matched.timeToPeakMs,
    earlyLateEnergyRatioDb: document.matched.earlyLateEnergyRatioDb,
    controlsDescription,
    description: `Checked ${state.toLowerCase()} impulse fixture: peak ${document.matched.timeToPeakMs.toFixed(1)} milliseconds, early/late ratio ${document.matched.earlyLateEnergyRatioDb.toFixed(1)} decibels. Reference evidence, not the current live capture.`,
  };
}
