import { describe, expect, it } from 'vitest';
import { isEmergencyMuteShortcut, parseRecoveryState } from './crashRecovery';

describe('crash recovery and Emergency Mute', () => {
  it('parses bounded native recovery state and rejects ambiguous data', () => {
    expect(parseRecoveryState(JSON.stringify({ available: true, quarantined: false, attempts: 1,
      incidentId: 'incident-1', candidateName: 'last-known-valid.autosave.rvp.json', candidateHash: 'abc123', message: 'Starts muted.' }))).toMatchObject({ available: true, attempts: 1 });
    expect(() => parseRecoveryState({ available: true, attempts: -1 })).toThrow(/fields/);
  });

  it('activates once from every focus context without treating repeat as a toggle', () => {
    const event = { ctrlKey: true, metaKey: false, shiftKey: true, key: 'M', repeat: false };
    expect(isEmergencyMuteShortcut(event)).toBe(true);
    expect(isEmergencyMuteShortcut({ ...event, repeat: true })).toBe(false);
    expect(isEmergencyMuteShortcut({ ...event, shiftKey: false })).toBe(false);
    expect(isEmergencyMuteShortcut({ ...event, key: 'm', ctrlKey: false, metaKey: true })).toBe(true);
  });
});
