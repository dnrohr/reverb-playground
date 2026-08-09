import type { Edge, Node } from '@xyflow/react';
import type { PatchNodeData } from './graph';
import { energyLevelClass, type EnergyLevels } from './energyTelemetry';

const appendClass = (existing: string | undefined, added: string) => [existing, added].filter(Boolean).join(' ');

export function decorateEnergy(nodes: Node<PatchNodeData>[], edges: Edge[], levels: EnergyLevels) {
  return {
    nodes: nodes.map((node) => ({ ...node, className: appendClass(node.className, energyLevelClass(levels[node.id] ?? 0)) })),
    edges: edges.map((edge) => ({ ...edge, className: appendClass(edge.className, energyLevelClass(levels[edge.source] ?? 0)) })),
  };
}
