import userGuide from '../../docs/user-guide.md?raw';
import barrArchitectures from '../../docs/keith-barr-reverb-architectures.md?raw';
import moduleReference from '../../docs/module-and-visualization-reference.md?raw';
import schematicInteractions from '../../docs/schematic-editor-interactions.md?raw';
import audioWorkflow from '../../docs/audio-workflow-guide.md?raw';
import topologyResearch from '../../docs/large-modulated-and-shimmer-reverb-topologies.md?raw';

export type HelpArticleId = 'user-guide' | 'barr-architectures' | 'module-reference' | 'schematic-interactions' | 'audio-workflow' | 'topology-research';

export interface HelpArticle {
  id: HelpArticleId;
  title: string;
  summary: string;
  sourcePath: string;
  markdown: string;
}

export const helpArticles: readonly HelpArticle[] = [
  { id: 'user-guide', title: 'User guide', summary: 'Audition, build, compare, diagnose, tune, export, and recover.', sourcePath: 'docs/user-guide.md', markdown: userGuide },
  { id: 'barr-architectures', title: 'Keith Barr reverb architectures', summary: 'Documented evidence, reconstruction boundaries, diagrams, and sources.', sourcePath: 'docs/keith-barr-reverb-architectures.md', markdown: barrArchitectures },
  { id: 'module-reference', title: 'Module and visualization reference', summary: 'Signals, units, controls, latency, safety, and analysis views.', sourcePath: 'docs/module-and-visualization-reference.md', markdown: moduleReference },
  { id: 'schematic-interactions', title: 'Schematic editor interactions', summary: 'Create, connect, select, route, group, and inspect a patch.', sourcePath: 'docs/schematic-editor-interactions.md', markdown: schematicInteractions },
  { id: 'audio-workflow', title: 'Audio workflow guide', summary: 'Live input, files, looping, impulse capture, mixing, and WAV export.', sourcePath: 'docs/audio-workflow-guide.md', markdown: audioWorkflow },
  { id: 'topology-research', title: 'Modulated, reverse, and shimmer research', summary: 'Sourced topology patterns and explicit inference boundaries.', sourcePath: 'docs/large-modulated-and-shimmer-reverb-topologies.md', markdown: topologyResearch },
] as const;

const byId = new Map(helpArticles.map((article) => [article.id, article]));
const byFile = new Map(helpArticles.map((article) => [article.sourcePath.split('/').at(-1)!, article.id]));

export const helpArticle = (id: HelpArticleId): HelpArticle => byId.get(id)!;

export function searchHelpArticles(query: string): readonly HelpArticle[] {
  const terms = query.trim().toLocaleLowerCase().split(/\s+/).filter(Boolean);
  if (!terms.length) return helpArticles;
  return helpArticles.filter((article) => {
    const searchable = `${article.title}\n${article.summary}\n${article.markdown}`.toLocaleLowerCase();
    return terms.every((term) => searchable.includes(term));
  });
}

export function helpArticleIdForHref(href: string): HelpArticleId | null {
  const clean = href.split('#')[0].split('?')[0].replaceAll('\\', '/');
  return byFile.get(clean.split('/').at(-1) ?? '') ?? null;
}

export function markdownHeadingId(text: string): string {
  return text.toLocaleLowerCase().replace(/[`*_]/g, '').replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '') || 'section';
}
