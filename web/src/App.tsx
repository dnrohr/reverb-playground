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
  type Node,
  type OnSelectionChangeParams,
  type Viewport,
} from '@xyflow/react';
import { createFlowModel, parseRuntimeSnapshot, type PatchNodeData, type RuntimeSnapshot } from './graph';
import { PatchNode } from './PatchNode';
import { callNative } from './nativeBridge';
import { parsePatchJson, writePatchJson } from './patchPersistence';
import {
  commitParameterEdit as commitHistoryEdit,
  redoParameterEdit as takeRedo,
  undoParameterEdit as takeUndo,
  type ParameterEdit,
} from './parameterHistory';

const modules = [
  { group: 'I/O', items: ['Stereo Input', 'Stereo Output'] },
  { group: 'SIGNAL', items: ['Gain / Invert', 'Sum', 'Delay', 'Allpass', 'Low-pass'] },
  { group: 'MODULATION', items: ['LFO', 'Scale + Offset'] },
];

function formatValue(value: number, unit: string) {
  if (unit === 'milliseconds') return `${value.toFixed(2)} ms`;
  if (unit === 'hertz') return `${value.toLocaleString()} Hz`;
  return value.toFixed(2);
}

function Editor({ snapshot }: { snapshot: RuntimeSnapshot }) {
  const { fitView, setViewport: setFlowViewport } = useReactFlow();
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
    void callNative('setRuntimeParameter', nodeId, parameterId, value);
  }, [setNodes]);

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
      if (event.key.toLowerCase() === 'r' && !(event.target instanceof HTMLInputElement))
        resetReference();
    };
    window.addEventListener('keydown', handleKey);
    return () => window.removeEventListener('keydown', handleKey);
  }, [redo, resetReference, undo]);

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
          await callNative('setRuntimeParameter', node.id, parameter.id, parameter.value);
      }
      setSelectedNode(null);
      setSelectedEdge(null);
      setUndoStack([]);
      setRedoStack([]);
      setFileStatus({ kind: 'ok', message: `LOADED ${file.name.toUpperCase()} / SCHEMA V1` });
    } catch (reason) {
      setFileStatus({ kind: 'error', message: reason instanceof Error ? reason.message : 'Patch load failed' });
    }
  }, [setEdges, setFlowViewport, setNodes, snapshot]);

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
            <span className="pane-count">9</span>
          </div>
          <p className="pane-help">Curated reverb vocabulary. Construction unlocks in M3.</p>
          {modules.map((section) => (
            <section className="module-group" key={section.group}>
              <h2>{section.group}</h2>
              {section.items.map((item) => (
                <button className={`module-item${section.group === 'MODULATION' ? ' module-control' : ''}`} key={item} type="button">
                  <span className="module-glyph" aria-hidden="true" />
                  <span>{item}</span>
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
              <span>{snapshot.nodes.length} blocks / {snapshot.connections.length} cables</span>
            </div>
            <div className="canvas-actions">
              <span>{Math.round(viewport.zoom * 100)}%</span>
              <button type="button" onClick={savePatch}>SAVE PATCH</button>
              <button type="button" onClick={() => loadInput.current?.click()}>LOAD PATCH</button>
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
          <div className="flow-wrap">
            <ReactFlow
              nodes={nodes}
              edges={edges}
              nodeTypes={{ patchNode: PatchNode }}
              onNodesChange={onNodesChange}
              onEdgesChange={onEdgesChange}
              onSelectionChange={handleSelection}
              onViewportChange={setViewport}
              fitView
              fitViewOptions={{ padding: 0.16, minZoom: 0.58, maxZoom: 1.1 }}
              minZoom={0.4}
              maxZoom={1.8}
              deleteKeyCode={['Backspace', 'Delete']}
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
          <div className="pane-heading"><span>INSPECTOR</span><span className="inspector-state">LIVE COPY</span></div>
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
              <div className="selection-note">Live value from native DSP runtime contract v{snapshot.contractVersion}.</div>
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
            </div>
          )}
        </aside>
      </section>
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
