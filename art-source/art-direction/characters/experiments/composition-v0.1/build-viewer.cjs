const fs = require('fs');
const path = require('path');
const sharp = require(process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES ? process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES + '/sharp' : 'sharp');
(async()=>{
  const folder=__dirname, rawFragment=fs.readFileSync(path.join(folder,'viewer.fragment.html'),'utf8');
  const font=fs.readFileSync(path.join(folder,'qa','hanmo-labels.ttf')).toString('base64');
  const fragment=rawFragment.replace('<style>',"<style>@font-face{font-family:HanmoSans;src:url(data:font/ttf;base64,"+font+") format('truetype');font-weight:400;font-style:normal;}");
  const renderer=fs.readFileSync(path.join(folder,'renderer.js'),'utf8');
  const files=['head','body','hair','beard'];
  const standalone=fragment.replace('<!--SAMPLE_RENDERER-->','<script src="renderer.js"></script>')
    .replace('<!--SAMPLE_TEXTURES-->','<script>window.HANMO_SAMPLE_TEXTURES='+JSON.stringify(Object.fromEntries(files.map(n=>[n,'assets/'+n+'-atlas.png'])))+';</script>');
  fs.writeFileSync(path.join(folder,'full-quality.html'),'<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>汉末人物组合样板</title><style>body{margin:24px;background:#f7f5ef;}@media(max-width:650px){body{margin:12px;}}</style>'+standalone+'</html>');
  let textures={}, quality=72, output;
  while(true) {
    for(const name of files) {
      const widths={head:640,body:768,hair:768,beard:512};
      const buffer=await sharp(path.join(folder,'assets',name+'-atlas.png')).resize({width:widths[name]}).webp({quality,alphaQuality:100,effort:6}).toBuffer();
      textures[name]='data:image/webp;base64,'+buffer.toString('base64');
    }
    output=fragment.replace('<!--SAMPLE_RENDERER-->','<script>'+renderer+'</script>')
      .replace('<!--SAMPLE_TEXTURES-->','<script>window.HANMO_SAMPLE_TEXTURES='+JSON.stringify(textures)+';</script>');
    if(Buffer.byteLength(output)<252000||quality<40) break;
    quality-=6;
  }
  if(Buffer.byteLength(output)>262144)throw new Error('Page embed exceeds 256 KiB');
  fs.writeFileSync(path.join(folder,'page-preview.html'),output);
  fs.writeFileSync(path.join(folder,'index.html'),'<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>汉末人物组合样板</title><style>body{margin:12px;background:#f7f5ef;}</style>'+output+'</html>');
  fs.writeFileSync(path.join(folder,'qa','embed-test.html'),'<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><style>body{margin:12px;background:#f7f5ef;}</style>'+output+'</html>');
  console.log(JSON.stringify({pageBytes:Buffer.byteLength(output),previewQuality:quality,embeddedTextures:files.length}));
})();
