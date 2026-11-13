import { fileURLToPath } from "node:url";
import { defineConfig } from "astro/config";
import { unified } from "@astrojs/markdown-remark";
import { remarkAlert } from "remark-github-blockquote-alert";

// Docs live at the repo root (../docs), outside the Astro project root, so Vite's
// file watcher never sees them and edits to docs/**/*.md don't hot reload in dev.
// Add the docs directory to the dev watcher so content edits invalidate and refresh.
const docsDir = fileURLToPath(new URL("../docs", import.meta.url));

function watchExternalDocs() {
  return {
    name: "crossink:watch-external-docs",
    apply: "serve",
    configureServer(server) {
      server.watcher.add(docsDir);
    },
  };
}

function rewriteMarkdownLinks() {
  return (tree) => {
    visit(tree, (node) => {
      if (node.type !== "link" && node.type !== "image") {
        return;
      }

      if (typeof node.url !== "string") {
        return;
      }

      const rootDocUrl = node.url
        .replace(/^(\.\.\/)+SCOPE\.md(#.*)?$/i, "https://github.com/smashedllama/retroink/blob/main/SCOPE.md$2")
        .replace(
          /^(\.\.\/)+GOVERNANCE\.md(#.*)?$/i,
          "https://github.com/smashedllama/retroink/blob/main/GOVERNANCE.md$2",
        );
      if (rootDocUrl !== node.url) {
        node.url = rootDocUrl;
        return;
      }

      node.url = node.url.replace(/(^|\/)README\.md(#.*)?$/i, "$1index.html$2").replace(/\.md(#.*)?$/i, ".html$1");
    });
  };
}

function visit(node, visitor) {
  visitor(node);
  if (!Array.isArray(node.children)) {
    return;
  }
  for (const child of node.children) {
    visit(child, visitor);
  }
}

export default defineConfig({
  // Astro 7 defaults this to "jsx", which drops whitespace between inline
  // elements. Keep the HTML-aware compression this site was built with.
  compressHTML: true,
  build: {
    format: "file",
  },
  markdown: {
    // Astro 7's default processor is Sätteri, which does not run remark
    // plugins. This site rewrites doc links and renders GitHub alerts.
    processor: unified({
      remarkPlugins: [remarkAlert, rewriteMarkdownLinks],
    }),
  },
  vite: {
    plugins: [watchExternalDocs()],
    build: {
      // Vite 8 minifies CSS with Lightning CSS, which rewrites the
      // JetBrains Mono latin unicode-range U+0000-00FF into the invalid
      // token U+??. esbuild leaves that range intact.
      cssMinify: "esbuild",
    },
  },
  // Deployed as a GitHub Pages project site (github.com/smashedllama/retroink),
  // which serves from a /retroink subpath, not the domain root.
  site: "https://smashedllama.github.io",
  base: "/retroink",
});
