import type { Viewport } from '@xyflow/react';
import type { GraphState, PatchNodeData } from './graph';
import { semanticGraphHash, snapshotGraph } from './graphHistory';
import type { ImpulseCaptureResult } from './impulseCapture';
import type { QualityPolicy } from './patchPersistence';

export type ComparisonSlot = 'A' | 'B';
export type ComparisonMode = 'raw' | 'matched';

export interface ComparisonProbe {
  accepted: boolean;
  reason: string;
  rms: number;
  rmsDb: number;
  peak: number;
  frameCount: number;
  generation: number;
}

export interface ComparisonSnapshot {
  slot: ComparisonSlot;
  label: string;
  graph: GraphState;
  graphHash: string;
  viewport: Viewport;
  qualityPolicy: QualityPolicy;
  wetGain: number;
  dryGain: number;
  latencySamples: number | null;
  probe: ComparisonProbe | null;
}

export interface ComparisonDiff {
  addedBlocks: string[];
  removedBlocks: string[];
  changedBlocks: string[];
  gainChanges: string[];
  latencyDeltaSamples: number | null;
}

const db = (linear: number) => 20 * Math.log10(Math.max(linear, 1e-12));

export function analyseComparisonProbe(capture: ImpulseCaptureResult): ComparisonProbe {
  if (capture.frameCount < 128 || capture.left.length !== capture.frameCount || capture.right.length !== capture.frameCount)
    return { accepted: false, reason: 'INSUFFICIENT PROBE LENGTH', rms: 0, rmsDb: -240, peak: 0, frameCount: capture.frameCount, generation: capture.generation };
  let sumSquares = 0; let peak = 0;
  for (let frame = 0; frame < capture.frameCount; frame++) {
    const left = capture.left[frame]; const right = capture.right[frame];
    if (!Number.isFinite(left) || !Number.isFinite(right))
      return { accepted: false, reason: 'UNSTABLE NON-FINITE OUTPUT', rms: 0, rmsDb: -240, peak: Number.POSITIVE_INFINITY, frameCount: capture.frameCount, generation: capture.generation };
    peak = Math.max(peak, Math.abs(left), Math.abs(right));
    sumSquares += left * left + right * right;
  }
  const rms = Math.sqrt(sumSquares / (capture.frameCount * 2));
  const reason = peak >= 1 ? 'UNSTABLE OR CLIPPED OUTPUT' : rms <= 1e-7 ? 'SILENT PROBE' : '';
  return { accepted: reason === '', reason, rms, rmsDb: db(rms), peak, frameCount: capture.frameCount, generation: capture.generation };
}

export function captureComparisonSnapshot(input: {
  slot: ComparisonSlot; label: string; graph: GraphState; viewport: Viewport; qualityPolicy: QualityPolicy;
  wetGain: number; dryGain: number; latencySamples: number | null; capture: ImpulseCaptureResult | null;
  captureGraphHash: string | null;
}): ComparisonSnapshot {
  const graph = snapshotGraph(input.graph); const graphHash = semanticGraphHash(graph);
  return { slot: input.slot, label: input.label.trim() || `Snapshot ${input.slot}`, graph, graphHash,
    viewport: { ...input.viewport }, qualityPolicy: input.qualityPolicy,
    wetGain: input.wetGain, dryGain: input.dryGain, latencySamples: input.latencySamples,
    probe: input.capture && input.captureGraphHash === graphHash ? analyseComparisonProbe(input.capture) : null };
}

const canonicalNode = (node: { data: PatchNodeData }) => JSON.stringify({ type: node.data.type, userName: node.data.userName,
  parameters: node.data.parameters.map(({ id, value, modulation }) => ({ id, value, modulation })) });

export function compareSnapshots(a: ComparisonSnapshot, b: ComparisonSnapshot): ComparisonDiff {
  const aNodes = new Map(a.graph.nodes.map((node) => [node.id, node]));
  const bNodes = new Map(b.graph.nodes.map((node) => [node.id, node]));
  const addedBlocks = [...bNodes.keys()].filter((id) => !aNodes.has(id)).sort();
  const removedBlocks = [...aNodes.keys()].filter((id) => !bNodes.has(id)).sort();
  const changedBlocks = [...aNodes.keys()].filter((id) => bNodes.has(id) && canonicalNode(aNodes.get(id)!) !== canonicalNode(bNodes.get(id)!)).sort();
  const gainChanges: string[] = [];
  if (a.wetGain !== b.wetGain) gainChanges.push(`Wet ${a.wetGain.toFixed(3)} → ${b.wetGain.toFixed(3)}`);
  if (a.dryGain !== b.dryGain) gainChanges.push(`Dry ${a.dryGain.toFixed(3)} → ${b.dryGain.toFixed(3)}`);
  for (const id of changedBlocks) {
    const before = aNodes.get(id)!.data.parameters.find((parameter) => parameter.id === 'gain')?.value;
    const after = bNodes.get(id)!.data.parameters.find((parameter) => parameter.id === 'gain')?.value;
    if (before !== undefined && after !== undefined && before !== after) gainChanges.push(`${id} ${before.toFixed(3)} → ${after.toFixed(3)}`);
  }
  return { addedBlocks, removedBlocks, changedBlocks, gainChanges,
    latencyDeltaSamples: a.latencySamples === null || b.latencySamples === null ? null : b.latencySamples - a.latencySamples };
}

export function comparisonMatch(a: ComparisonSnapshot | null, b: ComparisonSnapshot | null) {
  if (!a || !b) return { accepted: false as const, reason: 'CAPTURE BOTH SNAPSHOTS' };
  if (!a.probe || !b.probe) return { accepted: false as const, reason: 'CAPTURE AN IMPULSE FOR EACH GRAPH BEFORE STORING IT' };
  if (!a.probe.accepted) return { accepted: false as const, reason: `A: ${a.probe.reason}` };
  if (!b.probe.accepted) return { accepted: false as const, reason: `B: ${b.probe.reason}` };
  const target = Math.min(a.probe.rms, b.probe.rms);
  const adjustmentA = db(target / a.probe.rms); const adjustmentB = db(target / b.probe.rms);
  return { accepted: true as const, reason: '', adjustmentA, adjustmentB,
    targetRmsDb: db(target), evidence: `${a.probe.frameCount}/${b.probe.frameCount} frames · impulse generations ${a.probe.generation}/${b.probe.generation}` };
}

export const comparisonGainLinear = (adjustmentDb: number) => Math.pow(10, adjustmentDb / 20);
