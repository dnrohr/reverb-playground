import { describe, expect, it } from 'vitest';
import { parseHostPatchStateResult } from './hostPatchState';

describe('native host patch state response', () => {
  it('parses the JSON string returned by JUCE native integration', () => {
    expect(parseHostPatchStateResult('{"accepted":true,"error":""}')).toEqual({ accepted: true, error: '' });
    expect(parseHostPatchStateResult({ accepted: false, error: 'invalid graph' })).toEqual({ accepted: false, error: 'invalid graph' });
  });

  it('rejects malformed bridge responses', () => {
    expect(() => parseHostPatchStateResult('not-json')).toThrow('not valid JSON');
    expect(() => parseHostPatchStateResult({ accepted: 'yes', error: '' })).toThrow('fields are invalid');
  });
});
