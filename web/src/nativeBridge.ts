interface JuceBackend {
  addEventListener(name: string, listener: (payload: { promiseId: number; result: unknown }) => void): void;
  emitEvent(name: string, payload: unknown): void;
}

declare global {
  interface Window {
    __JUCE__?: {
      backend?: JuceBackend;
      initialisationData?: { __juce__functions?: string[] };
    };
  }
}

let nextPromiseId = 0;
const pending = new Map<number, (result: unknown) => void>();
let listening = false;

function backend(): JuceBackend | undefined {
  const value = window.__JUCE__?.backend;
  if (!value) {
    if (import.meta.env.DEV) return undefined;
    throw new Error('JUCE native integration is unavailable');
  }
  if (!listening) {
    value.addEventListener('__juce__complete', ({ promiseId, result }) => {
      pending.get(promiseId)?.(result);
      pending.delete(promiseId);
    });
    listening = true;
  }
  return value;
}

export function callNative(name: string, ...parameters: unknown[]): Promise<unknown> {
  const nativeBackend = backend();
  if (!nativeBackend) {
    if (import.meta.env.DEV && new URLSearchParams(window.location.search).get('recoveryFixture') === '1') {
      if (name === 'getRecoveryState') return Promise.resolve(JSON.stringify({ available: true, quarantined: false, attempts: 1,
        incidentId: '20260904T201500-4a8c1d2e', candidateName: 'last-known-valid.autosave.rvp.json', candidateHash: '7f8c9d0e12ab34cd',
        message: 'The previous standalone session did not exit cleanly. Recovery is optional and starts muted.' }));
      if (name === 'setEmergencyMuted' || name === 'openCrashReportsFolder' || name === 'declineRecovery') return Promise.resolve(true);
    }
    return Promise.resolve(undefined);
  }
  const resultId = nextPromiseId++;
  const result = new Promise<unknown>((resolve) => pending.set(resultId, resolve));
  nativeBackend.emitEvent('__juce__invoke', { name, params: parameters, resultId });
  return result;
}
