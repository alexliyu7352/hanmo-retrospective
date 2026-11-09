const fs=require('fs'),path=require('path');
const {createCanvas,loadImage}=require(process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES?process.env.CODEX_PRIMARY_RUNTIME_NODE_MODULES+'/@napi-rs/canvas':'@napi-rs/canvas');
(async()=>{
 const c=createCanvas(1024,832),ctx=c.getContext('2d');ctx.fillStyle='#e9e0cd';ctx.fillRect(0,0,c.width,c.height);
 const left=await loadImage(path.join(__dirname,'exports','portrait-4.png')),right=await loadImage(path.join(__dirname,'assets','dedicated-target.png'));
 ctx.drawImage(left,0,0,512,768);ctx.drawImage(right,512,0,512,768);ctx.font='24px sans-serif';ctx.fillStyle='#252a28';ctx.fillText('A',244,806);ctx.fillText('B',756,806);
 fs.writeFileSync(path.join(__dirname,'qa','assembly-vs-target.png'),c.toBuffer('image/png'));
 const thumbs=createCanvas(1024,292),t=thumbs.getContext('2d');t.fillStyle='#e9e0cd';t.fillRect(0,0,1024,292);t.font='18px sans-serif';t.fillStyle='#252a28';
 for(let i=1;i<=8;i++)for(const [row,size] of [[0,88],[1,96]]){const image=await loadImage(path.join(__dirname,'exports',`avatar-${i}-${size}.png`));const x=(i-1)*128+(128-size)/2,y=row*144;t.drawImage(image,x,y);t.fillText(String(i).padStart(2,'0')+' / '+size,(i-1)*128+26,y+size+26);}
 fs.writeFileSync(path.join(__dirname,'qa','avatar-sizes.png'),thumbs.toBuffer('image/png'));
 console.log('Review images rendered from actual composed portraits.');
})();
