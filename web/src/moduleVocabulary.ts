import type { PatchNodeData, PatchParameter } from './graph';
import type { ModuleType } from './modules';

export interface ModuleVocabulary {
  displayName: string;
  signal: string;
  audibleRole: string;
  advancedParameterIds: readonly string[];
}

export const moduleVocabulary: Record<ModuleType, ModuleVocabulary> = {
  'stereo-input': { displayName: 'Stereo Input', signal: 'STEREO BOUNDARY → 2 MONO AUDIO OUTPUTS', audibleRole: 'Brings left and right source channels into the patch.', advancedParameterIds: [] },
  'stereo-output': { displayName: 'Stereo Output', signal: '2 MONO AUDIO INPUTS → STEREO BOUNDARY', audibleRole: 'Returns explicit left and right channels from the patch.', advancedParameterIds: [] },
  gain: { displayName: 'Gain', signal: '1 MONO AUDIO IN → 1 MONO AUDIO OUT', audibleRole: 'Sets level; negative Gain reverses polarity.', advancedParameterIds: [] },
  sum: { displayName: 'Sum (+)', signal: '2 MONO AUDIO IN → 1 MONO AUDIO OUT', audibleRole: 'Adds two signals without hidden normalization.', advancedParameterIds: [] },
  delay: { displayName: 'Delay', signal: '1 MONO AUDIO IN → 1 MONO AUDIO OUT', audibleRole: 'Adds causal time and creates repeat spacing or feedback memory.', advancedParameterIds: [] },
  allpass: { displayName: 'Allpass', signal: '1 MONO AUDIO IN → 1 MONO AUDIO OUT', audibleRole: 'Diffuses transients while preserving ideal broadband magnitude.', advancedParameterIds: [] },
  lowpass: { displayName: 'Low-pass', signal: '1 MONO AUDIO IN → 1 MONO AUDIO OUT', audibleRole: 'Removes high-frequency energy for damping and tone.', advancedParameterIds: [] },
  'pitch-shift': { displayName: 'Pitch Shift', signal: '1 MONO AUDIO IN → 1 MONO AUDIO OUT', audibleRole: 'Moves pitch by a musical ratio with causal grain latency.', advancedParameterIds: ['grain', 'overlap', 'direction', 'phase'] },
  macro: { displayName: 'Macro', signal: '1 BRANCHABLE CONTROL OUTPUT', audibleRole: 'Provides one named performance control for visible mappings.', advancedParameterIds: ['default-value', 'center-detent'] },
  lfo: { displayName: 'LFO', signal: '1 BRANCHABLE CONTROL OUTPUT', audibleRole: 'Generates repeating modulation; it does not carry audio.', advancedParameterIds: ['phase', 'waveform', 'run-mode'] },
  'control-map': { displayName: 'Curve Mapper', signal: '1 CONTROL IN → 1 CONTROL OUT', audibleRole: 'Scales, shapes, offsets, and clamps a visible control path.', advancedParameterIds: ['polarity', 'curve-family', 'curve-amount', 'exponent', 'clamp-min', 'clamp-max'] },
  'envelope-follower': { displayName: 'Envelope Follower', signal: '1 MONO AUDIO IN → 1 CONTROL OUT', audibleRole: 'Converts signal magnitude into a normalized detector control.', advancedParameterIds: [] },
  'hold-gate': { displayName: 'Hold Gate', signal: '1 MONO AUDIO + 1 CONTROL IN → 1 MONO AUDIO OUT', audibleRole: 'Opens, holds, and closes an audio path from detector level.', advancedParameterIds: ['attack', 'hold', 'release'] },
};

export const vocabularyFor = (data: PatchNodeData): ModuleVocabulary | null => moduleVocabulary[data.type as ModuleType] ?? null;

export function moduleSignalBadge(data: PatchNodeData): string {
  if (data.type === 'stereo-input') return 'STEREO → 2× MONO';
  if (data.type === 'stereo-output') return '2× MONO → STEREO';
  if (data.type === 'envelope-follower') return 'MONO AUDIO → CONTROL';
  if (data.type === 'hold-gate') return 'MONO AUDIO + CONTROL';
  if (data.role === 'control') return 'CONTROL SIGNAL';
  return 'MONO AUDIO';
}

export function visibleModuleLabel(data: PatchNodeData): string {
  if (data.userName?.trim()) return data.userName.trim();
  if (data.type === 'gain' && data.label === 'Gain / Invert') return 'Gain';
  return data.label;
}

export const isAdvancedParameter = (data: PatchNodeData, parameterId: string) =>
  vocabularyFor(data)?.advancedParameterIds.includes(parameterId) ?? false;

export function parameterBehavior(data: PatchNodeData, parameter: PatchParameter): string[] {
  const result: string[] = [];
  if (parameter.modulation) result.push('MODULATED', 'SMOOTHED');
  else if (data.type === 'macro' && parameter.id === 'value') result.push('SMOOTHED RUNTIME');
  else result.push('BASE ONLY', 'CROSSFADED REBUILD');
  return result;
}

export function parameterDisplayName(id: string): string {
  return id.replaceAll('-', ' ').replace(/\b\w/g, (letter) => letter.toUpperCase());
}
