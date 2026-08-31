import { BaseEdge, EdgeLabelRenderer, getSmoothStepPath, type EdgeProps } from '@xyflow/react';
import type { CableLayout } from './graph';

const linePath = (points: Array<{ x: number; y: number }>) => points.map((point, index) => `${index ? 'L' : 'M'} ${point.x} ${point.y}`).join(' ');
const toward = (a: { x: number; y: number }, b: { x: number; y: number }, distance = 54) => {
  const length = Math.max(1, Math.hypot(b.x - a.x, b.y - a.y)); return { x: a.x + (b.x - a.x) * distance / length, y: a.y + (b.y - a.y) * distance / length };
};

export function RoutedEdge({ id, sourceX, sourceY, targetX, targetY, sourcePosition, targetPosition, markerEnd, style, data, selected }: EdgeProps) {
  const layout = (data?.layout ?? {}) as CableLayout; const points = [{ x: sourceX, y: sourceY }, ...(layout.waypoints ?? []), { x: targetX, y: targetY }];
  const [fallback] = getSmoothStepPath({ sourceX, sourceY, targetX, targetY, sourcePosition, targetPosition });
  const fullPath = layout.waypoints?.length ? linePath(points) : fallback;
  if (!layout.portal || selected) return <>
    <BaseEdge id={id} path={fullPath} markerEnd={markerEnd} style={style} />
    {(layout.waypoints ?? []).map((point, index) => <circle key={index} className="cable-waypoint" cx={point.x} cy={point.y} r={5} />)}
    {layout.portal ? <EdgeLabelRenderer><span className="portal-path-revealed">{layout.portal.name} · COMPLETE PATH</span></EdgeLabelRenderer> : null}
  </>;
  const source = points[0]; const target = points.at(-1)!; const sourceStub = toward(source, points[1] ?? target); const targetStub = toward(target, points.at(-2) ?? source);
  return <>
    <BaseEdge id={`${id}-out`} path={linePath([source, sourceStub])} style={style} />
    <BaseEdge id={`${id}-in`} path={linePath([target, targetStub])} markerEnd={markerEnd} style={style} />
    <EdgeLabelRenderer>
      <span className="routing-portal routing-portal-source" style={{ transform: `translate(-50%, -50%) translate(${sourceStub.x}px,${sourceStub.y}px)` }}>{layout.portal.name} →</span>
      <span className="routing-portal routing-portal-target" style={{ transform: `translate(-50%, -50%) translate(${targetStub.x}px,${targetStub.y}px)` }}>→ {layout.portal.name}</span>
    </EdgeLabelRenderer>
  </>;
}
