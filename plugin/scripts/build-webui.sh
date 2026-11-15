#!/bin/bash
# Assembles plugin/WebUI/ from DM5Editor.html: precompiled JSX plus local React and fonts.
set -euo pipefail
cd "$(dirname "$0")/../.."

node - <<'EOF'
const fs = require('fs');
const path = require('path');
const esbuild = require('esbuild');

const out = 'plugin/WebUI';
const vendor = path.join(out, 'vendor');
fs.rmSync(out, { recursive: true, force: true });
fs.mkdirSync(path.join(vendor, 'fonts'), { recursive: true });

const html = fs.readFileSync('DM5Editor.html', 'utf8');
const open = '<script type="text/babel">';
const start = html.indexOf(open);
const end = html.indexOf('</script>', start);
if (start < 0 || end < 0) throw new Error('JSX script block not found in DM5Editor.html');
const jsx = html.slice(start + open.length, end);
const { code } = esbuild.transformSync(jsx, { loader: 'jsx', target: 'es2020' });
if (code.includes('</script')) throw new Error('compiled code contains </script');
fs.writeFileSync(path.join(out, 'index.html'),
  html.slice(0, start) + '<script>\n' + code + html.slice(end));

const copy = (from, to) => fs.copyFileSync(path.join('node_modules', from), path.join(vendor, to));
copy('react/umd/react.production.min.js', 'react.production.min.js');
copy('react-dom/umd/react-dom.production.min.js', 'react-dom.production.min.js');

const fonts = [
  ['Orbitron', 'orbitron', [400, 700, 900]],
  ['Share Tech Mono', 'share-tech-mono', [400]],
  ['Rajdhani', 'rajdhani', [400, 500, 600, 700]],
];
let css = '';
for (const [family, pkg, weights] of fonts)
  for (const w of weights) {
    const file = `${pkg}-latin-${w}-normal.woff2`;
    copy(`@fontsource/${pkg}/files/${file}`, `fonts/${file}`);
    css += `@font-face{font-family:'${family}';font-style:normal;font-weight:${w};font-display:swap;src:url(fonts/${file}) format('woff2');}\n`;
  }
fs.writeFileSync(path.join(vendor, 'fonts.css'), css);
console.log('Built ' + out);
EOF
