// Dump the demo pattern as engine ops (text) so the native harness can render it.
const { chromium } = require('/opt/npm-tools/node_modules/playwright');
const fs = require('fs'), path = require('path');
(async () => {
  const browser = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium-1194/chrome-linux/chrome' });
  const page = await browser.newPage();
  await page.goto('file://' + path.resolve(__dirname, '../dist/local/index.html'));
  const ops = await page.evaluate(() => window.__impakt.stateOps(window.__impakt.demoState()));
  fs.writeFileSync(process.argv[2], ops.map(o => o.join(' ')).join('\n') + '\n');
  console.log(ops.length, 'ops');
  await browser.close();
})();
