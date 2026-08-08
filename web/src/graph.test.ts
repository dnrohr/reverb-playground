import { describe, expect, it } from 'vitest';
import { createFlowModel, deleteSelected } from './graph';

describe('reference graph presentation model', () => {
  it('preserves all stable node and connection identities', () => {
    const model = createFlowModel();
    expect(model.nodes).toHaveLength(10);
    expect(model.edges).toHaveLength(11);
    expect(model.nodes.map((node) => node.id)).toContain('diffuser-1');
    expect(model.edges.map((edge) => edge.id)).toContain('tank-to-right');
  });

  it('deletes selected nodes and every incident connection from only the UI copy', () => {
    const model = createFlowModel();
    const selected = model.nodes.map((node) => ({ ...node, selected: node.id === 'tank-2' }));
    const result = deleteSelected(selected, model.edges);
    expect(result.nodes.some((node) => node.id === 'tank-2')).toBe(false);
    expect(result.edges.some((edge) => edge.source === 'tank-2' || edge.target === 'tank-2')).toBe(false);
    expect(createFlowModel().nodes.some((node) => node.id === 'tank-2')).toBe(true);
  });

  it('marks audio cables with a semantic class independent of color', () => {
    const model = createFlowModel();
    expect(model.edges.every((edge) => edge.className?.includes('signal-audio'))).toBe(true);
  });
});
