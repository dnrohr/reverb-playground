import type { ImpulseCaptureResult } from './impulseCapture';
import type { FactoryPatchId } from './factoryPatches';

export type TeachingPatchId = FactoryPatchId | 'custom';
export type OverlayTone = 'rise' | 'peak' | 'gate' | 'hold' | 'release' | 'cutoff';

export interface OverlayRegion {
  label: string;
  startFrame: number;
  endFrame: number;
  tone: OverlayTone;
}

export interface OverlayMarker {
  label: string;
  frame: number;
  tone: OverlayTone;
}

export interface ArchitectureOverlay {
  title: string;
  explanation: string;
  regions: OverlayRegion[];
  markers: OverlayMarker[];
}

export interface GateTeachingParameters {
  detectorReleaseMilliseconds: number;
  holdMilliseconds: number;
  releaseMilliseconds: number;
}

interface EnvelopeLandmarks {
  onsetFrame: number;
  peakFrame: number;
  cutoffFrame: number;
}

function smoothedLandmarks(capture: Pick<ImpulseCaptureResult, 'left' | 'right' | 'sampleRate' | 'frameCount'>): EnvelopeLandmarks | null {
  const windowFrames = Math.max(1, Math.round(capture.sampleRate * .01));
  const bucketCount = Math.ceil(capture.frameCount / windowFrames);
  const energy = new Array<number>(bucketCount).fill(0);
  for (let frame = 0; frame < capture.frameCount; frame += 1)
    energy[Math.floor(frame / windowFrames)]! += capture.left[frame]! ** 2 + capture.right[frame]! ** 2;
  const peakEnergy = Math.max(...energy);
  if (!(peakEnergy > 0)) return null;
  const peakBucket = energy.indexOf(peakEnergy);
  const onsetBucket = energy.findIndex((value) => value > peakEnergy * 1e-8);
  let cutoffBucket = energy.findIndex((value, bucket) => bucket > peakBucket && value <= peakEnergy * 1e-4);
  if (cutoffBucket < 0) cutoffBucket = bucketCount - 1;
  return {
    onsetFrame: Math.min(capture.frameCount - 1, onsetBucket * windowFrames),
    peakFrame: Math.min(capture.frameCount - 1, peakBucket * windowFrames + Math.floor(windowFrames / 2)),
    cutoffFrame: Math.min(capture.frameCount - 1, cutoffBucket * windowFrames),
  };
}

export function architectureOverlay(
  capture: Pick<ImpulseCaptureResult, 'left' | 'right' | 'sampleRate' | 'frameCount'>,
  patchId: TeachingPatchId,
  gate: GateTeachingParameters = { detectorReleaseMilliseconds: 20, holdMilliseconds: 120, releaseMilliseconds: 8 },
): ArchitectureOverlay | null {
  const landmarks = smoothedLandmarks(capture);
  if (!landmarks) return null;
  if (patchId === 'causal-reverse-envelope' || patchId === 'modulated-cosmic-reverse') return {
    title: 'Why this swells',
    explanation: patchId === 'modulated-cosmic-reverse'
      ? 'Visible weighted delays build a causal rise before a damped feedback space. Slow moving allpass times add pitch-active motion; this is an original large-space design, not a proprietary algorithm reconstruction or true time reversal.'
      : 'Visible weighted delays build a causal response toward a late peak. This does not reverse sample order and cannot place wet sound before the triggering event.',
    regions: [{ label: 'RISING ENERGY', startFrame: landmarks.onsetFrame, endFrame: landmarks.peakFrame, tone: 'rise' }],
    markers: [
      { label: 'LATE PEAK', frame: landmarks.peakFrame, tone: 'peak' },
      { label: '-40 dB', frame: landmarks.cutoffFrame, tone: 'cutoff' },
    ],
  };
  if (patchId === 'level-gated-room') {
    const millisecondsToFrames = (milliseconds: number) => Math.round(capture.sampleRate * milliseconds / 1_000);
    const gateEnd = Math.min(landmarks.cutoffFrame, landmarks.onsetFrame + millisecondsToFrames(gate.detectorReleaseMilliseconds));
    const holdEnd = Math.min(landmarks.cutoffFrame, gateEnd + millisecondsToFrames(gate.holdMilliseconds));
    const releaseEnd = Math.min(landmarks.cutoffFrame, holdEnd + millisecondsToFrames(gate.releaseMilliseconds));
    return {
      title: 'Why this stops',
      explanation: 'The input-derived detector opens the visible gates, their hold keeps the diffuse room audible, and release closes it. Repeated or sustained input can retrigger this level gate.',
      regions: [
        { label: 'GATE OPEN', startFrame: landmarks.onsetFrame, endFrame: gateEnd, tone: 'gate' },
        { label: 'HOLD', startFrame: gateEnd, endFrame: holdEnd, tone: 'hold' },
        { label: 'RELEASE', startFrame: holdEnd, endFrame: releaseEnd, tone: 'release' },
      ],
      markers: [{ label: 'CUTOFF', frame: landmarks.cutoffFrame, tone: 'cutoff' }],
    };
  }
  return null;
}
