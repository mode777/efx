<script lang="ts">
  import type { Sample } from './catalog';

  let {
    samples,
    selectedId,
    onSelect,
  }: {
    samples: Sample[];
    selectedId: string | null;
    onSelect: (sample: Sample) => void;
  } = $props();

  let query = $state('');

  const filtered = $derived(
    samples.filter((s) =>
      `${s.title} ${s.category}`.toLowerCase().includes(query.toLowerCase())
    )
  );

  const groups = $derived.by(() => {
    const map = new Map<string, Sample[]>();
    for (const s of filtered) {
      const list = map.get(s.category) ?? [];
      list.push(s);
      map.set(s.category, list);
    }
    return [...map.entries()];
  });
</script>

<div class="list">
  <div class="head">
    <span class="brand">EMOTIONFX</span>
    <span class="sub">sample gallery</span>
  </div>

  <input
    class="search"
    type="search"
    placeholder="filter samples…"
    bind:value={query}
    aria-label="Filter samples"
  />

  <div class="scroll">
    {#each groups as [category, items] (category)}
      <div class="group">{category}</div>
      {#each items as sample (sample.id)}
        <button
          class="item"
          class:active={sample.id === selectedId}
          onclick={() => onSelect(sample)}
          title={sample.description}
        >
          <span class="name">{sample.title}</span>
          {#if sample.origin === 'curated'}<span class="tag">demo</span>{/if}
        </button>
      {/each}
    {/each}
    {#if groups.length === 0}
      <div class="empty">no matching samples</div>
    {/if}
  </div>
</div>

<style>
  .list {
    display: flex;
    flex-direction: column;
    width: 100%;
    min-height: 0;
  }

  .head {
    padding: 12px 14px 8px;
    display: flex;
    flex-direction: column;
  }

  .brand {
    font-size: 20px;
    font-weight: 800;
    letter-spacing: 0.16em;
    color: #eaf8ff;
    text-shadow: var(--efx-glow);
  }

  .sub {
    font-size: 10px;
    text-transform: uppercase;
    letter-spacing: 0.3em;
    color: var(--efx-text-dim);
  }

  .search {
    margin: 8px 12px;
    padding: 6px 10px;
    background: #071021;
    border: 1px solid var(--efx-edge);
    border-radius: var(--efx-radius);
    color: var(--efx-text);
    font-family: var(--efx-mono);
    font-size: 12px;
    outline: none;
  }

  .search:focus {
    border-color: var(--efx-accent);
    box-shadow: var(--efx-glow);
  }

  .scroll {
    flex: 1;
    min-height: 0;
    overflow-y: auto;
    padding: 0 8px 12px;
  }

  .group {
    margin: 12px 8px 4px;
    font-size: 10px;
    text-transform: uppercase;
    letter-spacing: 0.18em;
    color: var(--efx-accent-dim);
    border-bottom: 1px solid var(--efx-edge-soft);
    padding-bottom: 3px;
  }

  .item {
    display: flex;
    align-items: center;
    width: 100%;
    padding: 6px 10px;
    margin: 1px 0;
    background: transparent;
    border: 1px solid transparent;
    border-radius: var(--efx-radius);
    color: var(--efx-text-dim);
    font-size: 13px;
    text-align: left;
    cursor: pointer;
  }

  .item:hover {
    color: var(--efx-text);
    background: rgba(53, 224, 255, 0.06);
  }

  .item.active {
    color: #eaf8ff;
    background: linear-gradient(90deg, rgba(53, 224, 255, 0.22), rgba(53, 224, 255, 0.02));
    border-color: var(--efx-accent-dim);
    box-shadow: inset 2px 0 0 var(--efx-accent);
  }

  .name {
    flex: 1;
  }

  .tag {
    font-size: 9px;
    text-transform: uppercase;
    letter-spacing: 0.1em;
    color: var(--efx-accent);
    border: 1px solid var(--efx-edge);
    border-radius: 3px;
    padding: 0 4px;
  }

  .empty {
    padding: 20px 12px;
    color: var(--efx-text-dim);
    font-style: italic;
  }
</style>
