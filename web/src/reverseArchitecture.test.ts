import { describe, expect, it } from 'vitest';
import requirements from '../../docs/reverse-and-gated-architecture-requirements.md?raw';

describe('reverse and gated product terminology contract', () => {
  it('keeps all four audible and impulse-response concepts distinct', () => {
    expect(requirements).toContain('**True time reversal**');
    expect(requirements).toContain('**Causal reverse-envelope approximation**');
    expect(requirements).toContain('**Gated reverb**');
    expect(requirements).toContain('**Bloom-like slow attack**');
    expect(requirements).toMatch(/sample order is the exact reverse/i);
    expect(requirements).toMatch(/defining evidence is truncation/i);
    expect(requirements).toMatch(/smooth nonzero decay tail/i);
  });

  it('names the feasible first method and fixes the minimum primitive boundary', () => {
    expect(requirements).toMatch(/first factory construction is \*\*Causal Reverse Envelope\*\*/);
    expect(requirements).toMatch(/no lookahead, reported plugin latency/i);
    expect(requirements).toMatch(/### Envelope Follower/);
    expect(requirements).toMatch(/### Hold Gate/);
    expect(requirements).toMatch(/bundled tap-bank macro is rejected/i);
  });

  it('prohibits mislabeling a slow diffuse attack as reverse', () => {
    expect(requirements).toMatch(/No patch is labeled \*\*reverse\*\* solely because it has a long diffuse attack/i);
    expect(requirements).toMatch(/a slow allpass build alone is insufficient/i);
  });
});
