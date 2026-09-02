import { describe, expect, it } from 'vitest';
import { teachingTopicFor } from './teaching';

describe('contextual teaching content', () => {
  it('explains the explicit mono sum using documented and implementation labels', () => {
    const topic = teachingTopicFor('sum');
    expect(topic?.documented).toMatch(/mono summed input/i);
    expect(topic?.reconstruction).toMatch(/explicit Mono Sum/i);
    expect(topic?.takeaway).toMatch(/Stereo width/i);
  });

  it('explains both stereo taps as views of one shared tank', () => {
    for (const id of ['left-tap', 'right-tap']) {
      const topic = teachingTopicFor(id);
      expect(topic?.documented).toMatch(/left and right DAC/i);
      expect(topic?.reconstruction).toMatch(/same shared Tank 2/i);
    }
  });

  it('does not imply the development reference already contains historical feedback', () => {
    const topic = teachingTopicFor('tank-1');
    expect(topic?.documented).toMatch(/recirculating loop/i);
    expect(topic?.reconstruction).toMatch(/omits the outer feedback cable/i);
  });

  it('does not repeat a Barr overview for unrelated selections or an empty inspector', () => {
    expect(teachingTopicFor()).toBeNull();
    expect(teachingTopicFor('unrelated-delay')).toBeNull();
  });
});
