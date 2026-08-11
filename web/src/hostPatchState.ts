export interface HostPatchStateResult { accepted: boolean; error: string }

export function parseHostPatchStateResult(input: unknown): HostPatchStateResult {
  let value = input;
  if (typeof value === 'string') {
    try { value = JSON.parse(value); } catch { throw new Error('Host state response is not valid JSON'); }
  }
  if (typeof value !== 'object' || value === null || Array.isArray(value))
    throw new Error('Host state response is not an object');
  const result = value as Partial<HostPatchStateResult>;
  if (typeof result.accepted !== 'boolean' || typeof result.error !== 'string')
    throw new Error('Host state response fields are invalid');
  return result as HostPatchStateResult;
}
