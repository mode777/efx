<script lang="ts">
  import { onMount } from 'svelte';

  let {
    value,
    onChange,
    dts,
  }: {
    value: string;
    onChange: (v: string) => void;
    dts: string;
  } = $props();

  const MONACO_VERSION = '0.52.2';
  const MONACO_BASE = `https://cdn.jsdelivr.net/npm/monaco-editor@${MONACO_VERSION}/min/vs`;

  let host: HTMLDivElement | undefined = $state();
  let editor: any = $state(null);
  let fallback = $state(false);

  function loadMonaco(): Promise<any> {
    return new Promise((resolve, reject) => {
      const w = window as any;
      if (w.monaco) {
        resolve(w.monaco);
        return;
      }
      const start = () => {
        w.require.config({ paths: { vs: MONACO_BASE } });
        w.require(['vs/editor/editor.main'], () => resolve(w.monaco), reject);
      };
      const existing = document.getElementById('monaco-loader') as HTMLScriptElement | null;
      if (existing) {
        existing.addEventListener('load', start);
        existing.addEventListener('error', () => reject(new Error('monaco cdn failed')));
        return;
      }
      const script = document.createElement('script');
      script.id = 'monaco-loader';
      script.src = `${MONACO_BASE}/loader.js`;
      script.onload = start;
      script.onerror = () => reject(new Error('monaco cdn failed'));
      document.head.appendChild(script);
      window.setTimeout(() => {
        if (!w.monaco) reject(new Error('monaco load timed out'));
      }, 8000);
    });
  }

  onMount(() => {
    let cancelled = false;
    loadMonaco()
      .then((monaco) => {
        if (cancelled || !host) return;
        monaco.languages.typescript.javascriptDefaults.addExtraLib(dts, 'efx.d.ts');
        monaco.languages.typescript.javascriptDefaults.setCompilerOptions({
          target: monaco.languages.typescript.ScriptTarget.ES2020,
          allowNonTsExtensions: true,
          lib: ['es2020', 'dom'],
        });
        monaco.languages.typescript.javascriptDefaults.setDiagnosticsOptions({
          noSemanticValidation: false,
          noSyntaxValidation: false,
        });
        editor = monaco.editor.create(host, {
          value,
          language: 'javascript',
          theme: 'vs-dark',
          automaticLayout: true,
          minimap: { enabled: false },
          fontSize: 13,
          lineNumbers: 'on',
          scrollBeyondLastLine: false,
          tabSize: 4,
        });
        editor.onDidChangeModelContent(() => onChange(editor.getValue()));
      })
      .catch(() => {
        if (!cancelled) fallback = true;
      });
    return () => {
      cancelled = true;
      editor?.dispose?.();
    };
  });

  // Keep the editor in sync when the parent replaces the value (select/reset)
  // without clobbering in-progress typing.
  $effect(() => {
    const v = value;
    if (editor && editor.getValue() !== v) {
      editor.setValue(v);
    }
  });
</script>

{#if fallback}
  <textarea
    class="fallback"
    spellcheck="false"
    value={value}
    oninput={(e) => onChange((e.currentTarget as HTMLTextAreaElement).value)}
  ></textarea>
{:else}
  <div class="editor" bind:this={host}></div>
{/if}

<style>
  .editor,
  .fallback {
    width: 100%;
    height: 100%;
  }

  .fallback {
    resize: none;
    border: 0;
    outline: none;
    background: #0b1020;
    color: var(--efx-text);
    font-family: var(--efx-mono);
    font-size: 13px;
    padding: 10px;
    tab-size: 4;
  }
</style>
