(function (root) {
  'use strict';
  const manifest = {
    version: '0.1.0',
    scope: 'adult-medium-male-calm-pose',
    canvas: [512, 768],
    head: { x: 80, y: 22, width: 352, height: 352 },
    avatar: { x: 96, y: 0, width: 352, height: 352 },
    sourceGrids: { head: [2, 3], body: [2, 1], hair: [2, 1], beard: [1, 1] },
    patches: {
      eyes: { x: 177, y: 138, width: 226, height: 126, feather: 18 },
      nose: { x: 231, y: 214, width: 130, height: 112, feather: 18 },
      mouth: { x: 209, y: 298, width: 154, height: 92, feather: 18 }
    },
    hairRegistration: [
      { scale: 0.95, x: -28, y: -30 },
      { scale: 0.84, x: 38, y: -30 }
    ],
    features: { eyes: ['锐长眉眼', '舒展眉眼'], nose: ['直鼻', '圆鼻'], mouth: ['薄唇', '丰唇'] },
    defaults: { eyes: 0, nose: 0, mouth: 0, expression: 0, hair: 1, body: 0, beard: 0 },
    layerOrder: ['head', 'nose', 'eyes', 'mouth', 'beard', 'body', 'hair'],
    expressionTiles: [[1, 3], [2, 4]],
    unsupported: ['other-face-families', 'children', 'age-mapping', 'skin-tint', 'independent-front-back-hair', 'additional-body-poses', 'godot-integration']
  };

  function makeRenderer(makeCanvas, images) {
    const tile = (name, index) => {
      const grid = manifest.sourceGrids[name], image = images[name];
      const w = image.width / grid[0], h = image.height / grid[1];
      const c = makeCanvas(512, name === 'body' ? 768 : 512);
      c.getContext('2d').drawImage(image, index % grid[0] * w, Math.floor(index / grid[0]) * h,
        w, h, 0, 0, c.width, c.height);
      return c;
    };
    const heads = Array.from({ length: 6 }, (_, i) => tile('head', i));
    const bodies = [tile('body', 0), tile('body', 1)];
    // The generated atlases contain a few alpha=1 specks in the reserved head
    // area; this explicit empty region is part of the garment slot contract.
    bodies.forEach(c => c.getContext('2d').clearRect(0, 0, 512, 298));
    const hairs = [tile('hair', 0), tile('hair', 1)];
    const beard = tile('beard', 0), layerCache = new Map();
    // Isolate a stray pixel sliver from the adjacent atlas cell, not part of the headwear.
    hairs[1].getContext('2d').clearRect(0, 0, 20, 512);
    const cache = new Map();
    function patch(kind, index) {
      const key = kind + ':' + index;
      if (cache.has(key)) return cache.get(key);
      const c = makeCanvas(512, 512), ctx = c.getContext('2d');
      ctx.drawImage(heads[index], 0, 0);
      const pixels = ctx.getImageData(0, 0, 512, 512), d = pixels.data, p = manifest.patches[kind];
      const smooth = n => { n = Math.max(0, Math.min(1, n)); return n * n * (3 - 2 * n); };
      for (let y = 0; y < 512; y++) for (let x = 0; x < 512; x++) {
        const alpha = smooth((x - p.x) / p.feather) * smooth((p.x + p.width - x) / p.feather)
          * smooth((y - p.y) / p.feather) * smooth((p.y + p.height - y) / p.feather);
        d[(y * 512 + x) * 4 + 3] *= alpha;
      }
      ctx.putImageData(pixels, 0, 0);
      cache.set(key, c);
      return c;
    }
    function validState(input) {
      const state = {};
      for (const key of Object.keys(manifest.defaults)) {
        const value = input && input[key];
        state[key] = value === 0 || value === 1 ? value : manifest.defaults[key];
      }
      return state;
    }
    function layer(name, state) {
      state = validState(state);
      const key = name + ':' + (state[name] || 0) + ':' + (['eyes', 'mouth'].includes(name) ? state.expression : 0);
      if (layerCache.has(key)) return layerCache.get(key);
      const c = makeCanvas(512, 768), ctx = c.getContext('2d'), h = manifest.head;
      if (name === 'body') ctx.drawImage(bodies[state.body], 0, 0);
      else if (name === 'beard') { if (state.beard) ctx.drawImage(beard, h.x, h.y, h.width, h.height); }
      else if (name === 'hair') {
        const r = manifest.hairRegistration[state.hair], factor = h.width / 512;
        ctx.drawImage(hairs[state.hair], h.x + r.x * factor, h.y + r.y * factor,
          h.width * r.scale, h.height * r.scale);
      } else {
        let source = heads[0];
        if (name !== 'head') {
          const expression = name === 'nose' ? 0 : state.expression;
          source = patch(name, manifest.expressionTiles[state[name]][expression]);
        }
        ctx.drawImage(source, h.x, h.y, h.width, h.height);
      }
      if (name === 'head') {
        // The garment owns the lower-neck interface. Keep a shared narrow neck
        // bridge and hide the source head's broad shoulder stub below the jaw.
        const pixels = ctx.getImageData(0, 0, 512, 768), d = pixels.data;
        for (let y = 0; y < 768; y++) for (let x = 0; x < 512; x++) {
          // Both fitted headwear pieces cover the scalp above this shared line.
          if (y < 104) d[(y * 512 + x) * 4 + 3] = 0;
          if (y < 230) continue;
          const t = Math.max(0, Math.min(1, (y - 230) / 80));
          const left = 186 + t * 56, right = 360 - t * 30;
          const edge = Math.max(0, Math.min(1, (x - left) / 1.5, (right - x) / 1.5));
          d[(y * 512 + x) * 4 + 3] *= edge;
        }
        ctx.putImageData(pixels, 0, 0);
      }
      if (['eyes', 'nose', 'mouth'].includes(name)) {
        ctx.globalCompositeOperation = 'destination-in';
        ctx.drawImage(layer('head', state), 0, 0);
        ctx.globalCompositeOperation = 'source-over';
      }
      layerCache.set(key, c);
      return c;
    }
    function compose(input, hidden) {
      const state = validState(input), c = makeCanvas(512, 768), ctx = c.getContext('2d');
      for (const name of manifest.layerOrder) if (!(hidden || []).includes(name)) ctx.drawImage(layer(name, state), 0, 0);
      return c;
    }
    function avatar(input, size) {
      const c = makeCanvas(size, size), r = manifest.avatar;
      const portrait = compose(input);
      c.getContext('2d').drawImage(portrait, r.x, r.y, r.width, r.height, 0, 0, size, size);
      portrait.width = 1; portrait.height = 1;
      return c;
    }
    function states() {
      return Array.from({ length: 128 }, (_, mask) => Object.fromEntries(
        Object.keys(manifest.defaults).map((key, bit) => [key, (mask >> bit) & 1])));
    }
    return { manifest, validState, compose, avatar, layer, states, headTiles: heads };
  }
  root.HanmoSample = { manifest, makeRenderer };
  if (typeof module !== 'undefined') module.exports = root.HanmoSample;
})(typeof globalThis !== 'undefined' ? globalThis : this);
