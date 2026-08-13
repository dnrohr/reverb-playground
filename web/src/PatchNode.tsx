import { Handle, Position, type NodeProps } from '@xyflow/react';
import type { PatchNodeData } from './graph';

const prettyUnit = (unit: string) => unit === 'milliseconds' ? 'ms' : unit === 'hertz' ? 'Hz' : '';

export function PatchNode({ data, selected }: NodeProps & { data: PatchNodeData }) {
  const inputs = data.ports.filter((port) => port.direction === 'input');
  const outputs = data.ports.filter((port) => port.direction === 'output');
  const topFor = (index: number, total: number) => `${((index + 1) / (total + 1)) * 100}%`;

  return (
    <article className={`patch-node role-${data.role}${selected ? ' is-selected' : ''}`} aria-label={`${data.label} ${data.type}`}>
      {inputs.map((port, index) => (
        <Handle
          className={`port port-${port.signal}`}
          id={port.id}
          key={port.id}
          position={Position.Left}
          style={{ top: topFor(index, inputs.length) }}
          type="target"
        />
      ))}
      <div className="node-kicker">{data.role}</div>
      <h3>{data.type === 'macro' ? data.userName : data.label}</h3>
      <div className="node-type">{data.type}</div>
      {data.type === 'envelope-follower' ? <div className="signal-operation">AUDIO → ENVELOPE 0…1</div> : null}
      {data.type === 'hold-gate' ? <div className="signal-operation">AUDIO × CONTROL GATE</div> : null}
      <div className="energy-meter" aria-hidden="true"><i /><i /><i /><i /><i /></div>
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
        <Handle
          className={`port port-${port.signal}`}
          id={port.id}
          key={port.id}
          position={Position.Right}
          style={{ top: topFor(index, outputs.length) }}
          type="source"
        />
      ))}
    </article>
  );
}
