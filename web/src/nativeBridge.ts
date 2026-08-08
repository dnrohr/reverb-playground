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
  if (!nativeBackend) return Promise.resolve(undefined);
  const resultId = nextPromiseId++;
  const result = new Promise<unknown>((resolve) => pending.set(resultId, resolve));
  nativeBackend.emitEvent('__juce__invoke', { name, params: parameters, resultId });
  return result;
}
