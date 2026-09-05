export interface RecoveryState {
  available: boolean;
  quarantined: boolean;
  attempts: number;
  incidentId: string;
  candidateName: string;
  candidateHash: string;
  message: string;
}

export function parseRecoveryState(payload: unknown): RecoveryState {
  const value = typeof payload === 'string' ? JSON.parse(payload) : payload;
  if (!value || typeof value !== 'object') throw new Error('Recovery state is invalid');
  const state = value as Record<string, unknown>;
  if (typeof state.available !== 'boolean' || typeof state.quarantined !== 'boolean'
    || !Number.isInteger(state.attempts) || Number(state.attempts) < 0
    || typeof state.incidentId !== 'string' || typeof state.candidateName !== 'string' || typeof state.candidateHash !== 'string'
    || typeof state.message !== 'string') throw new Error('Recovery state fields are invalid');
  return state as unknown as RecoveryState;
}

export function isEmergencyMuteShortcut(event: Pick<KeyboardEvent, 'ctrlKey' | 'metaKey' | 'shiftKey' | 'key' | 'repeat'>): boolean {
  return (event.ctrlKey || event.metaKey) && event.shiftKey
    && event.key.toLowerCase() === 'm' && !event.repeat;
}
