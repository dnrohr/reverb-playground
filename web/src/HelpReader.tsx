import { Fragment, useEffect, useMemo, useRef, useState, type JSX, type ReactNode } from 'react';
import { helpArticle, helpArticleIdForHref, markdownHeadingId, searchHelpArticles, type HelpArticleId } from './helpLibrary';

interface HelpReaderProps {
  articleId: HelpArticleId;
  onArticle(id: HelpArticleId): void;
  onClose(): void;
}

const inlinePattern = /(`[^`]+`|\*\*[^*]+\*\*|\[[^\]]+\]\([^)]+\))/g;

function inlineMarkdown(text: string, onArticle: (id: HelpArticleId) => void): ReactNode[] {
  return text.split(inlinePattern).filter(Boolean).map((part, index) => {
    if (part.startsWith('`') && part.endsWith('`')) return <code key={index}>{part.slice(1, -1)}</code>;
    if (part.startsWith('**') && part.endsWith('**')) return <strong key={index}>{part.slice(2, -2)}</strong>;
    const link = /^\[([^\]]+)\]\(([^)]+)\)$/.exec(part);
    if (link) {
      const target = helpArticleIdForHref(link[2]);
      if (target) return <a key={index} href={`#help-${target}`} onClick={(event) => { event.preventDefault(); onArticle(target); }}>{link[1]}</a>;
      const external = /^https?:\/\//i.test(link[2]);
      if (external || link[2].startsWith('#')) return <a key={index} href={link[2]} {...(external ? { target: '_blank', rel: 'noreferrer' } : {})}>{link[1]}</a>;
      return <span className="help-source-link" key={index} title="This related source document is available in the project documentation.">{link[1]}</span>;
    }
    return <Fragment key={index}>{part}</Fragment>;
  });
}

function isBlockStart(lines: string[], index: number): boolean {
  const line = lines[index] ?? '';
  const next = lines[index + 1] ?? '';
  return !line.trim() || /^#{1,6}\s/.test(line) || /^```/.test(line) || /^>\s?/.test(line)
    || /^\s*[-*+]\s+/.test(line) || /^\s*\d+[.)]\s+/.test(line)
    || (line.includes('|') && /^\s*\|?\s*:?-{3,}/.test(next));
}

function MarkdownArticle({ markdown, onArticle }: { markdown: string; onArticle(id: HelpArticleId): void }) {
  const blocks = useMemo(() => {
    const lines = markdown.replaceAll('\r\n', '\n').split('\n');
    const result: ReactNode[] = [];
    let index = 0;
    while (index < lines.length) {
      const line = lines[index];
      if (!line.trim()) { index++; continue; }
      const heading = /^(#{1,6})\s+(.+)$/.exec(line);
      if (heading) {
        const level = heading[1].length; const text = heading[2].trim(); const id = markdownHeadingId(text);
        const Heading = `h${level}` as keyof JSX.IntrinsicElements;
        result.push(<Heading id={id} key={`h-${index}`} tabIndex={-1}>{inlineMarkdown(text, onArticle)}</Heading>); index++; continue;
      }
      const fence = /^```(.*)$/.exec(line);
      if (fence) {
        const code: string[] = []; index++;
        while (index < lines.length && !/^```/.test(lines[index])) code.push(lines[index++]);
        if (index < lines.length) index++;
        result.push(<pre key={`code-${index}`} aria-label={fence[1] ? `${fence[1]} diagram or code` : 'Diagram or code'}><code>{code.join('\n')}</code></pre>); continue;
      }
      if (line.includes('|') && /^\s*\|?\s*:?-{3,}/.test(lines[index + 1] ?? '')) {
        const rows: string[][] = [];
        const split = (value: string) => value.trim().replace(/^\||\|$/g, '').split('|').map((cell) => cell.trim());
        const header = split(line); index += 2;
        while (index < lines.length && lines[index].includes('|') && lines[index].trim()) rows.push(split(lines[index++]));
        result.push(<div className="help-table-scroll" key={`table-${index}`}><table><thead><tr>{header.map((cell, cellIndex) => <th key={cellIndex} scope="col">{inlineMarkdown(cell, onArticle)}</th>)}</tr></thead>
          <tbody>{rows.map((row, rowIndex) => <tr key={rowIndex}>{row.map((cell, cellIndex) => <td key={cellIndex}>{inlineMarkdown(cell, onArticle)}</td>)}</tr>)}</tbody></table></div>); continue;
      }
      const unordered = /^\s*[-*+]\s+(.+)$/.exec(line); const ordered = /^\s*\d+[.)]\s+(.+)$/.exec(line);
      if (unordered || ordered) {
        const items: string[] = []; const pattern = ordered ? /^\s*\d+[.)]\s+(.+)$/ : /^\s*[-*+]\s+(.+)$/;
        while (index < lines.length) {
          const item = pattern.exec(lines[index]); if (!item) break;
          const wrapped = [item[1]]; index++;
          while (index < lines.length && lines[index].trim() && !pattern.test(lines[index]) && !isBlockStart(lines, index)) wrapped.push(lines[index++].trim());
          items.push(wrapped.join(' '));
        }
        const List = ordered ? 'ol' : 'ul'; result.push(<List key={`list-${index}`}>{items.map((item, itemIndex) => <li key={itemIndex}>{inlineMarkdown(item, onArticle)}</li>)}</List>); continue;
      }
      if (/^>\s?/.test(line)) {
        const quote: string[] = []; while (index < lines.length && /^>\s?/.test(lines[index])) quote.push(lines[index++].replace(/^>\s?/, ''));
        result.push(<blockquote key={`quote-${index}`}>{inlineMarkdown(quote.join(' '), onArticle)}</blockquote>); continue;
      }
      const paragraph = [line.trim()]; index++;
      while (index < lines.length && !isBlockStart(lines, index)) paragraph.push(lines[index++].trim());
      result.push(<p key={`p-${index}`}>{inlineMarkdown(paragraph.join(' '), onArticle)}</p>);
    }
    return result;
  }, [markdown, onArticle]);
  return <>{blocks}</>;
}

export function HelpReader({ articleId, onArticle, onClose }: HelpReaderProps) {
  const [query, setQuery] = useState('');
  const closeButton = useRef<HTMLButtonElement | null>(null);
  const dialog = useRef<HTMLElement | null>(null);
  const priorFocus = useRef<HTMLElement | null>(document.activeElement instanceof HTMLElement ? document.activeElement : null);
  const article = helpArticle(articleId);
  const matches = searchHelpArticles(query);
  useEffect(() => {
    closeButton.current?.focus();
    return () => {
      const target = priorFocus.current?.isConnected ? priorFocus.current : document.querySelector<HTMLElement>('[data-help-launcher="true"]');
      target?.focus();
    };
  }, []);
  useEffect(() => {
    const handleKey = (event: KeyboardEvent) => {
      if (event.key === 'Escape') { event.preventDefault(); onClose(); return; }
      if (event.key !== 'Tab' || !dialog.current) return;
      const focusable = Array.from(dialog.current.querySelectorAll<HTMLElement>('button:not([disabled]), a[href], input:not([disabled]), [tabindex]:not([tabindex="-1"])'))
        .filter((element) => element.offsetParent !== null);
      if (!focusable.length) return;
      const first = focusable[0]; const last = focusable[focusable.length - 1];
      if (event.shiftKey && (document.activeElement === first || !dialog.current.contains(document.activeElement))) {
        event.preventDefault(); last.focus();
      } else if (!event.shiftKey && (document.activeElement === last || !dialog.current.contains(document.activeElement))) {
        event.preventDefault(); first.focus();
      }
    };
    window.addEventListener('keydown', handleKey); return () => window.removeEventListener('keydown', handleKey);
  }, [onClose]);
  useEffect(() => { document.querySelector('.help-article')?.scrollTo({ top: 0, behavior: 'auto' }); }, [articleId]);
  return <div className="help-backdrop" role="presentation">
    <section ref={dialog} className="help-reader" role="dialog" aria-modal="true" aria-labelledby="help-title">
      <header><div><span>HELP / {article.title.toUpperCase()}</span><h2 id="help-title">Offline help library</h2></div><button ref={closeButton} type="button" onClick={onClose}>RETURN TO EDITOR</button></header>
      <aside className="help-navigation" aria-label="Help articles">
        <label><span>SEARCH HELP</span><input type="search" value={query} onChange={(event) => setQuery(event.target.value)} placeholder="architecture, export, delay…" /></label>
        <nav>{matches.map((candidate) => <button type="button" key={candidate.id} aria-current={candidate.id === articleId ? 'page' : undefined} onClick={() => onArticle(candidate.id)}>
          <strong>{candidate.title}</strong><span>{candidate.summary}</span></button>)}</nav>
        {!matches.length ? <p>No offline articles match every search term.</p> : null}
      </aside>
      <article className="help-article" aria-label={article.title}>
        <div className="help-breadcrumbs"><button type="button" onClick={() => onArticle('user-guide')}>HELP</button><span>/</span><strong>{article.title}</strong></div>
        <div className="help-provenance"><span>OFFLINE / RENDERED</span><code>{article.sourcePath}</code></div>
        <MarkdownArticle markdown={article.markdown} onArticle={onArticle} />
      </article>
    </section>
  </div>;
}
