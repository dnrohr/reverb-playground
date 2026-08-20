import type { Edge, Node, Viewport } from '@xyflow/react';
import type { PatchNodeData, RuntimeSnapshot } from './graph';
import { createFlowModel } from './graph';
import { parsePatchJson } from './patchPersistence';
import reverseEnvelopePatch from '../../factory-patches/causal-reverse-envelope.rvp.json?raw';
import levelGatedPatch from '../../factory-patches/level-gated-room.rvp.json?raw';
import cosmicReversePatch from '../../factory-patches/modulated-cosmic-reverse.rvp.json?raw';
import gravityDiffusionPatch from '../../factory-patches/gravity-diffusion.rvp.json?raw';
import safeParallelShimmerPatch from '../../factory-patches/safe-parallel-shimmer.rvp.json?raw';

export type FactoryPatchId = 'barr-reference' | 'causal-reverse-envelope' | 'level-gated-room' | 'modulated-cosmic-reverse' | 'gravity-diffusion' | 'safe-parallel-shimmer';
export type ComparisonPatchId = Exclude<FactoryPatchId, 'barr-reference'>;

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
  {
    id: 'modulated-cosmic-reverse',
    label: 'Modulated Cosmic Reverse',
    graphName: 'MODULATED-COSMIC-REVERSE.graph',
    filename: 'modulated-cosmic-reverse.rvp.json',
    summary: 'Rising taps enter a damped, slowly modulated feedback space',
  },
  {
    id: 'gravity-diffusion',
    label: 'Gravity Diffusion',
    graphName: 'GRAVITY-DIFFUSION.graph',
    filename: 'gravity-diffusion.rvp.json',
    summary: 'Eight-stage causal inverse, bloom, and forward diffusion instrument',
  },
  {
    id: 'safe-parallel-shimmer',
    label: 'Safe Parallel Shimmer',
    graphName: 'SAFE-PARALLEL-SHIMMER.graph',
    filename: 'safe-parallel-shimmer.rvp.json',
    summary: 'One non-recirculating +12-semitone halo beside an aligned normal tail',
  },
] as const;

const rawPatches: Partial<Record<FactoryPatchId, string>> = {
  'causal-reverse-envelope': reverseEnvelopePatch,
  'level-gated-room': levelGatedPatch,
  'modulated-cosmic-reverse': cosmicReversePatch,
  'gravity-diffusion': gravityDiffusionPatch,
  'safe-parallel-shimmer': safeParallelShimmerPatch,
};

export function factoryPatchDescription(id: FactoryPatchId): FactoryPatchDescription {
  return factoryPatches.find((patch) => patch.id === id)!;
}

export function comparisonPatchAfterSelection(id: FactoryPatchId | 'custom', current: ComparisonPatchId): ComparisonPatchId {
  return id === 'barr-reference' || id === 'custom' ? current : id;
}

export function comparisonPatchLabel(id: ComparisonPatchId): string {
  switch (id) {
    case 'causal-reverse-envelope': return 'REVERSE ENV';
    case 'level-gated-room': return 'GATED';
    case 'modulated-cosmic-reverse': return 'COSMIC REV';
    case 'gravity-diffusion': return 'GRAVITY';
    case 'safe-parallel-shimmer': return 'PAR SHIMMER';
  }
}

export function loadFactoryPatch(id: FactoryPatchId, snapshot: RuntimeSnapshot): FactoryPatchGraph {
  if (id === 'barr-reference') {
    const graph = createFlowModel(snapshot);
    return { ...graph, viewport: { x: 0, y: 0, zoom: 1 } };
  }
  return parsePatchJson(rawPatches[id]!, snapshot);
}
