import { describe, expect, it } from 'vitest';
import qualificationJson from '../../artifacts/measurements/m25-5-primary-user-journeys.json?raw';

interface JourneyQualification {
  milestone: string;
  referenceViewport: {
    canvasPercentOfViewport: Record<string, number>;
  };
  keyboard: { standaloneEnteredWebView: boolean; entrySequence: string[] };
  journeys: Array<{
    id: string;
    commandCount: number;
    graphStructureEdits?: number;
    openedContextTabs?: string[];
    outcomes: Record<string, boolean>;
  }>;
  findings: { regressions: Array<{ status: string }> };
  externalValidation: { m7_5NonImplementerSessionsStillRequired: boolean };
}

const qualification = JSON.parse(qualificationJson) as JourneyQualification;

describe('M25.5 primary user journey qualification', () => {
  it('keeps the canvas dominant in every intentional arrangement', () => {
    expect(qualification.milestone).toBe('M25.5');
    expect(qualification.referenceViewport.canvasPercentOfViewport.balanced).toBeGreaterThan(50);
    expect(qualification.referenceViewport.canvasPercentOfViewport.createFocus).toBeGreaterThan(80);
    expect(qualification.referenceViewport.canvasPercentOfViewport.learnAndInspect).toBeGreaterThan(60);
  });

  it('records complete bounded journeys without changing their meaning', () => {
    expect(qualification.journeys.map((journey) => journey.id)).toEqual([
      'musician', 'sound-designer', 'learner',
    ]);
    for (const journey of qualification.journeys) {
      expect(journey.commandCount).toBeLessThanOrEqual(10);
      expect(Object.values(journey.outcomes).every(Boolean)).toBe(true);
    }
    expect(qualification.journeys[0]?.graphStructureEdits).toBe(0);
    expect(qualification.journeys[1]?.openedContextTabs).toEqual(['inspect']);
  });

  it('gates the candidate on keyboard entry and resolved internal regressions', () => {
    expect(qualification.keyboard.standaloneEnteredWebView).toBe(true);
    expect(qualification.keyboard.entrySequence).toContain('FILE');
    expect(qualification.findings.regressions.every(
      (finding) => finding.status === 'fixed-and-reverified',
    )).toBe(true);
    expect(qualification.externalValidation.m7_5NonImplementerSessionsStillRequired).toBe(true);
  });
});
