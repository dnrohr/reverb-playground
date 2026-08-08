import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  base: './',
  plugins: [react()],
  build: {
    outDir: '../src/ui/WebAssets',
    emptyOutDir: true,
    cssCodeSplit: false,
    rollupOptions: {
      output: {
        entryFileNames: 'editor.js',
        assetFileNames: (assetInfo) => assetInfo.names.some((name) => name.endsWith('.css')) ? 'editor.css' : 'asset-[name][extname]',
        chunkFileNames: 'chunk-[name].js'
      }
    }
  }
});
