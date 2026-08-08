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
import { commitGraphEdit, emptyGraphHistory, redoGraphEdit, undoGraphEdit } from './graphHistory';
import { connectGraph, decideConnection, insertSumForOccupiedInput } from './connectionEditing';
import { PatchNode } from './PatchNode';
import { callNative } from './nativeBridge';
import { parsePatchJson, writePatchJson } from './patchPersistence';
import researchText from '../../docs/keith-barr-reverb-architectures.md?raw';
import { teachingTopicFor, type TeachingTopic } from './teaching';
import {
  commitParameterEdit as commitHistoryEdit,
  redoParameterEdit as takeRedo,
  undoParameterEdit as takeUndo,
  type ParameterEdit,
} from './parameterHistory';

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

function Editor({ snapshot }: { snapshot: RuntimeSnapshot }) {
  const { fitView, setViewport: setFlowViewport, screenToFlowPosition } = useReactFlow();
  const initial = useMemo(() => createFlowModel(snapshot), [snapshot]);
  const [nodes, setNodes, onNodesChange] = useNodesState<Node<PatchNodeData>>(initial.nodes);
  const [edges, setEdges, onEdgesChange] = useEdgesState(initial.edges);
  const [selectedNode, setSelectedNode] = useState<Node<PatchNodeData> | null>(null);
  const [selectedEdge, setSelectedEdge] = useState<Edge | null>(null);
  const [viewport, setViewport] = useState<Viewport>({ x: 0, y: 0, zoom: 1 });
  const [undoStack, setUndoStack] = useState<ParameterEdit[]>([]);
  const [redoStack, setRedoStack] = useState<ParameterEdit[]>([]);
  const activeEdit = useRef<ParameterEdit | null>(null);
  const loadInput = useRef<HTMLInputElement | null>(null);
  const [fileStatus, setFileStatus] = useState<{ kind: 'ok' | 'error'; message: string } | null>(null);
  const [teachingEnabled, setTeachingEnabled] = useState(() => {
    try { return window.localStorage.getItem('reverb-playground-teaching') !== 'off'; } catch { return true; }
  });
  const [dismissedTeaching, setDismissedTeaching] = useState<string | null>(null);
  const [researchOpen, setResearchOpen] = useState(false);
  const [graphHistory, setGraphHistory] = useState(emptyGraphHistory);
  const [graphStatus, setGraphStatus] = useState<{ kind: 'ok' | 'error'; message: string } | null>(null);
  const [pendingConnection, setPendingConnection] = useState<Connection | null>(null);

  const applyGraph = useCallback((state: { nodes: Node<PatchNodeData>[]; edges: Edge[] }) => {
    setNodes(state.nodes); setEdges(state.edges); setSelectedNode(null); setSelectedEdge(null);
  }, [setEdges, setNodes]);

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

  const undoGraph = useCallback(() => { const result = undoGraphEdit(graphHistory); if (!result.edit) return; applyGraph(result.edit.before); setGraphHistory(result.history); setGraphStatus({ kind: 'ok', message: `UNDID ${result.edit.label.toUpperCase()}` }); }, [applyGraph, graphHistory]);
  const redoGraph = useCallback(() => { const result = redoGraphEdit(graphHistory); if (!result.edit) return; applyGraph(result.edit.after); setGraphHistory(result.history); setGraphStatus({ kind: 'ok', message: `REDID ${result.edit.label.toUpperCase()}` }); }, [applyGraph, graphHistory]);

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
    if (nodes.find((node) => node.id === nodeId)?.data.runtimeBound) void callNative('setRuntimeParameter', nodeId, parameterId, value);
  }, [nodes, setNodes]);

  const undo = useCallback(() => {
    const result = takeUndo({ undo: undoStack, redo: redoStack });
    const edit = result.edit;
    if (!edit) return;
    applyParameter(edit.nodeId, edit.parameterId, edit.before);
    setUndoStack(result.history.undo);
    setRedoStack(result.history.redo);
  }, [applyParameter, redoStack, undoStack]);

  const redo = useCallback(() => {
    const result = takeRedo({ undo: undoStack, redo: redoStack });
    const edit = result.edit;
    if (!edit) return;
    applyParameter(edit.nodeId, edit.parameterId, edit.after);
    setUndoStack(result.history.undo);
    setRedoStack(result.history.redo);
  }, [applyParameter, redoStack, undoStack]);

  const resetReference = useCallback(() => {
    const fresh = createFlowModel(snapshot);
    setNodes(fresh.nodes);
    setEdges(fresh.edges);
    setSelectedNode(null);
    setSelectedEdge(null);
    setUndoStack([]);
    setRedoStack([]);
    setGraphHistory(emptyGraphHistory());
    setPendingConnection(null);
    requestAnimationFrame(() => void fitView({ padding: 0.16, minZoom: 0.58, maxZoom: 1.1 }));
  }, [fitView, setEdges, setNodes, snapshot]);

  const handleSelection = useCallback(({ nodes: selectedNodes, edges: selectedEdges }: OnSelectionChangeParams) => {
    setSelectedNode((selectedNodes[0] as Node<PatchNodeData> | undefined) ?? null);
    setSelectedEdge(selectedEdges[0] ?? null);
  }, []);

  useEffect(() => {
    const handleKey = (event: KeyboardEvent) => {
      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'z') {
        event.preventDefault();
        if (event.shiftKey) redo(); else undo();
        return;
      }
      if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'y') {
        event.preventDefault();
        redo();
        return;
      }
      if ((event.key === 'Delete' || event.key === 'Backspace') && !(event.target instanceof HTMLInputElement)) { event.preventDefault(); removeSelection(); return; }
      if (event.key.toLowerCase() === 'r' && !(event.target instanceof HTMLInputElement))
        resetReference();
    };
    window.addEventListener('keydown', handleKey);
    return () => window.removeEventListener('keydown', handleKey);
  }, [redo, removeSelection, resetReference, undo]);

  const beginParameterEdit = useCallback((nodeId: string, parameterId: string, before: number) => {
    activeEdit.current = { nodeId, parameterId, before, after: before };
  }, []);

  const changeParameter = useCallback((nodeId: string, parameterId: string, value: number) => {
    applyParameter(nodeId, parameterId, value);
    if (activeEdit.current?.nodeId === nodeId && activeEdit.current.parameterId === parameterId)
      activeEdit.current.after = value;
  }, [applyParameter]);

  const commitParameterEdit = useCallback(() => {
    const edit = activeEdit.current;
    activeEdit.current = null;
    if (!edit) return;
    const result = commitHistoryEdit({ undo: undoStack, redo: redoStack }, edit);
    setUndoStack(result.undo);
    setRedoStack(result.redo);
  }, [redoStack, undoStack]);

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
      setUndoStack([]);
      setRedoStack([]);
      setGraphHistory(emptyGraphHistory());
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
        <div className="header-status" aria-label="Audition status">
          <span className="status-dot" aria-hidden="true" />
          <span>RUNTIME BOUND / {snapshot.sampleRate > 0 ? `${(snapshot.sampleRate / 1000).toFixed(1)} kHz` : 'awaiting audio'}</span>
        </div>
      </header>

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
              <button type="button" onClick={savePatch}>SAVE PATCH</button>
              <button type="button" onClick={() => loadInput.current?.click()}>LOAD PATCH</button>
              <button type="button" disabled={!graphHistory.undo.length} onClick={undoGraph}>UNDO STRUCTURE</button>
              <button type="button" disabled={!graphHistory.redo.length} onClick={redoGraph}>REDO</button>
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
              nodes={nodes}
              edges={edges}
              nodeTypes={{ patchNode: PatchNode }}
              onNodesChange={onNodesChange}
              onEdgesChange={onEdgesChange}
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
                <button type="button" disabled={!undoStack.length} onClick={undo}>UNDO</button>
                <button type="button" disabled={!redoStack.length} onClick={redo}>REDO</button>
              </div>
              <div className="selection-note">{selectedNode.data.runtimeBound ? `Live value from native DSP runtime contract v${snapshot.contractVersion}.` : 'Draft graph block. Saved values are editable; audio compilation arrives in a later milestone.'}</div>
              {showTeaching ? <TeachingCard topic={teachingTopicFor(selectedNode.id)} onDismiss={() => setDismissedTeaching(teachingKey)} onResearch={() => setResearchOpen(true)} /> : null}
            </div>
          ) : selectedEdge ? (
            <div className="inspector-content">
              <div className="selection-kicker">SELECTED CABLE</div>
              <h2>Audio connection</h2>
              <code>{selectedEdge.id}</code>
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
              <kbd>DELETE</kbd><span>remove from UI copy</span>
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
