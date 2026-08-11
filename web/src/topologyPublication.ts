import type { Edge, Node } from '@xyflow/react';
import type { PatchNodeData } from './graph';

export interface GraphPublicationResult {
  accepted: boolean;
  revision: number;
  error: string;
}

export function audibleGraphFingerprint(nodes: Node<PatchNodeData>[], edges: Edge[]): string {
  const semanticNodes = nodes.map((node) => ({
    id: node.id,
    type: node.data.type,
    ports: node.data.ports,
    parameters: node.data.parameters.map(({ id, value, unit, modulation }) => ({ id, value, unit, modulation })),
  })).sort((left, right) => left.id.localeCompare(right.id));
  const semanticEdges = edges.map((edge) => ({
    id: edge.id,
    source: edge.source,
    sourceHandle: edge.sourceHandle ?? '',
    target: edge.target,
    targetHandle: edge.targetHandle ?? '',
  })).sort((left, right) => left.id.localeCompare(right.id));
  return JSON.stringify({ nodes: semanticNodes, edges: semanticEdges });
}

export function parseGraphPublicationResult(value: unknown): GraphPublicationResult {
  const parsed = typeof value === 'string' ? JSON.parse(value) as unknown : value;
  if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) throw new Error('graph publication result must be an object');
  const result = parsed as Partial<GraphPublicationResult>;
  if (typeof result.accepted !== 'boolean') throw new Error('graph publication accepted flag is invalid');
  if (typeof result.revision !== 'number' || !Number.isSafeInteger(result.revision) || result.revision < 0) throw new Error('graph publication revision is invalid');
  if (typeof result.error !== 'string') throw new Error('graph publication error is invalid');
  if ((result.accepted && (result.revision === 0 || result.error.length > 0))
    || (!result.accepted && (result.revision !== 0 || result.error.length === 0)))
    throw new Error('graph publication result is inconsistent');
  return result as GraphPublicationResult;
}
