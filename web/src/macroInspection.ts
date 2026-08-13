import type { Edge, Node } from '@xyflow/react';
import { curveMapControlValue, type ControlCurveFamily } from './controlSemantics';
import type { PatchNodeData } from './graph';

export interface MacroDestination {
  nodeId: string;
  parameterId: string;
  minimum: number;
  maximum: number;
  unit: string;
}

export interface MacroInspection {
  nodeIds: string[];
  edgeIds: string[];
  destinations: MacroDestination[];
}

const parameterValue = (node: Node<PatchNodeData>, id: string, fallback: number) =>
  node.data.parameters.find((parameter) => parameter.id === id)?.value ?? fallback;

function mapperRange(node: Node<PatchNodeData>, input: { minimum: number; maximum: number }) {
  const familyValue = parameterValue(node, 'curve-family', 0);
  const family: ControlCurveFamily = familyValue >= 1.5 ? 'exponential' : familyValue >= 0.5 ? 'power' : 'linear';
  const polarity = parameterValue(node, 'polarity', 1) >= 0.5 ? 'bipolar' : 'unipolar';
  const map = (value: number) => curveMapControlValue(
    value, family, parameterValue(node, 'curve-amount', 0), parameterValue(node, 'exponent', 1),
    parameterValue(node, 'scale', 1), parameterValue(node, 'offset', 0), polarity,
    parameterValue(node, 'clamp-min', -1), parameterValue(node, 'clamp-max', 1),
  );
  const first = map(input.minimum); const second = map(input.maximum);
  return { minimum: Math.min(first, second), maximum: Math.max(first, second) };
}

export function inspectMacroReachability(
  macroId: string, nodes: Node<PatchNodeData>[], edges: Edge[], maximumDestinations = 128,
): MacroInspection {
  const byId = new Map(nodes.map((node) => [node.id, node]));
  const outgoing = new Map<string, Edge[]>();
  for (const edge of edges.filter((candidate) => candidate.data?.signal === 'control')) {
    const list = outgoing.get(edge.source) ?? []; list.push(edge); outgoing.set(edge.source, list);
  }
  const nodeIds = new Set([macroId]); const edgeIds = new Set<string>();
  const destinations: MacroDestination[] = [];
  const queue: Array<{ nodeId: string; range: { minimum: number; maximum: number } }> = [{ nodeId: macroId, range: { minimum: -1, maximum: 1 } }];
  const visited = new Set<string>();
  while (queue.length && destinations.length < maximumDestinations) {
    const current = queue.shift()!;
    const visitKey = `${current.nodeId}:${current.range.minimum}:${current.range.maximum}`;
    if (visited.has(visitKey)) continue; visited.add(visitKey);
    for (const edge of outgoing.get(current.nodeId) ?? []) {
      edgeIds.add(edge.id); nodeIds.add(edge.target);
      const target = byId.get(edge.target); if (!target) continue;
      if (target.data.type === 'control-map' && edge.targetHandle === 'in') {
        queue.push({ nodeId: target.id, range: mapperRange(target, current.range) });
        continue;
      }
      const targetParameter = target.data.parameters.find((parameter) => parameter.modulation?.portId === edge.targetHandle);
      if (!targetParameter?.modulation) continue;
      const modulation = targetParameter.modulation;
      const normalize = (value: number) => modulation.polarity === 'bipolar'
        ? Math.min(1, Math.max(-1, value)) : Math.min(1, Math.max(0, value));
      const map = (value: number) => Math.min(modulation.clampMaximum, Math.max(
        modulation.clampMinimum, targetParameter.value + modulation.amount * normalize(value),
      ));
      const first = map(current.range.minimum); const second = map(current.range.maximum);
      destinations.push({
        nodeId: target.id, parameterId: targetParameter.id,
        minimum: Math.min(first, second), maximum: Math.max(first, second), unit: targetParameter.unit,
      });
    }
  }
  return { nodeIds: [...nodeIds], edgeIds: [...edgeIds], destinations };
}

export function decorateMacroReachability(
  nodes: Node<PatchNodeData>[], edges: Edge[], inspection: MacroInspection | null,
): { nodes: Node<PatchNodeData>[]; edges: Edge[] } {
  if (!inspection) return { nodes, edges };
  const nodeIds = new Set(inspection.nodeIds); const edgeIds = new Set(inspection.edgeIds);
  return {
    nodes: nodes.map((node) => !nodeIds.has(node.id) ? node : { ...node, className: `${node.className ?? ''} macro-reachable`.trim() }),
    edges: edges.map((edge) => !edgeIds.has(edge.id) ? edge : { ...edge, className: `${edge.className ?? ''} macro-reachable`.trim() }),
  };
}
