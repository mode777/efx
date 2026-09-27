<script lang="ts">
  let {
    code,
    runId,
    onError,
  }: {
    code: string;
    runId: number;
    onError: (message: string) => void;
  } = $props();

  let wrapEl: HTMLDivElement | undefined = $state();
  let frameEl: HTMLIFrameElement | undefined = $state();
  let boxW = $state(320);
  let boxH = $state(240);

  // Latest code as a non-reactive value so the run effect below depends only
  // on the run token, never on keystrokes.
  const latest: { code: string } = { code: '' };
  $effect(() => {
    latest.code = code;
  });

  // Fit a 4:3 surface into the available panel (design D10): the sample frame
  // is 640x480, so matching the aspect keeps samples composed as authored.
  $effect(() => {
    const el = wrapEl;
    if (!el || typeof ResizeObserver === 'undefined') return;
    const fit = () => {
      const w = el.clientWidth;
      const h = el.clientHeight;
      let bw = w;
      let bh = (w * 3) / 4;
      if (bh > h) {
        bh = h;
        bw = (h * 4) / 3;
      }
      boxW = Math.max(1, Math.floor(bw));
      boxH = Math.max(1, Math.floor(bh));
    };
    const ro = new ResizeObserver(fit);
    ro.observe(el);
    fit();
    return () => ro.disconnect();
  });

  // One fresh iframe per run: state isolation and WebGL-context release.
  $effect(() => {
    const el = frameEl;
    void runId;
    if (!el) return;
    function onMessage(ev: MessageEvent) {
      if (ev.origin !== window.location.origin) return;
      if (ev.source !== el!.contentWindow) return;
      const data = ev.data ?? {};
      if (data.type === 'ready') {
        el!.contentWindow!.postMessage(
          { type: 'run', code: latest.code },
          window.location.origin
        );
      } else if (data.type === 'error') {
        onError(String(data.message ?? 'script error'));
      }
    }
    window.addEventListener('message', onMessage);
    return () => window.removeEventListener('message', onMessage);
  });
</script>

<div class="frame-wrap" bind:this={wrapEl}>
  <div class="frame-box" style="width: {boxW}px; height: {boxH}px;">
    {#key runId}
      <iframe bind:this={frameEl} src="./runner.html" title="EmotionFX sample" class="frame"></iframe>
    {/key}
  </div>
</div>

<style>
  .frame-wrap {
    width: 100%;
    height: 100%;
    display: flex;
    align-items: center;
    justify-content: center;
  }

  .frame-box {
    border: 1px solid var(--efx-edge);
    border-radius: 3px;
    overflow: hidden;
    box-shadow: 0 0 24px rgba(53, 224, 255, 0.12), inset 0 0 0 1px rgba(0, 0, 0, 0.6);
    background: #02030a;
  }

  .frame {
    display: block;
    width: 100%;
    height: 100%;
    border: 0;
  }
</style>
