import type { Edge, Node, XYPosition } from '@xyflow/react';
import type { GraphState, HierarchyPresentation, PatchNodeData } from './graph';
import { createModuleNode } from './modules';

export interface SubpatchPortBinding {
  id: string; signal: 'audio' | 'control'; direction: 'input' | 'output'; nodeId: string; portId: string;
}
export interface SubpatchInstance {
  id: string; definitionId: string; definitionVersion: number; definitionName: string;
  memberNodeIds: string[]; ports: SubpatchPortBinding[];
}
export interface SubpatchDefinition {
  id: string; version: number; name: string; description: string;
  instantiate(instanceId: string, position: XYPosition): { nodes: Node<PatchNodeData>[]; edges: Edge[]; ports: SubpatchPortBinding[] };
}

const parameter = (node: Node<PatchNodeData>, id: string, value: number) => ({
  ...node, data: { ...node.data, parameters: node.data.parameters.map((item) => item.id === id ? { ...item, value } : item) },
});
const edge = (id: string, source: string, sourceHandle: string, target: string, targetHandle: string): Edge => ({
  id, source, sourceHandle, target, targetHandle, type: 'smoothstep', className: 'signal-edge signal-audio',
  data: { signal: 'audio' }, interactionWidth: 24,
});

export const diffuseDelayDefinition: SubpatchDefinition = {
  id: 'rp.diffuse-delay', version: 1, name: 'Diffuse Delay',
  description: 'One editable all-pass followed by one editable mono delay.',
  instantiate(instanceId, position) {
    const allpassId = `${instanceId}-allpass`; const delayId = `${instanceId}-delay`;
    const allpass = parameter(parameter(createModuleNode('allpass', allpassId, position), 'delay', 7.3), 'coefficient', 0.62);
    const delay = parameter(createModuleNode('delay', delayId, { x: position.x + 220, y: position.y }), 'delay', 31.1);
    return { nodes: [allpass, delay], edges: [edge(`${instanceId}-internal`, allpassId, 'out', delayId, 'in')], ports: [
      { id: 'in', signal: 'audio', direction: 'input', nodeId: allpassId, portId: 'in' },
      { id: 'out', signal: 'audio', direction: 'output', nodeId: delayId, portId: 'out' },
    ] };
  },
};
export const subpatchDefinitions = [diffuseDelayDefinition] as const;
export const subpatchDefinitionById = new Map(subpatchDefinitions.map((definition) => [definition.id, definition]));

function uniqueInstanceId(nodes: Node<PatchNodeData>[]) {
  const used = new Set(nodes.flatMap((node) => node.data.subpatchInstance ? [node.data.subpatchInstance.id] : []));
  let index = 1; while (used.has(`diffuse-delay-${index}`)) index += 1; return `diffuse-delay-${index}`;
}

export function instantiateSubpatch(state: GraphState, definition: SubpatchDefinition, position: XYPosition): GraphState {
  const instanceId = uniqueInstanceId(state.nodes); const built = definition.instantiate(instanceId, position);
  const metadata: SubpatchInstance = { id: instanceId, definitionId: definition.id, definitionVersion: definition.version,
    definitionName: definition.name, memberNodeIds: built.nodes.map((node) => node.id), ports: built.ports };
  const hierarchy: HierarchyPresentation = {
    id: instanceId, kind: 'subpatch', name: definition.name, collapsed: true,
    memberNodeIds: [...metadata.memberNodeIds], position: { ...position }, nestedViewport: { x: 0, y: 0, zoom: 1 },
    ports: built.ports.map((port) => ({ id: port.id, name: port.id.toUpperCase(), signal: port.signal,
      direction: port.direction, targets: [{ nodeId: port.nodeId, portId: port.portId }] })),
  };
  const nodes = built.nodes.map((node) => ({ ...node, selected: true, className: `${node.className ?? ''} subpatch-member`,
    data: { ...node.data, runtimeBound: false, subpatchInstance: structuredClone(metadata),
      hierarchyPresentation: structuredClone(hierarchy) } }));
  return { nodes: [...state.nodes.map((node) => ({ ...node, selected: false })), ...nodes],
    edges: [...state.edges.map((item) => ({ ...item, selected: false })), ...built.edges] };
}

export function inspectSubpatchInstances(nodes: Node<PatchNodeData>[]): SubpatchInstance[] {
  const instances = new Map<string, SubpatchInstance>();
  for (const node of nodes) if (node.data.subpatchInstance && !instances.has(node.data.subpatchInstance.id))
    instances.set(node.data.subpatchInstance.id, structuredClone(node.data.subpatchInstance));
  return [...instances.values()].sort((a, b) => a.id.localeCompare(b.id));
}

export function subpatchInstanceStatus(instance: SubpatchInstance, nodes: Node<PatchNodeData>[]) {
  const definition = subpatchDefinitionById.get(instance.definitionId);
  const missingMembers = instance.memberNodeIds.filter((id) => !nodes.some((node) => node.id === id));
  if (!definition) return { kind: 'missing' as const, message: 'Definition unavailable; expanded primitives remain audible and editable.' };
  if (missingMembers.length) return { kind: 'detached' as const, message: `${missingMembers.length} member block(s) were removed; this instance is detached.` };
  if (definition.version > instance.definitionVersion) return { kind: 'update' as const, message: `Version ${definition.version} is available; this patch remains pinned to version ${instance.definitionVersion}.` };
  return { kind: 'current' as const, message: `Pinned to ${definition.name} v${instance.definitionVersion}; edits affect only this instance.` };
}

export function detachSubpatchInstance(state: GraphState, instanceId: string): GraphState {
  return { nodes: state.nodes.map((node) => node.data.subpatchInstance?.id === instanceId
    ? { ...node, className: node.className?.replace(/\s*subpatch-member\b/g, ''),
      data: { ...node.data, subpatchInstance: undefined, hierarchyPresentation: undefined } } : node), edges: state.edges };
}
