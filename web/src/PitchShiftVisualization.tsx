import type { CSSProperties } from 'react';
import type { PatchParameter } from './graph';
import { pitchShiftLatencySamples } from './pitchShiftContract';

interface PitchShiftVisualizationProps {
  parameters: PatchParameter[];
  reducedMotion: boolean;
  sampleRate?: number;
  compact?: boolean;
}

const value = (parameters: PatchParameter[], id: string, fallback: number) =>
  parameters.find((parameter) => parameter.id === id)?.value ?? fallback;

export function PitchShiftVisualization({ parameters, reducedMotion, sampleRate = 48_000, compact = false }: PitchShiftVisualizationProps) {
  const semitones = value(parameters, 'semitones', 12);
  const grain = value(parameters, 'grain', 60);
  const overlap = value(parameters, 'overlap', 0.5);
  const reverse = value(parameters, 'direction', 0) >= 0.5;
  const phase = value(parameters, 'phase', 0);
  const latencySamples = pitchShiftLatencySamples(sampleRate);
  const cycleSeconds = Math.max(0.65, grain / 55);
  const phaseB = (phase + 0.5) % 1;
  const style = {
    '--grain-cycle': `${cycleSeconds.toFixed(2)}s`, '--grain-overlap': `${Math.round(overlap * 100)}%`,
    '--grain-phase-a': `${(-cycleSeconds * phase).toFixed(3)}s`, '--grain-phase-b': `${(-cycleSeconds * phaseB).toFixed(3)}s`,
    '--grain-position-a': `${Math.round(phase * 100)}%`, '--grain-position-b': `${Math.round(phaseB * 100)}%`,
  } as CSSProperties;
  return (
    <figure
      className={`pitch-visualization${compact ? ' is-compact' : ''}${reverse ? ' is-reverse' : ''}${reducedMotion ? ' is-reduced' : ''}`}
      style={style}
      aria-label={`Illustrative dual grain phase, ${reverse ? 'reverse' : 'forward'}, ${semitones >= 0 ? '+' : ''}${semitones.toFixed(2)} semitones`}
    >
      {!compact ? <figcaption><span>ILLUSTRATIVE GRAIN PHASE</span><strong>{reverse ? 'REVERSE GRAINS' : 'FORWARD GRAINS'}</strong></figcaption> : null}
      <div className="grain-rails" aria-hidden="true">
        <i className="grain-window grain-window-a"><b /></i>
        <i className="grain-window grain-window-b"><b /></i>
      </div>
      {!compact ? <>
        <div className="pitch-visual-facts">
          <span>{semitones >= 0 ? '+' : ''}{semitones.toFixed(2)} st</span>
          <span>{grain.toFixed(1)} ms grain</span>
          <span>{Math.round(overlap * 100)}% overlap</span>
          <span>{phase.toFixed(3)} cycle phase</span>
        </div>
        <dl className="pitch-readonly">
          <div><dt>LATENCY</dt><dd>{latencySamples.toLocaleString()} samples / {(latencySamples * 1000 / sampleRate).toFixed(2)} ms</dd></div>
          <div><dt>QUALITY</dt><dd>Dual grain · linear interpolation</dd></div>
          <div><dt>MOTION</dt><dd>{reducedMotion ? 'Static reduced-motion phase' : 'Design-state animation'}</dd></div>
        </dl>
        <p>Phase is derived from saved controls for teaching only. It is not measured audio or sample-accurate read-head telemetry. Musical-ratio resampling is not a fixed-Hz frequency shift or moving-Delay Doppler effect. Reverse changes samples inside each causal grain—not the whole reverb and never pre-input audio.</p>
      </> : null}
    </figure>
  );
}
