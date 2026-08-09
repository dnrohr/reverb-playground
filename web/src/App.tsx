import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
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
import { createFlowModel, deleteSelected, parseRuntimeSnapshot, type PatchNodeData, type RuntimeSnapshot } from './graph';
import { createModuleNode, moduleDefinitions, nextNodeId, type ModuleType } from './modules';
import { commitGraphEdit, emptyGraphHistory, isHistoryClean, markHistoryClean, redoGraphEdit, snapshotGraph, undoGraphEdit } from './graphHistory';
import { copySelectedGraph, pasteGraph, type GraphClipboard } from './graphClipboard';
import { decorateFeedbackLoops, inspectFeedbackLoops, type FeedbackLoopInspection } from './loopInspection';
import { connectGraph, decideConnection, insertSumForOccupiedInput } from './connectionEditing';
import { PatchNode } from './PatchNode';
import { callNative } from './nativeBridge';
import { parsePatchJson, writePatchJson } from './patchPersistence';
import researchText from '../../docs/keith-barr-reverb-architectures.md?raw';
import { teachingTopicFor, type TeachingTopic } from './teaching';
import { parseImpulseCaptureResult, parseImpulseCaptureStatus, type ImpulseCaptureResult, type ImpulseCaptureStatus } from './impulseCapture';
import { analyseResponse, decayPoints, frameWindow, rt60Explanation, waveformBuckets } from './responseAnalysis';
import { decorateEnergy } from './energyDecoration';
import { parseEnergyTelemetry, shouldRunEnergyTelemetry, smoothEnergy, type EnergyLevels } from './energyTelemetry';

const modules = [
  { group: 'I/O', items: moduleDefinitions.filter((item) => item.role === 'io') },
  { group: 'SIGNAL', items: moduleDefinitions.filter((item) => item.role !== 'io') },
];

function formatValue(value: number, unit: string) {
  if (unit === 'milliseconds') return `${value.toFixed(2)} ms`;
  if (unit === 'hertz') return `${value.toLocaleString()} Hz`;
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

function LoopInspector({ inspection, activeIndex, onActiveIndex }: {
  inspection: FeedbackLoopInspection; activeIndex: number; onActiveIndex: (index: number) => void;
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
      <div className="loop-heading"><span>FEEDBACK LOOP</span><strong>{activeIndex + 1} / {inspection.loops.length}</strong></div>
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
  const [muteInput, setMuteInput] = useState(true);
  const [status, setStatus] = useState<ImpulseCaptureStatus | null>(null);
  const [result, setResult] = useState<ImpulseCaptureResult | null>(null);
  const [error, setError] = useState('');

  const start = useCallback(async () => {
    setError(''); setResult(null);
    try {
      const response = await callNative('startImpulseCapture', length, threshold, muteInput);
      if (response === undefined) { setError('NATIVE AUDIO REQUIRED FOR CAPTURE'); return; }
      setStatus(parseImpulseCaptureStatus(response));
    } catch (reason) { setError(reason instanceof Error ? reason.message : 'Capture could not start'); }
  }, [length, muteInput, threshold]);

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
    <label className="measurement-check"><input type="checkbox" checked={muteInput} disabled={busy} onChange={(event) => setMuteInput(event.target.checked)} /> MUTE LIVE INPUT</label>
    <button type="button" disabled={busy || sampleRate <= 0} onClick={() => void start()}>{busy ? 'CAPTURING…' : 'CAPTURE IMPULSE'}</button>
    <div className="measurement-readout" role="status">
      {error || (sampleRate <= 0 ? 'WAITING FOR AUDIO DEVICE' : result ? `${result.frameCount.toLocaleString()} FRAMES / ${(result.frameCount / result.sampleRate * 1000).toFixed(1)} ms / STOP: ${result.stopReason === 'threshold' ? `${result.stopThresholdDb} dBFS` : 'MAX LENGTH'}` : busy && status ? `${status.capturedMilliseconds.toFixed(1)} / ${status.maximumLengthMilliseconds.toFixed(0)} ms` : '0.1 PEAK / READY')}
    </div>
  </section>;
}

function ResponseViewer({ capture, onClose }: { capture: ImpulseCaptureResult; onClose: () => void }) {
  const analysis = useMemo(() => analyseResponse(capture), [capture]);
  const [zoom, setZoom] = useState(1);
  const [pan, setPan] = useState(0);
  const window = frameWindow(capture.frameCount, zoom, pan);
  const left = waveformBuckets(capture.left, window, 450);
  const right = waveformBuckets(capture.right, window, 450);
  const decay = decayPoints(analysis.decayDb, window, 450);
  const x = (frame: number) => 70 + 900 * (frame - window.start) / Math.max(1, window.end - window.start - 1);
  const peak = Math.max(analysis.peakLeft, analysis.peakRight, 1e-9);
  const envelope = (buckets: ReturnType<typeof waveformBuckets>, center: number) => buckets.map((bucket) => `M${x(bucket.frame).toFixed(1)},${(center - 34 * bucket.maximum / peak).toFixed(1)}L${x(bucket.frame).toFixed(1)},${(center - 34 * bucket.minimum / peak).toFixed(1)}`).join('');
  const decayPath = decay.map((point, index) => `${index ? 'L' : 'M'}${x(point.frame).toFixed(1)},${(228 + Math.min(90, Math.max(0, -point.decibels))).toFixed(1)}`).join('');
  const startMs = window.start / capture.sampleRate * 1000;
  const endMs = window.end / capture.sampleRate * 1000;
  const setBoundedZoom = (value: number) => setZoom(Math.max(1, Math.min(256, value)));
  return <section className="response-viewer" aria-label="Stereo impulse response viewer">
    <header><div><span>CAPTURE #{capture.generation} / STEREO RESPONSE</span><h2>Impulse and energy decay</h2></div><button type="button" onClick={onClose}>CLOSE ×</button></header>
    <div className="response-metrics">
      <div><span>WINDOW</span><strong>{startMs.toFixed(2)}–{endMs.toFixed(2)} ms</strong></div>
      <div><span>PEAK L / R</span><strong>{analysis.peakLeft.toFixed(4)} / {analysis.peakRight.toFixed(4)}</strong></div>
      <div><span>ONSET</span><strong>{analysis.onsetFrame === null ? 'NONE' : `${(analysis.onsetFrame / capture.sampleRate * 1000).toFixed(2)} ms`}</strong></div>
      <div><span>RT60 / T30 FIT</span><strong>{analysis.rt60Seconds === null ? 'NOT ESTIMATED' : `${analysis.rt60Seconds.toFixed(3)} s`}</strong></div>
    </div>
    <div className="response-navigation">
      <button type="button" onClick={() => { setBoundedZoom(16); setPan(0); }}>EARLY / 16×</button>
      <button type="button" disabled={zoom >= 256} onClick={() => setBoundedZoom(zoom * 2)}>ZOOM IN</button>
      <button type="button" disabled={zoom <= 1} onClick={() => setBoundedZoom(zoom / 2)}>ZOOM OUT</button>
      <button type="button" onClick={() => { setBoundedZoom(1); setPan(0); }}>FULL TAIL</button>
      <label>PAN <input aria-label="Pan response window" type="range" min="0" max="1" step="0.001" value={pan} disabled={zoom === 1} onChange={(event) => setPan(Number(event.target.value))} /></label>
      <output>{zoom.toFixed(0)}×</output>
    </div>
    <div className="response-legend" aria-label="Channel line styles"><span><i className="legend-left" />L / SOLID / UPPER</span><span><i className="legend-right" />R / DASHED / LOWER</span><span><i className="legend-decay" />ENERGY DECAY / SCHROEDER</span></div>
    <svg className="response-chart" viewBox="0 0 1000 330" role="img" aria-label={`Left solid upper waveform, right dashed lower waveform, and combined energy decay from ${startMs.toFixed(2)} to ${endMs.toFixed(2)} milliseconds`} onWheel={(event) => { event.preventDefault(); setBoundedZoom(event.deltaY < 0 ? zoom * 2 : zoom / 2); }}>
      <g className="chart-grid"><path d="M70 75H970M70 160H970M70 228H970M70 263H970M70 298H970M70 318H970" /><text x="12" y="79">L</text><text x="12" y="164">R</text><text x="12" y="232">0 dB</text><text x="12" y="267">-35</text><text x="12" y="322">-90</text></g>
      <path className="wave-left" d={envelope(left, 75)} />
      <path className="wave-right" d={envelope(right, 160)} />
      <path className="decay-line" d={decayPath} />
      <path className="fit-band" d="M70 233H970M70 263H970" />
      <text className="axis-label" x="70" y="328">{startMs.toFixed(2)} ms</text><text className="axis-label axis-end" x="970" y="328">{endMs.toFixed(2)} ms</text>
    </svg>
    {analysis.rt60Refusal ? <div className="rt60-refusal" role="note"><strong>RT60 withheld</strong><span>{rt60Explanation(analysis.rt60Refusal)}</span></div> : <div className="rt60-method"><strong>T30 estimate</strong><span>Linear fit from -5 to -35 dB, extrapolated to -60 dB.</span></div>}
  </section>;
}

function Editor({ snapshot }: { snapshot: RuntimeSnapshot }) {
  const { fitView, setViewport: setFlowViewport, screenToFlowPosition } = useReactFlow();
  const initial = useMemo(() => createFlowModel(snapshot), [snapshot]);
  const [nodes, setNodes, onNodesChange] = useNodesState<Node<PatchNodeData>>(initial.nodes);
  const [edges, setEdges, onEdgesChange] = useEdgesState(initial.edges);
  const [selectedNode, setSelectedNode] = useState<Node<PatchNodeData> | null>(null);
  const [selectedEdge, setSelectedEdge] = useState<Edge | null>(null);
  const [viewport, setViewport] = useState<Viewport>({ x: 0, y: 0, zoom: 1 });
  const activeEdit = useRef<{ label: string; before: ReturnType<typeof snapshotGraph> } | null>(null);
  const dragStart = useRef<ReturnType<typeof snapshotGraph> | null>(null);
  const clipboard = useRef<GraphClipboard | null>(null);
  const pasteCount = useRef(0);
  const loadInput = useRef<HTMLInputElement | null>(null);
  const [fileStatus, setFileStatus] = useState<{ kind: 'ok' | 'error'; message: string } | null>(null);
  const [teachingEnabled, setTeachingEnabled] = useState(() => {
    try { return window.localStorage.getItem('reverb-playground-teaching') !== 'off'; } catch { return true; }
  });
  const [dismissedTeaching, setDismissedTeaching] = useState<string | null>(null);
  const [researchOpen, setResearchOpen] = useState(false);
  const [graphHistory, setGraphHistory] = useState(() => emptyGraphHistory(initial));
  const [graphStatus, setGraphStatus] = useState<{ kind: 'ok' | 'error'; message: string } | null>(null);
  const [pendingConnection, setPendingConnection] = useState<Connection | null>(null);
  const [activeLoopIndex, setActiveLoopIndex] = useState(0);
  const [responseCapture, setResponseCapture] = useState<ImpulseCaptureResult | null>(null);
  const receiveCapture = useCallback((capture: ImpulseCaptureResult) => setResponseCapture(capture), []);
  const [reducedMotion, setReducedMotion] = useState(() => window.matchMedia('(prefers-reduced-motion: reduce)').matches);
  const [energyEnabled, setEnergyEnabled] = useState(() => !window.matchMedia('(prefers-reduced-motion: reduce)').matches);
  const [energyLevels, setEnergyLevels] = useState<EnergyLevels>({});

  const loopInspection = useMemo(() => selectedNode
    ? inspectFeedbackLoops(nodes, edges, { nodeId: selectedNode.id })
    : selectedEdge ? inspectFeedbackLoops(nodes, edges, { edgeId: selectedEdge.id }) : null,
  [edges, nodes, selectedEdge, selectedNode]);
  const normalizedLoopIndex = loopInspection?.loops.length ? activeLoopIndex % loopInspection.loops.length : 0;
  const loopDecoratedGraph = useMemo(() => decorateFeedbackLoops(nodes, edges, loopInspection, normalizedLoopIndex), [edges, loopInspection, nodes, normalizedLoopIndex]);
  const displayedGraph = useMemo(() => decorateEnergy(loopDecoratedGraph.nodes, loopDecoratedGraph.edges, energyLevels), [energyLevels, loopDecoratedGraph]);

  useEffect(() => {
    const preference = window.matchMedia('(prefers-reduced-motion: reduce)');
    const update = () => { setReducedMotion(preference.matches); if (preference.matches) setEnergyEnabled(false); };
    preference.addEventListener('change', update);
    return () => preference.removeEventListener('change', update);
  }, []);

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

  const applyGraph = useCallback((state: { nodes: Node<PatchNodeData>[]; edges: Edge[] }) => {
    setNodes(state.nodes); setEdges(state.edges); setSelectedNode(null); setSelectedEdge(null);
  }, [setEdges, setNodes]);

  const publishRuntimeParameters = useCallback((state: { nodes: Node<PatchNodeData>[] }) => {
    for (const node of state.nodes) for (const parameter of node.data.parameters)
      if (node.data.runtimeBound) {
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
    const protectedNode = nodes.find((node) => node.selected && (node.data.type === 'stereo-input' || node.data.type === 'stereo-output'));
    if (protectedNode) { setGraphStatus({ kind: 'error', message: `${protectedNode.data.label} is required and cannot be deleted.` }); return; }
    const before = { nodes, edges }; const after = deleteSelected(nodes, edges); if (after.nodes.length === nodes.length && after.edges.length === edges.length) return;
    applyGraph(after); setGraphHistory((history) => commitGraphEdit(history, 'Delete selection', before, after)); setGraphStatus({ kind: 'ok', message: 'DELETED SELECTION + INCIDENT CABLES / UNDO AVAILABLE' });
  }, [applyGraph, edges, nodes]);

  const undoGraph = useCallback(() => { const result = undoGraphEdit(graphHistory); if (!result.edit) return; applyGraph(result.edit.before); publishRuntimeParameters(result.edit.before); setGraphHistory(result.history); setGraphStatus({ kind: 'ok', message: `UNDID ${result.edit.label.toUpperCase()}` }); }, [applyGraph, graphHistory, publishRuntimeParameters]);
  const redoGraph = useCallback(() => { const result = redoGraphEdit(graphHistory); if (!result.edit) return; applyGraph(result.edit.after); publishRuntimeParameters(result.edit.after); setGraphHistory(result.history); setGraphStatus({ kind: 'ok', message: `REDID ${result.edit.label.toUpperCase()}` }); }, [applyGraph, graphHistory, publishRuntimeParameters]);

  const commitConnection = useCallback((connection: Connection) => {
    const before = { nodes, edges }; const decision = decideConnection(nodes, edges, connection);
    if (decision.kind === 'invalid') { setGraphStatus({ kind: 'error', message: decision.message }); return; }
    if (decision.kind === 'occupied') {
      setPendingConnection(connection); setGraphStatus({ kind: 'error', message: 'INPUT OCCUPIED / REPLACE ITS CABLE OR INSERT +' }); return;
    }
    const after = connectGraph(before, connection); applyGraph(after);
    setGraphHistory((history) => commitGraphEdit(history, 'Create cable', before, after)); setGraphStatus({ kind: 'ok', message: 'CONNECTED MONO AUDIO CABLE' });
  }, [applyGraph, edges, nodes]);

  const resolveOccupied = useCallback((action: 'replace' | 'sum' | 'cancel') => {
    const connection = pendingConnection; setPendingConnection(null); if (!connection || action === 'cancel') { setGraphStatus(null); return; }
    const before = { nodes, edges }; const after = action === 'replace' ? connectGraph(before, connection, true) : insertSumForOccupiedInput(before, connection);
    applyGraph(after); setGraphHistory((history) => commitGraphEdit(history, action === 'replace' ? 'Replace cable' : 'Insert +', before, after));
    setGraphStatus({ kind: 'ok', message: action === 'replace' ? 'REPLACED OCCUPIED INPUT CABLE' : 'INSERTED + AND REWIRED BOTH SOURCES' });
  }, [applyGraph, edges, nodes, pendingConnection]);

  const applyParameter = useCallback((nodeId: string, parameterId: string, value: number) => {
    const update = (node: Node<PatchNodeData>) => node.id !== nodeId ? node : {
      ...node,
      data: {
        ...node.data,
        parameters: node.data.parameters.map((parameter) => parameter.id === parameterId ? { ...parameter, value } : parameter),
      },
    };
    setNodes((current) => current.map(update));
    setSelectedNode((current) => current ? update(current) : null);
    if (nodes.find((node) => node.id === nodeId)?.data.runtimeBound) {
      try { void callNative('setRuntimeParameter', nodeId, parameterId, value).catch(() => undefined); }
      catch { /* browser prototype has no native bridge */ }
    }
  }, [nodes, setNodes]);

  const resetReference = useCallback(() => {
    const fresh = createFlowModel(snapshot);
    const before = { nodes, edges };
    applyGraph(fresh);
    publishRuntimeParameters(fresh);
    setGraphHistory((history) => commitGraphEdit(history, 'Reset patch', before, fresh));
    setPendingConnection(null);
    requestAnimationFrame(() => void fitView({ padding: 0.16, minZoom: 0.58, maxZoom: 1.1 }));
  }, [applyGraph, edges, fitView, nodes, publishRuntimeParameters, snapshot]);

  const copySelection = useCallback(() => {
    const copied = copySelectedGraph({ nodes, edges });
    if (!copied) { setGraphStatus({ kind: 'error', message: 'SELECT ONE OR MORE NON-I/O BLOCKS TO COPY' }); return; }
    clipboard.current = copied; pasteCount.current = 0;
    void navigator.clipboard?.writeText(JSON.stringify({ format: 'reverb-playground-subgraph-v1', ...copied })).catch(() => undefined);
    setGraphStatus({ kind: 'ok', message: `COPIED ${copied.nodes.length} BLOCK${copied.nodes.length === 1 ? '' : 'S'} + ${copied.edges.length} INTERNAL CABLE${copied.edges.length === 1 ? '' : 'S'}` });
  }, [edges, nodes]);

  const pasteSelection = useCallback(() => {
    if (!clipboard.current) { setGraphStatus({ kind: 'error', message: 'GRAPH CLIPBOARD IS EMPTY' }); return; }
    const before = { nodes, edges };
    const after = pasteGraph(before, clipboard.current, 40 * ++pasteCount.current);
    applyGraph(after); setGraphHistory((history) => commitGraphEdit(history, 'Paste selection', before, after));
    setGraphStatus({ kind: 'ok', message: `PASTED ${clipboard.current.nodes.length} BLOCK${clipboard.current.nodes.length === 1 ? '' : 'S'} / NEW IDS ASSIGNED` });
  }, [applyGraph, edges, nodes]);

  const handleSelection = useCallback(({ nodes: selectedNodes, edges: selectedEdges }: OnSelectionChangeParams) => {
    setSelectedNode((selectedNodes[0] as Node<PatchNodeData> | undefined) ?? null);
    setSelectedEdge(selectedEdges[0] ?? null);
    setActiveLoopIndex(0);
  }, []);

  useEffect(() => {
    const handleKey = (event: KeyboardEvent) => {
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
      if ((event.key === 'Delete' || event.key === 'Backspace') && !(event.target instanceof HTMLInputElement)) { event.preventDefault(); removeSelection(); return; }
      if (event.key.toLowerCase() === 'r' && !(event.target instanceof HTMLInputElement))
        resetReference();
    };
    window.addEventListener('keydown', handleKey);
    return () => window.removeEventListener('keydown', handleKey);
  }, [copySelection, pasteSelection, redoGraph, removeSelection, resetReference, undoGraph]);

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
      const json = writePatchJson(nodes, edges, viewport);
      parsePatchJson(json, snapshot);
      const url = URL.createObjectURL(new Blob([json], { type: 'application/json' }));
      const link = document.createElement('a');
      link.href = url;
      link.download = 'barr-reference.rvp.json';
      document.body.appendChild(link);
      link.click();
      link.remove();
      window.setTimeout(() => URL.revokeObjectURL(url), 0);
      setGraphHistory((history) => markHistoryClean(history, { nodes, edges }));
      setFileStatus({ kind: 'ok', message: 'SAVED BARR-REFERENCE.RVP.JSON / SCHEMA V1' });
    } catch (reason) {
      setFileStatus({ kind: 'error', message: reason instanceof Error ? reason.message : 'Patch save failed' });
    }
  }, [edges, nodes, snapshot, viewport]);

  const loadPatch = useCallback(async (file: File) => {
    try {
      const loaded = parsePatchJson(await file.text(), snapshot);
      setNodes(loaded.nodes);
      setEdges(loaded.edges);
      setViewport(loaded.viewport);
      await setFlowViewport(loaded.viewport);
      for (const node of loaded.nodes) {
        for (const parameter of node.data.parameters)
          if (node.data.runtimeBound) await callNative('setRuntimeParameter', node.id, parameter.id, parameter.value);
      }
      setSelectedNode(null);
      setSelectedEdge(null);
      setGraphHistory(emptyGraphHistory(loaded));
      setPendingConnection(null);
      setFileStatus({ kind: 'ok', message: `LOADED ${file.name.toUpperCase()} / SCHEMA V1` });
    } catch (reason) {
      setFileStatus({ kind: 'error', message: reason instanceof Error ? reason.message : 'Patch load failed' });
    }
  }, [setEdges, setFlowViewport, setNodes, snapshot]);

  const teachingKey = selectedNode?.id ?? 'overview';
  const showTeaching = teachingEnabled && dismissedTeaching !== teachingKey;
  const toggleTeaching = useCallback(() => {
    setTeachingEnabled((current) => {
      const next = !current;
      try { window.localStorage.setItem('reverb-playground-teaching', next ? 'on' : 'off'); } catch { /* preference remains session-local */ }
      return next;
    });
  }, []);

  return (
    <main className="editor-shell">
      <header className="editor-header">
        <div>
          <div className="eyebrow">SCHEMATIC EDITOR / BARR REFERENCE</div>
          <h1>Patch architecture</h1>
        </div>
        <div className="header-runtime">
          <button className="energy-toggle" type="button" aria-pressed={energyEnabled} disabled={reducedMotion} onClick={() => setEnergyEnabled((value) => !value)} title={reducedMotion ? 'Disabled by the operating-system reduced-motion preference' : 'Toggle measured node and cable energy'}>
            ENERGY {reducedMotion ? 'REDUCED' : energyEnabled ? 'ON' : 'OFF'}
          </button>
          <div className="header-status" aria-label="Audition status">
            <span className="status-dot" aria-hidden="true" />
            <span>RUNTIME BOUND / {snapshot.sampleRate > 0 ? `${(snapshot.sampleRate / 1000).toFixed(1)} kHz` : 'awaiting audio'}</span>
          </div>
        </div>
      </header>

      <MeasurementBar sampleRate={snapshot.sampleRate} onCapture={receiveCapture} />

      {responseCapture ? <ResponseViewer key={responseCapture.generation} capture={responseCapture} onClose={() => setResponseCapture(null)} /> : null}

      <section className="workspace">
        <aside className="module-library" aria-label="Module library">
          <div className="pane-heading">
            <span>MODULES</span>
            <span className="pane-count">{moduleDefinitions.length}</span>
          </div>
          <p className="pane-help">Click a primitive to place it near the canvas center. Audio cables are mono.</p>
          {modules.map((section) => (
            <section className="module-group" key={section.group}>
              <h2>{section.group}</h2>
              {section.items.map((item) => (
                <button className="module-item" key={item.type} type="button" onClick={() => addModule(item.type)}>
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
            <div>
              <strong>REFERENCE.graph</strong>
              <span>{nodes.length} blocks / {edges.length} cables</span>
            </div>
            <div className="canvas-actions">
              <span>{Math.round(viewport.zoom * 100)}%</span>
              <span className={`clean-state ${isHistoryClean(graphHistory, { nodes, edges }) ? 'is-clean' : 'is-dirty'}`}>
                {isHistoryClean(graphHistory, { nodes, edges }) ? 'SAVED' : 'UNSAVED'}
              </span>
              <button type="button" onClick={savePatch}>SAVE PATCH</button>
              <button type="button" onClick={() => loadInput.current?.click()}>LOAD PATCH</button>
              <button type="button" disabled={!graphHistory.undo.length} onClick={undoGraph}>UNDO</button>
              <button type="button" disabled={!graphHistory.redo.length} onClick={redoGraph}>REDO</button>
              <button type="button" onClick={copySelection}>COPY</button>
              <button type="button" disabled={!clipboard.current} onClick={pasteSelection}>PASTE</button>
              <button type="button" onClick={removeSelection}>DELETE</button>
              <button type="button" onClick={resetReference}>RESET VIEW COPY</button>
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
            </div>
          </div>
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
            <div className="connection-offer" role="dialog" aria-label="Occupied input options">
              <strong>INPUT ALREADY HAS A CABLE</strong>
              <span>Replace it, or insert an explicit Sum (+) block to preserve both sources.</span>
              <div><button type="button" onClick={() => resolveOccupied('sum')}>INSERT +</button><button type="button" onClick={() => resolveOccupied('replace')}>REPLACE CABLE</button><button type="button" onClick={() => resolveOccupied('cancel')}>CANCEL</button></div>
            </div>
          ) : null}
          <div className="flow-wrap">
            <ReactFlow
              nodes={displayedGraph.nodes}
              edges={displayedGraph.edges}
              nodeTypes={{ patchNode: PatchNode }}
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
              onViewportChange={setViewport}
              fitView
              fitViewOptions={{ padding: 0.16, minZoom: 0.58, maxZoom: 1.1 }}
              minZoom={0.4}
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
              aria-label="Barr reference patch graph"
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

        <aside className="inspector" aria-label="Inspector">
          <div className="pane-heading"><span>INSPECTOR</span><button className="teaching-toggle" type="button" aria-pressed={teachingEnabled} onClick={toggleTeaching}>LEARN {teachingEnabled ? 'ON' : 'OFF'}</button></div>
          {selectedNode ? (
            <div className="inspector-content" key={selectedNode.id}>
              <div className="selection-kicker">SELECTED BLOCK</div>
              <h2>{selectedNode.data.label}</h2>
              <code>{selectedNode.id}</code>
              {loopInspection ? <LoopInspector inspection={loopInspection} activeIndex={normalizedLoopIndex} onActiveIndex={setActiveLoopIndex} /> : null}
              <dl className="property-list">
                <div><dt>TYPE</dt><dd>{selectedNode.data.type}</dd></div>
                <div><dt>ROLE</dt><dd>{selectedNode.data.role}</dd></div>
                <div><dt>PORTS</dt><dd>{selectedNode.data.ports.length} mono</dd></div>
              </dl>
              <h3>PARAMETERS</h3>
              {selectedNode.data.parameters.length ? selectedNode.data.parameters.map((parameter) => (
                <div className="parameter-card" key={parameter.id}>
                  <span>{parameter.id.toUpperCase()}</span>
                  <label className="parameter-value">
                    <span className="sr-only">{`${selectedNode.data.label} ${parameter.id} numeric value`}</span>
                    <input
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
                    />
                    <strong>{parameter.unit === 'milliseconds' ? 'ms' : parameter.unit === 'hertz' ? 'Hz' : ''}</strong>
                  </label>
                  <small>{parameter.unit}</small>
                  <input
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
                  />
                </div>
              )) : <p className="empty-parameters">No editable parameters.</p>}
              <div className="history-actions">
                <button type="button" disabled={!graphHistory.undo.length} onClick={undoGraph}>UNDO</button>
                <button type="button" disabled={!graphHistory.redo.length} onClick={redoGraph}>REDO</button>
              </div>
              <div className="selection-note">{selectedNode.data.runtimeBound ? `Live value from native DSP runtime contract v${snapshot.contractVersion}.` : 'Draft graph block. Saved values are editable; audio compilation arrives in a later milestone.'}</div>
              {showTeaching ? <TeachingCard topic={teachingTopicFor(selectedNode.id)} onDismiss={() => setDismissedTeaching(teachingKey)} onResearch={() => setResearchOpen(true)} /> : null}
            </div>
          ) : selectedEdge ? (
            <div className="inspector-content">
              <div className="selection-kicker">SELECTED CABLE</div>
              <h2>Audio connection</h2>
              <code>{selectedEdge.id}</code>
              {loopInspection ? <LoopInspector inspection={loopInspection} activeIndex={normalizedLoopIndex} onActiveIndex={setActiveLoopIndex} /> : null}
              <dl className="property-list">
                <div><dt>FROM</dt><dd>{selectedEdge.source}</dd></div>
                <div><dt>TO</dt><dd>{selectedEdge.target}</dd></div>
                <div><dt>SIGNAL</dt><dd>AUDIO / SOLID</dd></div>
              </dl>
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
              {showTeaching ? <TeachingCard topic={teachingTopicFor()} onDismiss={() => setDismissedTeaching(teachingKey)} onResearch={() => setResearchOpen(true)} /> : null}
            </div>
          )}
        </aside>
      </section>
      {researchOpen ? (
        <div className="research-backdrop" role="presentation" onMouseDown={() => setResearchOpen(false)}>
          <section className="research-reader" role="dialog" aria-modal="true" aria-label="Keith Barr architecture research" onMouseDown={(event) => event.stopPropagation()}>
            <header><div><span>OFFLINE RESEARCH / DOCUMENTED SOURCES</span><h2>Keith Barr reverb architectures</h2></div><button type="button" aria-label="Close architecture research" onClick={() => setResearchOpen(false)}>CLOSE ×</button></header>
            <pre>{researchText}</pre>
          </section>
        </div>
      ) : null}
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
