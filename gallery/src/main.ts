import { mount } from 'svelte';
import './design/tokens.css';
import App from './App.svelte';

const target = document.getElementById('app');
if (!target) {
  throw new Error('gallery: missing #app mount point');
}

const app = mount(App, { target });

export default app;
