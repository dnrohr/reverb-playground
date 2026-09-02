import type { PatchNodeData, PatchParameter } from './graph';
import type { ModuleType } from './modules';

export interface ModuleVocabulary {
  displayName: string;
  signal: string;
  audibleRole: string;
  increasingControl: string;
  purpose: string;
  latencySafety: string;
  advancedParameterIds: readonly string[];
}

export const moduleVocabulary: Record<ModuleType, ModuleVocabulary> = {
  'stereo-input': { displayName: 'Stereo Input', signal: 'STEREO BOUNDARY → 2 MONO AUDIO OUTPUTS', audibleRole: 'Brings left and right source channels into the patch.', increasingControl: 'No control: it passes the selected source without adding gain or latency.', purpose: 'Makes channel conversion and routing explicit at the graph boundary.', latencySafety: 'No added latency; exactly one Stereo Input is required and cannot be deleted.', advancedParameterIds: [] },
  'stereo-output': { displayName: 'Stereo Output', signal: '2 MONO AUDIO INPUTS → STEREO BOUNDARY', audibleRole: 'Returns explicit left and right channels from the patch.', increasingControl: 'No control: it adds no width, gain, or processing.', purpose: 'Shows exactly which two mono paths become the host or standalone stereo output.', latencySafety: 'Reports the maximum compiled input-path latency; exactly one Stereo Output is required.', advancedParameterIds: [] },
  gain: { displayName: 'Gain', signal: '1 MONO AUDIO IN → 1 MONO AUDIO OUT', audibleRole: 'Sets level; negative Gain reverses polarity.', increasingControl: 'Increasing Gain makes the path louder; crossing zero restores signal with opposite polarity.', purpose: 'Balances branches, sets feedback return, mutes at zero, and performs explicit subtraction with negative values.', latencySafety: 'No algorithmic latency; feedback magnitude near or above unity can become unsafe.', advancedParameterIds: [] },
  sum: { displayName: 'Sum (+)', signal: '2 MONO AUDIO IN → 1 MONO AUDIO OUT', audibleRole: 'Adds two signals without hidden normalization.', increasingControl: 'No control: louder inputs create a louder sum and may clip or strengthen feedback.', purpose: 'Makes every mix point explicit, including feedback matrices and stereo-to-mono folds.', latencySafety: 'No added latency; summed feedback still requires a causal Delay and conservative loop gain.', advancedParameterIds: [] },
  delay: { displayName: 'Delay', signal: '1 MONO AUDIO IN → 1 MONO AUDIO OUT', audibleRole: 'Adds causal time and creates repeat spacing or feedback memory.', increasingControl: 'Increasing Delay moves echoes farther apart and lengthens any feedback-loop traversal.', purpose: 'Creates predelay, echo spacing, decorrelation, and the causal memory required by feedback.', latencySafety: 'Adds the displayed time to its path; moving time is smoothed but can produce Doppler pitch.', advancedParameterIds: [] },
  allpass: { displayName: 'Allpass', signal: '1 MONO AUDIO IN → 1 MONO AUDIO OUT', audibleRole: 'Diffuses transients while preserving ideal broadband magnitude.', increasingControl: 'Increasing Delay spreads events farther apart; increasing coefficient strengthens the dispersive pattern.', purpose: 'Raises echo density and breaks up obvious repeats without acting as a tone filter.', latencySafety: 'Adds its delay to path latency; coefficient is hard-limited to −0.95…+0.95 for stability.', advancedParameterIds: [] },
  lowpass: { displayName: 'Low-pass', signal: '1 MONO AUDIO IN → 1 MONO AUDIO OUT', audibleRole: 'Removes high-frequency energy for damping and tone.', increasingControl: 'Increasing Cutoff sounds brighter because more high-frequency energy passes.', purpose: 'Controls reverb color and makes high frequencies decay faster inside feedback.', latencySafety: 'No reported algorithmic latency; effective cutoff stays below the sample-rate limit.', advancedParameterIds: [] },
  'pitch-shift': { displayName: 'Pitch Shift', signal: '1 MONO AUDIO IN → 1 MONO AUDIO OUT', audibleRole: 'Moves pitch by a musical ratio with causal grain latency.', increasingControl: 'Increasing Semitones raises the regenerated pitch, up to one octave.', purpose: 'Creates shimmer branches and pitch-regenerated feedback using an explicit visible processor.', latencySafety: 'Adds fixed prepared grain latency; conservative delayed feedback is required for safe regeneration.', advancedParameterIds: ['grain', 'overlap', 'direction', 'phase'] },
  macro: { displayName: 'Macro', signal: '1 BRANCHABLE CONTROL OUTPUT', audibleRole: 'Provides one named performance control for visible mappings.', increasingControl: 'Increasing Value moves every connected mapper through its visible scale, offset, curve, and clamp.', purpose: 'Turns several explicit parameter mappings into one playable control without hiding their destinations.', latencySafety: 'Carries no audio and adds no audio latency; runtime movement uses a fixed 20 ms ramp.', advancedParameterIds: ['default-value', 'center-detent'] },
  lfo: { displayName: 'LFO', signal: '1 BRANCHABLE CONTROL OUTPUT', audibleRole: 'Generates repeating modulation; it does not carry audio.', increasingControl: 'Increasing Frequency makes connected movement cycle faster.', purpose: 'Adds repeatable motion to delays, allpasses, filters, pitch, or mapped performance controls.', latencySafety: 'Evaluated at the bounded control rate and interpolated; it creates no audio feedback by itself.', advancedParameterIds: ['phase', 'waveform', 'run-mode'] },
  'control-map': { displayName: 'Curve Mapper', signal: '1 CONTROL IN → 1 CONTROL OUT', audibleRole: 'Scales, shapes, offsets, and clamps a visible control path.', increasingControl: 'Increasing Scale strengthens or reverses the destination movement according to its sign.', purpose: 'Makes modulation range, polarity, curvature, offset, and safety bounds inspectable before connection.', latencySafety: 'Control-rate only; clamp limits prevent the mapping from exceeding its declared output range.', advancedParameterIds: ['polarity', 'curve-family', 'curve-amount', 'exponent', 'clamp-min', 'clamp-max'] },
  'envelope-follower': { displayName: 'Envelope Follower', signal: '1 MONO AUDIO IN → 1 CONTROL OUT', audibleRole: 'Converts signal magnitude into a normalized detector control.', increasingControl: 'Increasing Attack or Release makes the detector respond or recover more slowly.', purpose: 'Lets signal level drive gates and other explicit control mappings.', latencySafety: 'Does not delay the audio path; timing edits publish through the normal safe graph transition.', advancedParameterIds: [] },
  'hold-gate': { displayName: 'Hold Gate', signal: '1 MONO AUDIO + 1 CONTROL IN → 1 MONO AUDIO OUT', audibleRole: 'Opens, holds, and closes an audio path from detector level.', increasingControl: 'Increasing Threshold requires a stronger detector signal to open; longer timing values soften or extend the gate.', purpose: 'Builds explicit gated or inverse-envelope structures without a hidden level detector.', latencySafety: 'Cannot amplify; timing edits use a bounded graph transition and delayed feedback remains required.', advancedParameterIds: ['attack', 'hold', 'release'] },
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
  else result.push('STEADY GRAPH', 'TRANSITIONING TO EDITED GRAPH');
  return result;
}

export function parameterDisplayName(id: string): string {
  return id.replaceAll('-', ' ').replace(/\b\w/g, (letter) => letter.toUpperCase());
}
