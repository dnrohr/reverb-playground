import type { GraphState, PatchNodeData } from './graph';
import type { Node } from '@xyflow/react';

export type AssistedTuningSuggestionId = 'smoother' | 'less-metallic' | 'wider' | 'less-modulated';

export interface AssistedTuningSuggestion {
  id: AssistedTuningSuggestionId;
  label: string;
  summary: string;
  reason: string;
  changes: string[];
  preserves: string[];
  parameterPatterns: RegExp[];
}

export const assistedTuningSuggestions: AssistedTuningSuggestion[] = [
  { id: 'smoother', label: 'SMOOTHER', summary: 'Use the best eligible rendered delay set.',
    reason: 'The measured candidate lowers recurrence while retaining dense, decorrelated decay.',
    changes: ['Tank delays → 35.2 / 45.9 / 72.7 / 110.3 ms', 'Return gains recalculated for the same 2.1 s RT60'],
    preserves: ['Topology, damping, Width, and diffuser modulation'],
    parameterPatterns: [/^line-delay-[1-4]\.delay$/, /^line-return-[1-4]\.gain$/] },
  { id: 'less-metallic', label: 'LESS METALLIC', summary: 'Darken each circulation path.',
    reason: 'Lower damping cutoffs suppress persistent high-frequency modes without changing topology.',
    changes: ['Line damping cutoffs × 0.72', 'Minimum cutoff remains 1,200 Hz'],
    preserves: ['Delay set, decay returns, Width, and modulation'],
    parameterPatterns: [/^line-damping-[1-4]\.cutoff$/] },
  { id: 'wider', label: 'WIDER', summary: 'Move the explicit Width macro toward its wide endpoint.',
    reason: 'The right pickup vector diverges farther from the left while preserving its vector energy.',
    changes: ['Right pickup vector → 65% toward the wide endpoint', 'Pickup vector energy normalized; no hidden decorrelator'],
    preserves: ['Tank delays, decay, damping, and diffuser modulation'],
    parameterPatterns: [/^right-pickup-[1-4]\.gain$/, /^width-macro\.value$/] },
  { id: 'less-modulated', label: 'LESS MODULATED', summary: 'Reduce moving-diffuser depth.',
    reason: 'Shallower delay motion reduces pitch movement while retaining slow decorrelation.',
    changes: ['Per-line allpass modulation depth → 0.12 ms', 'LFO rates and visible routing unchanged'],
    preserves: ['Delay set, decay, damping, Width, and LFO rates'],
    parameterPatterns: [/^line-diffusion-[1-4]\.delay$/] },
];

export interface AssistedTuningCompatibility {
  kind: 'compatible' | 'replacement' | 'conflict';
  parameterKeys: string[];
  overlap: string[];
  message: string;
}

export function assistedTuningParameterKeys(state: GraphState, id: AssistedTuningSuggestionId): string[] {
  const suggestion = assistedTuningSuggestions.find((item) => item.id === id)!;
  return state.nodes.flatMap((node) => node.data.parameters
    .map((parameter) => `${node.id}.${parameter.id}`)
    .filter((key) => suggestion.parameterPatterns.some((pattern) => pattern.test(key)))).sort();
}

export function assessAssistedTuning(state: GraphState, id: AssistedTuningSuggestionId,
  applied: AssistedTuningSuggestionId[]): AssistedTuningCompatibility {
  const parameterKeys = assistedTuningParameterKeys(state, id);
  const previous = applied.flatMap((appliedId) => assistedTuningParameterKeys(state, appliedId));
  const overlap = parameterKeys.filter((key) => previous.includes(key));
  if (!overlap.length) return { kind: 'compatible', parameterKeys, overlap,
    message: applied.length ? 'Compatible with applied tuning; earlier changes are preserved.' : 'Compatible with the current graph.' };
  if (applied.includes(id)) return { kind: 'replacement', parameterKeys, overlap,
    message: 'This tuning has already been applied. Previewing it again would replace the same parameter set.' };
  return { kind: 'conflict', parameterKeys, overlap,
    message: `Conflicts with applied tuning at ${overlap.join(', ')}.` };
}

function updateParameter(node: Node<PatchNodeData>, parameterId: string,
  update: (parameter: PatchNodeData['parameters'][number]) => PatchNodeData['parameters'][number]) {
  return { ...node, data: { ...node.data, parameters: node.data.parameters.map((parameter) => parameter.id === parameterId ? update(parameter) : parameter) } };
}

export function createAssistedTuningPreview(state: GraphState, id: AssistedTuningSuggestionId): GraphState {
  const preview = structuredClone(state);
  if (id === 'smoother') {
    const delays = [35.2, 45.9, 72.7, 110.3];
    const diffusers = [5.3, 7.1, 8.9, 11.3];
    preview.nodes = preview.nodes.map((node) => {
      const delayMatch = /^line-delay-([1-4])$/.exec(node.id);
      if (delayMatch) return updateParameter(node, 'delay', (parameter) => ({ ...parameter, value: delays[Number(delayMatch[1]) - 1]! }));
      const returnMatch = /^line-return-([1-4])$/.exec(node.id);
      if (returnMatch) {
        const index = Number(returnMatch[1]) - 1;
        const gain = Math.min(.98, 10 ** (-3 * (delays[index]! + diffusers[index]!) / 1000 / 2.1));
        return updateParameter(node, 'gain', (parameter) => ({ ...parameter, value: gain,
          modulation: parameter.modulation ? { ...parameter.modulation, amount: gain * .12 } : parameter.modulation }));
      }
      return node;
    });
  } else if (id === 'less-metallic') {
    preview.nodes = preview.nodes.map((node) => /^line-damping-[1-4]$/.test(node.id)
      ? updateParameter(node, 'cutoff', (parameter) => ({ ...parameter, value: Math.max(1_200, parameter.value * .72) })) : node);
  } else if (id === 'wider') {
    const pickups = preview.nodes.filter((node) => /^right-pickup-[1-4]$/.test(node.id));
    const current = pickups.map((node) => node.data.parameters.find((parameter) => parameter.id === 'gain')?.value ?? 0);
    const target = pickups.map((node, index) => {
      const modulation = node.data.parameters.find((parameter) => parameter.id === 'gain')?.modulation;
      return current[index]! + .65 * (modulation?.amount ?? 0);
    });
    const norm = (values: number[]) => Math.sqrt(values.reduce((total, value) => total + value * value, 0));
    const scale = norm(target) > 0 ? norm(current) / norm(target) : 1;
    preview.nodes = preview.nodes.map((node) => {
      const match = /^right-pickup-([1-4])$/.exec(node.id);
      if (match) return updateParameter(node, 'gain', (parameter) => ({ ...parameter, value: target[Number(match[1]) - 1]! * scale }));
      return node.id === 'width-macro' ? updateParameter(node, 'value', (parameter) => ({ ...parameter, value: 0 })) : node;
    });
  } else {
    preview.nodes = preview.nodes.map((node) => /^line-diffusion-[1-4]$/.test(node.id)
      ? updateParameter(node, 'delay', (parameter) => ({ ...parameter,
        modulation: parameter.modulation ? { ...parameter.modulation, amount: .12 } : parameter.modulation })) : node);
  }
  return preview;
}

export function supportsAssistedTuning(state: GraphState) {
  const ids = new Set(state.nodes.map((node) => node.id));
  return ['line-delay-1', 'line-delay-2', 'line-delay-3', 'line-delay-4',
    'line-return-1', 'line-return-2', 'line-return-3', 'line-return-4', 'width-macro']
    .every((id) => ids.has(id));
}
