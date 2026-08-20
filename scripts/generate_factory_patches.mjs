import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
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
const lfo = (id, frequency, phase = 0, waveform = 0) => ({
  id, type: 'lfo',
  ports: [controlIn('frequency-mod'), controlIn('phase-mod'), controlIn('waveform-mod'), controlIn('run-mode-mod'), controlOut()],
  parameters: [
    parameter('frequency', frequency, 'hertz', 1, 0.01, 100), parameter('phase', phase, 'cycles', 0.25, 0, 0.999),
    parameter('waveform', waveform, 'waveform', 1, 0, 1), parameter('run-mode', 0, 'run-mode', 1, 0, 1),
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

const modulatedCosmicReverse = patch([
  stereoInput(), sum('input-sum'),
  allpass('input-diffusion-7-9ms', 7.9, 0.62), allpass('input-diffusion-13-1ms', 13.1, 0.66),
  delay('rise-early-80ms', 80), gain('weight-early-0-18', 0.18),
  delay('rise-middle-240ms', 240), gain('weight-middle-0-42', 0.42),
  delay('rise-peak-520ms', 520), gain('weight-peak-0-72', 0.72),
  sum('sum-early-middle'), sum('sum-rising-envelope'), sum('tank-input'),
  allpass('tank-diffusion-a-19-7ms', 19.7, 0.68), allpass('tank-diffusion-b-31-3ms', 31.3, 0.71),
  delay('tank-space-173ms', 173), lowpass('tail-damping-4-8khz', 4800), gain('feedback-0-58', 0.58),
  gain('output-level-0-55', 0.55),
  allpass('stereo-left-11-9ms', 11.9, 0.54), allpass('stereo-right-17-3ms', 17.3, 0.57),
  lfo('slow-drift-a-0-11hz', 0.11, 0, 0), lfo('slow-drift-b-0-073hz', 0.073, 0.37, 1),
  stereoOutput(),
], [
  cable('input-l', 'input', 'out-l', 'input-sum', 'in-a'), cable('input-r', 'input', 'out-r', 'input-sum', 'in-b'),
  cable('sum-diffusion-a', 'input-sum', 'out', 'input-diffusion-7-9ms', 'in'),
  cable('diffusion-a-b', 'input-diffusion-7-9ms', 'out', 'input-diffusion-13-1ms', 'in'),
  cable('diffuse-early', 'input-diffusion-13-1ms', 'out', 'rise-early-80ms', 'in'),
  cable('early-weight', 'rise-early-80ms', 'out', 'weight-early-0-18', 'in'),
  cable('diffuse-middle', 'input-diffusion-13-1ms', 'out', 'rise-middle-240ms', 'in'),
  cable('middle-weight', 'rise-middle-240ms', 'out', 'weight-middle-0-42', 'in'),
  cable('diffuse-peak', 'input-diffusion-13-1ms', 'out', 'rise-peak-520ms', 'in'),
  cable('peak-weight', 'rise-peak-520ms', 'out', 'weight-peak-0-72', 'in'),
  cable('early-sum', 'weight-early-0-18', 'out', 'sum-early-middle', 'in-a'),
  cable('middle-sum', 'weight-middle-0-42', 'out', 'sum-early-middle', 'in-b'),
  cable('early-middle-envelope', 'sum-early-middle', 'out', 'sum-rising-envelope', 'in-a'),
  cable('peak-envelope', 'weight-peak-0-72', 'out', 'sum-rising-envelope', 'in-b'),
  cable('envelope-tank', 'sum-rising-envelope', 'out', 'tank-input', 'in-a'),
  cable('feedback-return', 'feedback-0-58', 'out', 'tank-input', 'in-b'),
  cable('tank-diffusion-a', 'tank-input', 'out', 'tank-diffusion-a-19-7ms', 'in'),
  cable('tank-diffusion-b', 'tank-diffusion-a-19-7ms', 'out', 'tank-diffusion-b-31-3ms', 'in'),
  cable('tank-space', 'tank-diffusion-b-31-3ms', 'out', 'tank-space-173ms', 'in'),
  cable('space-damping', 'tank-space-173ms', 'out', 'tail-damping-4-8khz', 'in'),
  cable('damping-feedback', 'tail-damping-4-8khz', 'out', 'feedback-0-58', 'in'),
  cable('damping-output', 'tail-damping-4-8khz', 'out', 'output-level-0-55', 'in'),
  cable('level-left', 'output-level-0-55', 'out', 'stereo-left-11-9ms', 'in'),
  cable('level-right', 'output-level-0-55', 'out', 'stereo-right-17-3ms', 'in'),
  cable('left-out', 'stereo-left-11-9ms', 'out', 'output', 'in-l'),
  cable('right-out', 'stereo-right-17-3ms', 'out', 'output', 'in-r'),
  cable('drift-a-tank-a', 'slow-drift-a-0-11hz', 'out', 'tank-diffusion-a-19-7ms', 'delay-mod'),
  cable('drift-a-left', 'slow-drift-a-0-11hz', 'out', 'stereo-left-11-9ms', 'delay-mod'),
  cable('drift-b-tank-b', 'slow-drift-b-0-073hz', 'out', 'tank-diffusion-b-31-3ms', 'delay-mod'),
  cable('drift-b-right', 'slow-drift-b-0-073hz', 'out', 'stereo-right-17-3ms', 'delay-mod'),
], [
  position('input', 0, 260), position('input-sum', 180, 260),
  position('input-diffusion-7-9ms', 360, 220), position('input-diffusion-13-1ms', 540, 220),
  position('rise-early-80ms', 730, 0), position('weight-early-0-18', 910, 0),
  position('rise-middle-240ms', 730, 190), position('weight-middle-0-42', 910, 190),
  position('rise-peak-520ms', 730, 380), position('weight-peak-0-72', 910, 380),
  position('sum-early-middle', 1090, 90), position('sum-rising-envelope', 1270, 200),
  position('tank-input', 1450, 200), position('tank-diffusion-a-19-7ms', 1640, 140),
  position('tank-diffusion-b-31-3ms', 1830, 140), position('tank-space-173ms', 2020, 140),
  position('tail-damping-4-8khz', 2210, 140), position('feedback-0-58', 2020, 390),
  position('output-level-0-55', 2400, 140), position('stereo-left-11-9ms', 2590, 40),
  position('stereo-right-17-3ms', 2590, 270), position('output', 2800, 150),
  position('slow-drift-a-0-11hz', 1640, 520), position('slow-drift-b-0-073hz', 1830, 520),
]);

const catalog = {
  catalogVersion: 1,
  patches: [
    {
      id: 'barr-reference',
      family: 'barr-reference',
      status: 'complete',
      document: {
        kind: 'native-runtime',
        path: 'src/graph/Source/BarrReferenceGraph.cpp',
        schemaVersion: 2,
        engineVersion: '0.1',
      },
      license: { expression: 'AGPL-3.0-only', file: 'LICENSE' },
      provenance: {
        kind: 'project-authored',
        source: 'src/dsp/Source/BarrReferenceRuntime.cpp',
        description: 'Public primitives and project-authored parameters; no ROM-derived data.',
      },
    },
    {
      id: 'causal-reverse-envelope',
      family: 'reverse-style',
      status: 'complete',
      document: {
        kind: 'checked-in-json',
        path: 'factory-patches/causal-reverse-envelope.rvp.json',
        schemaVersion: 2,
        engineVersion: '0.1',
      },
      license: { expression: 'AGPL-3.0-only', file: 'LICENSE' },
      provenance: {
        kind: 'project-authored-generated',
        source: 'scripts/generate_factory_patches.mjs',
        description: 'Generated from project-authored public-primitives topology and parameters.',
      },
    },
    {
      id: 'level-gated-room',
      family: 'gated',
      status: 'complete',
      document: {
        kind: 'checked-in-json',
        path: 'factory-patches/level-gated-room.rvp.json',
        schemaVersion: 2,
        engineVersion: '0.1',
      },
      license: { expression: 'AGPL-3.0-only', file: 'LICENSE' },
      provenance: {
        kind: 'project-authored-generated',
        source: 'scripts/generate_factory_patches.mjs',
        description: 'Generated from project-authored public-primitives topology and parameters.',
      },
    },
    {
      id: 'modulated-cosmic-reverse',
      family: 'modulated-reverse-style',
      status: 'complete',
      document: {
        kind: 'checked-in-json',
        path: 'factory-patches/modulated-cosmic-reverse.rvp.json',
        schemaVersion: 2,
        engineVersion: '0.1',
      },
      license: { expression: 'AGPL-3.0-only', file: 'LICENSE' },
      provenance: {
        kind: 'project-authored-generated',
        source: 'scripts/generate_factory_patches.mjs',
        description: 'Original public-primitives topology inferred from documented large, inverse, modulated reverb behavior; not a reconstruction of a proprietary algorithm.',
      },
    },
    {
      id: 'gravity-diffusion',
      family: 'gravity-diffusion',
      status: 'complete',
      document: {
        kind: 'checked-in-json',
        path: 'factory-patches/gravity-diffusion.rvp.json',
        schemaVersion: 2,
        engineVersion: '0.1',
      },
      license: { expression: 'AGPL-3.0-only', file: 'LICENSE' },
      provenance: {
        kind: 'project-authored-generated',
        source: 'src/graph/Source/GravityDiffusionGraph.cpp',
        description: 'Generated from the project-authored eight-stage public-primitives Gravity Diffusion graph; no proprietary preset or algorithm reconstruction.',
      },
    },
    {
      id: 'safe-parallel-shimmer',
      family: 'parallel-shimmer',
      status: 'complete',
      document: {
        kind: 'checked-in-json',
        path: 'factory-patches/safe-parallel-shimmer.rvp.json',
        schemaVersion: 2,
        engineVersion: '0.1',
      },
      license: { expression: 'AGPL-3.0-only', file: 'LICENSE' },
      provenance: {
        kind: 'project-authored-generated',
        source: 'src/graph/Source/SafeParallelShimmerGraph.cpp',
        description: 'Generated from the project-authored public-primitives parallel octave topology; not a reconstruction of a proprietary shimmer algorithm.',
      },
    },
    {
      id: 'split-feedback-shimmer',
      family: 'feedback-shimmer',
      status: 'complete',
      document: {
        kind: 'checked-in-json',
        path: 'factory-patches/split-feedback-shimmer.rvp.json',
        schemaVersion: 2,
        engineVersion: '0.1',
      },
      license: { expression: 'AGPL-3.0-only', file: 'LICENSE' },
      provenance: {
        kind: 'project-authored-generated',
        source: 'src/graph/Source/SplitFeedbackShimmerGraph.cpp',
        description: 'Generated from the project-authored public-primitives split-feedback octave topology; not a reconstruction of a proprietary shimmer algorithm.',
      },
    },
    {
      id: 'reverse-cosmic-shimmer',
      family: 'reverse-cosmic-shimmer',
      status: 'complete',
      document: {
        kind: 'checked-in-json',
        path: 'factory-patches/reverse-cosmic-shimmer.rvp.json',
        schemaVersion: 2,
        engineVersion: '0.1',
      },
      license: { expression: 'AGPL-3.0-only', file: 'LICENSE' },
      provenance: {
        kind: 'project-authored-generated',
        source: 'src/graph/Source/ReverseCosmicShimmerGraph.cpp',
        description: 'Generated from the project-authored public-primitives causal-rise and dual reverse-grain topology; behavioral synthesis, not a reconstruction of a proprietary algorithm or preset.',
      },
    },
  ],
};

await mkdir(outputDirectory, { recursive: true });
const checkOnly = process.argv.includes('--check');
for (const [filename, document] of [
  ['causal-reverse-envelope.rvp.json', reverseEnvelope],
  ['level-gated-room.rvp.json', levelGatedRoom],
  ['modulated-cosmic-reverse.rvp.json', modulatedCosmicReverse],
  ['catalog.json', catalog],
]) {
  const outputPath = resolve(outputDirectory, filename);
  const expected = `${JSON.stringify(document, null, 2)}\n`;
  if (checkOnly) {
    const actual = await readFile(outputPath, 'utf8');
    if (actual !== expected) throw new Error(`${filename} is stale; run node scripts/generate_factory_patches.mjs`);
  } else {
    await writeFile(outputPath, expected, 'utf8');
  }
}

const gravityFactoryPath = resolve(outputDirectory, 'gravity-diffusion.rvp.json');
const gravityFactoryBytes = await readFile(gravityFactoryPath);
const gravityFactoryHash = createHash('sha256').update(gravityFactoryBytes).digest('hex');
if (gravityFactoryHash !== '8e683dfb595c6f24ac5882e46e6d3fbffcb92790a5616f1bd2b31be1c8973124')
  throw new Error('gravity-diffusion.rvp.json is stale; run .\\scripts\\generate_gravity_factory_patch.ps1 -Configuration Release');

const shimmerFactoryPath = resolve(outputDirectory, 'safe-parallel-shimmer.rvp.json');
const shimmerFactoryBytes = await readFile(shimmerFactoryPath);
const shimmerFactoryHash = createHash('sha256').update(shimmerFactoryBytes).digest('hex');
if (shimmerFactoryHash !== '2dacd5e59d3b72f1ac4203f42f85e3a2e3ed1e1cacd3253998b1cf8fd7cccb91')
  throw new Error('safe-parallel-shimmer.rvp.json is stale; run .\\scripts\\generate_safe_parallel_shimmer_factory.ps1 -Configuration Release');

const splitShimmerFactoryPath = resolve(outputDirectory, 'split-feedback-shimmer.rvp.json');
const splitShimmerFactoryBytes = await readFile(splitShimmerFactoryPath);
const splitShimmerFactoryHash = createHash('sha256').update(splitShimmerFactoryBytes).digest('hex');
if (splitShimmerFactoryHash !== '4aea9287200d5c585d0e0a982e3bc7114b2bbc8b43e1fabd7bfafd66dbddd9ff')
  throw new Error('split-feedback-shimmer.rvp.json is stale; run .\\scripts\\generate_split_feedback_shimmer_factory.ps1 -Configuration Release');

const reverseCosmicFactoryPath = resolve(outputDirectory, 'reverse-cosmic-shimmer.rvp.json');
const reverseCosmicFactoryBytes = await readFile(reverseCosmicFactoryPath);
const reverseCosmicFactoryHash = createHash('sha256').update(reverseCosmicFactoryBytes).digest('hex');
if (reverseCosmicFactoryHash !== '07c1e4376a9f6e2b61cb7369799457c8e6b5f56be4d172dca5637ba198724e04')
  throw new Error('reverse-cosmic-shimmer.rvp.json is stale; run .\\scripts\\generate_reverse_cosmic_shimmer_factory.ps1 -Configuration Release');
