import type { Edge, Node } from '@xyflow/react';
import type { GraphState, PatchNodeData, PatchParameter } from './graph';

export type ParameterUpdateSource = 'pointer' | 'numeric' | 'control-modulation' | 'matrix-edit'
  | 'assisted-preview' | 'factory-load' | 'patch-restore' | 'host-restore' | 'automation'
  | 'native-snapshot' | 'undo' | 'redo' | 'ab-promotion' | 'topology-publication';

export interface ParameterPublication<T> { revision: number; source: ParameterUpdateSource; value: T }

export function acceptParameterPublication<T>(current: ParameterPublication<T> | null,
  incoming: ParameterPublication<T>): ParameterPublication<T> {
  return current && incoming.revision < current.revision ? current : incoming;
}
export interface ParameterDisplayState {
  savedBase: number;
  liveValue: number;
  previewValue: number | null;
  pendingTopology: boolean;
  liveModulated: boolean;
  label: string;
}

const bounded = (value: number, parameter: PatchParameter) => Math.min(parameter.modulation?.clampMaximum ?? parameter.maximum,
  Math.max(parameter.modulation?.clampMinimum ?? parameter.minimum, value));

function parameterValue(graph: GraphState | null, nodeId: string, parameterId: string) {
  return graph?.nodes.find((node) => node.id === nodeId)?.data.parameters
    .find((parameter) => parameter.id === parameterId)?.value ?? null;
}

export function resolveParameterDisplay(node: Node<PatchNodeData>, parameter: PatchParameter,
  controlEdges: Edge[], previewGraph: GraphState | null, pendingTopology: boolean): ParameterDisplayState {
  const control = parameter.modulation ? controlEdges.find((edge) => edge.target === node.id
    && edge.targetHandle === parameter.modulation?.portId && edge.data?.signal === 'control')?.data?.controlValue : undefined;
  const liveModulated = typeof control === 'number' && Number.isFinite(control);
  const liveValue = liveModulated ? bounded(parameter.value + parameter.modulation!.amount * control, parameter) : parameter.value;
  const previewValue = parameterValue(previewGraph, node.id, parameter.id);
  const hasPreview = previewValue !== null && Math.abs(previewValue - parameter.value) > 1e-12;
  const layers = ['SAVED BASE'];
  if (liveModulated) layers.push('LIVE MODULATED');
  if (hasPreview) layers.push('AUDITION PREVIEW');
  if (pendingTopology) layers.push('PENDING TOPOLOGY');
  return { savedBase: parameter.value, liveValue, previewValue: hasPreview ? previewValue : null,
    pendingTopology, liveModulated, label: layers.join(' · ') };
}
