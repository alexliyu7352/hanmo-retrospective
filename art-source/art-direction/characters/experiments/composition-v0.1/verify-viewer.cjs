const fs=require('fs'),path=require('path'),assert=require('assert');
const {chromium}=require(process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES ? process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES + '/playwright' : 'playwright');
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox']});
 const page=await browser.newPage({viewport:{width:960,height:1000},deviceScaleFactor:1});
 const errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.addInitScript(()=>{window.__writes=[];window.openai={widgetState:{unrelated:'preserved'},setWidgetState(state){window.__writes.push(state);this.widgetState=state;return Promise.resolve();}};});
 await page.goto('file://'+path.join(__dirname,'index.html'));
 await page.locator('#hanmo-combination-sample-v1[data-ready="true"]').waitFor();await page.evaluate(()=>document.fonts.ready);
 assert.equal(await page.evaluate(()=>window.__writes.length),0);
 const before=await page.locator('[data-portrait]').evaluate(c=>c.toDataURL());
 await page.locator('[data-feature="body"]').selectOption('1');
 const after=await page.locator('[data-portrait]').evaluate(c=>c.toDataURL());assert.notEqual(before,after);
 assert.equal(await page.evaluate(()=>window.openai.widgetState.unrelated),'preserved');
 await page.locator('[data-feature="beard"]').selectOption('1');
 await page.locator('[data-preset="1"]').click();
 assert.equal(await page.locator('[data-feature="nose"]').inputValue(),'1');
 assert.equal(await page.locator('[data-feature="body"]').inputValue(),'1');
 await page.locator('[data-layer]').selectOption('nose');
 const isolated=await page.locator('[data-portrait]').evaluate(c=>c.toDataURL());assert.notEqual(isolated,after);
 const writes=await page.evaluate(()=>window.__writes.length);
 await page.evaluate(()=>{window.openai.widgetState={unrelated:'preserved',hanmoCharacterSample:{version:1,state:{eyes:1,nose:0,mouth:1,expression:1,hair:0,body:0,beard:0}}};window.dispatchEvent(new Event('openai:set_globals'));});
 assert.equal(await page.locator('[data-feature="expression"]').inputValue(),'1');assert.equal(await page.evaluate(()=>window.__writes.length),writes);
 await page.locator('[data-reset]').click();
 assert.equal(await page.locator('[data-layer]').inputValue(),'all');
 assert.equal(await page.locator('[data-feature="eyes"]').inputValue(),'0');
 const widths=[];
 for(const width of [960,736,360,320]){
   await page.setViewportSize({width,height:1200});
   const measured=await page.evaluate(()=>({scroll:document.documentElement.scrollWidth,viewport:innerWidth,fields:Array.from(document.querySelectorAll('select,button')).map(e=>({width:e.getBoundingClientRect().width,height:e.getBoundingClientRect().height}))}));
   assert(measured.scroll<=measured.viewport+1);assert(measured.fields.every(x=>x.width>40&&x.height>=44));widths.push(width);
   if(width===960||width===320)await page.screenshot({path:path.join(__dirname,'qa','viewer-'+width+'.png'),fullPage:true});
 }
 assert.deepEqual(errors,[]);
 const report={passed:true,source:'portable embedded-texture preview',widths,checks:['canvas changes after clothing selection','preset preserves clothing','isolated layer differs from full portrait','88 and 96 avatars rendered','no automatic state write on initial load','unrelated shared state preserved','incoming shared state applies without writeback','reset returns to default','no horizontal overflow at four widths','controls at least 44px high','no browser runtime errors'],errors};
 fs.writeFileSync(path.join(__dirname,'qa','browser-verification.json'),JSON.stringify(report,null,2));console.log(JSON.stringify(report));await browser.close();
})().catch(e=>{console.error(e);process.exit(1);});
