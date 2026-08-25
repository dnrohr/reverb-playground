export const pitchShiftMinimumSemitones = -12;
export const pitchShiftMaximumSemitones = 12;
export const pitchShiftMaximumExcursionMilliseconds = 360;

export const pitchShiftLatencySamples = (sampleRate: number) =>
  Math.ceil((pitchShiftMaximumExcursionMilliseconds / 1000) * sampleRate) + 2;
