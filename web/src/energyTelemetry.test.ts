import { describe, expect, it } from 'vitest';
import { energyLevelClass, parseEnergyTelemetry, rmsToLevel, shouldRunEnergyTelemetry, smoothEnergy } from './energyTelemetry';

const frame = (rms: number) => ({ formatVersion: 1 as const, enabled: true, coherent: true, generation: 1, observedSampleValues: 100, nodes: [{ nodeId: 'tank-1', rms }] });

describe('energy telemetry presentation', () => {
  it('validates fixed native snapshots and rejects unsafe values', () => {
    expect(parseEnergyTelemetry(JSON.stringify(frame(.1))).nodes[0]).toEqual({ nodeId: 'tank-1', rms: .1 });
    expect(() => parseEnergyTelemetry({ ...frame(.1), nodes: [{ nodeId: 'tank-1', rms: Number.NaN }] })).toThrow(/invalid RMS/);
    expect(() => parseEnergyTelemetry({ ...frame(.1), nodes: [{ nodeId: 'x', rms: 0 }, { nodeId: 'x', rms: 0 }] })).toThrow(/identity/);
  });

  it('maps logarithmic energy and applies fast attack with smooth release', () => {
    expect(rmsToLevel(1)).toBe(1);
    expect(rmsToLevel(10 ** (-72 / 20))).toBeCloseTo(0, 6);
    const attacked = smoothEnergy({}, frame(1), 42);
    expect(attacked['tank-1']).toBeCloseTo(1 - Math.exp(-1), 5);
    const released = smoothEnergy(attacked, frame(0), 42);
    expect(released['tank-1']).toBeGreaterThan(0);
    expect(released['tank-1']).toBeLessThan(attacked['tank-1']!);
    expect(energyLevelClass(attacked['tank-1']!)).toBe('energy-level-4');
  });

  it('holds on incoherent frames and clears immediately when disabled', () => {
    const previous = { 'tank-1': .5 };
    expect(smoothEnergy(previous, { ...frame(1), coherent: false }, 33)).toBe(previous);
    expect(smoothEnergy(previous, { ...frame(1), enabled: false }, 33)).toEqual({});
    expect(shouldRunEnergyTelemetry(true, false)).toBe(true);
    expect(shouldRunEnergyTelemetry(true, true)).toBe(false);
    expect(shouldRunEnergyTelemetry(false, false)).toBe(false);
  });
});
