<script lang="ts">
  import catalogJson from './samples/generated.json';
  import type { Catalog, Sample } from './lib/catalog';
  import SampleList from './lib/SampleList.svelte';
  import Frame from './lib/Frame.svelte';
  import Editor from './lib/Editor.svelte';
  import efxDts from './api/efx.d.ts?raw';

  const catalog = catalogJson as Catalog;
  const samples: Sample[] = catalog.samples;

  let selectedId: string | null = $state(samples.length ? samples[0].id : null);
  let editorCode: string = $state(samples.length ? samples[0].source : '');
  let runId = $state(0);
  let error: string | null = $state(null);

  const selected = $derived(samples.find((s) => s.id === selectedId) ?? null);
  const frameAssets = $derived(selected?.assets ?? null);

  function select(sample: Sample) {
    selectedId = sample.id;
    editorCode = sample.source;
    error = null;
    runId++;
  }

  function run() {
    error = null;
    runId++;
  }

  function reset() {
    if (selected) {
      editorCode = selected.source;
    }
    error = null;
    runId++;
  }
</script>

<div class="shell">
  <aside class="catalog panel">
    <SampleList {samples} {selectedId} onSelect={select} />
  </aside>

  <main class="stage">
    <section class="viewport panel">
      <header class="bar">
        <span class="dot"></span>
        <span class="title">{selected?.title ?? 'No sample'}</span>
        {#if selected}<span class="badge">{selected.category}</span>{/if}
        <span class="spacer"></span>
        <a class="hint link" href="./api/">API Reference</a>
      </header>
      <div class="screen">
        {#if selectedId}
          <Frame code={editorCode} assets={frameAssets} {runId} onError={(m) => (error = m)} />
        {/if}
      </div>
      {#if error}
        <div class="error" role="alert">
          <strong>script error:</strong> {error}
        </div>
      {/if}
    </section>

    <section class="code panel">
      <header class="bar">
        <span class="title">main.js</span>
        <span class="spacer"></span>
        <button class="btn" onclick={reset}>Reset</button>
        <button class="btn primary" onclick={run}>Run</button>
      </header>
      <div class="editor-wrap">
        <Editor value={editorCode} onChange={(v) => (editorCode = v)} dts={efxDts} />
      </div>
    </section>
  </main>
</div>

<style>
  .shell {
    display: grid;
    grid-template-columns: 300px 1fr;
    gap: 12px;
    height: 100%;
    padding: 12px;
  }

  .catalog {
    min-height: 0;
    overflow: hidden;
    display: flex;
  }

  .stage {
    min-height: 0;
    display: grid;
    grid-template-rows: minmax(0, 1.35fr) minmax(0, 1fr);
    gap: 12px;
  }

  .viewport,
  .code {
    min-height: 0;
    display: flex;
    flex-direction: column;
  }

  .bar {
    display: flex;
    align-items: center;
    gap: 10px;
    padding: 8px 12px;
    border-bottom: 1px solid var(--efx-edge);
    background: linear-gradient(180deg, rgba(53, 224, 255, 0.08), transparent);
    letter-spacing: 0.06em;
  }

  .bar .dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: var(--efx-accent);
    box-shadow: var(--efx-glow);
  }

  .bar .title {
    font-weight: 600;
    text-transform: uppercase;
    font-size: 12px;
  }

  .badge {
    font-size: 10px;
    text-transform: uppercase;
    letter-spacing: 0.1em;
    color: var(--efx-accent);
    border: 1px solid var(--efx-accent-dim);
    border-radius: 999px;
    padding: 1px 8px;
  }

  .spacer {
    flex: 1;
  }

  .hint {
    font-size: 10px;
    color: var(--efx-text-dim);
    letter-spacing: 0.08em;
    text-transform: uppercase;
  }

  .hint.link {
    color: var(--efx-accent);
    text-decoration: none;
  }

  .hint.link:hover {
    text-decoration: underline;
  }

  .screen {
    flex: 1;
    min-height: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 10px;
    background: radial-gradient(70% 70% at 50% 40%, #0a1330, #04060e);
  }

  .error {
    margin: 0 10px 10px;
    padding: 8px 12px;
    border: 1px solid var(--efx-danger);
    border-radius: var(--efx-radius);
    color: #ffd7db;
    background: rgba(120, 20, 35, 0.35);
    font-family: var(--efx-mono);
    font-size: 12px;
    max-height: 96px;
    overflow: auto;
  }

  .editor-wrap {
    flex: 1;
    min-height: 0;
  }
</style>
