import type { Edge, Node, Viewport } from '@xyflow/react';
import type { PatchNodeData, RuntimeSnapshot } from './graph';
import { createFlowModel } from './graph';
import { parsePatchJson } from './patchPersistence';
import reverseEnvelopePatch from '../../factory-patches/causal-reverse-envelope.rvp.json?raw';
import levelGatedPatch from '../../factory-patches/level-gated-room.rvp.json?raw';

export type FactoryPatchId = 'barr-reference' | 'causal-reverse-envelope' | 'level-gated-room';

export interface FactoryPatchDescription {
  id: FactoryPatchId;
  label: string;
  graphName: string;
  filename: string;
  summary: string;
}

export interface FactoryPatchGraph {
  nodes: Node<PatchNodeData>[];
  edges: Edge[];
  viewport: Viewport;
}

export const factoryPatches: readonly FactoryPatchDescription[] = [
  {
    id: 'barr-reference',
    label: 'Barr Reference',
    graphName: 'BARR-REFERENCE.graph',
    filename: 'barr-reference.rvp.json',
    summary: 'Keith Barr-style recirculating diffusion tank',
  },
  {
    id: 'causal-reverse-envelope',
    label: 'Causal Reverse Envelope',
    graphName: 'CAUSAL-REVERSE.graph',
    filename: 'causal-reverse-envelope.rvp.json',
    summary: 'Weighted delay taps build toward a late peak',
  },
  {
    id: 'level-gated-room',
    label: 'Level-Gated Room',
    graphName: 'LEVEL-GATED-ROOM.graph',
    filename: 'level-gated-room.rvp.json',
    summary: 'Envelope follower and hold gates cut a diffuse room tail',
  },
] as const;

const rawPatches: Partial<Record<FactoryPatchId, string>> = {
  'causal-reverse-envelope': reverseEnvelopePatch,
  'level-gated-room': levelGatedPatch,
};

export function factoryPatchDescription(id: FactoryPatchId): FactoryPatchDescription {
  return factoryPatches.find((patch) => patch.id === id)!;
}

export function loadFactoryPatch(id: FactoryPatchId, snapshot: RuntimeSnapshot): FactoryPatchGraph {
  if (id === 'barr-reference') {
    const graph = createFlowModel(snapshot);
    return { ...graph, viewport: { x: 0, y: 0, zoom: 1 } };
  }
  return parsePatchJson(rawPatches[id]!, snapshot);
}
