# Repository agent workflow

These rules apply to automated/AI-assisted changes in this repository.

- Treat `master` as release-quality. Do not merge into `master` without an explicit instruction to merge.
- Branches under `ai/**` are development branches. It is fine to use multiple small, reviewable commits while iterating.
- On `ai/**`, intermediate commits may skip targeted development CI when CI evidence is not useful yet, but do not use GitHub's standard `[skip ci]` / `[ci skip]` markers. Those strings can survive into a squash commit and suppress the required CI run on `master`. Use only repository-specific skip conditions/markers that cannot disable GitHub Actions globally.
- On ordinary (non-`ai/**`) branches, prefer preparing the complete logical change first and committing it once, unless there is a specific reason to split the history.
- Do not make a series of temporary commits on an ordinary branch merely to squash them afterwards.
- When squash-merging a development PR, write an explicit final squash commit title and body that summarize the PR as one coherent change. Prefer the PR title and a concise PR-style summary of the resulting behavior, tests, and important compatibility notes. Do not let GitHub concatenate the intermediate commit messages into the squash commit body.
- Before merging, ensure the final squash commit message does not accidentally contain CI control tokens such as `[skip ci]` or `[ci skip]`.
- For XMPP protocol work, check the current XEP/RFC text before encoding protocol assumptions. Keep parsing/signaling policy separated from UI/application policy.
- Prefer extending the existing architecture over creating a parallel signaling/media stack.
