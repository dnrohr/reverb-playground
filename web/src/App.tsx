import { useCallback, useEffect, useMemo, useState } from 'react';
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
  const { fitView } = useReactFlow();
  const initial = useMemo(() => createFlowModel(snapshot), [snapshot]);
  const [nodes, setNodes, onNodesChange] = useNodesState<Node<PatchNodeData>>(initial.nodes);
  const [edges, setEdges, onEdgesChange] = useEdgesState(initial.edges);
  const [selectedNode, setSelectedNode] = useState<Node<PatchNodeData> | null>(null);
  const [selectedEdge, setSelectedEdge] = useState<Edge | null>(null);
  const [viewport, setViewport] = useState<Viewport>({ x: 0, y: 0, zoom: 1 });

  const resetReference = useCallback(() => {
    const fresh = createFlowModel(snapshot);
    setNodes(fresh.nodes);
    setEdges(fresh.edges);
    setSelectedNode(null);
    setSelectedEdge(null);
    requestAnimationFrame(() => void fitView({ padding: 0.16, minZoom: 0.58, maxZoom: 1.1 }));
  }, [fitView, setEdges, setNodes, snapshot]);

  const handleSelection = useCallback(({ nodes: selectedNodes, edges: selectedEdges }: OnSelectionChangeParams) => {
    setSelectedNode((selectedNodes[0] as Node<PatchNodeData> | undefined) ?? null);
    setSelectedEdge(selectedEdges[0] ?? null);
  }, []);

  useEffect(() => {
    const handleKey = (event: KeyboardEvent) => {
      if (event.key.toLowerCase() === 'r' && !(event.target instanceof HTMLInputElement))
        resetReference();
    };
    window.addEventListener('keydown', handleKey);
    return () => window.removeEventListener('keydown', handleKey);
  }, [resetReference]);

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
              <button type="button" onClick={resetReference}>RESET VIEW COPY</button>
            </div>
          </div>
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
                  <strong>{formatValue(parameter.value, parameter.unit)}</strong>
                  <small>{parameter.unit}</small>
                </div>
              )) : <p className="empty-parameters">No editable parameters.</p>}
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
