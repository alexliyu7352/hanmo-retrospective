const fs = require('fs');
const path = require('path');
const { createCanvas, loadImage, GlobalFonts } = require(process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES ? process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES + '/@napi-rs/canvas' : '@napi-rs/canvas');
const { makeRenderer } = require('./renderer.js');
GlobalFonts.registerFromPath(path.join(__dirname, 'qa', 'hanmo-labels.ttf'), 'HanmoSans');

(async () => {
  const images = {};
  for (const name of ['head', 'body', 'hair', 'beard']) images[name] = await loadImage(path.join(__dirname, 'assets', name + '-atlas.png'));
  const renderer = makeRenderer(createCanvas, images);
  fs.writeFileSync(path.join(__dirname, 'manifest.json'), JSON.stringify(renderer.manifest, null, 2));
  const presets = [
    { ...renderer.manifest.defaults },
    { ...renderer.manifest.defaults, body: 1 },
    { ...renderer.manifest.defaults, hair: 0 },
    { ...renderer.manifest.defaults, beard: 1 },
    { ...renderer.manifest.defaults, expression: 1 },
    { ...renderer.manifest.defaults, eyes: 1 },
    { ...renderer.manifest.defaults, nose: 1, mouth: 1 },
    { ...renderer.manifest.defaults, eyes: 1, nose: 1, mouth: 1, body: 1, hair: 0, expression: 1 }
  ];
  const canvas = createCanvas(2048, 1664), ctx = canvas.getContext('2d');
  ctx.fillStyle = '#e9e0cd'; ctx.fillRect(0, 0, canvas.width, canvas.height);
  presets.forEach((state, i) => {
    const x = i % 4 * 512, y = Math.floor(i / 4) * 832;
    ctx.drawImage(renderer.compose(state), x, y);
    ctx.fillStyle = '#252a28'; ctx.font = '24px HanmoSans';
    ctx.fillText(['01 初始', '02 行旅衣身', '03 束发', '04 短须', '05 微笑', '06 舒展眉眼', '07 圆鼻丰唇', '08 五官方案 B'][i], x + 24, y + 800);
    for (const size of [88, 96]) {
      fs.writeFileSync(path.join(__dirname, 'exports', `avatar-${i+1}-${size}.png`), renderer.avatar(state, size).toBuffer('image/png'));
    }
    fs.writeFileSync(path.join(__dirname, 'exports', `portrait-${i+1}.png`), renderer.compose(state).toBuffer('image/png'));
  });
  fs.writeFileSync(path.join(__dirname, 'qa', 'composition-check.png'), canvas.toBuffer('image/png'));
  const base = renderer.compose(renderer.manifest.defaults);
  fs.writeFileSync(path.join(__dirname, 'qa', 'base.png'), base.toBuffer('image/png'));
  const layerNames = renderer.manifest.layerOrder;
  const layers = createCanvas(2048, 1536), lctx = layers.getContext('2d');
  lctx.fillStyle = '#e9e0cd';lctx.fillRect(0,0,2048,1536);
  layerNames.forEach((name,i)=>lctx.drawImage(renderer.layer(name,{...renderer.manifest.defaults,beard:1}),i%4*512,Math.floor(i/4)*768));
  fs.writeFileSync(path.join(__dirname,'qa','layers-check.png'),layers.toBuffer('image/png'));
  console.log(JSON.stringify({files:['qa/composition-check.png','qa/base.png','qa/layers-check.png'],presets:presets.length,states:renderer.states().length}));
})();
