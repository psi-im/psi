# Real AvatarCache MUC lookup microbenchmark

This benchmark intentionally does **not** use the synthetic `AvatarNegativeMemo`
model from the old migration plan. It invokes the actual
`AvatarCache::getMucAvatar(Jid)` in `src/avatars.cpp`, with the actual
`iconset_`, `jidToIcons`, `FileCache`, and `VCardFactory` lookup path.

For comparison, it uses a single `QHash<QString,QPixmap>` keyed by occupant
nick, corresponding to the current `NativeChatAvatars` hot lookup. It does
not instantiate that UI helper or include message-widget painting.

## Run

Build standard BASIC Psi with an additional opt-in switch:

```sh
cmake -S . -B build-bench -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCHAT_TYPE=BASIC -DENABLE_AVATAR_CACHE_BENCHMARK=ON \
  # ...your usual Qt5 or Qt6 build options and dependencies...
cmake --build build-bench --parallel 4

# On Linux, use a throwaway HOME, not your real Psi profile.
tmp_home="$(mktemp -d)"
HOME="$tmp_home" QT_QPA_PLATFORM=offscreen PSI_AVATAR_CACHE_BENCHMARK=1 \
  ./build-bench/psi/psi
rm -rf "$tmp_home"
```

The program exits after printing results (without starting an XMPP session).

## Workload

- 1000 synthetic MUC occupants, 20% with a valid stored 96px PNG and 80%
  without any avatar.
- Successful images are installed through the actual `setIcon()` and warmed
  through `getMucAvatar()`, so subsequent calls hit the real decoded iconset.
- Missing entries traverse the real negative path through `VCardFactory`.
- Each case performs 32 passes, repeated 7 times; reports median
  microseconds per lookup for central vs local, including JID construction
  on the central path. Setup, image decoding, and disk writes are excluded.
- No network access, IQ, MAM, painting, scrollback layout, or UI frames.
  No speed threshold in CI; hosted-runner noise makes that unreliable.

**Caveat:** this is a warm microbenchmark of the actual cache resolver, not an
end-to-end chat timeline benchmark. It tests current native MUC lookup
semantics but does not cover per-account or occupant lifecycle changes.
