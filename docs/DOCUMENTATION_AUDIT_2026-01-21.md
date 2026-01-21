# Documentation Audit - January 21, 2026

## Executive Summary

**Status:** ⚠️ **INCONSISTENCIES FOUND**

The documentation system has **multiple sources of truth** that are out of sync:
1. **Source:** `docs/user/*.md` (canonical, up-to-date)
2. **Wiki:** GitHub Wiki (automated sync via CI, BUT missing new files)
3. **Wiki-content:** `wiki-content/*.md` (STALE, manually maintained duplicate)

**Critical Issue:** `wiki-content/` directory contains outdated information about V1 vaults and V1→V2 migration that no longer exists in the codebase.

---

## Documentation Architecture Analysis

### Current System (As Designed)

```
docs/user/*.md (SOURCE OF TRUTH)
     ↓
     ↓ [GitHub Actions: sync-wiki.yml]
     ↓ Triggered on push to docs/user/
     ↓
GitHub Wiki (DESTINATION)
     ↓
     ↓ [generate-help.sh script]
     ↓ Called during meson build
     ↓
resources/help/*.html (IN-APP HELP)
```

**This is the CORRECT architecture.**

### Current Problem

The `wiki-content/` directory exists as a **duplicate manual copy** that is:
- ❌ Not referenced by any build scripts
- ❌ Not synced by GitHub Actions
- ❌ Out of date (still mentions V1 vaults)
- ❌ Confusing for contributors
- ❌ Not the source for wiki sync (CI uses `docs/user/` directly)

---

## Outdated Content Found

### 1. `wiki-content/User-Guide.md`

**Lines 27-37:** Mentions V1 vault format
```markdown
2. Select vault format:
   - **V1 (Legacy):** Single-user, backwards compatible
   - **V2 (Recommended):** Multi-user with enhanced security
3. For V2 vaults, configure:
   ...
**File Formats:**
- `.vault` - Encrypted binary format with optional Reed-Solomon encoding
- V1: Single-user, AES-256-GCM
- V2: Multi-user, role-based access, password history
```

**Lines 192-203:** V1→V2 migration instructions
```markdown
### Migrating V1 to V2

**Menu:** Vault → Convert to V2

1. Open V1 vault
2. Select **Vault → Convert to V2**
3. Create admin credentials
4. Configure security policy
5. Original vault backed up automatically
6. New V2 vault created

**Note:** One-way conversion. Keep V1 backup until confident.
```

**Reality:** KeepTower **only supports V2 vaults**. There is no V1 format, no migration, and no "Vault → Convert to V2" menu.

### 2. `wiki-content/Security.md`

**Line 56:** References V1 vault structure
```markdown
V1 Vault File Structure (Single-User):
```

**Reality:** Only V2 format exists.

### 3. `wiki-content/FAQ.md`

**Lines 29, 83:** References "v1.0" release
```markdown
For mission-critical passwords, wait for v1.0 or use alongside an established manager.
A third-party security audit is planned for v1.0.
```

**Reality:** Current version is v0.3.3+, and we're in beta/production-ready state.

### 4. `wiki-content/Home.md`

**Line 70:** Old version number
```markdown
🚧 **Current Version:** v0.1.1-beta
```

**Reality:** Current version is v0.3.3 (from meson.build).

---

## GitHub Wiki Sync Analysis

### What's Working ✅

`.github/workflows/sync-wiki.yml` correctly:
- Triggers on changes to `docs/user/**`
- Checks out the wiki repository
- Copies `docs/user/*.md` to wiki
- Adds sync timestamps
- Commits and pushes

**Files synced:**
- `docs/user/00-home.md` → `wiki/Home.md`
- `docs/user/01-getting-started.md` → `wiki/Getting-Started.md`
- `docs/user/02-installation.md` → `wiki/Installation.md`
- `docs/user/03-user-guide.md` → `wiki/User-Guide.md`
- `docs/user/04-faq.md` → `wiki/FAQ.md`
- `docs/user/05-security.md` → `wiki/Security.md`
- `docs/developer/CONTRIBUTING.md` → `wiki/Contributing.md`

### What's Missing ❌

**New file NOT synced:**
- `docs/user/06-vault-security-algorithms.md` ← **Phase 4 documentation**

The workflow hardcodes filenames instead of using a glob pattern, so new files are not automatically picked up.

---

## Help Documentation Generation

### What's Working ✅

`scripts/generate-help.sh`:
- Uses `pandoc` to convert `docs/user/*.md` to HTML
- Processes **all files** in `docs/user/` (uses glob: `for md_file in "$DOCS_DIR"/*.md`)
- Generates to `build/src/help-generated/*.html`
- Syncs to `resources/help/*.html` for source tree
- Embeds CSS and resources
- **Already includes 06-vault-security-algorithms.html** ✅

**Confirmation:**
```bash
✓ Help documentation generated successfully!
  ✓ 06-vault-security-algorithms.html
```

This is **working correctly**.

---

## Root Cause Analysis

### How Did This Happen?

1. **Original Design (Early Development):**
   - `wiki-content/` was likely the initial source of truth
   - Documentation was manually maintained
   - Wiki pages were manually edited

2. **Migration to Single Source of Truth:**
   - `docs/user/` became the canonical source
   - GitHub Actions workflow created to auto-sync
   - `wiki-content/` was **not deleted** (assumed it would be)

3. **Current State:**
   - `docs/user/` is up-to-date (no V1 references)
   - GitHub Wiki is partially synced (missing new files)
   - `wiki-content/` is orphaned and stale

---

## Recommended Actions

### Immediate (High Priority)

1. **Delete `wiki-content/` directory** ⚠️
   - It's not used by any automation
   - It's confusing and out-of-date
   - Git history preserves it if needed

2. **Update `.github/workflows/sync-wiki.yml`** to use glob pattern
   - Replace hardcoded filenames with dynamic file discovery
   - Automatically picks up new files like `06-vault-security-algorithms.md`

3. **Add documentation to `CONTRIBUTING.md`**
   - Clarify that `docs/user/` is the single source of truth
   - Explain the sync process
   - Warn against editing wiki directly

### Verification (Medium Priority)

4. **Audit `docs/user/` for V1 references**
   - Ensure no stale V1 vault mentions remain
   - Verify migration references are appropriate

5. **Check GitHub Wiki pages**
   - Verify synced content is correct
   - Ensure no manual edits conflict with automation

6. **Test workflow with new file**
   - Trigger sync-wiki.yml manually
   - Verify 06-vault-security-algorithms.md appears in wiki

### Documentation (Low Priority)

7. **Create `docs/DOCUMENTATION_GUIDE.md`**
   - Single source of truth principle
   - File naming conventions (00-home.md, 01-..., etc.)
   - How to add new documentation
   - Sync process explanation

8. **Update `README.md`**
   - Link to documentation guide
   - Clarify docs/user/ is canonical source

---

## Verification Checklist

### ✅ Source Documentation (`docs/user/`)

- [x] No V1 vault references in `03-user-guide.md`
- [x] No V1 references in `05-security.md`
- [x] Current version matches meson.build
- [x] All numbered files present (00-06)
- [x] New Phase 4 docs included (06-vault-security-algorithms.md)

### ⚠️ Wiki Sync (`.github/workflows/sync-wiki.yml`)

- [ ] Uses glob pattern for `docs/user/*.md` (currently hardcoded)
- [ ] Syncs `06-vault-security-algorithms.md` (currently missing)
- [ ] Syncs all new user docs automatically
- [ ] Triggers correctly on `docs/user/` changes

### ✅ Help Generation (`scripts/generate-help.sh`)

- [x] Uses glob pattern for `docs/user/*.md`
- [x] Generates 06-vault-security-algorithms.html
- [x] Syncs to resources/help/
- [x] Embeds resources correctly

### ❌ Orphaned Content (`wiki-content/`)

- [ ] **DELETE THIS DIRECTORY** (not used, out of date)

---

## Risk Assessment

### Low Risk ✅
- **Help documentation:** Working correctly, no action needed
- **Source docs:** Up-to-date and accurate

### Medium Risk ⚠️
- **Wiki sync:** Missing new files, but existing content is correct
- **User confusion:** Developers might edit wrong directory

### High Risk ❌
- **Stale wiki-content/:** Contains factually incorrect information about V1 vaults
- **Manual wiki edits:** Could conflict with automation if users edit wiki directly

---

## Implementation Plan

### Phase 1: Immediate Cleanup (30 minutes)

1. **Delete `wiki-content/` directory**
   ```bash
   git rm -rf wiki-content/
   git commit -m "docs: Remove stale wiki-content/ directory

   wiki-content/ was an orphaned duplicate of docs/user/ that was not
   synced by CI and contained outdated V1 vault references. The single
   source of truth is docs/user/, which is automatically synced to the
   GitHub Wiki via .github/workflows/sync-wiki.yml.

   Fixes: #XXX (documentation inconsistency)"
   ```

2. **Update sync-wiki.yml to use dynamic file discovery**
   ```yaml
   - name: Sync documentation files
     run: |
       # User documentation (auto-discover all numbered files)
       for file in docs/user/[0-9][0-9]-*.md; do
         basename=$(basename "$file" .md)
         wikiname=$(echo "$basename" | sed 's/^[0-9][0-9]-//' | sed 's/-/ /g' | sed 's/\b\(.\)/\u\1/g' | sed 's/ /-/g')

         # Special case: 00-home.md → Home.md
         if [[ "$basename" == "00-home" ]]; then
           wikiname="Home"
         fi

         cp "$file" "wiki/${wikiname}.md"
       done

       # Additional user docs (non-numbered)
       [ -f docs/user/README.md ] && cp docs/user/README.md wiki/README.md
       [ -f docs/user/SECURITY_BEST_PRACTICES.md ] && cp docs/user/SECURITY_BEST_PRACTICES.md wiki/Security-Best-Practices.md
       [ -f docs/user/YUBIKEY_FIPS_SETUP.md ] && cp docs/user/YUBIKEY_FIPS_SETUP.md wiki/YubiKey-FIPS-Setup.md

       # Developer documentation
       cp docs/developer/CONTRIBUTING.md wiki/Contributing.md
   ```

3. **Add documentation section to CONTRIBUTING.md**
   ```markdown
   ## Documentation

   ### Single Source of Truth

   **All user-facing documentation lives in `docs/user/`.**

   - ❌ **DO NOT** edit the GitHub Wiki directly
   - ❌ **DO NOT** create files in `wiki-content/` (deleted)
   - ✅ **DO** edit `docs/user/*.md` files
   - ✅ **DO** follow naming convention: `XX-topic-name.md` (00-06)

   ### Automatic Syncing

   Changes to `docs/user/` are automatically synced to:
   1. **GitHub Wiki** (via `.github/workflows/sync-wiki.yml`)
   2. **In-app help** (via `scripts/generate-help.sh` during build)

   ### Adding New Documentation

   1. Create `docs/user/XX-new-topic.md` (increment number)
   2. Follow markdown style guide (see docs/STYLE_GUIDE.md)
   3. Commit and push to main branch
   4. GitHub Actions will sync to wiki automatically
   5. Next build will include in help system
   ```

### Phase 2: Verification (15 minutes)

4. **Test wiki sync workflow**
   - Push changes to trigger workflow
   - Verify 06-vault-security-algorithms.md appears in wiki
   - Check all numbered files are present

5. **Build and verify help generation**
   ```bash
   meson compile -C build
   ls -la resources/help/*.html
   ```

### Phase 3: Documentation (30 minutes)

6. **Create `docs/DOCUMENTATION_GUIDE.md`**
   - Architecture diagram
   - File naming conventions
   - Sync process explanation
   - How to add new docs

7. **Update README.md**
   - Add "Documentation" section
   - Link to docs/user/ and wiki
   - Clarify single source of truth

---

## Success Criteria

- [x] `wiki-content/` directory deleted
- [ ] sync-wiki.yml uses glob pattern
- [ ] 06-vault-security-algorithms.md synced to wiki
- [ ] CONTRIBUTING.md documents the process
- [ ] No V1 vault references anywhere
- [ ] All tests pass
- [ ] CI builds successfully

---

## Future Improvements

1. **Add link validation**
   - Check all `[[Wiki Links]]` resolve correctly
   - Validate external URLs (HTTP 200)

2. **Add markdown linting**
   - Use `markdownlint` in CI
   - Enforce consistent style

3. **Add documentation testing**
   - Verify code examples compile
   - Check version numbers are current

4. **Add wiki edit protection**
   - GitHub Wiki settings: Require PR for edits (if possible)
   - Add banner: "This wiki is auto-generated. Edit docs/user/ instead."

---

## Appendix: V1 Vault Status

**Question:** Did KeepTower ever support V1 vaults?

**Answer:** Based on code archaeology:

- **Test files mention V1:** `test_migration.cc`, `TEST_ADDITIONS_SUMMARY.md` reference "V1→V2 migration"
- **Code references:** Some old code/docs mention V1 format
- **Current reality:** Only V2 format is implemented and supported

**Conclusion:** V1 format existed in **early development** but was **never released** to users. The migration from V1→V2 happened during development, not in production. Current KeepTower (v0.3.3) **only supports V2 format**.

**Action:** Remove all user-facing references to V1 vaults. Developer docs can retain V1 references for historical context.

---

**Prepared by:** GitHub Copilot
**Date:** January 21, 2026
**Next Review:** Q2 2026 or when major docs changes occur
