import { Handle, Position, type NodeProps } from '@xyflow/react';
import type { PatchNodeData } from './graph';
import { PitchShiftVisualization } from './PitchShiftVisualization';
import { moduleSignalBadge, visibleModuleLabel, vocabularyFor } from './moduleVocabulary';

const prettyUnit = (unit: string) => unit === 'milliseconds' ? 'ms' : unit === 'hertz' ? 'Hz' : unit === 'semitones' ? 'st' : '';

export function PatchNode({ data, selected }: NodeProps & { data: PatchNodeData }) {
  const inputs = data.ports.filter((port) => port.direction === 'input');
  const outputs = data.ports.filter((port) => port.direction === 'output');
  const reversed = data.orientation === 'reverse';
  const inputSide = reversed ? Position.Right : Position.Left;
  const outputSide = reversed ? Position.Left : Position.Right;
  const hierarchy = data.hierarchyPresentation;
  const isHierarchyParent = Boolean(hierarchy && data.type === 'compound-summary');
  const topFor = (index: number, total: number) => `${((index + 1) / (total + 1)) * 100}%`;

  if (isHierarchyParent && hierarchy) return (
    <article className={`patch-node hierarchy-parent${reversed ? ' is-reversed' : ''}${selected ? ' is-selected' : ''}`}
      aria-label={`${data.label}, ${inputs.length} inputs, ${outputs.length} outputs. Double-click to open nested schematic.`}>
      {inputs.map((port, index) => <span className={`hierarchy-port-anchor ${reversed ? 'hierarchy-port-out' : 'hierarchy-port-in'}`} key={port.id} style={{ top: topFor(index, inputs.length) }}>
        <Handle className={`port port-${port.signal}`} id={port.id} position={inputSide} type="target" />
        <i>{(hierarchy.ports.find((item) => item.id === port.id)?.name ?? port.id).replace(/^IN\s*/i, '')}</i>
      </span>)}
      {outputs.map((port, index) => <span className={`hierarchy-port-anchor ${reversed ? 'hierarchy-port-in' : 'hierarchy-port-out'}`} key={port.id} style={{ top: topFor(index, outputs.length) }}>
        <i>{(hierarchy.ports.find((item) => item.id === port.id)?.name ?? port.id).replace(/^OUT\s*/i, '')}</i>
        <Handle className={`port port-${port.signal}`} id={port.id} position={outputSide} type="source" />
      </span>)}
      <span className={`hierarchy-side-label ${reversed ? 'hierarchy-side-out' : 'hierarchy-side-in'}`}>IN</span>
      <span className={`hierarchy-side-label ${reversed ? 'hierarchy-side-in' : 'hierarchy-side-out'}`}>OUT</span>
      <div className="node-kicker">{hierarchy.kind === 'compound' ? 'COMPOUND' : 'SUBPATCH'}</div>
      <h3>{data.label}</h3>
      <div className="hierarchy-topology">{hierarchy.kind === 'compound' ? '16 GAINS · 12 SUMS' : `${hierarchy.memberNodeIds.length} PRIMITIVES`}</div>
      <div className="hierarchy-open-cue">↳ OPEN SCHEMATIC</div>
      <div className="energy-meter" aria-hidden="true"><i /><i /><i /><i /><i /></div>
    </article>
  );

  return (
    <article className={`patch-node role-${data.role}${reversed ? ' is-reversed' : ''}${selected ? ' is-selected' : ''}`} aria-label={`${visibleModuleLabel(data)} ${moduleSignalBadge(data)}. ${reversed ? 'Right-to-left presentation; signal direction unchanged. ' : ''}${vocabularyFor(data)?.audibleRole ?? data.type}`}>
      {inputs.map((port, index) => (
        <span className={`hierarchy-port-anchor ${reversed ? 'hierarchy-port-out' : 'hierarchy-port-in'}`} key={port.id} style={{ top: topFor(index, inputs.length) }}>
          <Handle className={`port port-${port.signal}`} id={port.id} position={inputSide} type="target" />
          {isHierarchyParent && hierarchy ? <i>{hierarchy.ports.find((item) => item.id === port.id)?.name ?? port.id}</i> : null}
        </span>
      ))}
      <div className="node-kicker">{data.role}</div>
      <h3>{visibleModuleLabel(data)}</h3>
      <div className="node-type">{data.type}</div>
      <div className="node-signal">{moduleSignalBadge(data)}</div>
      {data.hierarchyBoundary ? <div className="hierarchy-node-summary"><b>{data.hierarchyBoundary.direction.toUpperCase()}</b><span>STABLE BOUNDARY</span></div> : null}
      {data.type === 'envelope-follower' ? <div className="signal-operation">AUDIO → ENVELOPE 0…1</div> : null}
      {data.type === 'hold-gate' ? <div className="signal-operation">AUDIO × CONTROL GATE</div> : null}
      {data.type === 'pitch-shift' ? <div className="signal-operation">MUSICAL RATIO · NOT FIXED HZ / DOPPLER</div> : null}
      <div className="energy-meter" aria-hidden="true"><i /><i /><i /><i /><i /></div>
      {data.type === 'pitch-shift' ? <PitchShiftVisualization parameters={data.parameters} reducedMotion={false} compact /> : null}
      {data.controlPreview ? (
        <div className="control-preview" aria-label={`${data.controlPreview.label} control preview ${data.controlPreview.value.toFixed(2)}`}>
          <span>{data.controlPreview.label}</span>
          <output>{data.controlPreview.value >= 0 ? '+' : ''}{data.controlPreview.value.toFixed(2)}</output>
          <i style={{ left: `${(data.controlPreview.value + 1) * 50}%` }} />
        </div>
      ) : null}
      {data.parameters.length > 0 && (
        <div className="node-parameter">
          {data.parameters[0].value.toLocaleString(undefined, { maximumFractionDigits: 2 })} {prettyUnit(data.parameters[0].unit)}
        </div>
      )}
      {outputs.map((port, index) => (
        <span className={`hierarchy-port-anchor ${reversed ? 'hierarchy-port-in' : 'hierarchy-port-out'}`} key={port.id} style={{ top: topFor(index, outputs.length) }}>
          {isHierarchyParent && hierarchy ? <i>{hierarchy.ports.find((item) => item.id === port.id)?.name ?? port.id}</i> : null}
        <Handle className={`port port-${port.signal}`} id={port.id} position={outputSide} type="source" />
        </span>
      ))}
    </article>
  );
}
