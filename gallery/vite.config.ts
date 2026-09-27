import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';

// `base: './'` keeps the built site portable under a GitHub Pages project
// subpath (e.g. /emotion-fx/) as well as at a domain root.
export default defineConfig({
  base: './',
  plugins: [svelte()],
  build: {
    target: 'es2020',
    outDir: 'dist',
    emptyOutDir: true,
    // Emscripten's player_web.* are copied in as static assets by
    // scripts/prepare-player.mjs; never inline them into the JS bundle.
    assetsInlineLimit: 0,
  },
});
