# Repository agent workflow

These rules apply to automated/AI-assisted changes in this repository.

- Treat `master` as release-quality. Do not merge into `master` without an explicit instruction to merge.
- Branches under `ai/**` are development branches. It is fine to use multiple small, reviewable commits while iterating.
- On `ai/**`, add `[skip ci]` to intermediate commits when CI evidence is not useful yet. Run the targeted CI gate once the change reaches a meaningful checkpoint.
- On ordinary (non-`ai/**`) branches, prefer preparing the complete logical change first and committing it once, unless there is a specific reason to split the history.
- Do not make a series of temporary commits on an ordinary branch merely to squash them afterwards.
- For XMPP protocol work, check the current XEP/RFC text before encoding protocol assumptions. Keep parsing/signaling policy separated from UI/application policy.
- Prefer extending the existing architecture over creating a parallel signaling/media stack.
