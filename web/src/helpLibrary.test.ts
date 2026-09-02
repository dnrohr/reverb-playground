import { describe, expect, it } from 'vitest';
import appSource from './App.tsx?raw';
import helpReaderSource from './HelpReader.tsx?raw';
import patchPersistenceSource from './patchPersistence.ts?raw';
import { helpArticleIdForHref, helpArticles, markdownHeadingId, searchHelpArticles } from './helpLibrary';

describe('rendered offline help library', () => {
  it('ships authoritative Markdown for every navigable article', () => {
    expect(helpArticles.length).toBeGreaterThanOrEqual(6);
    for (const article of helpArticles) {
      expect(article.sourcePath).toMatch(/^docs\/.+\.md$/);
      expect(article.markdown).toContain('# ');
      expect(article.summary.length).toBeGreaterThan(30);
    }
  });

  it('searches article content with every entered term', () => {
    expect(searchHelpArticles('export wav').map((article) => article.id)).toContain('user-guide');
    expect(searchHelpArticles('Keith Barr').map((article) => article.id)).toContain('barr-architectures');
    expect(searchHelpArticles('term-that-is-not-present')).toEqual([]);
  });

  it('routes known local Markdown links and leaves external sources as links', () => {
    expect(helpArticleIdForHref('keith-barr-reverb-architectures.md')).toBe('barr-architectures');
    expect(helpArticleIdForHref('../docs/audio-workflow-guide.md#safe-audition-sequence')).toBe('audio-workflow');
    expect(helpArticleIdForHref('https://example.com/source')).toBeNull();
    expect(markdownHeadingId('MIDIVerb II and “Bloom”')).toBe('midiverb-ii-and-bloom');
  });

  it('renders headings tables code links breadcrumbs search and keyboard return without patch semantics', () => {
    for (const token of ['<table>', '<pre', 'help-breadcrumbs', 'type="search"', "event.key === 'Escape'", "event.key !== 'Tab'", 'RETURN TO EDITOR', 'help-source-link'])
      expect(helpReaderSource).toContain(token);
    expect(appSource).toContain("setHelpArticleId('user-guide')");
    expect(appSource).not.toContain('CONTEXTUAL LEARNING');
    expect(appSource).not.toContain('LEARN {teachingEnabled');
    expect(patchPersistenceSource).not.toContain('helpArticleId');
  });
});
