const { chromium } = require('/opt/npm-tools/node_modules/playwright');
const fs = require('fs'), path = require('path');
(async () => {
  const browser = await chromium.launch({ executablePath: '/opt/pw-browsers/chromium-1194/chrome-linux/chrome' });
  const page = await browser.newPage();
  await page.goto('file://' + path.resolve(__dirname, '../dist/local/index.html'));
  fs.writeFileSync(process.argv[2], await page.evaluate(() => JSON.stringify(window.__impakt.demoState())));
  await browser.close();
})();
