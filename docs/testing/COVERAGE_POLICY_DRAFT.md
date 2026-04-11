# KeepTower Coverage Policy Draft

> Draft status: this document is a proposed policy for later adoption and revision.
> It is not yet the canonical repository quality gate.
> The canonical current scorecard remains in `ROADMAP.md`.

## Purpose

This draft defines a practical coverage policy for KeepTower that improves test quality
without turning aggregate coverage into a low-value optimization target.

The policy is designed to support milestone `A+ Gap Closure`, especially `#29`, while
remaining realistic for the current repository state.

## Current Position

At the time of drafting, KeepTower is still below the desired A+ coverage threshold in
core and library code. The main gaps are concentrated in a small number of high-impact,
branch-heavy files such as the vault managers and YubiKey-related flows.

Because of that, this policy uses a tiered approach:
- Tier 1 for repository-wide ratchets and regression control
- Tier 2 for stronger expectations on security-critical and workflow-critical modules

## Policy Goals

- Raise overall repository coverage toward a practical medium-term target of `80%` line coverage.
- Improve branch coverage on security-critical code rather than relying on line coverage alone.
- Prevent regressions on touched files.
- Focus testing effort on high-risk workflows: authentication, vault lifecycle, migration,
  persistence, backup, crypto orchestration, and YubiKey enforcement.

## Metrics To Keep

The current project metrics are still useful and should remain the base measurement set:
- Line coverage
- Function coverage
- Branch coverage

Interpretation guidance:
- Line coverage is the repository-wide progress metric.
- Branch coverage is the stronger quality signal for critical code.
- Function coverage is a helpful completeness signal, but weaker than branch coverage for
  security-sensitive logic.

## Tier 1: Repository-Wide Policy

Tier 1 is intended to be practical in the near term and suitable for routine CI use.

### Tier 1 Rules

- Do not allow meaningful repository-wide coverage regressions.
- Do not allow touched files to lose coverage without an explicit reason.
- Prefer focused tests for new core/security changes instead of relying on incidental
  aggregate coverage growth.
- Track line, function, and branch coverage on the repository as a whole.

### Tier 1 Medium-Term Goal

- Repository-wide line coverage target: `80%`

This is intended as a practical project goal, not an immediate hard gate while the largest
coverage hotspots remain unresolved.

## Tier 2: Critical-Module Policy

Tier 2 applies only to named modules that directly affect security, correctness, or vault
workflow integrity.

### Proposed Tier 2 Module Set

- `src/core/VaultManager.cc`
- `src/core/VaultManagerV2.cc`
- `src/core/services/V2AuthService.cc`
- `src/core/services/VaultFileService.cc`
- `src/core/services/VaultYubiKeyService.cc`
- `src/core/services/KeySlotManager.cc`
- `src/lib/crypto/KeyWrapping.cc`
- `src/lib/crypto/VaultCryptoService.cc`
- `src/lib/storage/VaultIO.cc`
- `src/lib/yubikey/YubiKeyManager.cc`
- `src/lib/fips/FipsProviderManager.cc`

This list is intentionally draft-only and should be revised when the hotspot files shrink
and the security boundary is re-evaluated.

### Tier 2 Expectations

- Higher branch coverage expectations than the repository-wide baseline
- Focused testing for negative paths and security invariants
- No relaxed treatment for “hard to reach” branches without an explicit rationale

### Proposed Tier 2 Threshold Progression

Near-term draft target:
- `85%` line coverage
- `70%` branch coverage

Longer-term draft target:
- `90%+` branch coverage for mature critical modules
- `95%` line coverage only where the module is small enough and stable enough for that to
  remain a meaningful target

These are revision points, not immediate hard requirements.

## Additional Quality Signals

Line coverage alone is not sufficient for KeepTower. The following signals should be used to
improve test quality.

### 1. Negative-Path Coverage

Critical workflows should explicitly cover failure behavior, including:
- authentication failures
- corrupted vault input
- tampered ciphertext
- invalid policy transitions
- migration failures
- backup/save failure handling
- permission-denied paths

### 2. State-Transition Coverage

Test state changes, not just single calls. Examples:
- create -> save -> close -> reopen
- login -> forced password change -> save -> reopen
- migrate -> persist -> reopen
- enroll/unenroll YubiKey -> reopen -> revalidate behavior

### 3. Persistence and Invariant Coverage

For critical modules, tests should verify that the expected state survives save/reopen cycles
and that security invariants fail closed when data is corrupted.

### 4. Complexity-Weighted Coverage

Functions with higher branching complexity should receive direct tests instead of being
counted as “covered” only because a large integration test passed through them incidentally.

Draft rule:
- Functions with cyclomatic complexity greater than `5` should have at least one focused
  test covering both a success path and a failure or alternative path.

This should be introduced when a stable complexity tool is chosen for the repository.

### 5. Mutation Testing

Mutation score is useful, but should be periodic and selective because of cost.

Draft use:
- Run on a small set of Tier 2 modules
- Use periodically or before release-quality checkpoints
- Draft target: `85%` mutation score on selected critical modules

This is not recommended as an every-commit repository-wide gate.

### 6. Traceability for Critical Features

For security-critical features, tests should be mappable to the intended guarantee.

Examples:
- wrong password never decrypts vault data
- tampered ciphertext fails closed
- standard users cannot perform admin-only actions
- policy changes persist and affect later validation behavior
- migration state changes persist across reopen cycles

This can be implemented through disciplined test naming and issue/PR references rather than
heavy formal tooling.

## Metrics Not Recommended as Immediate Global Gates

The following are valuable in some contexts, but are not recommended as immediate repository-
wide adoption requirements for KeepTower:

- MC/DC for the full repository
- strict assertion-density thresholds
- repository-wide mutation score gates on every CI run

Rationale:
- MC/DC is expensive and best reserved for explicitly classified high-assurance logic.
- Assertion density is easy to game and does not reliably measure test value.
- Full-repo mutation testing is expensive for this C++ codebase and should remain selective.

## Adoption Plan Draft

### Stage 1

- Keep current line/function/branch reporting in CI.
- Continue `#29` coverage work on the largest low-coverage core paths.
- Use Tier 1 behavior informally: no regressions, focused tests on touched critical files.

### Stage 2

- Name and publish the Tier 2 critical-module set.
- Track line and branch coverage for those modules separately.
- Start enforcing no-regression expectations on those modules in review.

### Stage 3

- Introduce a complexity tool and apply the complexity-weighted test expectation.
- Add periodic mutation testing for a small Tier 2 subset.

### Stage 4

- Revisit the policy after coverage hotspots shrink.
- Decide whether parts of this draft should become canonical quality gates in `ROADMAP.md`
  and CI configuration.

## Draft Conclusion

For KeepTower, the best practical approach is:
- keep line, function, and branch coverage as the base metrics
- use line coverage as the repository-level progress signal
- use branch coverage as the stronger signal for critical code
- add Tier 1 and Tier 2 policy behavior before introducing heavier methods

This draft should be revised once the repository is closer to the `80%` line-coverage goal and
the current hotspot files are substantially reduced.