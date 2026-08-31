import { useCallback, useEffect, useMemo, useRef, useState, type KeyboardEvent as ReactKeyboardEvent } from 'react';
import {
  Background,
  BackgroundVariant,
  Controls,
  MiniMap,
  ReactFlow,
  ReactFlowProvider,
  useEdgesState,
  useNodesState,
  useReactFlow,
  type Edge,
  type Connection,
  type Node,
  type OnSelectionChangeParams,
  type Viewport,
} from '@xyflow/react';
import { createFlowModel, deleteSelected, parseRuntimeSnapshot, type GraphState, type PatchNodeData, type RuntimeSnapshot } from './graph';
import { createModuleNode, moduleDefinitions, nextNodeId, type ModuleType } from './modules';
import { commitGraphEdit, emptyGraphHistory, isHistoryClean, markHistoryClean, redoGraphEdit, snapshotGraph, undoGraphEdit } from './graphHistory';
import { copySelectedGraph, pasteGraph, type GraphClipboard } from './graphClipboard';
import { decorateFeedbackLoops, decorateRunawayFeedbackLoop, inspectFeedbackLoops, inspectMostRelevantFeedbackLoop, type FeedbackLoopInspection } from './loopInspection';
import { connectGraph, decideConnection, insertSumForOccupiedInput, previewSumForOccupiedInput } from './connectionEditing';
import { PatchNode } from './PatchNode';
import { callNative } from './nativeBridge';
import { parsePatchJson, writePatchJson, type QualityPolicy } from './patchPersistence';
import researchText from '../../docs/keith-barr-reverb-architectures.md?raw';
import { teachingTopicFor, type TeachingTopic } from './teaching';
import { parseImpulseCaptureResult, parseImpulseCaptureStatus, type ImpulseCaptureResult, type ImpulseCaptureStatus } from './impulseCapture';
import { analyseResponse, decayPoints, frameWindow, rt60Explanation, waveformBuckets } from './responseAnalysis';
import { analyseDensity } from './densityAnalysis';
import { decorateEnergy } from './energyDecoration';
import { parseEnergyTelemetry, shouldRunEnergyTelemetry, smoothEnergy, type EnergyLevels } from './energyTelemetry';
import { formatBytes, parseRuntimeDiagnostics, type RuntimeDiagnostics } from './runtimeDiagnostics';
import { audibleGraphFingerprint, parseGraphPublicationResult } from './topologyPublication';
import { curveMappingRange, decorateControlPreview, type ControlCurveFamily } from './controlSemantics';
import { comparisonPatchAfterSelection, comparisonPatchLabel, factoryPatchDescription, factoryPatches, loadFactoryPatch, type ComparisonPatchId, type FactoryPatchId } from './factoryPatches';
import { architectureOverlay, type GateTeachingParameters, type TeachingPatchId } from './architectureOverlay';
import { parseHostPatchStateResult } from './hostPatchState';
import { decorateMacroReachability, inspectMacroReachability } from './macroInspection';
import { gravityFocusNodeIds, predictGravityEnvelope } from './gravityPresentation';
import { gravityMeasuredReference } from './gravityReference';
import { PitchShiftVisualization } from './PitchShiftVisualization';
import { parallelShimmerBranch, parallelShimmerTeaching } from './parallelShimmerTeaching';
import { decorateSplitShimmerFocus, splitFeedbackShimmerTeaching, splitShimmerLoopKind, type SplitShimmerLoopFocus } from './splitFeedbackShimmerTeaching';
import { decorateReverseCosmicFocus, reverseCosmicShimmerTeaching, type ReverseCosmicFocus } from './reverseCosmicShimmerTeaching';
import { collapseMatrixMixer, inspectMatrixMixer, type MatrixMixerInspection } from './matrixMixerPresentation';
import { collapseGraphGroups, createGraphGroup, inspectGraphGroups, removeGraphGroup, renameGraphGroup, setGraphGroupCollapsed } from './graphGroups';
import { addCableWaypoint, alignSelectedNodes, arrangeGraphGroup, cableLayout, clearCableWaypoints, setRoutingPortal, traceCable, updateCableWaypoint, type AlignmentCommand, type TraceDirection } from './graphRouting';
import { RoutedEdge } from './RoutedEdge';
import { assistedTuningSuggestions, createAssistedTuningPreview, supportsAssistedTuning, type AssistedTuningSuggestionId } from './assistedTuning';
import { editorCommandBarHeight, measurementBarHeight } from './workspaceChrome';
import { captureRevisionState, contextTabFor, emphasizeAnalyzeLayout, shouldPollRuntimeDiagnostics, type ContextIntent, type ContextTab } from './contextDock';
import {
  arrangementPresentation,
  parseWorkspacePresentation,
  resolveWorkspaceLayout,
  toggleWorkspaceDock,
  workspaceGridColumns,
  workspacePresentationStorageKey,
  type WorkspaceArrangement,
} from './workspaceLayout';

const modules = [
  { group: 'I/O', items: moduleDefinitions.filter((item) => item.role === 'io') },
  { group: 'SIGNAL', items: moduleDefinitions.filter((item) => item.role !== 'io' && item.role !== 'control') },
  { group: 'CONTROL', items: moduleDefinitions.filter((item) => item.role === 'control') },
];
const contextTabs: ContextTab[] = ['inspect', 'analyze', 'learn'];
const contextTabLabels: Record<ContextTab, string> = { inspect: 'Inspect', analyze: 'Analyze', learn: 'Learn' };

const parameterChoices = (unit: string): { value: number; label: string }[] | null => {
  if (unit === 'boolean') return [{ value: 0, label: 'OFF' }, { value: 1, label: 'ON' }];
  if (unit === 'curve-family') return [{ value: 0, label: 'LINEAR' }, { value: 1, label: 'POWER' }, { value: 2, label: 'EXPONENTIAL' }];
  if (unit === 'waveform') return [{ value: 0, label: 'SINE' }, { value: 1, label: 'TRIANGLE' }];
  if (unit === 'run-mode') return [{ value: 0, label: 'FREE RUN' }, { value: 1, label: 'RESTART ON TRANSPORT' }];
  if (unit === 'polarity') return [{ value: 0, label: 'UNIPOLAR 0…1' }, { value: 1, label: 'BIPOLAR −1…+1' }];
  if (unit === 'direction') return [{ value: 0, label: 'FORWARD GRAINS' }, { value: 1, label: 'REVERSE INSIDE EACH GRAIN' }];
  return null;
};

function formatValue(value: number, unit: string) {
  if (unit === 'milliseconds') return `${value.toFixed(2)} ms`;
  if (unit === 'hertz') return `${value.toLocaleString()} Hz`;
  if (unit === 'semitones') return `${value >= 0 ? '+' : ''}${value.toFixed(2)} st`;
  return value.toFixed(2);
}

function TeachingCard({ topic, onDismiss, onResearch }: {
  topic: TeachingTopic; onDismiss: () => void; onResearch: () => void;
}) {
  return (
    <section className="teaching-card" aria-label="Contextual explanation">
      <div className="teaching-title"><span>LEARN / CONTEXT</span><button type="button" aria-label="Dismiss explanation" onClick={onDismiss}>×</button></div>
      <h3>{topic.title}</h3>
      <h4>DOCUMENTED BARR / MIDIVERB</h4><p>{topic.documented}</p>
      <h4>THIS RECONSTRUCTION</h4><p>{topic.reconstruction}</p>
      <h4>LISTEN / NOTICE</h4><p>{topic.takeaway}</p>
      <button className="research-link" type="button" onClick={onResearch}>READ OFFLINE ARCHITECTURE RESEARCH</button>
    </section>
  );
}

function ParallelShimmerTeaching({ selectedNodeId }: { selectedNodeId?: string }) {
  const branch = parallelShimmerBranch(selectedNodeId);
  return (
    <section className="parallel-shimmer-teaching" aria-label="Safe Parallel Shimmer signal paths">
      <header><span>{parallelShimmerTeaching.title}</span><strong>{parallelShimmerTeaching.method}</strong></header>
      <div className={branch === 'normal' ? 'is-active' : ''}>
        <b>NORMAL</b><span>{parallelShimmerTeaching.normal}</span>
      </div>
      <div className={branch === 'octave' ? 'is-active' : ''}>
        <b>OCTAVE +12</b><span>{parallelShimmerTeaching.octave}</span>
      </div>
      <p>{parallelShimmerTeaching.contrast}</p>
    </section>
  );
}

function MatrixMixerInspector({ inspection }: { inspection: MatrixMixerInspection }) {
  return <section className="matrix-mixer-inspector" aria-label="Matrix Mixer coefficients and energy">
    <header><span>4×4 COEFFICIENTS / EDITED</span><strong>{inspection.orthogonal ? 'ORTHOGONAL / PREDICTED' : 'CUSTOM / PREDICTED'}</strong></header>
    <div className="matrix-coefficients" role="table" aria-label="Matrix coefficient table">
      {inspection.coefficients.flatMap((row, output) => row.map((value, input) => <span role="cell"
        className={value < 0 ? 'is-negative' : 'is-positive'} key={`${output}-${input}`}>{value >= 0 ? '+' : '−'}{Math.abs(value).toFixed(2)}</span>))}
    </div>
    <div className="matrix-energy"><span>ROW ENERGY {inspection.rowEnergy.map((value) => value.toFixed(2)).join(' · ')}</span>
      <span>COLUMN ENERGY {inspection.columnEnergy.map((value) => value.toFixed(2)).join(' · ')}</span></div>
    <p>{inspection.reason}</p><small>Expand to inspect or edit every authoritative Gain and Sum block.</small>
  </section>;
}

function AssistedTuningPanel({ previewId, busy, onPreview, onAccept, onCancel, onClose }: {
  previewId: AssistedTuningSuggestionId | null; busy: boolean;
  onPreview: (id: AssistedTuningSuggestionId) => void; onAccept: () => void;
  onCancel: () => void; onClose: () => void;
}) {
  return <section className="assisted-tuning-panel" aria-label="Assisted tuning suggestions">
    <header><div><span>ASSISTED TUNING</span><strong>PREVIEW IS NOT SAVED</strong></div><button type="button" aria-label="Close assisted tuning" onClick={onClose}>×</button></header>
    <p>Each option publishes a crossfaded audition runtime while the visible and saved graph remain exact.</p>
    <div className="tuning-suggestions">
      {assistedTuningSuggestions.map((suggestion) => <article className={previewId === suggestion.id ? 'is-previewing' : ''} key={suggestion.id}>
        <div><strong>{suggestion.label}</strong><span>{suggestion.summary}</span></div>
        <p>{suggestion.reason}</p>
        <ul>{suggestion.changes.map((change) => <li key={change}>{change}</li>)}</ul>
        <button type="button" disabled={busy} aria-pressed={previewId === suggestion.id} onClick={() => onPreview(suggestion.id)}>
          {previewId === suggestion.id ? 'PREVIEWING' : 'PREVIEW'}
        </button>
      </article>)}
    </div>
    <footer><button type="button" disabled={!previewId || busy} onClick={onAccept}>ACCEPT + ADD TO UNDO</button><button type="button" disabled={!previewId || busy} onClick={onCancel}>CANCEL / RESTORE EXACT</button></footer>
  </section>;
}

function DenseFigureEightTeaching() {
  return (
    <section className="parallel-shimmer-teaching" aria-label="Dense Figure Eight architecture teaching">
      <header><span>DENSE FIGURE EIGHT</span><strong>CROSS-COUPLED TANK</strong></header>
      <div><b>BRANCH A</b><span>209.3 ms nominal traversal · positive return</span></div>
      <div><b>BRANCH B</b><span>242.9 ms nominal traversal · inverted return</span></div>
      <p>Each branch feeds the other. Unequal delays, distributed allpasses, damping, and moving taps turn repeat trains into a denser late field.</p>
      <small>DECAY adjusts both calculated returns; TONE moves both loop filters; MOTION changes the two independent modulation rates.</small>
    </section>
  );
}

function FourLineFdnTeaching({ collapsed }: { collapsed: boolean }) {
  return <section className="parallel-shimmer-teaching" aria-label="Four-Line Dense Room circulation teaching">
    <header><span>FOUR-LINE DENSE ROOM</span><strong>HADAMARD FDN</strong></header>
    <div><b>1 → 4</b><span>Each return is distributed across every line at ±0.50.</span></div>
    <div><b>ENERGY</b><span>The normalized matrix preserves vector energy; per-line gains set decay.</span></div>
    <p>Unequal damped delays circulate through an orthogonal matrix, while signed pickup vectors expose two different views of the same field.</p>
    <small>{collapsed ? 'The matrix is compact for navigation. Expand Matrix reveals all 16 Gains and 12 Sums.' : 'Expanded view is authoritative and editable. Collapse Matrix restores the compact navigation view.'}</small>
  </section>;
}

function SplitFeedbackShimmerTeaching({ focus, onFocus }: {
  focus: SplitShimmerLoopFocus; onFocus: (focus: SplitShimmerLoopFocus) => void;
}) {
  return (
    <section className="split-shimmer-teaching" aria-label="Split-Feedback Shimmer circulation teaching">
      <header><span>{splitFeedbackShimmerTeaching.title}</span><strong>{splitFeedbackShimmerTeaching.method}</strong></header>
      <div className="split-loop-focus" role="group" aria-label="Highlight shimmer path">
        {(['normal', 'shifted', 'shared'] as const).map((id) => <button type="button" key={id}
          className={focus === id ? 'is-active' : ''} aria-pressed={focus === id} onClick={() => onFocus(id)}>
          {id === 'normal' ? 'NORMAL LOOP' : id === 'shifted' ? 'SHIFTED LOOP' : 'SHARED TANK'}
        </button>)}
      </div>
      <p className="split-path-copy">{splitFeedbackShimmerTeaching[focus]}</p>
      <ol className="circulation-steps">{splitFeedbackShimmerTeaching.circulation.map((step) => <li key={step.pass}>
        <b>{step.pass}</b><strong>{step.frequency}</strong><span>{step.note}</span>
      </li>)}</ol>
      <p>{splitFeedbackShimmerTeaching.evidence}</p>
      <small>{splitFeedbackShimmerTeaching.boundary}</small>
    </section>
  );
}

function ReverseCosmicShimmerTeaching({ focus, onFocus }: {
  focus: ReverseCosmicFocus; onFocus: (focus: ReverseCosmicFocus) => void;
}) {
  return (
    <section className="reverse-cosmic-teaching" aria-label="Reverse Cosmic Shimmer signal-path teaching">
      <header><span>{reverseCosmicShimmerTeaching.title}</span><strong>{reverseCosmicShimmerTeaching.method}</strong></header>
      <div className="reverse-cosmic-focus" role="group" aria-label="Highlight Reverse Cosmic Shimmer path">
        {(['rise', 'grains', 'feedback', 'motion'] as const).map((id) => <button type="button" key={id}
          className={focus === id ? 'is-active' : ''} aria-pressed={focus === id} onClick={() => onFocus(id)}>
          {id === 'rise' ? 'CAUSAL RISE' : id === 'grains' ? 'REVERSE GRAINS' : id === 'feedback' ? 'DARK RETURNS' : 'STEREO MOTION'}
        </button>)}
      </div>
      <p className="reverse-cosmic-path-copy">{reverseCosmicShimmerTeaching[focus]}</p>
      <p>{reverseCosmicShimmerTeaching.evidence}</p>
      <small>{reverseCosmicShimmerTeaching.boundary}</small>
    </section>
  );
}

function LoopInspector({ inspection, activeIndex, onActiveIndex, splitFeedback = false }: {
  inspection: FeedbackLoopInspection; activeIndex: number; onActiveIndex: (index: number) => void;
  splitFeedback?: boolean;
}) {
  const loop = inspection.loops[activeIndex];
  if (!loop) return (
    <section className="loop-inspector loop-empty" aria-label="Feedback loop inspection">
      <div className="loop-heading"><span>FEEDBACK LOOPS</span><strong>NONE</strong></div>
      <p>No directed feedback loop contains this selection.</p>
    </section>
  );
  return (
    <section className="loop-inspector" aria-label="Feedback loop inspection">
      <div className="loop-heading"><span>FEEDBACK LOOP{splitFeedback ? ` / ${splitShimmerLoopKind(loop.nodeIds)}` : ''}</span><strong>{activeIndex + 1} / {inspection.loops.length}</strong></div>
      {inspection.loops.length > 1 ? <div className="loop-navigation">
        <button type="button" aria-label="Previous feedback loop" onClick={() => onActiveIndex((activeIndex + inspection.loops.length - 1) % inspection.loops.length)}>PREV</button>
        <button type="button" aria-label="Next feedback loop" onClick={() => onActiveIndex((activeIndex + 1) % inspection.loops.length)}>NEXT</button>
      </div> : null}
      <dl className="loop-facts">
        <div><dt>NOMINAL DELAY</dt><dd>{loop.nominalDelayMilliseconds.toFixed(2)} ms</dd></div>
        <div><dt>BLOCKS</dt><dd>{loop.nodeIds.length}</dd></div>
      </dl>
      <h3>CONSTITUENT BLOCKS</h3>
      <code className="loop-path">{[...loop.nodeIds, loop.nodeIds[0]].join(' → ')}</code>
      <h3>POLARITY / GAIN</h3>
      {loop.gainElements.length ? <ul>{loop.gainElements.map((element) => <li key={`${element.nodeId}.${element.parameter}`}><code>{element.nodeId}</code><span>{element.parameter} {element.value.toLocaleString()}</span></li>)}</ul> : <p>None in this loop.</p>}
      <h3>FILTERS</h3>
      {loop.filters.length ? <ul>{loop.filters.map((element) => <li key={`${element.nodeId}.${element.parameter}`}><code>{element.nodeId}</code><span>{element.value.toLocaleString()} Hz</span></li>)}</ul> : <p>None in this loop.</p>}
      {inspection.truncated ? <p className="loop-truncated">Additional loops omitted at the bounded inspection limit.</p> : null}
    </section>
  );
}

function MeasurementBar({ sampleRate, onCapture }: { sampleRate: number; onCapture: (capture: ImpulseCaptureResult) => void }) {
  const [length, setLength] = useState(2000);
  const [threshold, setThreshold] = useState(-80);
  const [status, setStatus] = useState<ImpulseCaptureStatus | null>(null);
  const [result, setResult] = useState<ImpulseCaptureResult | null>(null);
  const [error, setError] = useState('');

  const start = useCallback(async () => {
    setError(''); setResult(null);
    try {
      const response = await callNative('startImpulseCapture', length, threshold);
      if (response === undefined) { setError('NATIVE AUDIO REQUIRED FOR CAPTURE'); return; }
      setStatus(parseImpulseCaptureStatus(response));
    } catch (reason) { setError(reason instanceof Error ? reason.message : 'Capture could not start'); }
  }, [length, threshold]);

  useEffect(() => {
    if (status?.state !== 'armed' && status?.state !== 'capturing') return;
    const timer = window.setInterval(() => {
      void callNative('getImpulseCaptureStatus').then((response) => {
        const next = parseImpulseCaptureStatus(response);
        setStatus(next);
        if (next.state === 'complete') {
          window.clearInterval(timer);
          return callNative('getImpulseCapture').then((capture) => {
            const parsed = parseImpulseCaptureResult(capture); setResult(parsed); onCapture(parsed);
          });
        }
      }).catch((reason: unknown) => setError(reason instanceof Error ? reason.message : 'Capture status failed'));
    }, 100);
    return () => window.clearInterval(timer);
  }, [onCapture, status?.state]);

  const busy = status?.state === 'armed' || status?.state === 'capturing';
  return <section className={`measurement-bar ${busy ? 'is-capturing' : ''}`} aria-label="Impulse response capture">
    <div className="measurement-title"><span>MEASURE</span><strong>IMPULSE RESPONSE</strong></div>
    <label>MAX LENGTH <select aria-label="Capture maximum length" value={length} disabled={busy} onChange={(event) => setLength(Number(event.target.value))}><option value={500}>500 ms</option><option value={2000}>2,000 ms</option><option value={5000}>5,000 ms</option><option value={10000}>10,000 ms</option></select></label>
    <label>STOP BELOW <select aria-label="Capture stop threshold" value={threshold} disabled={busy} onChange={(event) => setThreshold(Number(event.target.value))}><option value={-60}>-60 dBFS</option><option value={-80}>-80 dBFS</option><option value={-100}>-100 dBFS</option><option value={-120}>-120 dBFS</option></select></label>
    <span className="measurement-check">INPUT ISOLATED</span>
    <button type="button" disabled={busy || sampleRate <= 0} onClick={() => void start()}>{busy ? 'CAPTURING…' : 'CAPTURE IMPULSE'}</button>
    <div className="measurement-readout" role="status">
      {error || (sampleRate <= 0 ? 'WAITING FOR AUDIO DEVICE' : result ? `${result.frameCount.toLocaleString()} FRAMES / ${(result.frameCount / result.sampleRate * 1000).toFixed(1)} ms / STOP: ${result.stopReason === 'threshold' ? `${result.stopThresholdDb} dBFS` : 'MAX LENGTH'}` : busy && status ? `${status.capturedMilliseconds.toFixed(1)} / ${status.maximumLengthMilliseconds.toFixed(0)} ms` : '0.1 PEAK / READY')}
    </div>
  </section>;
}

function ResponseViewer({ capture, patchId, gateTeaching, teachingEnabled, captureRevision, activeRevision, onClose }: {
  capture: ImpulseCaptureResult;
  patchId: TeachingPatchId;
  gateTeaching: GateTeachingParameters;
  teachingEnabled: boolean;
  captureRevision: number;
  activeRevision: number;
  onClose: () => void;
}) {
  const analysis = useMemo(() => analyseResponse(capture), [capture]);
  const displayedRt60 = patchId === 'level-gated-room' ? null : analysis.rt60Seconds;
  const displayedRt60Refusal = patchId === 'level-gated-room' ? 'abrupt-cutoff' as const : analysis.rt60Refusal;
  const teachingOverlay = useMemo(
    () => teachingEnabled ? architectureOverlay(capture, patchId, gateTeaching) : null,
    [capture, gateTeaching, patchId, teachingEnabled],
  );
  const [zoom, setZoom] = useState(1);
  const [pan, setPan] = useState(0);
  const [densityVisible, setDensityVisible] = useState(false);
  const density = useMemo(() => densityVisible ? analyseDensity(capture) : null, [capture, densityVisible]);
  const window = frameWindow(capture.frameCount, zoom, pan);
  const left = waveformBuckets(capture.left, window, 450);
  const right = waveformBuckets(capture.right, window, 450);
  const decay = decayPoints(analysis.decayDb, window, 450);
  const x = (frame: number) => 70 + 900 * (frame - window.start) / Math.max(1, window.end - window.start - 1);
  const peak = Math.max(analysis.peakLeft, analysis.peakRight, 1e-9);
  const envelope = (buckets: ReturnType<typeof waveformBuckets>, center: number) => buckets.map((bucket) => `M${x(bucket.frame).toFixed(1)},${(center - 34 * bucket.maximum / peak).toFixed(1)}L${x(bucket.frame).toFixed(1)},${(center - 34 * bucket.minimum / peak).toFixed(1)}`).join('');
  const decayPath = decay.map((point, index) => `${index ? 'L' : 'M'}${x(point.frame).toFixed(1)},${(228 + Math.min(90, Math.max(0, -point.decibels))).toFixed(1)}`).join('');
  const visibleRegions = teachingOverlay?.regions
    .map((region) => ({ ...region, startFrame: Math.max(window.start, region.startFrame), endFrame: Math.min(window.end - 1, region.endFrame) }))
    .filter((region) => region.endFrame > region.startFrame) ?? [];
  const visibleMarkers = teachingOverlay?.markers.filter((marker) => marker.frame >= window.start && marker.frame < window.end) ?? [];
  const startMs = window.start / capture.sampleRate * 1000;
  const endMs = window.end / capture.sampleRate * 1000;
  const setBoundedZoom = (value: number) => setZoom(Math.max(1, Math.min(256, value)));
  const revisionState = captureRevisionState(captureRevision, activeRevision);
  return <section className="response-viewer context-response" aria-label="Stereo impulse response viewer">
    <header><div><span>CAPTURE #{capture.generation} / MEASURED / REVISION {captureRevision > 0 ? `#${captureRevision}` : 'UNKNOWN'} / {revisionState}</span><h2>Impulse and energy decay</h2></div><button type="button" onClick={onClose}>CLEAR ×</button></header>
    {revisionState === 'STALE' ? <p className="stale-evidence" role="status">STALE CAPTURE — the active graph is now revision #{activeRevision}. Values remain visible as historical evidence.</p> : null}
    <div className="response-metrics">
      <div><span>WINDOW / MEASURED</span><strong>{startMs.toFixed(2)}–{endMs.toFixed(2)} ms</strong></div>
      <div><span>PEAK L / R / MEASURED</span><strong>{analysis.peakLeft.toFixed(4)} / {analysis.peakRight.toFixed(4)}</strong></div>
      <div><span>ONSET / MEASURED</span><strong>{analysis.onsetFrame === null ? 'NONE' : `${(analysis.onsetFrame / capture.sampleRate * 1000).toFixed(2)} ms`}</strong></div>
      <div><span>RT60 / ESTIMATED</span><strong>{displayedRt60 === null ? patchId === 'level-gated-room' ? 'NOT MEANINGFUL' : 'NOT ESTIMATED' : `${displayedRt60.toFixed(3)} s`}</strong></div>
    </div>
    <div className="response-navigation">
      <button type="button" aria-pressed={densityVisible} onClick={() => setDensityVisible((visible) => !visible)}>{densityVisible ? 'HIDE DENSITY' : 'DENSITY INSPECTOR'}</button>
      <button type="button" onClick={() => { setBoundedZoom(16); setPan(0); }}>EARLY / 16×</button>
      <button type="button" disabled={zoom >= 256} onClick={() => setBoundedZoom(zoom * 2)}>ZOOM IN</button>
      <button type="button" disabled={zoom <= 1} onClick={() => setBoundedZoom(zoom / 2)}>ZOOM OUT</button>
      <button type="button" onClick={() => { setBoundedZoom(1); setPan(0); }}>FULL TAIL</button>
      <label>PAN <input aria-label="Pan response window" type="range" min="0" max="1" step="0.001" value={pan} disabled={zoom === 1} onChange={(event) => setPan(Number(event.target.value))} /></label>
      <output>{zoom.toFixed(0)}×</output>
    </div>
    <div className="response-legend" aria-label="Channel line styles"><span><i className="legend-left" />L / SOLID / UPPER</span><span><i className="legend-right" />R / DASHED / LOWER</span><span><i className="legend-decay" />ENERGY DECAY / SCHROEDER</span></div>
    <svg className="response-chart" viewBox="0 0 1000 330" role="img" aria-label={`Left solid upper waveform, right dashed lower waveform, and combined energy decay from ${startMs.toFixed(2)} to ${endMs.toFixed(2)} milliseconds${teachingOverlay ? `, with ${teachingOverlay.regions.map((region) => region.label).join(', ')} regions` : ''}`} onWheel={(event) => { event.preventDefault(); setBoundedZoom(event.deltaY < 0 ? zoom * 2 : zoom / 2); }}>
      <g className="chart-grid"><path d="M70 75H970M70 160H970M70 228H970M70 263H970M70 298H970M70 318H970" /><text x="12" y="79">L</text><text x="12" y="164">R</text><text x="12" y="232">0 dB</text><text x="12" y="267">-35</text><text x="12" y="322">-90</text></g>
      {teachingOverlay ? <g className="architecture-overlay" aria-label={teachingOverlay.title}>
        {visibleRegions.map((region) => <g className={`overlay-${region.tone}`} key={`${region.label}-${region.startFrame}`}>
          <rect x={x(region.startFrame)} y="203" width={Math.max(2, x(region.endFrame) - x(region.startFrame))} height="115" />
          <text x={x(region.startFrame) + 5} y="219">{region.label}</text>
        </g>)}
        {visibleMarkers.map((marker) => <g className={`overlay-marker overlay-${marker.tone}`} key={`${marker.label}-${marker.frame}`}>
          <path d={`M${x(marker.frame).toFixed(1)} 42V318`} />
          <text x={Math.min(905, x(marker.frame) + 5)} y="52">{marker.label}</text>
        </g>)}
      </g> : null}
      <path className="wave-left" d={envelope(left, 75)} />
      <path className="wave-right" d={envelope(right, 160)} />
      <path className="decay-line" d={decayPath} />
      <path className="fit-band" d="M70 233H970M70 263H970" />
      <text className="axis-label" x="70" y="328">{startMs.toFixed(2)} ms</text><text className="axis-label axis-end" x="970" y="328">{endMs.toFixed(2)} ms</text>
    </svg>
    {density ? <section className="density-inspector" aria-label="Perceptual density inspector">
      <header><div><span>DENSITY / 40 ms WINDOWS</span><strong>Temporal buildup and recurrence</strong></div><p>Solid density · dashed recurrence · dotted spectral flatness. Shapes and labels duplicate color.</p></header>
      <svg className="density-chart" viewBox="0 0 1000 220" role="img" aria-label="Echo density solid line, recurrence dashed line, spectral flatness dotted line, and prominent recurrence markers over time">
        <g className="density-grid"><path d="M60 20V190H980M60 20H980M60 105H980M60 190H980" /><text x="12" y="24">1.0</text><text x="12" y="109">0.5</text><text x="12" y="194">0.0</text></g>
        <path className="density-line" d={density.points.map((point, index) => `${index ? 'L' : 'M'}${(60 + 920 * point.startFrame / Math.max(1, capture.frameCount)).toFixed(1)},${(190 - 170 * point.echoDensity).toFixed(1)}`).join('')} />
        <path className="recurrence-line" d={density.points.map((point, index) => `${index ? 'L' : 'M'}${(60 + 920 * point.startFrame / Math.max(1, capture.frameCount)).toFixed(1)},${(190 - 170 * point.recurrence).toFixed(1)}`).join('')} />
        <path className="flatness-line" d={density.points.map((point, index) => `${index ? 'L' : 'M'}${(60 + 920 * point.startFrame / Math.max(1, capture.frameCount)).toFixed(1)},${(190 - 170 * point.spectralFlatness).toFixed(1)}`).join('')} />
        {density.points.filter((point) => point.recurrence >= .55).sort((a, b) => b.recurrence - a.recurrence).slice(0, 3).map((point) => {
          const markerX = 60 + 920 * point.startFrame / Math.max(1, capture.frameCount);
          return <g className="recurrence-marker" key={point.startFrame}><path d={`M${markerX.toFixed(1)} 18V190`} /><text x={Math.min(900, markerX + 5)} y="34">REPEAT {point.recurrenceMilliseconds.toFixed(1)} ms</text></g>;
        })}
        <text className="axis-label" x="60" y="211">0 ms</text><text className="axis-label axis-end" x="980" y="211">{(capture.frameCount / capture.sampleRate * 1000).toFixed(0)} ms</text>
      </svg>
      <div className="density-regions">{density.summaries.map((region) => <section key={region.name}>
        <strong>{region.name}</strong><span>DENSITY {region.echoDensity.toFixed(2)}</span><span>PEAKS {region.activePeaksPerSecond.toFixed(0)}/s</span>
        <span>CREST {region.crestFactor.toFixed(2)}</span><span>REPEAT {region.recurrence.toFixed(2)} @ {region.recurrenceMilliseconds.toFixed(1)} ms</span>
        <span>FLAT {region.spectralFlatness.toFixed(2)}</span><span>STEREO {region.stereoCorrelation.toFixed(2)}</span>
      </section>)}</div>
      {teachingEnabled ? <p className="density-teaching"><strong>READ TOGETHER:</strong> Density near 1 with lower crest means a noise-like temporal field. Strong recurrence marks repeating or ringing structure. Flatness describes spectrum, not temporal density; stereo correlation describes width, not quality.</p> : null}
    </section> : null}
    {teachingOverlay ? <section className="architecture-explanation" aria-label="Architecture explanation">
      <strong>{teachingOverlay.title}</strong><span>{teachingOverlay.explanation}</span>
    </section> : null}
    {displayedRt60Refusal ? <div className="rt60-refusal" role="note"><strong>RT60 withheld</strong><span>{rt60Explanation(displayedRt60Refusal)}</span></div> : <div className="rt60-method"><strong>T30 estimate</strong><span>Linear fit from -5 to -35 dB, extrapolated to -60 dB.</span></div>}
  </section>;
}

function DiagnosticsPanel({ diagnostics, runawayLoop, canUndo, onUndo, onRecover }: { diagnostics: RuntimeDiagnostics | null; runawayLoop: FeedbackLoopInspection | null; canUndo: boolean; onUndo: () => void; onRecover: () => void }) {
  return <section className={`diagnostics-panel context-diagnostics ${diagnostics?.mute.safetyLatched ? 'has-safety-latch' : ''}`} aria-label="Runtime resource and safety diagnostics">
    <header><div><span>RUNTIME DIAGNOSTICS / CURRENT REVISION</span><h2>Resources and safety</h2></div></header>
    {!diagnostics ? <p className="diagnostics-waiting">Waiting for a coherent native snapshot…</p> : <>
      <div className={`mute-diagnostic ${diagnostics.mute.active ? 'is-muted' : ''}`} role={diagnostics.mute.safetyLatched ? 'alert' : 'status'}>
        <strong>{diagnostics.mute.safetyLatched ? 'SAFETY MUTE LATCHED' : diagnostics.mute.manual ? 'MANUAL MUTE ACTIVE' : 'AUDIO SAFETY READY'}</strong>
        <span>{diagnostics.mute.safetyLatched ? 'Editing and Undo remain available. Reduce gain or undo the risky edit, then recover explicitly.' : 'No numerical safety latch is active.'}</span>
      </div>
      <div className="diagnostic-grid">
        <section><span>WORK / PLAN ESTIMATE</span><strong>{diagnostics.workloadEstimate.scalarOperationsPerSample} ops/sample</strong><small>{diagnostics.workloadEstimate.executionDomain} · {(diagnostics.workloadEstimate.scalarOperationsPerSecond / 1_000_000).toFixed(2)} M scalar ops/s</small></section>
        <section><span>LIVE / MEASURED</span><strong>{diagnostics.liveCpu.loadPercent.toFixed(2)}% CPU</strong><small>{diagnostics.liveCpu.peakLoadPercent.toFixed(2)}% peak · {diagnostics.liveCpu.processedBlocks.toLocaleString()} blocks</small></section>
        <section><span>MEMORY / PREPARED</span><strong>{formatBytes(diagnostics.preparedGraph.preparedStorageBytes)}</strong><small>{diagnostics.preparedGraph.physicalAudioBufferCount}/{diagnostics.preparedGraph.logicalSignalCount} buffers · {formatBytes(diagnostics.preparedGraph.bufferBytesSaved)} saved · {diagnostics.preparedGraph.copiesAvoided} copies avoided</small></section>
        <section><span>LATENCY / COMPILED</span><strong>{diagnostics.latency.samples.toLocaleString()} samples</strong><small>{diagnostics.latency.milliseconds.toFixed(2)} ms · host {diagnostics.latency.hostReportedSamples.toLocaleString()}</small></section>
        <section><span>CLIPPING / MEASURED</span><strong>{diagnostics.clipping.samples.toLocaleString()} samples</strong><small>{diagnostics.clipping.blocks.toLocaleString()} affected blocks</small></section>
        <section><span>GRAPH / PREPARED</span><strong>{diagnostics.preparedGraph.nodeCount} nodes</strong><small>{diagnostics.preparedGraph.connectionCount} cables · {diagnostics.preparedGraph.fusedKernelCount} fused / {diagnostics.preparedGraph.simdKernelCount} SIMD kernels</small></section>
      </div>
      <dl className="revision-diagnostic">
        <div><dt>ACTIVE GRAPH REVISION</dt><dd>#{diagnostics.topologyPublication.activeRevision || diagnostics.activeGraphRevision}</dd></div>
        <div><dt>PENDING / FAILED</dt><dd>{diagnostics.topologyPublication.pendingRevision ? `#${diagnostics.topologyPublication.pendingRevision}` : '—'} / {diagnostics.topologyPublication.failedRevision ? `#${diagnostics.topologyPublication.failedRevision}` : '—'}</dd></div>
        <div><dt>TOPOLOGY CROSSFADE</dt><dd>{diagnostics.topologyPublication.crossfadeTotalSamples ? `${diagnostics.topologyPublication.crossfadePositionSamples}/${diagnostics.topologyPublication.crossfadeTotalSamples} samples` : 'IDLE'}</dd></div>
        <div><dt>LAST CROSSFADE</dt><dd>{diagnostics.topologyPublication.completedCrossfades ? `#${diagnostics.topologyPublication.lastCrossfadeFromRevision} → #${diagnostics.topologyPublication.lastCrossfadeToRevision}` : '—'}</dd></div>
        <div><dt>SUPERSEDED / RECLAIMED</dt><dd>{diagnostics.topologyPublication.supersededRequests} requests · {diagnostics.topologyPublication.supersededCompilations} compiled / {diagnostics.topologyPublication.reclaimedRuntimes}</dd></div>
        <div><dt>COMPILE / VALIDATE</dt><dd>{diagnostics.preparedGraph.compileTiming.totalMicroseconds.toLocaleString()} / {diagnostics.preparedGraph.compileTiming.validationMicroseconds.toLocaleString()} µs</dd></div>
        <div><dt>SCHEDULE / PREPARE</dt><dd>{diagnostics.preparedGraph.compileTiming.schedulingMicroseconds.toLocaleString()} / {diagnostics.preparedGraph.compileTiming.preparationMicroseconds.toLocaleString()} µs</dd></div>
        <div><dt>REQUEST → ACTIVE</dt><dd>{diagnostics.preparedGraph.compileTiming.requestToActiveMicroseconds.toLocaleString()} µs</dd></div>
        <div><dt>LAST SUPERSEDED WORK</dt><dd>{diagnostics.topologyPublication.lastSupersededCompileMicroseconds ? `${diagnostics.topologyPublication.lastSupersededCompileMicroseconds.toLocaleString()} µs` : '—'}</dd></div>
        <div><dt>SUCCESSFUL RECOVERIES</dt><dd>{diagnostics.recoveryCount}</dd></div>
        {diagnostics.latency.outputPaths.map((path) => <div key={path.outputPort}><dt>OUTPUT PATH / {path.outputPort.toUpperCase()}</dt><dd>{path.samples.toLocaleString()} samples · {path.nodeIds.join(' → ') || 'direct'}</dd></div>)}
        {diagnostics.latency.parallelJoins.filter((join) => join.uncompensatedSamples > 0).map((join) => <div key={join.nodeId}><dt>UNCOMPENSATED / {join.nodeId}</dt><dd>{join.uncompensatedSamples.toLocaleString()} samples ({join.minimumInputSamples.toLocaleString()} → {join.maximumInputSamples.toLocaleString()})</dd></div>)}
      </dl>
      <section className="workload-profile"><span>OFFLINE PLAN PROFILE / ESTIMATED</span>{[...diagnostics.workloadEstimate.families].sort((a, b) => b.estimatedScalarOperationsPerSample - a.estimatedScalarOperationsPerSample).slice(0, 6).map((family) => <div key={family.family}><strong>{family.family}</strong><b>{family.nodeCount} × / {family.estimatedScalarOperationsPerSample} ops/sample</b></div>)}</section>
      <section className="workload-profile"><span>BUFFER LIVENESS / COMPILED</span><div><strong>peak live</strong><b>{diagnostics.preparedGraph.peakLiveBufferCount} physical buffers</b></div>{diagnostics.preparedGraph.bufferRetentionReasons.filter((reason) => reason.signalCount > 0).map((reason) => <div key={reason.reason}><strong>{reason.reason}</strong><b>{reason.signalCount} retained</b></div>)}</section>
      <section className="workload-profile"><span>BLOCK KERNELS / COMPILED</span><div><strong>fused groups</strong><b>{diagnostics.preparedGraph.fusedKernelCount} kernels / {diagnostics.preparedGraph.fusedNodeCount} folded blocks</b></div><div><strong>SIMD eligible</strong><b>{diagnostics.preparedGraph.simdKernelCount} prepared kernels</b></div>{diagnostics.preparedGraph.fusionPreventionReasons.filter((reason) => reason.signalCount > 0).map((reason) => <div key={reason.reason}><strong>{reason.reason}</strong><b>{reason.signalCount} boundaries</b></div>)}</section>
      <p className="diagnostic-basis-note">Plan operations are a static complexity estimate. Live CPU is aggregate callback timing. Compiled latency is audible sample delay. Compile and request-to-active values measure off-thread preparation and publication—not audio work.</p>
      <p className="latency-policy">{diagnostics.latency.compensationPolicy}</p>
      {diagnostics.topologyPublication.failure ? <p className="topology-failure">REVISION #{diagnostics.topologyPublication.failedRevision}: {diagnostics.topologyPublication.failure}</p> : null}
      {diagnostics.lastSafetyEvent ? <section className="safety-event"><span>LAST SAFETY EVENT #{diagnostics.lastSafetyEvent.generation}</span><strong>{diagnostics.lastSafetyEvent.kind.toUpperCase()} / {diagnostics.lastSafetyEvent.channel.toUpperCase()}</strong><p>Sample {diagnostics.lastSafetyEvent.sampleIndex.toLocaleString()} of graph revision <b>#{diagnostics.lastSafetyEvent.graphRevision}</b>. This identity remains fixed when later edits change the active revision.</p></section> : <p className="no-safety-event">No NaN, infinity, or runaway event recorded.</p>}
      {diagnostics.mute.safetyLatched ? runawayLoop?.loops[0] ? <section className="runaway-loop-event"><span>LIKELY FEEDBACK LOOP / HEURISTIC</span><strong>{runawayLoop.loops[0].nominalDelayMilliseconds.toFixed(2)} ms · {runawayLoop.loops[0].nodeIds.length} blocks</strong><code>{[...runawayLoop.loops[0].nodeIds, runawayLoop.loops[0].nodeIds[0]].join(' → ')}</code><p>Marked red on the graph. Ranking uses visible loop gain and nominal delay; it is guidance, not a stability proof.</p></section> : <p className="no-safety-event">No explicit delayed feedback loop could be identified for this event.</p> : null}
      <div className="diagnostic-actions"><button type="button" disabled={!canUndo} onClick={onUndo}>UNDO LAST EDIT</button><button type="button" disabled={!diagnostics.mute.safetyLatched} onClick={onRecover}>RECOVER AUDIO</button></div>
    </>}
  </section>;
}

function GravityPresentation({ value, destinationCount, showMeasuredReference, onBegin, onValue, onCommit, onFocus }: {
  value: number; destinationCount: number; onBegin: () => void; onValue: (value: number) => void;
  onCommit: () => void; onFocus: () => void; showMeasuredReference: boolean;
}) {
  const prediction = useMemo(() => predictGravityEnvelope(value), [value]);
  const measured = useMemo(() => showMeasuredReference ? gravityMeasuredReference(value) : null, [showMeasuredReference, value]);
  return <section className="gravity-presentation" aria-label="Gravity macro control">
    <header><span>GRAVITY</span><strong>{prediction.state}</strong></header>
    <div className="gravity-scale" aria-hidden="true"><span>INVERSE</span><span>BLOOM</span><span>FORWARD</span></div>
    <input className="gravity-slider" aria-label="Gravity bipolar control" type="range" min={-1} max={1} step={0.001} value={value}
      onPointerDown={onBegin} onChange={(event) => onValue(Number(event.target.value))} onPointerUp={onCommit}
      onKeyDown={(event) => { onBegin(); if (event.key === 'Enter') onCommit(); }} onBlur={onCommit} />
    <label className="gravity-exact-value"><span>EXACT VALUE</span><input aria-label="Gravity exact numeric value" type="number"
      min={-1} max={1} step={0.001} value={value} onFocus={onBegin} onChange={(event) => onValue(Number(event.target.value))}
      onKeyDown={(event) => { if (event.key === 'Enter') onCommit(); }} onBlur={onCommit} /></label>
    <figure className="gravity-envelope">
      <figcaption><span>DESIGN PREDICTION</span><strong>NOT MEASURED AUDIO</strong></figcaption>
      <svg viewBox="0 0 100 42" role="img" aria-label={prediction.description} preserveAspectRatio="none">
        <line x1={prediction.peakPosition * 100} x2={prediction.peakPosition * 100} y1="4" y2="40" />
        <path d={prediction.path} />
        {measured ? <path className="gravity-measured-envelope" d={measured.path} /> : null}
      </svg>
      <p>Shape guide from the Gravity coordinate. Capture an impulse to inspect actual response.</p>
      {measured ? <aside className="gravity-reference-comparison" aria-label={measured.description}>
        <strong>{measured.label}</strong>
        <span>PEAK {measured.peakMilliseconds.toFixed(1)} ms · EARLY/LATE {measured.earlyLateEnergyRatioDb.toFixed(1)} dB</span>
        <p>Checked fixture ({measured.controlsDescription}). Reference evidence—not the current capture. Any disagreement with the prediction is shown, not corrected.</p>
      </aside> : null}
    </figure>
    <button type="button" className="gravity-focus" onClick={onFocus}>EXPAND / FOCUS {destinationCount} MAPPINGS</button>
  </section>;
}

function Editor({ snapshot }: { snapshot: RuntimeSnapshot }) {
  const { fitView, setViewport: setFlowViewport, screenToFlowPosition } = useReactFlow();
  const restored = useMemo(() => snapshot.restoredPatch
    ? parsePatchJson(JSON.stringify(snapshot.restoredPatch), snapshot) : null, [snapshot]);
  const initial = useMemo(() => restored ?? { ...createFlowModel(snapshot), viewport: { x: 0, y: 0, zoom: 1 } }, [restored, snapshot]);
  const [nodes, setNodes, onNodesChange] = useNodesState<Node<PatchNodeData>>(initial.nodes);
  const [edges, setEdges, onEdgesChange] = useEdgesState(initial.edges);
  const [selectedNode, setSelectedNode] = useState<Node<PatchNodeData> | null>(null);
  const [selectedEdge, setSelectedEdge] = useState<Edge | null>(null);
  const [viewport, setViewport] = useState<Viewport>(initial.viewport);
  const activeEdit = useRef<{ label: string; before: ReturnType<typeof snapshotGraph> } | null>(null);
  const dragStart = useRef<ReturnType<typeof snapshotGraph> | null>(null);
  const clipboard = useRef<GraphClipboard | null>(null);
  const pasteCount = useRef(0);
  const loadInput = useRef<HTMLInputElement | null>(null);
  const occupiedConfirm = useRef<HTMLButtonElement | null>(null);
  const [fileStatus, setFileStatus] = useState<{ kind: 'ok' | 'error'; message: string } | null>(null);
  const [activePatchId, setActivePatchId] = useState<FactoryPatchId | 'custom'>(restored ? 'custom' : 'barr-reference');
  const [qualityPolicy, setQualityPolicy] = useState<QualityPolicy>(restored?.source.qualityPolicy ?? 'normal');
  const [comparisonPatchId, setComparisonPatchId] = useState<ComparisonPatchId>('causal-reverse-envelope');
  const [teachingEnabled, setTeachingEnabled] = useState(() => {
    try { return window.localStorage.getItem('reverb-playground-teaching') !== 'off'; } catch { return true; }
  });
  const [dismissedTeaching, setDismissedTeaching] = useState<string | null>(null);
  const [researchOpen, setResearchOpen] = useState(false);
  const [contextTab, setContextTab] = useState<ContextTab>('inspect');
  const [graphHistory, setGraphHistory] = useState(() => emptyGraphHistory(initial));
  const [graphStatus, setGraphStatus] = useState<{ kind: 'ok' | 'error'; message: string } | null>(null);
  const [pendingConnection, setPendingConnection] = useState<Connection | null>(null);
  const [groupNameDraft, setGroupNameDraft] = useState<string | null>(null);
  const [activeLoopIndex, setActiveLoopIndex] = useState(0);
  const [splitLoopFocus, setSplitLoopFocus] = useState<SplitShimmerLoopFocus>('shifted');
  const [reverseCosmicFocus, setReverseCosmicFocus] = useState<ReverseCosmicFocus>('grains');
  const [matrixCollapsed, setMatrixCollapsed] = useState(true);
  const [routeFocus, setRouteFocus] = useState<{ edgeId: string; direction: TraceDirection } | null>(null);
  const [tuningOpen, setTuningOpen] = useState(false);
  const [tuningBusy, setTuningBusy] = useState(false);
  const [tuningPreview, setTuningPreview] = useState<{ id: AssistedTuningSuggestionId; graph: GraphState } | null>(null);
  const [responseCapture, setResponseCapture] = useState<{
    capture: ImpulseCaptureResult;
    patchId: TeachingPatchId;
    gateTeaching: GateTeachingParameters;
    graphRevision: number;
  } | null>(null);
  const [reducedMotion, setReducedMotion] = useState(() => window.matchMedia('(prefers-reduced-motion: reduce)').matches);
  const [energyEnabled, setEnergyEnabled] = useState(() => !window.matchMedia('(prefers-reduced-motion: reduce)').matches);
  const [energyLevels, setEnergyLevels] = useState<EnergyLevels>({});
  const [openApplicationMenu, setOpenApplicationMenu] = useState<'file' | 'edit' | 'view' | 'help' | null>(null);
  const [standaloneAvailable, setStandaloneAvailable] = useState(false);
  const [windowWidth, setWindowWidth] = useState(() => window.innerWidth);
  const [workspacePresentation, setWorkspacePresentation] = useState(() => {
    try { return parseWorkspacePresentation(window.localStorage.getItem(workspacePresentationStorageKey)); }
    catch { return parseWorkspacePresentation(null); }
  });
  const [diagnostics, setDiagnostics] = useState<RuntimeDiagnostics | null>(null);
  const activeGraphRevision = useRef(0);
  const [controlPreviewTime, setControlPreviewTime] = useState(() => performance.now() / 1000);
  const audibleFingerprint = useMemo(() => audibleGraphFingerprint(nodes, edges), [edges, nodes]);
  const hostStateJson = useMemo(() => writePatchJson(nodes, edges, viewport, qualityPolicy), [edges, nodes, qualityPolicy, viewport]);
  const activePatch = activePatchId === 'custom' ? null : factoryPatchDescription(activePatchId);
  const baseWorkspaceLayout = useMemo(() => resolveWorkspaceLayout(windowWidth, workspacePresentation), [windowWidth, workspacePresentation]);
  const workspaceLayout = useMemo(() => emphasizeAnalyzeLayout(baseWorkspaceLayout,
    contextTab === 'analyze' && workspacePresentation.contextOpen), [baseWorkspaceLayout, contextTab, workspacePresentation.contextOpen]);
  const diagnosticsPollingEnabled = shouldPollRuntimeDiagnostics(contextTab,
    workspacePresentation.contextOpen, shouldRunEnergyTelemetry(energyEnabled, reducedMotion));

  useEffect(() => {
    void callNative('standaloneAuditionAvailable').then((available) => setStandaloneAvailable(available === true));
  }, []);

  useEffect(() => {
    const resize = () => setWindowWidth(window.innerWidth);
    window.addEventListener('resize', resize);
    return () => window.removeEventListener('resize', resize);
  }, []);

  useEffect(() => {
    try { window.localStorage.setItem(workspacePresentationStorageKey, JSON.stringify(workspacePresentation)); }
    catch { /* presentation remains session-local */ }
    if (standaloneAvailable) void callNative('setAuditionDrawerExpanded', workspacePresentation.auditionOpen);
  }, [standaloneAvailable, workspacePresentation]);

  useEffect(() => {
    if (!standaloneAvailable) return;
    const timer = window.setInterval(() => {
      void callNative('auditionDrawerExpanded').then((expanded) => {
        if (typeof expanded !== 'boolean') return;
        setWorkspacePresentation((current) => current.auditionOpen === expanded ? current : { ...current, auditionOpen: expanded });
      });
    }, 750);
    return () => window.clearInterval(timer);
  }, [standaloneAvailable]);

  const selectWorkspaceArrangement = useCallback((arrangement: WorkspaceArrangement) => {
    setWorkspacePresentation(arrangementPresentation(arrangement));
  }, []);
  const toggleDock = useCallback((dock: 'modules' | 'context') => {
    setWorkspacePresentation((current) => toggleWorkspaceDock(current, dock));
  }, []);
  const openContext = useCallback((intent: ContextIntent) => {
    const tab = contextTabFor(intent);
    setContextTab(tab);
    if (tab === 'analyze') setDiagnostics(null);
    setWorkspacePresentation((current) => current.contextOpen && current.narrowDock === 'context'
      ? current : { ...current, arrangement: 'custom', contextOpen: true, narrowDock: 'context' });
  }, []);
  const handleContextTabKey = useCallback((event: ReactKeyboardEvent<HTMLButtonElement>, tab: ContextTab) => {
    const current = contextTabs.indexOf(tab);
    const next = event.key === 'ArrowRight' ? (current + 1) % contextTabs.length
      : event.key === 'ArrowLeft' ? (current + contextTabs.length - 1) % contextTabs.length
        : event.key === 'Home' ? 0 : event.key === 'End' ? contextTabs.length - 1 : -1;
    if (next < 0) return;
    event.preventDefault();
    const nextTab = contextTabs[next];
    if (nextTab === 'analyze') setDiagnostics(null);
    setContextTab(nextTab);
    window.setTimeout(() => document.getElementById(`context-tab-${nextTab}`)?.focus(), 0);
  }, []);
  const receiveCapture = useCallback((capture: ImpulseCaptureResult) => {
    const parameter = (type: string, id: string, fallback: number) => nodes
      .find((node) => node.data.type === type)?.data.parameters
      .find((candidate) => candidate.id === id)?.value ?? fallback;
    setResponseCapture({
      capture,
      patchId: activePatchId,
      graphRevision: activeGraphRevision.current,
      gateTeaching: {
        detectorReleaseMilliseconds: parameter('envelope-follower', 'release', 20),
        holdMilliseconds: parameter('hold-gate', 'hold', 120),
        releaseMilliseconds: parameter('hold-gate', 'release', 8),
      },
    });
    openContext('measurement');
  }, [activePatchId, nodes, openContext]);

  const loopInspection = useMemo(() => selectedNode
    ? selectedNode.data.type === 'graph-group'
      ? (selectedNode.data.groupMemberIds ?? []).map((nodeId) => inspectFeedbackLoops(nodes, edges, { nodeId })).find((inspection) => inspection.loops.length) ?? null
      : inspectFeedbackLoops(nodes, edges, { nodeId: selectedNode.id })
    : selectedEdge ? inspectFeedbackLoops(nodes, edges, { edgeId: selectedEdge.id }) : null,
  [edges, nodes, selectedEdge, selectedNode]);
  const runawayLoopInspection = useMemo(() => diagnostics?.mute.safetyLatched
    ? inspectMostRelevantFeedbackLoop(nodes, edges) : null, [diagnostics?.mute.safetyLatched, edges, nodes]);
  const normalizedLoopIndex = loopInspection?.loops.length ? activeLoopIndex % loopInspection.loops.length : 0;
  const loopDecoratedGraph = useMemo(() => decorateFeedbackLoops(nodes, edges, loopInspection, normalizedLoopIndex), [edges, loopInspection, nodes, normalizedLoopIndex]);
  const splitFocusedGraph = useMemo(() => activePatchId === 'split-feedback-shimmer' && teachingEnabled
    ? decorateSplitShimmerFocus(loopDecoratedGraph.nodes, loopDecoratedGraph.edges, splitLoopFocus)
    : loopDecoratedGraph,
  [activePatchId, loopDecoratedGraph, splitLoopFocus, teachingEnabled]);
  const reverseCosmicFocusedGraph = useMemo(() => activePatchId === 'reverse-cosmic-shimmer' && teachingEnabled
    ? decorateReverseCosmicFocus(splitFocusedGraph.nodes, splitFocusedGraph.edges, reverseCosmicFocus)
    : splitFocusedGraph,
  [activePatchId, reverseCosmicFocus, splitFocusedGraph, teachingEnabled]);
  const safetyDecoratedGraph = useMemo(() => decorateRunawayFeedbackLoop(reverseCosmicFocusedGraph.nodes, reverseCosmicFocusedGraph.edges, runawayLoopInspection), [reverseCosmicFocusedGraph, runawayLoopInspection]);
  const energyDecoratedGraph = useMemo(() => decorateEnergy(safetyDecoratedGraph.nodes, safetyDecoratedGraph.edges, energyLevels), [energyLevels, safetyDecoratedGraph]);
  const selectedMappingRange = useMemo(() => {
    if (selectedNode?.data.type !== 'control-map') return null;
    const value = (id: string, fallback: number) => selectedNode.data.parameters.find((parameter) => parameter.id === id)?.value ?? fallback;
    const familyValue = value('curve-family', 0);
    const family: ControlCurveFamily = familyValue >= 1.5 ? 'exponential' : familyValue >= 0.5 ? 'power' : 'linear';
    return curveMappingRange(
      family, value('curve-amount', 0), value('exponent', 1), value('scale', 1), value('offset', 0),
      value('polarity', 1) >= 0.5 ? 'bipolar' : 'unipolar', value('clamp-min', -1), value('clamp-max', 1),
    );
  }, [selectedNode]);
  const selectedMacroInspection = useMemo(() => selectedNode?.data.type === 'macro'
    ? inspectMacroReachability(selectedNode.id, nodes, edges) : null, [edges, nodes, selectedNode]);
  const macroDecoratedGraph = useMemo(() => decorateMacroReachability(
    energyDecoratedGraph.nodes, energyDecoratedGraph.edges, selectedMacroInspection,
  ), [energyDecoratedGraph, selectedMacroInspection]);
  const displayedGraph = useMemo(() => decorateControlPreview(
    macroDecoratedGraph.nodes, macroDecoratedGraph.edges, controlPreviewTime,
  ), [controlPreviewTime, macroDecoratedGraph]);
  const routingFocusedGraph = useMemo(() => routeFocus
    ? traceCable({ nodes: displayedGraph.nodes, edges: displayedGraph.edges }, routeFocus.edgeId, routeFocus.direction)
    : displayedGraph, [displayedGraph, routeFocus]);
  const matrixInspection = useMemo(() => inspectMatrixMixer(nodes), [nodes]);
  const matrixIsCollapsed = Boolean(matrixInspection?.canCollapse && matrixCollapsed);
  const matrixDisplayedGraph = useMemo(() => collapseMatrixMixer(
    routingFocusedGraph.nodes, routingFocusedGraph.edges, matrixInspection, matrixIsCollapsed,
  ), [routingFocusedGraph, matrixInspection, matrixIsCollapsed]);
  const graphGroups = useMemo(() => inspectGraphGroups(nodes), [nodes]);
  const hasCollapsedGraphGroup = graphGroups.some((group) => group.collapsed);
  const groupDisplayedGraph = useMemo(() => collapseGraphGroups(
    hasCollapsedGraphGroup ? routingFocusedGraph.nodes : matrixDisplayedGraph.nodes,
    hasCollapsedGraphGroup ? routingFocusedGraph.edges : matrixDisplayedGraph.edges,
  ), [routingFocusedGraph, hasCollapsedGraphGroup, matrixDisplayedGraph]);
  const selectedGroupDisplayedGraph = useMemo(() => selectedNode?.data.type === 'graph-group' ? {
    nodes: groupDisplayedGraph.nodes.map((node) => ({ ...node, selected: node.id === selectedNode.id })),
    edges: groupDisplayedGraph.edges,
  } : groupDisplayedGraph, [groupDisplayedGraph, selectedNode]);
  const pendingConnectionDecision = pendingConnection ? decideConnection(nodes, edges, pendingConnection) : null;
  const pendingSignal = pendingConnectionDecision?.kind === 'occupied' ? pendingConnectionDecision.signal : null;
  const sumPreviewGraph = useMemo(() => {
    if (!pendingConnection || pendingSignal !== 'audio') return null;
    try { return previewSumForOccupiedInput({ nodes, edges }, pendingConnection); }
    catch { return null; }
  }, [edges, nodes, pendingConnection, pendingSignal]);
  const connectionDisplayedGraph = sumPreviewGraph ?? selectedGroupDisplayedGraph;
  const focusSelectedMacro = useCallback(() => {
    if (!selectedMacroInspection) return;
    const ids = new Set(gravityFocusNodeIds(selectedMacroInspection));
    void fitView({ nodes: nodes.filter((node) => ids.has(node.id)), padding: 0.22, duration: reducedMotion ? 0 : 350, maxZoom: 1.1 });
  }, [fitView, nodes, reducedMotion, selectedMacroInspection]);

  useEffect(() => {
    if (reducedMotion || !nodes.some((node) => node.data.role === 'control')) return;
    const timer = window.setInterval(() => setControlPreviewTime(performance.now() / 1000), 33);
    return () => window.clearInterval(timer);
  }, [nodes, reducedMotion]);

  useEffect(() => {
    const preference = window.matchMedia('(prefers-reduced-motion: reduce)');
    const update = () => { setReducedMotion(preference.matches); if (preference.matches) setEnergyEnabled(false); };
    preference.addEventListener('change', update);
    return () => preference.removeEventListener('change', update);
  }, []);

  useEffect(() => {
    if (!diagnosticsPollingEnabled) return;
    let cancelled = false;
    const timer = window.setTimeout(async () => {
      try {
        const result = parseGraphPublicationResult(await callNative('publishGraph', writePatchJson(nodes, edges, viewport, qualityPolicy)));
        if (!cancelled) setGraphStatus(result.accepted
          ? { kind: 'ok', message: `GRAPH REVISION #${result.revision} QUEUED FOR AUDITION` }
          : { kind: 'error', message: result.error });
      } catch (reason) {
        if (!cancelled && !import.meta.env.DEV) setGraphStatus({ kind: 'error', message: reason instanceof Error ? reason.message : 'Graph publication failed' });
      }
    }, 35);
    return () => { cancelled = true; window.clearTimeout(timer); };
  }, [audibleFingerprint, qualityPolicy]);

  useEffect(() => {
    let cancelled = false;
    const timer = window.setTimeout(async () => {
      try {
        const payload = await callNative('storePatchState', hostStateJson);
        if (payload !== undefined) {
          const result = parseHostPatchStateResult(payload);
          if (!cancelled && !result.accepted)
            setGraphStatus({ kind: 'error', message: result.error || 'Host state rejected' });
        }
      } catch (reason) {
        if (!cancelled && !import.meta.env.DEV)
          setGraphStatus({ kind: 'error', message: reason instanceof Error ? reason.message : 'Host state failed' });
      }
    }, 120);
    return () => { cancelled = true; window.clearTimeout(timer); };
  }, [hostStateJson]);

  useEffect(() => {
    if (!shouldRunEnergyTelemetry(energyEnabled, reducedMotion)) {
      setEnergyLevels({});
      void callNative('setEnergyTelemetryEnabled', false).catch(() => undefined);
      return;
    }
    let cancelled = false;
    let polling = false;
    let lastUpdate = performance.now();
    let lastGeneration = -1;
    let lastGenerationAt = lastUpdate;
    void callNative('setEnergyTelemetryEnabled', true).catch(() => undefined);
    const poll = async () => {
      if (polling) return;
      polling = true;
      try {
        let frame = parseEnergyTelemetry(await callNative('getEnergyTelemetry'));
        const now = performance.now();
        if (frame.revision !== activeGraphRevision.current) {
          if (!cancelled) setEnergyLevels({});
          return;
        }
        if (frame.generation !== lastGeneration) {
          lastGeneration = frame.generation;
          lastGenerationAt = now;
        } else if (now - lastGenerationAt > 100) {
          frame = { ...frame, nodes: frame.nodes.map((node) => ({ ...node, rms: 0 })) };
        }
        if (!cancelled) setEnergyLevels((previous) => smoothEnergy(previous, frame, now - lastUpdate));
        lastUpdate = now;
      } catch { /* development browser and dropped native frames retain the last coherent view */ }
      finally { polling = false; }
    };
    void poll();
    const timer = window.setInterval(() => void poll(), 33);
    return () => {
      cancelled = true;
      window.clearInterval(timer);
      void callNative('setEnergyTelemetryEnabled', false).catch(() => undefined);
    };
  }, [energyEnabled, reducedMotion]);

  useEffect(() => {
    let cancelled = false;
    let polling = false;
    const poll = async () => {
      if (polling) return;
      polling = true;
      try {
        const next = parseRuntimeDiagnostics(await callNative('getRuntimeDiagnostics'));
        if (!cancelled) {
          setDiagnostics(next);
          activeGraphRevision.current = next.topologyPublication.activeRevision;
          if (next.topologyPublication.failedRevision > 0
            && next.topologyPublication.failedRevision === next.topologyPublication.requestedRevision
            && next.topologyPublication.failure)
            setGraphStatus({ kind: 'error', message: `REVISION #${next.topologyPublication.failedRevision} REJECTED: ${next.topologyPublication.failure}` });
          if (next.mute.safetyLatched && (contextTab !== 'analyze' || !workspacePresentation.contextOpen)) openContext('diagnostics');
        }
      } catch { /* native snapshot may not be available in the development browser */ }
      finally { polling = false; }
    };
    void poll();
    const timer = window.setInterval(() => void poll(), 250);
    return () => { cancelled = true; window.clearInterval(timer); };
  }, [contextTab, diagnosticsPollingEnabled, openContext, workspacePresentation.contextOpen]);

  const applyGraph = useCallback((state: { nodes: Node<PatchNodeData>[]; edges: Edge[] }) => {
    setNodes(state.nodes); setEdges(state.edges); setSelectedNode(null); setSelectedEdge(null);
  }, [setEdges, setNodes]);

  const previewAssistedTuning = useCallback(async (id: AssistedTuningSuggestionId) => {
    const preview = createAssistedTuningPreview({ nodes, edges }, id);
    setTuningBusy(true);
    setTuningPreview({ id, graph: preview });
    try {
      const payload = await callNative('publishGraph', writePatchJson(preview.nodes, preview.edges, viewport, qualityPolicy));
      if (payload !== undefined) {
        const result = parseGraphPublicationResult(payload);
        if (!result.accepted) {
          setTuningPreview(null);
          setGraphStatus({ kind: 'error', message: `TUNING PREVIEW REJECTED: ${result.error}` });
          return;
        }
      }
      setGraphStatus({ kind: 'ok', message: `AUDITION PREVIEW / ${id.replace('-', ' ').toUpperCase()} / SAVED GRAPH UNCHANGED` });
    } catch (reason) {
      if (!import.meta.env.DEV) {
        setTuningPreview(null);
        setGraphStatus({ kind: 'error', message: reason instanceof Error ? reason.message : 'Tuning preview failed' });
      }
    } finally { setTuningBusy(false); }
  }, [edges, nodes, qualityPolicy, viewport]);

  const cancelAssistedTuning = useCallback(async () => {
    setTuningBusy(true);
    try { await callNative('publishGraph', writePatchJson(nodes, edges, viewport, qualityPolicy)); }
    catch { /* development browser has no native runtime */ }
    finally {
      setTuningPreview(null); setTuningBusy(false);
      setGraphStatus({ kind: 'ok', message: 'TUNING PREVIEW CANCELLED / EXACT GRAPH RESTORED' });
    }
  }, [edges, nodes, qualityPolicy, viewport]);

  const acceptAssistedTuning = useCallback(() => {
    if (!tuningPreview) return;
    const before = { nodes, edges };
    const after = tuningPreview.graph;
    applyGraph(after);
    setGraphHistory((history) => commitGraphEdit(history,
      `Accept ${tuningPreview.id.replace('-', ' ')} tuning`, before, after));
    setActivePatchId('custom');
    setTuningPreview(null);
    setGraphStatus({ kind: 'ok', message: `ACCEPTED ${tuningPreview.id.replace('-', ' ').toUpperCase()} / UNDO AVAILABLE` });
  }, [applyGraph, edges, nodes, tuningPreview]);

  useEffect(() => {
    if (tuningPreview) setTuningPreview(null);
    // A semantic edit or patch switch supersedes any audition-only runtime.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [audibleFingerprint]);

  const publishRuntimeParameters = useCallback((state: { nodes: Node<PatchNodeData>[] }) => {
    for (const node of state.nodes) for (const parameter of node.data.parameters)
      if (node.data.runtimeBound || (node.data.type === 'macro' && parameter.id === 'value')) {
        try { void callNative('setRuntimeParameter', node.id, parameter.id, parameter.value).catch(() => undefined); }
        catch { /* browser prototype has no native bridge */ }
      }
  }, []);

  const addModule = useCallback((type: ModuleType) => {
    if ((type === 'stereo-input' || type === 'stereo-output') && nodes.some((node) => node.data.type === type)) {
      setGraphStatus({ kind: 'error', message: `Exactly one ${type === 'stereo-input' ? 'Stereo Input' : 'Stereo Output'} is required and already exists.` }); return;
    }
    const before = { nodes, edges }; const draftCount = nodes.filter((item) => !item.data.runtimeBound).length;
    const center = screenToFlowPosition({ x: window.innerWidth * .5, y: window.innerHeight * .5 });
    const node = createModuleNode(type, nextNodeId(type, nodes), { x: center.x - 190 + (draftCount % 3) * 190, y: center.y - 100 + Math.floor(draftCount / 3) * 130 });
    const after = { nodes: [...nodes.map((item) => ({ ...item, selected: false })), { ...node, selected: true }], edges };
    applyGraph(after); setSelectedNode(node); setGraphHistory((history) => commitGraphEdit(history, `Create ${node.data.label}`, before, after)); setGraphStatus({ kind: 'ok', message: `CREATED ${node.id} / DRAFT GRAPH` });
  }, [applyGraph, edges, nodes, screenToFlowPosition]);

  const removeSelection = useCallback(() => {
    if (selectedNode?.data.type === 'graph-group') { setGraphStatus({ kind: 'error', message: 'VISUAL GROUPS CANNOT BE DELETED / EXPAND OR UNGROUP IT' }); return; }
    const protectedNode = nodes.find((node) => node.selected && (node.data.type === 'stereo-input' || node.data.type === 'stereo-output'));
    if (protectedNode) { setGraphStatus({ kind: 'error', message: `${protectedNode.data.label} is required and cannot be deleted.` }); return; }
    const before = { nodes, edges }; const after = deleteSelected(nodes, edges); if (after.nodes.length === nodes.length && after.edges.length === edges.length) return;
    applyGraph(after); setGraphHistory((history) => commitGraphEdit(history, 'Delete selection', before, after)); setGraphStatus({ kind: 'ok', message: 'DELETED SELECTION + INCIDENT CABLES / UNDO AVAILABLE' });
  }, [applyGraph, edges, nodes, selectedNode]);

  const undoGraph = useCallback(() => { const result = undoGraphEdit(graphHistory); if (!result.edit) return; applyGraph(result.edit.before); publishRuntimeParameters(result.edit.before); setGraphHistory(result.history); setGraphStatus({ kind: 'ok', message: `UNDID ${result.edit.label.toUpperCase()}` }); }, [applyGraph, graphHistory, publishRuntimeParameters]);
  const redoGraph = useCallback(() => { const result = redoGraphEdit(graphHistory); if (!result.edit) return; applyGraph(result.edit.after); publishRuntimeParameters(result.edit.after); setGraphHistory(result.history); setGraphStatus({ kind: 'ok', message: `REDID ${result.edit.label.toUpperCase()}` }); }, [applyGraph, graphHistory, publishRuntimeParameters]);

  const commitConnection = useCallback((connection: Connection) => {
    const before = { nodes, edges }; const decision = decideConnection(nodes, edges, connection);
    if (decision.kind === 'invalid') { setGraphStatus({ kind: 'error', message: decision.message }); return; }
    if (decision.kind === 'occupied') {
      setPendingConnection(connection); setGraphStatus({ kind: 'error', message: 'INPUT OCCUPIED / REPLACE ITS CABLE OR INSERT +' }); return;
    }
    const after = connectGraph(before, connection); applyGraph(after);
    setGraphHistory((history) => commitGraphEdit(history, 'Create cable', before, after)); setGraphStatus({ kind: 'ok', message: `CONNECTED ${decision.signal.toUpperCase()} CABLE` });
  }, [applyGraph, edges, nodes]);

  const resolveOccupied = useCallback((action: 'replace' | 'sum' | 'cancel') => {
    const connection = pendingConnection; setPendingConnection(null); if (!connection || action === 'cancel') { setGraphStatus(null); return; }
    const decision = decideConnection(nodes, edges, connection);
    if (action === 'sum' && (decision.kind !== 'occupied' || decision.signal !== 'audio')) { setGraphStatus({ kind: 'error', message: 'CONTROL INPUTS CANNOT INSERT AN AUDIO SUM' }); return; }
    const before = { nodes, edges }; const after = action === 'replace' ? connectGraph(before, connection, true) : insertSumForOccupiedInput(before, connection);
    applyGraph(after); setGraphHistory((history) => commitGraphEdit(history, action === 'replace' ? 'Replace cable' : 'Insert +', before, after));
    setGraphStatus({ kind: 'ok', message: action === 'replace' ? 'REPLACED OCCUPIED INPUT CABLE' : 'INSERTED + AND REWIRED BOTH SOURCES' });
  }, [applyGraph, edges, nodes, pendingConnection]);

  useEffect(() => {
    if (!pendingConnection) return;
    occupiedConfirm.current?.focus();
    const handleOccupiedKey = (event: KeyboardEvent) => {
      if (event.key === 'Escape') { event.preventDefault(); resolveOccupied('cancel'); }
    };
    window.addEventListener('keydown', handleOccupiedKey);
    return () => window.removeEventListener('keydown', handleOccupiedKey);
  }, [pendingConnection, pendingSignal, resolveOccupied]);

  const applyParameter = useCallback((nodeId: string, parameterId: string, value: number) => {
    const sourceNode = nodes.find((node) => node.id === nodeId);
    const detentEnabled = sourceNode?.data.type === 'macro'
      && sourceNode.data.parameters.find((parameter) => parameter.id === 'center-detent')?.value === 1;
    const appliedValue = sourceNode?.data.type === 'macro' && parameterId === 'value'
      && detentEnabled && Math.abs(value) <= 0.02 ? 0 : value;
    const update = (node: Node<PatchNodeData>) => node.id !== nodeId ? node : {
      ...node,
      data: {
        ...node.data,
        parameters: node.data.parameters.map((parameter) => parameter.id === parameterId ? { ...parameter, value: appliedValue } : parameter),
      },
    };
    setNodes((current) => current.map(update));
    setSelectedNode((current) => current ? update(current) : null);
    if (sourceNode?.data.runtimeBound || (sourceNode?.data.type === 'macro' && parameterId === 'value')) {
      try { void callNative('setRuntimeParameter', nodeId, parameterId, appliedValue).catch(() => undefined); }
      catch { /* browser prototype has no native bridge */ }
    }
  }, [nodes, setNodes]);

  const applyModulation = useCallback((nodeId: string, parameterId: string, change: Partial<NonNullable<PatchNodeData['parameters'][number]['modulation']>>) => {
    const update = (node: Node<PatchNodeData>) => node.id !== nodeId ? node : {
      ...node,
      data: {
        ...node.data,
        parameters: node.data.parameters.map((parameter) => parameter.id !== parameterId || !parameter.modulation
          ? parameter
          : { ...parameter, modulation: { ...parameter.modulation, ...change } }),
      },
    };
    setNodes((current) => current.map(update));
    setSelectedNode((current) => current ? update(current) : null);
  }, [setNodes]);

  const applyMacroName = useCallback((nodeId: string, name: string) => {
    const bounded = name.slice(0, 64);
    const update = (node: Node<PatchNodeData>) => node.id !== nodeId ? node : {
      ...node, data: { ...node.data, userName: bounded },
    };
    setNodes((current) => current.map(update));
    setSelectedNode((current) => current ? update(current) : null);
  }, [setNodes]);

  const resetPatch = useCallback(() => {
    const resetId = activePatchId === 'custom' ? 'barr-reference' : activePatchId;
    const fresh = loadFactoryPatch(resetId, snapshot);
    const before = { nodes, edges };
    applyGraph(fresh);
    publishRuntimeParameters(fresh);
    setGraphHistory((history) => commitGraphEdit(history, 'Reset patch', before, fresh));
    setPendingConnection(null);
    setActivePatchId(resetId);
    requestAnimationFrame(() => void fitView({ padding: 0.16, minZoom: 0.25, maxZoom: 1.1 }));
  }, [activePatchId, applyGraph, edges, fitView, nodes, publishRuntimeParameters, snapshot]);

  const selectFactoryPatch = useCallback(async (id: FactoryPatchId) => {
    try {
      const fresh = loadFactoryPatch(id, snapshot);
      applyGraph(fresh);
      publishRuntimeParameters(fresh);
      setViewport(fresh.viewport);
      await setFlowViewport(fresh.viewport);
      setGraphHistory(emptyGraphHistory(fresh));
      setPendingConnection(null);
      setActivePatchId(id);
      if (id === 'split-feedback-shimmer') setSplitLoopFocus('shifted');
      if (id === 'reverse-cosmic-shimmer') setReverseCosmicFocus('grains');
      setComparisonPatchId((current) => comparisonPatchAfterSelection(id, current));
      const description = factoryPatchDescription(id);
      setFileStatus({ kind: 'ok', message: `LOADED FACTORY / ${description.label.toUpperCase()}` });
      requestAnimationFrame(() => void fitView({ padding: 0.16, minZoom: 0.25, maxZoom: 1.1 }));
    } catch (reason) {
      setFileStatus({ kind: 'error', message: reason instanceof Error ? reason.message : 'Factory patch load failed' });
    }
  }, [applyGraph, fitView, publishRuntimeParameters, setFlowViewport, snapshot]);

  const copySelection = useCallback(() => {
    const collapsedMembers = selectedNode?.data.type === 'graph-group' ? new Set(selectedNode.data.groupMemberIds ?? []) : null;
    const sourceNodes = collapsedMembers ? nodes.map((node) => ({ ...node, selected: collapsedMembers.has(node.id) })) : nodes;
    const copied = copySelectedGraph({ nodes: sourceNodes, edges });
    if (!copied) { setGraphStatus({ kind: 'error', message: 'SELECT ONE OR MORE NON-I/O BLOCKS TO COPY' }); return; }
    clipboard.current = copied; pasteCount.current = 0;
    void navigator.clipboard?.writeText(JSON.stringify({ format: 'reverb-playground-subgraph-v1', ...copied })).catch(() => undefined);
    setGraphStatus({ kind: 'ok', message: `COPIED ${copied.nodes.length} BLOCK${copied.nodes.length === 1 ? '' : 'S'} + ${copied.edges.length} INTERNAL CABLE${copied.edges.length === 1 ? '' : 'S'}` });
  }, [edges, nodes, selectedNode]);

  const pasteSelection = useCallback(() => {
    if (!clipboard.current) { setGraphStatus({ kind: 'error', message: 'GRAPH CLIPBOARD IS EMPTY' }); return; }
    const before = { nodes, edges };
    const after = pasteGraph(before, clipboard.current, 40 * ++pasteCount.current);
    applyGraph(after); setGraphHistory((history) => commitGraphEdit(history, 'Paste selection', before, after));
    setGraphStatus({ kind: 'ok', message: `PASTED ${clipboard.current.nodes.length} BLOCK${clipboard.current.nodes.length === 1 ? '' : 'S'} / NEW IDS ASSIGNED` });
  }, [applyGraph, edges, nodes]);

  const beginCreateGroup = useCallback(() => {
    const selected = nodes.filter((node) => node.selected && node.data.role !== 'io');
    if (selected.length < 2) { setGraphStatus({ kind: 'error', message: 'SELECT AT LEAST TWO NON-I/O BLOCKS TO GROUP' }); return; }
    if (selected.some((node) => node.data.presentationGroup)) { setGraphStatus({ kind: 'error', message: 'NESTED GROUPS ARE NOT SUPPORTED / UNGROUP FIRST' }); return; }
    setGraphStatus(null);
    setGroupNameDraft(`Group ${graphGroups.length + 1}`);
  }, [graphGroups.length, nodes]);

  const commitCreateGroup = useCallback(() => {
    if (groupNameDraft === null) return;
    try {
      const before = { nodes, edges }; const after = createGraphGroup(before, groupNameDraft);
      applyGraph(after); setGraphHistory((history) => commitGraphEdit(history, 'Create group', before, after));
      setActivePatchId('custom'); setGroupNameDraft(null); setGraphStatus({ kind: 'ok', message: `CREATED ${groupNameDraft.trim().toUpperCase()} / LAYOUT ONLY` });
    } catch (reason) { setGraphStatus({ kind: 'error', message: reason instanceof Error ? reason.message.toUpperCase() : 'GROUP CREATION FAILED' }); }
  }, [applyGraph, edges, groupNameDraft, nodes]);

  const changeSelectedGroup = useCallback((action: 'collapse' | 'expand' | 'ungroup' | 'rename', name?: string) => {
    const group = selectedNode?.data.presentationGroup; if (!group) return;
    try {
      const before = { nodes, edges };
      const after = action === 'ungroup' ? removeGraphGroup(before, group.id)
        : action === 'rename' && name !== undefined ? renameGraphGroup(before, group.id, name)
          : setGraphGroupCollapsed(before, group.id, action === 'collapse');
      applyGraph(after); setGraphHistory((history) => commitGraphEdit(history,
        action === 'ungroup' ? 'Ungroup blocks' : action === 'rename' ? 'Rename group' : `${action} group`, before, after));
      setActivePatchId('custom'); setGraphStatus({ kind: 'ok', message: `${action.toUpperCase()} GROUP / LAYOUT ONLY / AUDIO GRAPH UNCHANGED` });
    } catch (reason) { setGraphStatus({ kind: 'error', message: reason instanceof Error ? reason.message.toUpperCase() : 'GROUP EDIT FAILED' }); }
  }, [applyGraph, edges, nodes, selectedNode]);

  const commitLayoutEdit = useCallback((label: string, transform: (state: GraphState) => GraphState) => {
    try {
      const before = { nodes, edges }; const after = transform(before); applyGraph(after);
      setGraphHistory((history) => commitGraphEdit(history, label, before, after)); setActivePatchId('custom');
      if (selectedEdge) setSelectedEdge(after.edges.find((edge) => edge.id === selectedEdge.id) ?? null);
      setGraphStatus({ kind: 'ok', message: `${label.toUpperCase()} / LAYOUT ONLY / AUDIO GRAPH UNCHANGED` });
    } catch (reason) { setGraphStatus({ kind: 'error', message: reason instanceof Error ? reason.message.toUpperCase() : 'LAYOUT EDIT FAILED' }); }
  }, [applyGraph, edges, nodes, selectedEdge]);

  const runAlignment = useCallback((command: AlignmentCommand) => {
    const label = command === 'left' ? 'Align left' : command === 'top' ? 'Align top' : `Distribute ${command}`;
    commitLayoutEdit(label, (state) => alignSelectedNodes(state, command));
  }, [commitLayoutEdit]);

  const arrangeSelectedGroup = useCallback(() => {
    const groupId = selectedNode?.data.presentationGroup?.id
      ?? nodes.find((node) => node.selected && node.data.presentationGroup)?.data.presentationGroup?.id;
    if (!groupId) { setGraphStatus({ kind: 'error', message: 'SELECT A GROUP OR ONE OF ITS MEMBERS FIRST' }); return; }
    commitLayoutEdit('Arrange group grid', (state) => arrangeGraphGroup(state, groupId));
  }, [commitLayoutEdit, nodes, selectedNode]);

  const focusCablePath = useCallback((direction: TraceDirection) => {
    if (!selectedEdge) return; const traced = traceCable({ nodes, edges }, selectedEdge.id, direction); setRouteFocus({ edgeId: selectedEdge.id, direction });
    void fitView({ nodes: nodes.filter((node) => traced.nodeIds.includes(node.id)), padding: 0.22, duration: reducedMotion ? 0 : 350, maxZoom: 1.1 });
  }, [edges, fitView, nodes, reducedMotion, selectedEdge]);

  const focusCompleteLoop = useCallback(() => {
    const loop = loopInspection?.loops[normalizedLoopIndex]; if (!loop) return; setRouteFocus(null);
    void fitView({ nodes: nodes.filter((node) => loop.nodeIds.includes(node.id)), padding: 0.22, duration: reducedMotion ? 0 : 350, maxZoom: 1.1 });
  }, [fitView, loopInspection, nodes, normalizedLoopIndex, reducedMotion]);

  const handleSelection = useCallback(({ nodes: selectedNodes, edges: selectedEdges }: OnSelectionChangeParams) => {
    const node = (selectedNodes[0] as Node<PatchNodeData> | undefined) ?? null;
    const edge = selectedEdges[0] ?? null;
    if (!node && !edge && selectedNode?.data.type === 'graph-group') return;
    setSelectedNode(node);
    setSelectedEdge(edge);
    setActiveLoopIndex(0);
    if (node) openContext(node.data.type === 'matrix-mixer' ? 'matrix' : 'node');
    else if (edge) openContext('cable');
  }, [openContext, selectedNode]);

  useEffect(() => {
    const handleKey = (event: KeyboardEvent) => {
      if (event.key === 'Escape' && openApplicationMenu) {
        event.preventDefault();
        setOpenApplicationMenu(null);
        return;
      }
      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'z') {
        event.preventDefault();
        if (event.shiftKey) redoGraph(); else undoGraph();
        return;
      }
      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'y') {
        event.preventDefault();
        redoGraph();
        return;
      }
      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'c' && !(event.target instanceof HTMLInputElement)) { event.preventDefault(); copySelection(); return; }
      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'v' && !(event.target instanceof HTMLInputElement)) { event.preventDefault(); pasteSelection(); return; }
      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'g' && !(event.target instanceof HTMLInputElement)) { event.preventDefault(); beginCreateGroup(); return; }
      if ((event.key === 'Delete' || event.key === 'Backspace') && !(event.target instanceof HTMLInputElement)) { event.preventDefault(); removeSelection(); return; }
      if (event.key.toLowerCase() === 'r' && !(event.target instanceof HTMLInputElement))
        resetPatch();
    };
    window.addEventListener('keydown', handleKey);
    return () => window.removeEventListener('keydown', handleKey);
  }, [beginCreateGroup, copySelection, openApplicationMenu, pasteSelection, redoGraph, removeSelection, resetPatch, undoGraph]);

  const beginParameterEdit = useCallback((nodeId: string, parameterId: string, _before: number) => {
    activeEdit.current = { label: `Edit ${nodeId}.${parameterId}`, before: snapshotGraph({ nodes, edges }) };
  }, [edges, nodes]);

  const changeParameter = useCallback((nodeId: string, parameterId: string, value: number) => {
    applyParameter(nodeId, parameterId, value);
  }, [applyParameter]);

  const commitParameterEdit = useCallback(() => {
    const edit = activeEdit.current;
    activeEdit.current = null;
    if (!edit) return;
    setGraphHistory((history) => commitGraphEdit(history, edit.label, edit.before, { nodes, edges }));
  }, [edges, nodes]);

  const savePatch = useCallback(() => {
    try {
      const json = writePatchJson(nodes, edges, viewport, qualityPolicy);
      parsePatchJson(json, snapshot);
      const url = URL.createObjectURL(new Blob([json], { type: 'application/json' }));
      const link = document.createElement('a');
      link.href = url;
      const filename = activePatch?.filename ?? 'custom-patch.rvp.json';
      link.download = filename;
      document.body.appendChild(link);
      link.click();
      link.remove();
      window.setTimeout(() => URL.revokeObjectURL(url), 0);
      setGraphHistory((history) => markHistoryClean(history, { nodes, edges }));
      setFileStatus({ kind: 'ok', message: `SAVED ${filename.toUpperCase()} / SCHEMA V2` });
    } catch (reason) {
      setFileStatus({ kind: 'error', message: reason instanceof Error ? reason.message : 'Patch save failed' });
    }
  }, [activePatch, edges, nodes, qualityPolicy, snapshot, viewport]);

  const loadPatch = useCallback(async (file: File) => {
    try {
      const loaded = parsePatchJson(await file.text(), snapshot);
      setNodes(loaded.nodes);
      setEdges(loaded.edges);
      setViewport(loaded.viewport);
      setQualityPolicy(loaded.source.qualityPolicy);
      await setFlowViewport(loaded.viewport);
      for (const node of loaded.nodes) {
        for (const parameter of node.data.parameters)
          if (node.data.runtimeBound || (node.data.type === 'macro' && parameter.id === 'value'))
            await callNative('setRuntimeParameter', node.id, parameter.id, parameter.value);
      }
      setSelectedNode(null);
      setSelectedEdge(null);
      setGraphHistory(emptyGraphHistory(loaded));
      setPendingConnection(null);
      setActivePatchId('custom');
      setFileStatus({ kind: 'ok', message: loaded.warnings.length
        ? `LOADED WITH MIGRATION / ${loaded.warnings.join(' ')}`
        : `LOADED ${file.name.toUpperCase()} / SCHEMA V2` });
    } catch (reason) {
      setFileStatus({ kind: 'error', message: reason instanceof Error ? reason.message : 'Patch load failed' });
    }
  }, [setEdges, setFlowViewport, setNodes, snapshot]);

  useEffect(() => {
    const handleFileShortcut = (event: KeyboardEvent) => {
      if (!(event.ctrlKey || event.metaKey)) return;
      if (event.key.toLowerCase() === 's') {
        event.preventDefault(); savePatch();
      } else if (event.key.toLowerCase() === 'o') {
        event.preventDefault(); loadInput.current?.click();
      }
    };
    window.addEventListener('keydown', handleFileShortcut);
    return () => window.removeEventListener('keydown', handleFileShortcut);
  }, [savePatch]);

  const teachingKey = selectedNode?.id ?? 'overview';
  const showTeaching = teachingEnabled && dismissedTeaching !== teachingKey;
  const toggleTeaching = useCallback(() => {
    setTeachingEnabled((current) => {
      const next = !current;
      try { window.localStorage.setItem('reverb-playground-teaching', next ? 'on' : 'off'); } catch { /* preference remains session-local */ }
      return next;
    });
  }, []);

  const measurementDrawerVisible = !standaloneAvailable || workspacePresentation.auditionOpen;
  return (
    <main className={`editor-shell${measurementDrawerVisible ? ' measurement-open' : ''}`}
      style={{ gridTemplateRows: `${editorCommandBarHeight}px minmax(0, 1fr) ${measurementDrawerVisible ? measurementBarHeight : 0}px` }}>
      <header className="editor-header" onMouseLeave={() => setOpenApplicationMenu(null)}>
        <nav className="application-menus" aria-label="Application menus">
          {(['file', 'edit', 'view', 'help'] as const).map((menu) => <div className="application-menu" key={menu}>
            <button type="button" aria-haspopup="menu" aria-expanded={openApplicationMenu === menu}
              onClick={() => setOpenApplicationMenu((open) => open === menu ? null : menu)}>{menu.toUpperCase()}</button>
            {openApplicationMenu === menu ? <div className="application-menu-popover" role="menu" aria-label={`${menu} menu`}>
              {menu === 'file' ? <>
                <button role="menuitem" type="button" onClick={() => { savePatch(); setOpenApplicationMenu(null); }}>SAVE PATCH <kbd>CTRL S</kbd></button>
                <button role="menuitem" type="button" onClick={() => { loadInput.current?.click(); setOpenApplicationMenu(null); }}>OPEN PATCH… <kbd>CTRL O</kbd></button>
                {standaloneAvailable ? <button role="menuitem" type="button" onClick={() => { void callNative('chooseAudioDevice'); setOpenApplicationMenu(null); }}>AUDIO DEVICE…</button> : null}
                <button role="menuitem" type="button" onClick={() => { resetPatch(); setOpenApplicationMenu(null); }}>RESET PATCH <kbd>R</kbd></button>
              </> : null}
              {menu === 'edit' ? <>
                <button role="menuitem" type="button" disabled={!graphHistory.undo.length} onClick={() => { undoGraph(); setOpenApplicationMenu(null); }}>UNDO <kbd>CTRL Z</kbd></button>
                <button role="menuitem" type="button" disabled={!graphHistory.redo.length} onClick={() => { redoGraph(); setOpenApplicationMenu(null); }}>REDO <kbd>CTRL Y</kbd></button>
                <button role="menuitem" type="button" onClick={() => { copySelection(); setOpenApplicationMenu(null); }}>COPY <kbd>CTRL C</kbd></button>
                <button role="menuitem" type="button" disabled={!clipboard.current} onClick={() => { pasteSelection(); setOpenApplicationMenu(null); }}>PASTE <kbd>CTRL V</kbd></button>
                <button role="menuitem" type="button" onClick={() => { beginCreateGroup(); setOpenApplicationMenu(null); }}>GROUP SELECTION… <kbd>CTRL G</kbd></button>
                <button role="menuitem" type="button" onClick={() => { runAlignment('left'); setOpenApplicationMenu(null); }}>ALIGN SELECTED LEFT</button>
                <button role="menuitem" type="button" onClick={() => { runAlignment('top'); setOpenApplicationMenu(null); }}>ALIGN SELECTED TOP</button>
                <button role="menuitem" type="button" onClick={() => { runAlignment('horizontal'); setOpenApplicationMenu(null); }}>DISTRIBUTE HORIZONTALLY</button>
                <button role="menuitem" type="button" onClick={() => { runAlignment('vertical'); setOpenApplicationMenu(null); }}>DISTRIBUTE VERTICALLY</button>
                <button role="menuitem" type="button" onClick={() => { arrangeSelectedGroup(); setOpenApplicationMenu(null); }}>ARRANGE GROUP GRID</button>
                <button role="menuitem" type="button" onClick={() => { removeSelection(); setOpenApplicationMenu(null); }}>DELETE SELECTION <kbd>DEL</kbd></button>
              </> : null}
              {menu === 'view' ? <>
                <label className="menu-select">WORKSPACE
                  <select aria-label="Workspace arrangement" value={workspacePresentation.arrangement}
                    onChange={(event) => selectWorkspaceArrangement(event.target.value as WorkspaceArrangement)}>
                    <option value="balanced">Balanced</option>
                    <option value="create">Create Focus</option>
                    <option value="learn">Learn &amp; Inspect</option>
                    {workspacePresentation.arrangement === 'custom' ? <option value="custom">Custom</option> : null}
                  </select>
                </label>
                <button role="menuitemradio" aria-checked={activePatchId === 'barr-reference'} type="button"
                  onClick={() => { void selectFactoryPatch('barr-reference'); setOpenApplicationMenu(null); }}>COMPARE A / BARR</button>
                <button role="menuitemradio" aria-checked={activePatchId === comparisonPatchId} type="button"
                  onClick={() => { void selectFactoryPatch(comparisonPatchId); setOpenApplicationMenu(null); }}>
                  COMPARE B / {comparisonPatchLabel(comparisonPatchId)}
                </button>
                <button role="menuitemcheckbox" aria-checked={workspacePresentation.modulesOpen} type="button"
                  onClick={() => toggleDock('modules')}>MODULE PALETTE {workspacePresentation.modulesOpen ? 'OPEN' : 'CLOSED'}</button>
                <button role="menuitemcheckbox" aria-checked={workspacePresentation.contextOpen} type="button"
                  onClick={() => toggleDock('context')}>CONTEXT DOCK {workspacePresentation.contextOpen ? 'OPEN' : 'CLOSED'}</button>
                {standaloneAvailable ? <button role="menuitemcheckbox" aria-checked={workspacePresentation.auditionOpen} type="button"
                  onClick={() => setWorkspacePresentation((current) => ({ ...current, auditionOpen: !current.auditionOpen }))}>
                  AUDIO DRAWER {workspacePresentation.auditionOpen ? 'OPEN' : 'CLOSED'}
                </button> : null}
                <label className="menu-select">PROCESSING QUALITY
                  <select aria-label="Processing quality" value={qualityPolicy}
                    onChange={(event) => setQualityPolicy(event.target.value as QualityPolicy)}>
                    <option value="draft">Draft / lowest cost</option>
                    <option value="normal">Normal / released sound</option>
                    <option value="high">High / cubic</option>
                  </select>
                </label>
                <button role="menuitemcheckbox" aria-checked={energyEnabled} type="button" disabled={reducedMotion}
                  onClick={() => setEnergyEnabled((enabled) => !enabled)}>ENERGY {reducedMotion ? 'REDUCED' : energyEnabled ? 'ON' : 'OFF'}</button>
                <button role="menuitem" type="button"
                  onClick={() => { openContext('diagnostics'); setOpenApplicationMenu(null); }}>ANALYZE / DIAGNOSTICS</button>
              </> : null}
              {menu === 'help' ? <>
                <button role="menuitemcheckbox" aria-checked={teachingEnabled} type="button" onClick={() => { toggleTeaching(); setOpenApplicationMenu(null); }}>CONTEXTUAL LEARNING {teachingEnabled ? 'ON' : 'OFF'}</button>
                <button role="menuitem" type="button" onClick={() => { setResearchOpen(true); openContext('research'); setOpenApplicationMenu(null); }}>KEITH BARR ARCHITECTURE NOTES</button>
                {snapshot.productVersion && snapshot.buildCommit ? <span className="menu-build">REVERB PLAYGROUND v{snapshot.productVersion}<br />{snapshot.buildCommit}</span> : null}
              </> : null}
            </div> : null}
          </div>)}
        </nav>
        <div className="header-runtime">
          <label className="factory-picker">
            <span>PATCH</span>
            <select
              aria-label="Factory patch"
              value={activePatchId}
              onChange={(event) => {
                const id = event.target.value;
                if (id !== 'custom') void selectFactoryPatch(id as FactoryPatchId);
              }}
            >
              {activePatchId === 'custom' ? <option value="custom">Custom / loaded file</option> : null}
              {factoryPatches.map((patch) => <option key={patch.id} value={patch.id}>{patch.label}</option>)}
            </select>
          </label>
          <div className="patch-identity"><strong>{activePatch?.graphName ?? 'CUSTOM.graph'}</strong>
            <span>{nodes.length} BLOCKS / {edges.length} CABLES</span></div>
          <span className={`clean-state ${isHistoryClean(graphHistory, { nodes, edges }) ? 'is-clean' : 'is-dirty'}`}>
            {isHistoryClean(graphHistory, { nodes, edges }) ? 'SAVED' : 'UNSAVED'}
          </span>
          <div className="comparison-switch" role="group" aria-label="Compare Barr reference with selected design">
            <button type="button" aria-pressed={activePatchId === 'barr-reference'} onClick={() => void selectFactoryPatch('barr-reference')}>A / BARR</button>
            <button type="button" aria-pressed={activePatchId === comparisonPatchId} onClick={() => void selectFactoryPatch(comparisonPatchId)}>
              B / {comparisonPatchLabel(comparisonPatchId)}
            </button>
          </div>
          <button className="energy-toggle" type="button" aria-pressed={energyEnabled} disabled={reducedMotion} onClick={() => setEnergyEnabled((value) => !value)} title={reducedMotion ? 'Disabled by the operating-system reduced-motion preference' : 'Toggle measured node and cable energy'}>
            ENERGY {reducedMotion ? 'REDUCED' : energyEnabled ? 'ON' : 'OFF'}
          </button>
          <button className="header-save" type="button" onClick={savePatch}>SAVE</button>
          <div className="header-status" aria-label="Audition status">
            <span className="status-dot" aria-hidden="true" />
            <span>{diagnostics?.topologyPublication.crossfadeTotalSamples
              ? `CROSSFADING #${diagnostics.topologyPublication.crossfadeFromRevision} → #${diagnostics.topologyPublication.activeRevision}`
              : diagnostics?.topologyPublication.pendingRevision
                ? `COMPILING #${diagnostics.topologyPublication.pendingRevision}`
                : diagnostics?.topologyPublication.activeRevision
                  ? `GRAPH ACTIVE #${diagnostics.topologyPublication.activeRevision}`
                  : `RUNTIME BOUND / ${snapshot.sampleRate > 0 ? `${(snapshot.sampleRate / 1000).toFixed(1)} kHz` : 'awaiting audio'}`}</span>
          </div>
        </div>
      </header>

      <section className={`workspace arrangement-${workspacePresentation.arrangement}${workspaceLayout.overlay ? ' workspace-overlay' : ''}`}
        style={{ gridTemplateColumns: workspaceGridColumns(workspaceLayout) }}>
        <aside className={`module-library${workspaceLayout.modulesVisible ? ' dock-visible' : ' dock-hidden'}`} aria-label="Module library" aria-hidden={!workspaceLayout.modulesVisible}>
          <div className="pane-heading">
            <span>MODULES</span>
            <div><span className="pane-count">{moduleDefinitions.length}</span><button className="dock-close" type="button" aria-label="Close module palette" onClick={() => toggleDock('modules')}>‹</button></div>
          </div>
          <p className="pane-help">Click a primitive to place it near the canvas center. Audio cables are mono.</p>
          {modules.map((section) => (
            <section className="module-group" key={section.group}>
              <h2>{section.group}</h2>
              {section.items.map((item) => (
                <button className={`module-item${item.role === 'control' ? ' module-control' : ''}`} key={item.type} type="button" onClick={() => addModule(item.type)}>
                  <span className="module-glyph" aria-hidden="true" />
                  <span>{item.label}</span>
                </button>
              ))}
            </section>
          ))}
          <div className="signal-legend" aria-label="Cable styles">
            <div><span className="legend-line audio-line" /> AUDIO / SOLID</div>
            <div><span className="legend-line control-line" /> CONTROL / DASHED</div>
          </div>
        </aside>

        <section className="canvas-pane" aria-label="Patch canvas">
          <div className="canvas-toolbar">
            <div><strong>PATCH CANVAS</strong><span>{Math.round(viewport.zoom * 100)}%</span>
              <label className="workspace-picker"><span>WORKSPACE</span><select aria-label="Workspace arrangement" value={workspacePresentation.arrangement}
                onChange={(event) => selectWorkspaceArrangement(event.target.value as WorkspaceArrangement)}>
                <option value="balanced">Balanced</option><option value="create">Create Focus</option><option value="learn">Learn &amp; Inspect</option>{workspacePresentation.arrangement === 'custom' ? <option value="custom">Custom</option> : null}
              </select></label>
            </div>
            <div className="canvas-actions">
              {supportsAssistedTuning({ nodes, edges }) ? <button type="button" className="tuning-open-button"
                aria-expanded={tuningOpen} onClick={() => setTuningOpen((open) => !open)}>TUNE</button> : null}
              {matrixInspection ? <button type="button" className="matrix-view-toggle"
                disabled={!matrixInspection.canCollapse}
                title={matrixInspection.reason}
                aria-pressed={!matrixIsCollapsed}
                onClick={() => setMatrixCollapsed((collapsed) => !collapsed)}>
                {!matrixInspection.canCollapse ? 'MATRIX EXPANDED / UNSAFE' : matrixIsCollapsed ? 'EXPAND MATRIX' : 'COLLAPSE MATRIX'}
              </button> : null}
              {matrixInspection ? <button type="button" className="matrix-view-toggle"
                onClick={() => {
                  setSelectedNode(matrixDisplayedGraph.nodes.find((node) => node.id === 'matrix-mixer-view')
                  ?? { id: 'matrix-mixer-view', type: 'patchNode', position: { x: 0, y: 0 }, data: {
                    label: 'Matrix Mixer 4×4', type: 'matrix-mixer', role: 'routing', runtimeBound: true,
                    ports: [], parameters: [],
                  } });
                  openContext('matrix');
                }}>
                INSPECT MATRIX
              </button> : null}
            </div>
          </div>
          <input
            ref={loadInput}
            className="file-input"
            aria-label="Load patch file"
            type="file"
            accept=".json,application/json"
            onChange={(event) => {
              const file = event.target.files?.[0];
              if (file) void loadPatch(file);
              event.target.value = '';
            }}
          />
          {fileStatus ? (
            <div className={`file-status file-status-${fileStatus.kind}`} role={fileStatus.kind === 'error' ? 'alert' : 'status'}>
              <span>{fileStatus.message}</span>
              <button type="button" aria-label="Dismiss file status" onClick={() => setFileStatus(null)}>×</button>
            </div>
          ) : null}
          {graphStatus ? (
            <div className={`file-status file-status-${graphStatus.kind}`} role={graphStatus.kind === 'error' ? 'alert' : 'status'}>
              <span>{graphStatus.message}</span>
              <button type="button" aria-label="Dismiss graph status" onClick={() => setGraphStatus(null)}>×</button>
            </div>
          ) : null}
          {pendingConnection ? (
            <div className="connection-offer" role="dialog" aria-modal="true" aria-labelledby="occupied-title" aria-describedby="occupied-description">
              <strong id="occupied-title">INPUT ALREADY HAS A CABLE</strong>
              <span id="occupied-description">{pendingSignal === 'control' ? 'A parameter socket accepts one control cable. Replace it or cancel.' : 'The amber dashed Sum is a preview only. Confirm to preserve both audio sources, or choose another action.'}</span>
              <div>{pendingSignal === 'audio' ? <button ref={occupiedConfirm} type="button" onClick={() => resolveOccupied('sum')}>CONFIRM INSERT +</button> : null}<button ref={pendingSignal === 'control' ? occupiedConfirm : undefined} type="button" onClick={() => resolveOccupied('replace')}>REPLACE CABLE</button><button type="button" onClick={() => resolveOccupied('cancel')}>CANCEL</button></div>
            </div>
          ) : null}
          {groupNameDraft !== null ? (
            <div className="connection-offer group-name-dialog" role="dialog" aria-modal="true" aria-labelledby="group-name-title">
              <strong id="group-name-title">NAME VISUAL GROUP</strong>
              <span>Grouping changes layout only. Primitive blocks, cables, automation, and processing remain authoritative.</span>
              <input autoFocus aria-label="Group name" maxLength={64} value={groupNameDraft}
                onChange={(event) => setGroupNameDraft(event.target.value)}
                onKeyDown={(event) => { if (event.key === 'Enter') commitCreateGroup(); else if (event.key === 'Escape') setGroupNameDraft(null); }} />
              <div><button type="button" disabled={!groupNameDraft.trim()} onClick={commitCreateGroup}>CREATE GROUP</button><button type="button" onClick={() => setGroupNameDraft(null)}>CANCEL</button></div>
            </div>
          ) : null}
          <div className="flow-wrap">
            {!workspaceLayout.modulesVisible ? <button className="dock-reveal dock-reveal-left" type="button" onClick={() => toggleDock('modules')}>MODULES ›</button> : null}
            {!workspaceLayout.contextVisible ? <button className="dock-reveal dock-reveal-right" type="button" onClick={() => toggleDock('context')}>‹ CONTEXT</button> : null}
            <ReactFlow
              nodes={connectionDisplayedGraph.nodes}
              edges={connectionDisplayedGraph.edges}
              nodeTypes={{ patchNode: PatchNode }}
              edgeTypes={{ smoothstep: RoutedEdge }}
              onNodesChange={onNodesChange}
              onEdgesChange={onEdgesChange}
              onNodeDragStart={() => { dragStart.current = snapshotGraph({ nodes, edges }); }}
              onNodeDragStop={(_event, movedNode, movedNodes) => {
                const before = dragStart.current; dragStart.current = null; if (!before) return;
                const positions = new Map(movedNodes.map((node) => [node.id, node.position]));
                positions.set(movedNode.id, movedNode.position);
                const after = { nodes: nodes.map((node) => positions.has(node.id) ? { ...node, position: { ...positions.get(node.id)! } } : node), edges };
                setGraphHistory((history) => commitGraphEdit(history, `Move ${movedNode.id}`, before, after));
              }}
              onConnect={commitConnection}
              isValidConnection={(connection) => decideConnection(nodes, edges, { source: connection.source, sourceHandle: connection.sourceHandle ?? null, target: connection.target, targetHandle: connection.targetHandle ?? null }).kind !== 'invalid'}
              defaultEdgeOptions={{ interactionWidth: 24 }}
              onSelectionChange={handleSelection}
              onNodeClick={(_event, node) => {
                if ((node as Node<PatchNodeData>).data.type !== 'graph-group') return;
                setSelectedNode(node as Node<PatchNodeData>); setSelectedEdge(null); openContext('node');
              }}
              onEdgeClick={(_event, edge) => { setSelectedEdge(edges.find((candidate) => candidate.id === edge.id) ?? edge); setSelectedNode(null); setActiveLoopIndex(0); openContext('cable'); }}
              onPaneClick={() => { setSelectedNode(null); setSelectedEdge(null); setRouteFocus(null); }}
              onViewportChange={setViewport}
              defaultViewport={initial.viewport}
              fitView={!restored}
              fitViewOptions={{ padding: 0.16, minZoom: 0.58, maxZoom: 1.1 }}
              minZoom={0.2}
              maxZoom={1.8}
              deleteKeyCode={null}
              selectionKeyCode="Shift"
              multiSelectionKeyCode="Shift"
              panActivationKeyCode="Space"
              panOnDrag={[1, 2]}
              selectionOnDrag
              nodesFocusable
              edgesFocusable
              elevateEdgesOnSelect
              proOptions={{ hideAttribution: false }}
              aria-label={`${activePatch?.label ?? 'Custom'} patch graph`}
            >
              <Background color="#34414a" gap={22} size={1.2} variant={BackgroundVariant.Dots} />
              <Controls position="bottom-left" showInteractive={false} />
              <MiniMap
                pannable
                zoomable
                position="bottom-right"
                nodeColor={(node) => node.selected ? '#f2b44e' : '#52616d'}
                maskColor="rgba(9, 12, 15, 0.78)"
              />
            </ReactFlow>
            <div className="interaction-hint">SPACE + DRAG PAN · WHEEL ZOOM · SHIFT BOX SELECT · DELETE REMOVE · R RESET</div>
          </div>
        </section>

        <aside className={`inspector context-dock${workspaceLayout.contextVisible ? ' dock-visible' : ' dock-hidden'}`} aria-label="Context dock" aria-hidden={!workspaceLayout.contextVisible}>
          <div className="pane-heading context-heading"><span>CONTEXT</span><div><button className="teaching-toggle" type="button" aria-pressed={teachingEnabled} title="Toggle contextual explanations and response architecture overlays" onClick={toggleTeaching}>LEARN {teachingEnabled ? 'ON' : 'OFF'}</button><button className="dock-close" type="button" aria-label="Close context dock" onClick={() => toggleDock('context')}>›</button></div></div>
          <div className="context-tabs" role="tablist" aria-label="Context views">
            {contextTabs.map((tab) => <button id={`context-tab-${tab}`} role="tab" type="button" key={tab}
              aria-label={tab === 'inspect' ? 'Inspector context' : `${contextTabLabels[tab]} context`}
              aria-selected={contextTab === tab} aria-controls={`context-panel-${tab}`} tabIndex={contextTab === tab ? 0 : -1}
              onKeyDown={(event) => handleContextTabKey(event, tab)} onClick={() => { if (tab === 'analyze') setDiagnostics(null); setContextTab(tab); }}>{contextTabLabels[tab]}</button>)}
          </div>
          <div className="context-tab-panels">
          <section id="context-panel-inspect" className="context-panel context-inspect" role="tabpanel" aria-labelledby="context-tab-inspect" hidden={contextTab !== 'inspect'}>
          <div className="context-evidence-heading"><strong>PATCH VALUES</strong><span>EDITED / SAVED</span></div>
          {tuningOpen && supportsAssistedTuning({ nodes, edges }) ? <AssistedTuningPanel
            previewId={tuningPreview?.id ?? null} busy={tuningBusy}
            onPreview={(id) => { void previewAssistedTuning(id); }} onAccept={acceptAssistedTuning}
            onCancel={() => { void cancelAssistedTuning(); }} onClose={() => {
              if (tuningPreview) void cancelAssistedTuning();
              setTuningOpen(false);
            }} /> : null}
          {selectedNode ? (
            <div className="inspector-content" key={selectedNode.id}>
              <div className="selection-kicker">SELECTED BLOCK</div>
              <h2>{selectedNode.data.userName?.trim() || selectedNode.data.label}</h2>
              <code>{selectedNode.id}</code>
              {selectedNode.data.presentation === 'gravity' && selectedMacroInspection ? <GravityPresentation
                value={selectedNode.data.parameters.find((parameter) => parameter.id === 'value')?.value ?? 0}
                destinationCount={selectedMacroInspection.destinations.length}
                showMeasuredReference={activePatchId === 'gravity-diffusion'}
                onBegin={() => beginParameterEdit(selectedNode.id, 'value', selectedNode.data.parameters.find((parameter) => parameter.id === 'value')?.value ?? 0)}
                onValue={(value) => applyParameter(selectedNode.id, 'value', value)}
                onCommit={commitParameterEdit}
                onFocus={focusSelectedMacro}
              /> : null}
              {loopInspection ? <LoopInspector inspection={loopInspection} activeIndex={normalizedLoopIndex} onActiveIndex={setActiveLoopIndex} splitFeedback={activePatchId === 'split-feedback-shimmer'} /> : null}
              <dl className="property-list">
                <div><dt>TYPE</dt><dd>{selectedNode.data.type}</dd></div>
                <div><dt>ROLE</dt><dd>{selectedNode.data.role}</dd></div>
                <div><dt>PORTS</dt><dd>{selectedNode.data.ports.length} mono</dd></div>
              </dl>
              {selectedNode.data.presentationGroup ? <section className="group-inspector" aria-label="Visual group controls">
                <label><span>GROUP NAME</span><input aria-label="Group name" defaultValue={selectedNode.data.presentationGroup.name}
                  onKeyDown={(event) => { if (event.key === 'Enter') event.currentTarget.blur(); }}
                  onBlur={(event) => { if (event.currentTarget.value.trim() !== selectedNode.data.presentationGroup?.name) changeSelectedGroup('rename', event.currentTarget.value); }} /></label>
                <p>{selectedNode.data.groupMemberIds?.length ?? graphGroups.find((group) => group.id === selectedNode.data.presentationGroup?.id)?.nodeIds.length ?? 0} primitive blocks · presentation only</p>
                <div>
                  <button type="button" onClick={() => changeSelectedGroup(selectedNode.data.presentationGroup?.collapsed ? 'expand' : 'collapse')}>{selectedNode.data.presentationGroup.collapsed ? 'EXPAND GROUP' : 'COLLAPSE GROUP'}</button>
                  {selectedNode.data.presentationGroup.collapsed && loopInspection?.loops.length ? <button type="button" onClick={() => changeSelectedGroup('expand')}>REVEAL COMPLETE LOOP</button> : null}
                  <button type="button" onClick={() => changeSelectedGroup('ungroup')}>UNGROUP</button>
                </div>
                <small>Nested groups are intentionally unsupported.</small>
              </section> : null}
              {selectedNode.data.type === 'pitch-shift' ? <PitchShiftVisualization
                parameters={selectedNode.data.parameters}
                reducedMotion={reducedMotion}
                sampleRate={snapshot.sampleRate > 0 ? snapshot.sampleRate : 48_000}
              /> : null}
              {selectedNode.data.type === 'macro' ? <label className="macro-name-field">
                <span>MACRO NAME</span>
                <input
                  aria-label="Macro name"
                  maxLength={64}
                  value={selectedNode.data.userName ?? ''}
                  onFocus={() => beginParameterEdit(selectedNode.id, 'name', 0)}
                  onChange={(event) => applyMacroName(selectedNode.id, event.target.value)}
                  onBlur={() => {
                    if (!(selectedNode.data.userName ?? '').trim()) applyMacroName(selectedNode.id, 'Macro');
                    commitParameterEdit();
                  }}
                />
              </label> : null}
              {selectedMacroInspection ? <section className="macro-destinations" aria-label="Reachable mapped parameters">
                <header><span>REACHABLE MAPPINGS</span><strong>{selectedMacroInspection.destinations.length}</strong></header>
                {selectedMacroInspection.destinations.length ? <ul>{selectedMacroInspection.destinations.map((destination) => <li key={`${destination.nodeId}.${destination.parameterId}`}>
                  <code>{destination.nodeId}.{destination.parameterId}</code>
                  <span>{destination.minimum.toFixed(2)} … {destination.maximum.toFixed(2)} {destination.unit}</span>
                </li>)}</ul> : <p>Connect the explicit output through Curve Mapper blocks to parameter sockets.</p>}
              </section> : null}
              {selectedMappingRange ? (
                <section className="control-range-preview" aria-label="Predicted control output range">
                  <header><span>PREDICTED OUTPUT</span><strong>{selectedMappingRange.minimum.toFixed(2)} … {selectedMappingRange.maximum.toFixed(2)}</strong></header>
                  <div><i style={{ left: `${(selectedMappingRange.minimum + 1) * 50}%` }} /><i style={{ left: `${(selectedMappingRange.maximum + 1) * 50}%` }} /></div>
                  <p>CURVE → SCALE → OFFSET → CLAMP · visible before connection.</p>
                </section>
              ) : null}
              <h3>PARAMETERS</h3>
              {selectedNode.data.parameters.length ? selectedNode.data.parameters
                .filter((parameter) => selectedNode.data.presentation !== 'gravity' || parameter.id !== 'value')
                .map((parameter) => {
                const choices = parameterChoices(parameter.unit);
                return (
                <div className="parameter-card" key={parameter.id}>
                  <span>{parameter.id.toUpperCase()}</span>
                  <label className="parameter-value">
                    <span className="sr-only">{`${selectedNode.data.label} ${parameter.id} numeric value`}</span>
                    {choices ? <select
                      aria-label={`${selectedNode.data.label} ${parameter.id}`}
                      value={parameter.value}
                      onFocus={() => beginParameterEdit(selectedNode.id, parameter.id, parameter.value)}
                      onChange={(event) => changeParameter(selectedNode.id, parameter.id, Number(event.target.value))}
                      onBlur={commitParameterEdit}
                    >{choices.map((choice) => <option key={choice.value} value={choice.value}>{choice.label}</option>)}</select> : <input
                      className="parameter-number"
                      type="number"
                      min={parameter.minimum}
                      max={parameter.maximum}
                      step={parameter.step}
                      value={parameter.value}
                      onFocus={() => beginParameterEdit(selectedNode.id, parameter.id, parameter.value)}
                      onChange={(event) => changeParameter(selectedNode.id, parameter.id, Number(event.target.value))}
                      onKeyDown={(event) => { if (event.key === 'Enter') commitParameterEdit(); }}
                      onBlur={commitParameterEdit}
                    />}
                    <strong>{parameter.unit === 'milliseconds' ? 'ms' : parameter.unit === 'hertz' ? 'Hz' : parameter.unit === 'semitones' ? 'st' : ''}</strong>
                  </label>
                  <small>{parameter.unit}</small>
                  {!choices ? <input
                    aria-label={`${selectedNode.data.label} ${parameter.id}`}
                    type="range"
                    min={parameter.minimum}
                    max={parameter.maximum}
                    step={parameter.step}
                    value={parameter.value}
                    onPointerDown={() => beginParameterEdit(selectedNode.id, parameter.id, parameter.value)}
                    onChange={(event) => changeParameter(selectedNode.id, parameter.id, Number(event.target.value))}
                    onPointerUp={commitParameterEdit}
                    onKeyDown={(event) => {
                      if (!activeEdit.current) beginParameterEdit(selectedNode.id, parameter.id, parameter.value);
                      if (event.key === 'Enter') commitParameterEdit();
                    }}
                    onBlur={commitParameterEdit}
                  /> : null}
                  {parameter.modulation ? (
                    <section className="modulation-mapping" aria-label={`${selectedNode.data.label} ${parameter.id} modulation mapping`}>
                      <header><span>CONTROL SOCKET</span><code>{parameter.modulation.portId}</code></header>
                      <div className="mapping-formula">effective = clamp(base + amount × control)</div>
                      <label>
                        <span>POLARITY</span>
                        <select
                          value={parameter.modulation.polarity}
                          onFocus={() => beginParameterEdit(selectedNode.id, `${parameter.id} modulation`, parameter.value)}
                          onChange={(event) => applyModulation(selectedNode.id, parameter.id, { polarity: event.target.value as 'unipolar' | 'bipolar' })}
                          onBlur={commitParameterEdit}
                        ><option value="bipolar">BIPOLAR −1…+1</option><option value="unipolar">UNIPOLAR 0…1</option></select>
                      </label>
                      <label><span>AMOUNT</span><input type="number" step={parameter.step} value={parameter.modulation.amount} onFocus={() => beginParameterEdit(selectedNode.id, `${parameter.id} modulation`, parameter.value)} onChange={(event) => applyModulation(selectedNode.id, parameter.id, { amount: Number(event.target.value) })} onBlur={commitParameterEdit} /></label>
                      <div className="mapping-clamp">
                        <label><span>CLAMP MIN</span><input type="number" min={parameter.minimum} max={parameter.modulation.clampMaximum} step={parameter.step} value={parameter.modulation.clampMinimum} onFocus={() => beginParameterEdit(selectedNode.id, `${parameter.id} modulation`, parameter.value)} onChange={(event) => applyModulation(selectedNode.id, parameter.id, { clampMinimum: Number(event.target.value) })} onBlur={commitParameterEdit} /></label>
                        <label><span>CLAMP MAX</span><input type="number" min={parameter.modulation.clampMinimum} max={parameter.maximum} step={parameter.step} value={parameter.modulation.clampMaximum} onFocus={() => beginParameterEdit(selectedNode.id, `${parameter.id} modulation`, parameter.value)} onChange={(event) => applyModulation(selectedNode.id, parameter.id, { clampMaximum: Number(event.target.value) })} onBlur={commitParameterEdit} /></label>
                      </div>
                      <footer>
                        {parameter.id === 'delay' && (selectedNode.data.type === 'delay' || selectedNode.data.type === 'allpass')
                          ? '1 kHz control ticks / linear audio-rate ramp / fractional linear delay tap. Moving time intentionally produces Doppler and pitch effects; control sources are limited to 100 Hz.'
                          : parameter.id === 'coefficient' && selectedNode.data.type === 'allpass'
                            ? '1 kHz control ticks / linear audio-rate ramp / coefficient hard-limited to -0.95 through +0.95.'
                            : '1 kHz control ticks / linear interpolation to audio rate'}
                      </footer>
                    </section>
                  ) : null}
                </div>
                );
              }) : <p className="empty-parameters">No editable parameters.</p>}
              <div className="history-actions">
                <button type="button" disabled={!graphHistory.undo.length} onClick={undoGraph}>UNDO</button>
                <button type="button" disabled={!graphHistory.redo.length} onClick={redoGraph}>REDO</button>
              </div>
              <div className="selection-note">{selectedNode.data.type === 'graph-group'
                ? 'Visual boundary only. Expand to inspect and edit every authoritative primitive and cable; audio is never compiled from this summary.'
                : selectedNode.data.type === 'macro'
                ? 'Macro value changes use a fixed 20 ms runtime ramp and do not compile topology. Default and detent edits republish the visible graph.'
                : selectedNode.data.runtimeBound ? `Live value from native DSP runtime contract v${snapshot.contractVersion}.`
                  : 'Constructed graph block. Audible edits compile off-thread and crossfade into the live plugin at an audio-block boundary.'}</div>
            </div>
          ) : selectedEdge ? (
            <div className="inspector-content">
              <div className="selection-kicker">SELECTED CABLE</div>
              <h2>{selectedEdge.data?.signal === 'control' ? 'Control connection' : 'Audio connection'}</h2>
              <code>{selectedEdge.id}</code>
              {loopInspection ? <LoopInspector inspection={loopInspection} activeIndex={normalizedLoopIndex} onActiveIndex={setActiveLoopIndex} splitFeedback={activePatchId === 'split-feedback-shimmer'} /> : null}
              <dl className="property-list">
                <div><dt>FROM</dt><dd>{selectedEdge.source}</dd></div>
                <div><dt>TO</dt><dd>{selectedEdge.target}</dd></div>
                <div><dt>SIGNAL</dt><dd>{selectedEdge.data?.signal === 'control' ? 'CONTROL / DASHED' : 'AUDIO / SOLID'}</dd></div>
              </dl>
              <section className="cable-routing-inspector" aria-label="Cable routing and focus">
                <div className="cable-focus-actions">
                  <button type="button" onClick={() => focusCablePath('source')}>TRACE TO SOURCE</button>
                  <button type="button" onClick={() => focusCablePath('output')}>TRACE TO OUTPUT</button>
                  {loopInspection?.loops.length ? <button type="button" onClick={focusCompleteLoop}>FOCUS COMPLETE LOOP</button> : null}
                  {routeFocus ? <button type="button" onClick={() => setRouteFocus(null)}>CLEAR TRACE</button> : null}
                </div>
                <h3>ROUTING LAYOUT</h3>
                {(cableLayout(selectedEdge).waypoints ?? []).map((point, index) => <div className="waypoint-row" key={index}>
                  <span>POINT {index + 1}</span>
                  <label>X <input aria-label={`Waypoint ${index + 1} X`} type="number" value={point.x} onChange={(event) => commitLayoutEdit('Move cable waypoint', (state) => updateCableWaypoint(state, selectedEdge.id, index, { x: Number(event.target.value), y: point.y }))} /></label>
                  <label>Y <input aria-label={`Waypoint ${index + 1} Y`} type="number" value={point.y} onChange={(event) => commitLayoutEdit('Move cable waypoint', (state) => updateCableWaypoint(state, selectedEdge.id, index, { x: point.x, y: Number(event.target.value) }))} /></label>
                </div>)}
                <div className="cable-focus-actions"><button type="button" onClick={() => commitLayoutEdit('Add cable waypoint', (state) => addCableWaypoint(state, selectedEdge.id))}>ADD WAYPOINT</button>
                  {(cableLayout(selectedEdge).waypoints?.length ?? 0) > 0 ? <button type="button" onClick={() => commitLayoutEdit('Clear cable waypoints', (state) => clearCableWaypoints(state, selectedEdge.id))}>CLEAR WAYPOINTS</button> : null}</div>
                {cableLayout(selectedEdge).portal ? <label className="portal-name-field"><span>PAIRED PORTAL NAME</span><input aria-label="Paired routing portal name" maxLength={32} value={cableLayout(selectedEdge).portal!.name}
                  onChange={(event) => commitLayoutEdit('Rename routing portal', (state) => setRoutingPortal(state, selectedEdge.id, event.target.value))} /></label> : null}
                <button type="button" onClick={() => commitLayoutEdit(cableLayout(selectedEdge).portal ? 'Remove routing portal' : 'Create routing portal',
                  (state) => setRoutingPortal(state, selectedEdge.id, cableLayout(selectedEdge).portal ? undefined : `PORTAL ${selectedEdge.id}`.slice(0, 32)))}>{cableLayout(selectedEdge).portal ? 'REMOVE PAIRED PORTAL' : 'CREATE PAIRED PORTAL'}</button>
                <small>Waypoints and portals are saved layout only. Selecting a portal reveals its complete cable.</small>
              </section>
            </div>
          ) : (
            <div className="inspector-empty">
              <div className="empty-crosshair" aria-hidden="true">+</div>
              <h2>Nothing selected</h2>
              <p>Select a block or cable to inspect its identity, ports, and saved parameter values.</p>
              <kbd>TAB</kbd><span>focus graph elements</span>
              <kbd>ENTER</kbd><span>select focused item</span>
              <kbd>⌘/CTRL C</kbd><span>copy selected non-I/O blocks</span>
              <kbd>⌘/CTRL V</kbd><span>paste with new block and cable IDs</span>
              <kbd>DELETE</kbd><span>remove selected blocks or cables</span>
            </div>
          )}
          </section>

          <section id="context-panel-analyze" className="context-panel context-analyze" role="tabpanel" aria-labelledby="context-tab-analyze" hidden={contextTab !== 'analyze'}>
            <div className="context-evidence-heading"><strong>ACTIVE GRAPH EVIDENCE</strong><span>MEASURED / ESTIMATED / COMPILED</span></div>
            <section className="energy-analysis" aria-label="Energy analysis status">
              <header><strong>ENERGY / MEASURED RMS</strong><span>{energyEnabled && !reducedMotion ? 'VISIBLE' : 'DORMANT'}</span></header>
              <p>{energyEnabled && !reducedMotion
                ? `Current graph revision #${activeGraphRevision.current || '—'} publishes coherent per-operation RMS lanes at 30 Hz.`
                : 'Sample scanning and telemetry polling are disabled. Enable Energy to measure live node and cable activity.'}</p>
            </section>
            {responseCapture ? <ResponseViewer
              key={responseCapture.capture.generation}
              capture={responseCapture.capture}
              patchId={responseCapture.patchId}
              gateTeaching={responseCapture.gateTeaching}
              teachingEnabled={teachingEnabled}
              captureRevision={responseCapture.graphRevision}
              activeRevision={activeGraphRevision.current}
              onClose={() => setResponseCapture(null)}
            /> : <section className="analysis-empty"><strong>NO MEASURED RESPONSE</strong><p>Capture an isolated impulse to add waveform, decay, density, onset, and RT60 evidence here.</p></section>}
            {matrixInspection ? <section className="analysis-section"><div className="analysis-section-title"><strong>MATRIX</strong><span>PREDICTED FROM VISIBLE COEFFICIENTS</span></div><MatrixMixerInspector inspection={matrixInspection} /></section> : null}
            <DiagnosticsPanel diagnostics={diagnostics} runawayLoop={runawayLoopInspection} canUndo={graphHistory.undo.length > 0} onUndo={undoGraph} onRecover={() => { void callNative('resetSafety').catch(() => undefined); }} />
          </section>

          <section id="context-panel-learn" className="context-panel context-learn" role="tabpanel" aria-labelledby="context-tab-learn" hidden={contextTab !== 'learn'}>
            <div className="context-evidence-heading"><strong>ARCHITECTURE &amp; LISTENING</strong><span>DOCUMENTED / EXPLANATORY</span></div>
            {activePatchId === 'dense-figure-eight' && teachingEnabled ? <DenseFigureEightTeaching /> : null}
            {activePatchId === 'four-line-dense-room' && teachingEnabled ? <FourLineFdnTeaching collapsed={matrixIsCollapsed} /> : null}
            {activePatchId === 'safe-parallel-shimmer' && teachingEnabled ? <ParallelShimmerTeaching selectedNodeId={selectedNode?.id} /> : null}
            {activePatchId === 'split-feedback-shimmer' && teachingEnabled ? <SplitFeedbackShimmerTeaching focus={splitLoopFocus} onFocus={setSplitLoopFocus} /> : null}
            {activePatchId === 'reverse-cosmic-shimmer' && teachingEnabled ? <ReverseCosmicShimmerTeaching focus={reverseCosmicFocus} onFocus={setReverseCosmicFocus} /> : null}
            {showTeaching ? <TeachingCard topic={teachingTopicFor(selectedNode?.id)} onDismiss={() => setDismissedTeaching(teachingKey)} onResearch={() => setResearchOpen(true)} /> : null}
            {!teachingEnabled ? <section className="learn-disabled"><strong>CONTEXTUAL LEARNING IS OFF</strong><p>Inspector editing remains available. Enable Learn to restore documented architecture and listening guidance.</p><button type="button" onClick={toggleTeaching}>ENABLE LEARN</button></section> : null}
            {researchOpen ? <section className="research-inline" aria-label="Keith Barr architecture research">
              <header><div><span>OFFLINE RESEARCH / DOCUMENTED SOURCES</span><h2>Keith Barr reverb architectures</h2></div><button type="button" onClick={() => setResearchOpen(false)}>COLLAPSE</button></header>
              <pre>{researchText}</pre>
            </section> : !showTeaching ? <button className="research-link" type="button" onClick={() => setResearchOpen(true)}>OPEN KEITH BARR ARCHITECTURE LIBRARY</button> : null}
          </section>
          </div>
        </aside>
      </section>
      {measurementDrawerVisible ? <MeasurementBar sampleRate={snapshot.sampleRate} onCapture={receiveCapture} /> : null}
    </main>
  );
}

export function App() {
  const [snapshot, setSnapshot] = useState<RuntimeSnapshot | null>(null);
  const [error, setError] = useState('');

  useEffect(() => {
    const controller = new AbortController();
    fetch(new URL('./runtime-snapshot.json', window.location.href), { signal: controller.signal, cache: 'no-store' })
      .then((response) => {
        if (!response.ok) throw new Error(`Native runtime returned HTTP ${response.status}`);
        return response.json() as Promise<unknown>;
      })
      .then((payload) => setSnapshot(parseRuntimeSnapshot(payload)))
      .catch((reason: unknown) => {
        if (!controller.signal.aborted)
          setError(reason instanceof Error ? reason.message : 'Unknown runtime binding failure');
      });
    return () => controller.abort();
  }, []);

  if (error) return <main className="binding-state binding-error"><strong>RUNTIME BINDING FAILED</strong><span>{error}</span></main>;
  if (!snapshot) return <main className="binding-state"><strong>BINDING NATIVE RUNTIME…</strong></main>;
  return <ReactFlowProvider><Editor snapshot={snapshot} /></ReactFlowProvider>;
}
