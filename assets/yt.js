// Runs inside the hidden youtube.com page. Togu calls window.toguYt.run(id, fn, args); answers come back
// as console messages "togu-yt:<id>:<json>". Requests go out as the page's own fetch(), with the
// page's cookies and context, so they are the signed-in user's browsing.
(() => {
  if (window.toguYt) return;

  const ctx = () => {
    const c = JSON.parse(JSON.stringify(ytcfg.get('INNERTUBE_CONTEXT')));
    c.client.hl = 'ja';
    c.client.gl = 'JP';
    return c;
  };

  const cookie = name => {
    const m = document.cookie.match(new RegExp('(?:^|; )' + name.replace(/[$()*+.?[\\\]^{|}]/g, '\\$&') + '=([^;]*)'));
    return m ? decodeURIComponent(m[1]) : '';
  };

  const sha1 = async text => {
    const buf = await crypto.subtle.digest('SHA-1', new TextEncoder().encode(text));
    return Array.from(new Uint8Array(buf)).map(b => b.toString(16).padStart(2, '0')).join('');
  };

  // The Authorization header youtube.com's own scripts send when signed in.
  const auth = async () => {
    const sid = cookie('SAPISID') || cookie('__Secure-3PAPISID');
    if (!sid) return {};
    const ts = Math.floor(Date.now() / 1000);
    const origin = location.origin;
    const h = async n => `${n} ${ts}_${await sha1(`${ts} ${n === 'SAPISIDHASH' ? sid : (cookie(n === 'SAPISID1PHASH' ? '__Secure-1PAPISID' : '__Secure-3PAPISID') || sid)} ${origin}`)}`;
    return {
      Authorization: [await h('SAPISIDHASH'), await h('SAPISID1PHASH'), await h('SAPISID3PHASH')].join(' '),
      'X-Goog-AuthUser': ytcfg.get('SESSION_INDEX') || '0',
      'X-Origin': origin,
    };
  };

  const call = async (ep, body) => {
    const c = ctx();
    const headers = Object.assign({
      'Content-Type': 'application/json',
      'X-Youtube-Client-Name': String(ytcfg.get('INNERTUBE_CONTEXT_CLIENT_NAME') || 1),
      'X-Youtube-Client-Version': c.client.clientVersion,
    }, await auth());
    const vd = ytcfg.get('VISITOR_DATA');
    if (vd) headers['X-Goog-Visitor-Id'] = vd;
    const r = await fetch('/youtubei/v1/' + ep + '?prettyPrint=false', {
      method: 'POST', credentials: 'include', headers, body: JSON.stringify(Object.assign({ context: c }, body)),
    });
    if (!r.ok) throw new Error(ep + ': HTTP ' + r.status);
    return r.json();
  };

  // ---------------------------------------------------------------- parsing
  const text = t => !t ? '' : typeof t === 'string' ? t : t.simpleText || t.content || (t.runs || []).map(r => r.text).join('');
  const thumb = id => `https://i.ytimg.com/vi/${id}/mqdefault.jpg`;
  const seconds = s => {
    if (!s) return 0;
    let n = 0;
    for (const p of String(s).split(':')) n = n * 60 + (parseInt(p, 10) || 0);
    return n;
  };

  const fromVideoRenderer = v => {
    const owner = (v.ownerText || v.longBylineText || v.shortBylineText || {});
    const nav = ((owner.runs || [])[0] || {}).navigationEndpoint || {};
    const dur = text(v.lengthText) || (((v.thumbnailOverlays || []).find(o => o.thumbnailOverlayTimeStatusRenderer) || {}).thumbnailOverlayTimeStatusRenderer || {}).text;
    const avatar = (((((v.channelThumbnailSupportedRenderers || {}).channelThumbnailWithLinkRenderer || {}).thumbnail || {}).thumbnails) || [])[0];
    return {
      id: v.videoId,
      title: text(v.title),
      channel: text(owner),
      channelId: (nav.browseEndpoint || {}).browseId || '',
      avatar: avatar ? avatar.url : '',
      duration: seconds(text(dur)),
      views: text(v.shortViewCountText || v.viewCountText),
      published: text(v.publishedTimeText),
      live: !!(v.badges || []).find(b => JSON.stringify(b).includes('LIVE')),
      thumb: thumb(v.videoId),
    };
  };

  // YouTube's newer card format.
  const fromLockup = l => {
    if (l.contentType && l.contentType !== 'LOCKUP_CONTENT_TYPE_VIDEO') return null;
    const md = ((l.metadata || {}).lockupMetadataViewModel) || {};
    const rows = ((((md.metadata || {}).contentMetadataViewModel) || {}).metadataRows) || [];
    // Rows mix channel, views and age in varying order; tell them apart by what they say.
    let channel = '', views = '', published = '';
    for (const r of rows) for (const p of (r.metadataParts || [])) {
      const t = text(p.text), label = p.accessibilityLabel || t;
      if (/視聴|views?|watching/i.test(label) || (p.leadingIcon && /PLAY/.test(p.leadingIcon.name || ''))) views = views || label;
      else if (/前|ago|配信|Streamed|Premiere/i.test(t)) published = published || t;
      else if (t && !channel) channel = t;
    }
    let channelId = '';
    JSON.stringify(md, (k, v) => { if (k === 'browseId' && !channelId && String(v).startsWith('UC')) channelId = v; return v; });
    let duration = 0;
    JSON.stringify(l.contentImage || {}, (k, v) => {
      if (k === 'thumbnailBadgeViewModel' && !duration) duration = seconds(text(v.text));
      return v;
    });
    let avatar = '';
    JSON.stringify(md.image || {}, (k, v) => { if (k === 'url' && !avatar) avatar = v; return v; });
    return {
      id: l.contentId,
      title: text(md.title),
      channel,
      channelId,
      avatar,
      duration,
      views,
      published,
      live: false,
      thumb: thumb(l.contentId),
    };
  };

  // Every video in a response (shorts and ads skipped) and the next-page token.
  const collect = data => {
    const videos = [];
    const seen = new Set();
    let next = '';
    const walk = o => {
      if (Array.isArray(o)) { o.forEach(walk); return; }
      if (!o || typeof o !== 'object') return;
      if (o.videoRenderer && o.videoRenderer.videoId) {
        const v = fromVideoRenderer(o.videoRenderer);
        if (!seen.has(v.id)) { seen.add(v.id); videos.push(v); }
        return;
      }
      if (o.lockupViewModel) {
        const v = fromLockup(o.lockupViewModel);
        if (v && v.id && !seen.has(v.id)) { seen.add(v.id); videos.push(v); }
        return;
      }
      if (o.reelShelfRenderer || o.shortsLockupViewModel || o.adSlotRenderer || o.reelItemRenderer) return;
      if (o.continuationCommand && o.continuationCommand.token) next = o.continuationCommand.token;
      for (const k in o) walk(o[k]);
    };
    walk(data);
    return { videos, next };
  };

  // ---------------------------------------------------------------- API
  const api = {
    async state() {
      const signedIn = !!ytcfg.get('LOGGED_IN');
      if (!signedIn) return { signedIn };
      let name = '', avatar = '';
      try {
        const m = await call('account/account_menu', {});
        JSON.stringify(m, (k, v) => {
          if (k === 'accountName' && !name) name = text(v);
          if (k === 'accountPhoto' && !avatar) avatar = ((v.thumbnails || [])[0] || {}).url || '';
          return v;
        });
      } catch (e) { /* the name is only for show */ }
      return { signedIn, name, avatar };
    },
    async browse(browseId, params, next) {
      const data = await call('browse', next ? { continuation: next } : Object.assign({ browseId }, params ? { params } : {}));
      return collect(data);
    },
    home(next) { return api.browse('FEwhat_to_watch', '', next); },
    subscriptions(next) { return api.browse('FEsubscriptions', '', next); },
    // A channel's uploads, newest first.
    channel(id, next) { return api.browse(id, 'EgZ2aWRlb3PyBgQKAjoA', next); },
    async search(query, next) {
      // "EgIQAQ" = videos only.
      return collect(await call('search', next ? { continuation: next } : { query, params: 'EgIQAQ%3D%3D' }));
    },
  };

  window.toguYt = {
    run(id, fn, args) {
      Promise.resolve()
        .then(() => api[fn](...(args || [])))
        .then(r => console.log('togu-yt:' + id + ':' + JSON.stringify({ ok: true, data: r })))
        .catch(e => console.log('togu-yt:' + id + ':' + JSON.stringify({ ok: false, error: String(e && e.message || e) })));
    },
  };
  console.log('togu-yt:ready:' + JSON.stringify({ ok: true }));
})();
