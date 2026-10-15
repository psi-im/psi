// Test the production helper's bridge gating and error/launch lifecycle.
const assert = require('assert');
const fs = require('fs');
const vm = require('vm');
const source = fs.readFileSync(process.argv[2], 'utf8');
const begin = source.indexOf('addSharedMediaActions: function(');
const end = source.indexOf('\n        replaceYoutube', begin);
assert(begin >= 0 && end > begin);
class Element {
    constructor(tag) {
        this.tagName = tag.toUpperCase();
        this.children = []; this.listeners = {}; this.attributes = {};
        const classes = new Set();
        this.classList = { add: (...v) => v.forEach(x => classes.add(x)), remove: x => classes.delete(x), contains: x => classes.has(x) };
    }
    appendChild(child) { this.children.push(child); }
    setAttribute(key, value) { this.attributes[key] = value; }
    addEventListener(event, callback) { this.listeners[event] = callback; }
    querySelector() { return this.source; }
    emit(event) { this.listeners[event]?.({ preventDefault() {}, stopPropagation() {} }); }
    pause() { this.paused = true; }
}
const head = new Element('head');
const calls = [];
const session = {
    sharedMediaStrings: { open: 'open', choose: 'choose', unsupported: 'unsupported', network: 'network', decode: 'decode', failed: 'failed' },
    openSharedMedia: (...args) => calls.push(args)
};
const urlBegin = source.indexOf('sharedMediaUrl: function(');
const urlEnd = source.indexOf('\n        videoPreview:', urlBegin);
const mediaUrl = vm.runInNewContext('({' + source.slice(urlBegin, urlEnd) + '})', {
    URL, document: { baseURI: 'http://localhost:1234/chat' }, session
}).sharedMediaUrl;
const plain = '/psi/account/test/sharedfile/sha-1+abcdef';
assert.equal(mediaUrl(plain, true), 'http://localhost:1234' + plain + '?mode=stream');
assert.equal(mediaUrl(mediaUrl(plain, true), false), 'http://localhost:1234' + plain + '?mode=download');
assert.equal(mediaUrl('https://example.org/movie.webm', true), 'https://example.org/movie.webm');
const helper = vm.runInNewContext('({' + source.slice(begin, end) + '})', {
    document: { createElement: tag => new Element(tag), head,
        getElementById: id => head.children.find(child => child.id === id) }, session
}).addSharedMediaActions;
for (const kind of ['video', 'audio']) {
    const media = new Element(kind);
    media.controls = kind === 'video';
    media.source = new Element('source');
    const container = new Element('div');
    helper(media, container, mediaUrl(plain, true));
    assert(container.classList.contains('psi-shared-media-' + kind));
    const [status, buttons] = container.children[0].children;
    const [open, choose] = buttons.children;
    assert.equal(open.tagName, 'BUTTON');
    assert.equal(open.type, 'button');
    assert.equal(open.attributes['aria-label'], 'open');
    assert(status.hidden);
    const before = calls.length;
    media.source.emit('error');
    assert.equal(status.textContent, 'failed');
    assert(container.classList.contains('psi-media-failed'));
    assert.equal(media.controls, false);
    assert.equal(calls.length, before); // Never launch automatically on failure.
    for (const [code, message] of [[2, 'network'], [3, 'decode'], [4, 'unsupported']]) {
        media.error = { code }; media.emit('error');
        assert.equal(status.textContent, message);
        assert(!status.hidden);
    }
    media.emit('canplay');
    assert(status.hidden);
    assert(!container.classList.contains('psi-media-failed'));
    assert.equal(media.controls, kind === 'video');
    media.error = { code: 1 }; media.emit('error');
    assert(status.hidden);
    open.emit('click'); choose.emit('click');
    assert(media.paused);
}
assert.deepEqual(calls, [['sha-1+abcdef', false], ['sha-1+abcdef', true], ['sha-1+abcdef', false], ['sha-1+abcdef', true]]);
assert.equal(head.children.length, 1); // One shared stylesheet per document.
const outside = new Element('div');
helper(new Element('video'), outside, 'https://example.org/video.webm');
assert.equal(outside.children.length, 0);
delete session.openSharedMedia;
assert.equal(mediaUrl(plain, true), plain);
const legacy = new Element('div');
helper(new Element('audio'), legacy, '/psi/account/test/sharedfile/sha-1+abcdef');
assert.equal(legacy.children.length, 0); // No WebKit/Qite UI changes.
console.log('Shared-media actions: passed');
