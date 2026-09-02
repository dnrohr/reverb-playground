export interface TeachingTopic {
  title: string;
  documented: string;
  reconstruction: string;
  takeaway: string;
}

const topics: Record<string, TeachingTopic> = {
  sum: {
    title: 'Why stereo becomes mono here',
    documented: 'The documented MIDIVerb I signal path has a mono summed input and stereo output. Its reverb program builds one shared ambient field rather than two independent input tanks.',
    reconstruction: 'The plugin accepts stereo for modern use, then the explicit Mono Sum adds left and right and applies 0.5 gain. No hidden channel conversion occurs elsewhere.',
    takeaway: 'Stereo width can come from different output taps of one mixed tank; it does not require separate left and right reverbs.',
  },
  'left-tap': {
    title: 'Left view of one tank',
    documented: 'MIDIVerb I programs capture left and right DAC values at different instruction positions and can sum different internal delay taps into each output.',
    reconstruction: 'Left Tap is a visible terminal allpass fed from the same shared Tank 2 signal as Right Tap. This simple branch is not a claim about a specific ROM tap map.',
    takeaway: 'Related source, different temporal path: that is enough to make the wet channels distinct without hiding a second tank.',
  },
  'right-tap': {
    title: 'Right view of one tank',
    documented: 'MIDIVerb I programs capture left and right DAC values at different instruction positions and can sum different internal delay taps into each output.',
    reconstruction: 'Right Tap is a visible terminal allpass fed from the same shared Tank 2 signal as Left Tap. Its different delay produces a related but non-identical wet channel.',
    takeaway: 'Sparse, decorrelated observations of a common field are a core route to algorithmic stereo.',
  },
  tank: {
    title: 'Diffusion belongs inside feedback',
    documented: 'Barr described mature designs as allpasses and delays in one recirculating loop, so energy meets new diffusion before repeating and density grows around the ring.',
    reconstruction: 'The current development reference exposes the tank stages but intentionally omits the outer feedback cable while the general safe graph runtime is unfinished. It is presently a finite feed-forward teaching slice.',
    takeaway: 'When feedback construction arrives, the return must be explicit, delayed, bounded, and visible—not a hidden property of a Tank block.',
  },
  diffuser: {
    title: 'Allpass as temporal mixer',
    documented: 'Barr used delay allpasses to increase echo density while avoiding the static magnitude peaks produced by simply summing equal-weight delay taps.',
    reconstruction: 'This floating-point first-order allpass exposes delay and coefficient directly. It approximates the architectural idea, not MIDIVerb fixed-point arithmetic.',
    takeaway: 'The allpass rearranges energy in time; several stages turn a sparse event into a denser response.',
  },
  filter: {
    title: 'Bandwidth is architectural',
    documented: 'The original machine combined low sample rate, steep analog filtering, and filtering inside programs; bandwidth helped shape—and conceal—the digital structure.',
    reconstruction: 'This visible one-pole low-pass is a controllable approximation at the host sample rate. It does not emulate the original converters or analog filters.',
    takeaway: 'Darkening the signal changes both tone and how strongly metallic delay patterns are exposed.',
  },
  input: {
    title: 'Modern stereo boundary',
    documented: 'MIDIVerb I is documented as accepting a mono summed signal into its program while producing two output values.',
    reconstruction: 'The plugin boundary accepts two mono cables so stereo sources work naturally; the next explicit block decides how they combine.',
    takeaway: 'Channel conversion is part of the diagram, not an invisible convenience.',
  },
  output: {
    title: 'Two explicit mono outputs',
    documented: 'Different internal accumulations feed the historical left and right DAC capture points.',
    reconstruction: 'The output block merely exposes two mono input ports. All stereo differentiation happens in the visible left/right branches before it.',
    takeaway: 'Inspect the incoming cables to understand the width; the output block adds no secret processing.',
  },
};

export function teachingTopicFor(nodeId?: string): TeachingTopic | null {
  if (!nodeId) return null;
  if (nodeId === 'tank-1' || nodeId === 'tank-2') return topics.tank;
  if (nodeId === 'diffuser-1' || nodeId === 'diffuser-2') return topics.diffuser;
  if (nodeId === 'input-filter') return topics.filter;
  if (nodeId === 'input') return topics.input;
  if (nodeId === 'output') return topics.output;
  return topics[nodeId] ?? null;
}
