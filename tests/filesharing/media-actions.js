// Exercise the production helper with a small DOM double; no browser dependency.
const assert = require('assert');
const fs = require('fs');
const vm = require('vm');
const source = fs.readFileSync(process.argv[2], 'utf8');
const begin = source.indexOf('addSharedMediaActions: function(');
const end = source.indexOf('\n        replaceYoutube', begin);
assert(begin >= 0 && end > begin);
const helper = vm.runInNewContext('({' + source.slice(begin, end) + '})', {
    document: { createElement: tag => new Element(tag) },
    session: global.session = {
        sharedMediaStrings: { open: 'open', choose: 'choose', unsupported: 'unsupported', network: 'network', decode: 'decode', failed: 'failed' },
        openSharedMedia: (...args) => calls.push(args)
    }
}).addSharedMediaActions;
const calls = [];
class Element {
    constructor(tag) { this.tag = tag; this.children = []; this.listeners = {}; }
    appendChild(child) { this.children.push(child); }
    setAttribute() {}
    addEventListener(event, callback) { this.listeners[event] = callback; }
    querySelector() { return this.source; }
    emit(event) { this.listeners[event]?.({ preventDefault() {}, stopPropagation() {} }); }
    pause() { this.paused = true; }
}
const media = new Element('video');
media.source = new Element('source');
const container = new Element('div');
helper(media, container, '/psi/account/test/sharedfile/sha-1+abcdef');
const [status, open, choose] = container.children[0].children;
assert(status.hidden);
assert.equal(calls.length, 0); // Never launch on receipt or playback failure.
media.source.emit('error');
assert.equal(status.textContent, 'failed');
assert.equal(calls.length, 0);
for (const [code, message] of [[2, 'network'], [3, 'decode'], [4, 'unsupported']]) {
    media.error = { code };
    media.emit('error');
    assert.equal(status.textContent, message);
    assert(!status.hidden);
}
media.emit('canplay');
assert(status.hidden);
media.error = { code: 1 };
media.emit('error');
assert(status.hidden);
open.emit('click');
choose.emit('click');
assert(media.paused);
assert.deepEqual(calls, [['sha-1+abcdef', false], ['sha-1+abcdef', true]]);
const external = new Element('div');
helper(media, external, 'https://example.org/video.webm');
assert.equal(external.children.length, 0);
delete session.openSharedMedia;
const legacy = new Element('div');
helper(media, legacy, '/psi/account/test/sharedfile/sha-1+abcdef');
assert.equal(legacy.children.length, 0); // No changes to WebKit/Qite.
console.log('Shared-media actions: passed');
