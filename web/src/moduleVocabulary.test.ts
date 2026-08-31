import { describe, expect, it } from 'vitest';
import { createModuleNode, moduleDefinitions } from './modules';
import { isAdvancedParameter, moduleVocabulary, parameterBehavior, visibleModuleLabel } from './moduleVocabulary';

describe('module vocabulary and inspector disclosure', () => {
  it('audits every public module with signal and audible-role language', () => {
    expect(Object.keys(moduleVocabulary).sort()).toEqual(moduleDefinitions.map((definition) => definition.type).sort());
    for (const definition of moduleDefinitions) {
      const vocabulary = moduleVocabulary[definition.type];
      expect(vocabulary.displayName).toBe(definition.label);
      expect(vocabulary.displayName.length).toBeGreaterThan(0);
      expect(vocabulary.signal).toMatch(/AUDIO|CONTROL/);
      expect(vocabulary.audibleRole).toMatch(/[.]$/);
      expect(new Set(definition.ports.map((port) => `${port.direction}:${port.id}`)).size).toBe(definition.ports.length);
      expect(new Set(definition.parameters.map((parameter) => parameter.id)).size).toBe(definition.parameters.length);
      for (const parameter of definition.parameters) {
        expect(parameter.unit.length).toBeGreaterThan(0);
        expect(parameter.value).toBeGreaterThanOrEqual(parameter.minimum);
        expect(parameter.value).toBeLessThanOrEqual(parameter.maximum);
      }
      for (const id of vocabulary.advancedParameterIds) {
        expect(definition.parameters.some((parameter) => parameter.id === id)).toBe(true);
      }
    }
    expect(moduleDefinitions.find((definition) => definition.type === 'macro')?.parameters
      .find((parameter) => parameter.id === 'center-detent')?.value).toBe(1);
  });
  it('preserves the public saved type and parameter ID contract', () => {
    expect(Object.fromEntries(moduleDefinitions.map((definition) => [definition.type, definition.parameters.map((parameter) => parameter.id)]))).toEqual({
      'stereo-input': [], 'stereo-output': [], gain: ['gain'], sum: [], delay: ['delay'],
      allpass: ['delay', 'coefficient'], lowpass: ['cutoff'],
      'pitch-shift': ['semitones', 'grain', 'overlap', 'direction', 'phase'],
      macro: ['value', 'default-value', 'center-detent'],
      lfo: ['frequency', 'phase', 'waveform', 'run-mode'],
      'control-map': ['scale', 'offset', 'polarity', 'curve-family', 'curve-amount', 'exponent', 'clamp-min', 'clamp-max'],
      'envelope-follower': ['attack', 'release'], 'hold-gate': ['threshold', 'attack', 'hold', 'release'],
    });
  });
  it('renames only the visible Gain label while retaining stable IDs and negative inversion', () => {
    const gain = createModuleNode('gain', 'gain-1', { x: 0, y: 0 });
    expect(visibleModuleLabel(gain.data)).toBe('Gain');
    expect(gain.data.type).toBe('gain'); expect(gain.data.parameters[0].id).toBe('gain');
    expect(gain.data.parameters[0]).toMatchObject({ minimum: -1, maximum: 1, value: 1 });
  });
  it('places specialist fields in Advanced while keeping primary controls visible', () => {
    const pitch = createModuleNode('pitch-shift', 'pitch', { x: 0, y: 0 });
    expect(isAdvancedParameter(pitch.data, 'semitones')).toBe(false);
    for (const id of ['grain', 'overlap', 'direction', 'phase']) expect(isAdvancedParameter(pitch.data, id)).toBe(true);
    const gate = createModuleNode('hold-gate', 'gate', { x: 0, y: 0 });
    expect(isAdvancedParameter(gate.data, 'threshold')).toBe(false);
    expect(isAdvancedParameter(gate.data, 'hold')).toBe(true);
  });
  it('labels modulation as smoothed and base-only fields as crossfaded rebuilds', () => {
    const gain = createModuleNode('gain', 'gain', { x: 0, y: 0 });
    expect(parameterBehavior(gain.data, gain.data.parameters[0])).toEqual(['MODULATED', 'SMOOTHED']);
    const gate = createModuleNode('hold-gate', 'gate', { x: 0, y: 0 });
    expect(parameterBehavior(gate.data, gate.data.parameters[0])).toEqual(['BASE ONLY', 'CROSSFADED REBUILD']);
  });
});
