import { mkdir, writeFile } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const outputDirectory = resolve(root, 'factory-patches');

const audioIn = (id = 'in') => ({ id, signal: 'audio', direction: 'input' });
const audioOut = (id = 'out') => ({ id, signal: 'audio', direction: 'output' });
const controlIn = (id) => ({ id, signal: 'control', direction: 'input' });
const controlOut = (id = 'out') => ({ id, signal: 'control', direction: 'output' });
const mapping = (portId, amount, clampMinimum, clampMaximum) => ({
  portId, amount, polarity: 'bipolar', clampMinimum, clampMaximum,
});
const parameter = (id, value, unit, amount, minimum, maximum) => ({
  id, value, unit, modulation: mapping(`${id}-mod`, amount, minimum, maximum),
});
const staticParameter = (id, value, unit) => ({ id, value, unit });

const stereoInput = (id = 'input') => ({ id, type: 'stereo-input', ports: [audioOut('out-l'), audioOut('out-r')], parameters: [] });
const stereoOutput = (id = 'output') => ({ id, type: 'stereo-output', ports: [audioIn('in-l'), audioIn('in-r')], parameters: [] });
const sum = (id) => ({ id, type: 'sum', ports: [audioIn('in-a'), audioIn('in-b'), audioOut()], parameters: [] });
const gain = (id, value) => ({ id, type: 'gain', ports: [audioIn(), controlIn('gain-mod'), audioOut()], parameters: [parameter('gain', value, 'linear', 0.5, -1, 1)] });
const delay = (id, milliseconds) => ({ id, type: 'delay', ports: [audioIn(), controlIn('delay-mod'), audioOut()], parameters: [parameter('delay', milliseconds, 'milliseconds', 10, 0.1, 10000)] });
const allpass = (id, milliseconds, coefficient) => ({
  id, type: 'allpass', ports: [audioIn(), controlIn('delay-mod'), controlIn('coefficient-mod'), audioOut()],
  parameters: [
    parameter('delay', milliseconds, 'milliseconds', 2, 0.1, 100),
    parameter('coefficient', coefficient, 'unitless', 0.25, -0.95, 0.95),
  ],
});
const lowpass = (id, hertz) => ({ id, type: 'lowpass', ports: [audioIn(), controlIn('cutoff-mod'), audioOut()], parameters: [parameter('cutoff', hertz, 'hertz', 5000, 20, 20000)] });
const follower = (id, attack, release) => ({ id, type: 'envelope-follower', ports: [audioIn(), controlOut()], parameters: [staticParameter('attack', attack, 'milliseconds'), staticParameter('release', release, 'milliseconds')] });
const gate = (id, threshold, attack, hold, release) => ({
  id, type: 'hold-gate', ports: [audioIn(), controlIn('gate'), audioOut()],
  parameters: [
    staticParameter('threshold', threshold, 'unitless'),
    staticParameter('attack', attack, 'milliseconds'),
    staticParameter('hold', hold, 'milliseconds'),
    staticParameter('release', release, 'milliseconds'),
  ],
});
const cable = (id, fromNode, fromPort, toNode, toPort) => ({
  id, from: { nodeId: fromNode, portId: fromPort }, to: { nodeId: toNode, portId: toPort },
});
const position = (nodeId, x, y) => ({ nodeId, x, y });
const patch = (nodes, connections, positions) => ({
  schemaVersion: 2,
  engineVersion: '0.1',
  semantic: { nodes, connections },
  layout: { nodes: positions, viewport: { x: 0, y: 0, zoom: 0.72 } },
});

const reverseEnvelope = patch([
  stereoInput(), sum('input-sum'),
  allpass('diffusion-short', 7.1, 0.58), allpass('diffusion-long', 11.3, 0.62),
  delay('rise-early-45ms', 45), gain('weight-early-0-25', 0.25),
  delay('rise-middle-115ms', 115), gain('weight-middle-0-55', 0.55),
  delay('rise-peak-210ms', 210), gain('weight-peak-0-95', 0.95),
  sum('sum-early-middle'), sum('sum-rising-envelope'),
  lowpass('tone-6-5khz', 6500), gain('output-level-0-75', 0.75),
  allpass('stereo-left-8-7ms', 8.7, 0.45), allpass('stereo-right-14-3ms', 14.3, 0.48),
  stereoOutput(),
], [
  cable('input-l', 'input', 'out-l', 'input-sum', 'in-a'), cable('input-r', 'input', 'out-r', 'input-sum', 'in-b'),
  cable('sum-diffuse-short', 'input-sum', 'out', 'diffusion-short', 'in'), cable('diffuse-short-long', 'diffusion-short', 'out', 'diffusion-long', 'in'),
  cable('diffuse-early', 'diffusion-long', 'out', 'rise-early-45ms', 'in'), cable('early-weight', 'rise-early-45ms', 'out', 'weight-early-0-25', 'in'),
  cable('diffuse-middle', 'diffusion-long', 'out', 'rise-middle-115ms', 'in'), cable('middle-weight', 'rise-middle-115ms', 'out', 'weight-middle-0-55', 'in'),
  cable('diffuse-peak', 'diffusion-long', 'out', 'rise-peak-210ms', 'in'), cable('peak-weight', 'rise-peak-210ms', 'out', 'weight-peak-0-95', 'in'),
  cable('early-sum', 'weight-early-0-25', 'out', 'sum-early-middle', 'in-a'), cable('middle-sum', 'weight-middle-0-55', 'out', 'sum-early-middle', 'in-b'),
  cable('early-middle-envelope', 'sum-early-middle', 'out', 'sum-rising-envelope', 'in-a'), cable('peak-envelope', 'weight-peak-0-95', 'out', 'sum-rising-envelope', 'in-b'),
  cable('envelope-tone', 'sum-rising-envelope', 'out', 'tone-6-5khz', 'in'), cable('tone-level', 'tone-6-5khz', 'out', 'output-level-0-75', 'in'),
  cable('level-left', 'output-level-0-75', 'out', 'stereo-left-8-7ms', 'in'), cable('level-right', 'output-level-0-75', 'out', 'stereo-right-14-3ms', 'in'),
  cable('left-out', 'stereo-left-8-7ms', 'out', 'output', 'in-l'), cable('right-out', 'stereo-right-14-3ms', 'out', 'output', 'in-r'),
], [
  position('input', 0, 210), position('input-sum', 180, 210), position('diffusion-short', 360, 170), position('diffusion-long', 540, 170),
  position('rise-early-45ms', 730, 0), position('weight-early-0-25', 910, 0),
  position('rise-middle-115ms', 730, 170), position('weight-middle-0-55', 910, 170),
  position('rise-peak-210ms', 730, 340), position('weight-peak-0-95', 910, 340),
  position('sum-early-middle', 1090, 70), position('sum-rising-envelope', 1270, 170),
  position('tone-6-5khz', 1450, 170), position('output-level-0-75', 1630, 170),
  position('stereo-left-8-7ms', 1810, 80), position('stereo-right-14-3ms', 1810, 280), position('output', 2010, 180),
]);

const levelGatedRoom = patch([
  stereoInput(), sum('input-sum'), follower('level-detector', 0.1, 20),
  allpass('diffusion-1', 5.7, 0.86), allpass('diffusion-2', 9.1, 0.84), allpass('diffusion-3', 13.7, 0.82),
  lowpass('tone-7-2khz', 7200), gain('output-level-0-80', 0.80),
  allpass('stereo-left-4-3ms', 4.3, 0.42), allpass('stereo-right-7-9ms', 7.9, 0.46),
  gate('left-level-gate', 0.004, 2, 120, 8), gate('right-level-gate', 0.004, 2, 120, 8),
  stereoOutput(),
], [
  cable('input-l', 'input', 'out-l', 'input-sum', 'in-a'), cable('input-r', 'input', 'out-r', 'input-sum', 'in-b'),
  cable('sum-detector', 'input-sum', 'out', 'level-detector', 'in'), cable('sum-diffusion', 'input-sum', 'out', 'diffusion-1', 'in'),
  cable('diffusion-1-2', 'diffusion-1', 'out', 'diffusion-2', 'in'), cable('diffusion-2-3', 'diffusion-2', 'out', 'diffusion-3', 'in'),
  cable('diffusion-tone', 'diffusion-3', 'out', 'tone-7-2khz', 'in'), cable('tone-level', 'tone-7-2khz', 'out', 'output-level-0-80', 'in'),
  cable('level-left-diffusion', 'output-level-0-80', 'out', 'stereo-left-4-3ms', 'in'), cable('level-right-diffusion', 'output-level-0-80', 'out', 'stereo-right-7-9ms', 'in'),
  cable('left-audio-gate', 'stereo-left-4-3ms', 'out', 'left-level-gate', 'in'), cable('right-audio-gate', 'stereo-right-7-9ms', 'out', 'right-level-gate', 'in'),
  cable('detector-left-gate', 'level-detector', 'out', 'left-level-gate', 'gate'), cable('detector-right-gate', 'level-detector', 'out', 'right-level-gate', 'gate'),
  cable('left-out', 'left-level-gate', 'out', 'output', 'in-l'), cable('right-out', 'right-level-gate', 'out', 'output', 'in-r'),
], [
  position('input', 0, 210), position('input-sum', 180, 210), position('level-detector', 370, 20),
  position('diffusion-1', 370, 240), position('diffusion-2', 560, 240), position('diffusion-3', 750, 240),
  position('tone-7-2khz', 940, 240), position('output-level-0-80', 1130, 240),
  position('stereo-left-4-3ms', 1320, 120), position('stereo-right-7-9ms', 1320, 360),
  position('left-level-gate', 1520, 120), position('right-level-gate', 1520, 360), position('output', 1730, 240),
]);

await mkdir(outputDirectory, { recursive: true });
for (const [filename, document] of [
  ['causal-reverse-envelope.rvp.json', reverseEnvelope],
  ['level-gated-room.rvp.json', levelGatedRoom],
]) {
  await writeFile(resolve(outputDirectory, filename), `${JSON.stringify(document, null, 2)}\n`, 'utf8');
}
