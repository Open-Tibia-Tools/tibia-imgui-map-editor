---
name: code-review
description: Use when the user mentions reviews, GitHub reviews, PR feedback, review comments, "address the reviews", "reviews are here", "code review", or asks to respond to reviewer comments. Loads a rigorous workflow: read every review, attempt to disprove each element with codebase evidence, then decide to apply/skip/disprove. Addresses issues even outside the PR scope since reviewers examine the broader codebase.
---

# Code Review — Disprove-First Workflow

When a user asks you to address code reviews, follow this rigorous methodology. The default stance is **skeptical**: every review comment is guilty of being wrong until proven correct.

---

## Step 0 — Detect Review Context

The user may provide reviews in several ways. Determine the source:

| User says | Source |
|-----------|--------|
| "reviews are here", "address the reviews" (with no link) | Ask the user where the reviews are |
| Links to a GitHub PR (`github.com/.../pull/...`) | Use `gh pr view` and `gh pr diff` |
| Pastes review text in chat | Parse directly from the conversation |
| "check the review comments" on current branch | Use `gh pr view --comments` or `gh api` |

**IMPORTANT**: If the user mentions reviews but provides no link or text, ASK them to provide it. Do not guess.

### GitHub PR Review Fetching

To fetch all reviews and review comments from a GitHub PR:

```bash
# Get PR number from current branch
gh pr view --json number,title,url

# Get all reviews (approvals, change requests, comments)
gh pr view --json reviews --jq '.reviews[] | {author: .author.login, state: .state, body: .body}'

# Get inline review comments on diff
gh api "repos/:owner/:repo/pulls/$PR_NUMBER/comments" --jq '.[] | {path: .path, line: .line, author: .user.login, body: .body}'

# Get the full PR diff
gh pr diff
```

Aggregate ALL review feedback (approval comments, change requests, inline comments) before proceeding.

---

## Step 1 — Read All Reviews, Extract Every Claim

Read EVERY review comment. Do not skip any, even brief ones. For each review comment, extract discrete **claims** — a single review comment may contain multiple claims.

A claim is an actionable statement: a bug report, a style violation, a design criticism, a request for a change. Treat each claim independently.

Catalog them in your internal reasoning. Tag each claim with:
- **Source**: which reviewer, inline vs top-level
- **Location**: file path and line range if applicable
- **Type**: bug | style | design | performance | correctness | 

---

## Step 2 — Attempt to DISPROVE Every Claim

For every claim, BEFORE deciding whether to apply it, attempt to prove it wrong using evidence from the codebase. This is the most critical step.

### Techniques for Disproving

**Search existing code for counter-examples:**
- Does the codebase already follow a different pattern?
- Is the reviewer suggesting something that contradicts existing conventions?
- Is there a utility/helper the reviewer is unaware of that makes their suggestion moot?

**Check the broader codebase context:**
- Does the claim apply to 10+ other files the reviewer didn't notice? If so, it's a broader refactor — not this PR's responsibility.
- Is the reviewer's suggested approach already tried and reverted in git history?

**Verify factual correctness:**
- Is the reviewer's stated behavior actually observable?
- Does the reviewer claim a bug that doesn't reproduce?
- Is the reviewer's understanding of the code flow incorrect?

**Check consistency with project architecture docs:**
- Does `AGENTS.md` or the project README prescribe a different approach?
- Does the reviewer's suggestion violate documented layer dependency rules?

### Evidence Thresholds

| Confidence | Evidence | Action |
|-----------|----------|--------|
| **Disproven** | Codebase conclusively contradicts the claim | Explain why the reviewer is wrong. Do NOT apply. |
| **Weak** | Reviewer's point has merit but counter-evidence exists | Flag as "questionable" — ask the user if they want to apply |
| **Uncertain** | Neither proven nor disproven | Apply the change (defer to reviewer expertise) |

**Never disprove based on opinion.** If it's a style preference and the reviewer disagrees, the reviewer wins. Only disprove when codebase evidence contradicts factual claims.

---

## Step 3 — Decide: Apply or Skip

For every claim you cannot disprove, decide:

### Apply When:
- The claim identifies a genuine bug
- The claim fixes a violation of project conventions (check AGENTS.md, existing code)
- The claim improves correctness without regressing other concerns
- The claim is a scope expansion AND the reviewer is right that the issue matters
- The claim is minor and has no downside

### Skip When:
- The claim is a style preference that contradicts existing codebase style
- The claim would require touching 15+ unrelated files (flag as follow-up issue)
- The claim is already addressed by a different PR or commit
- The claim is factually wrong (see Step 2)
- The claim is purely cosmetic with zero user-facing impact AND project doesn't prioritize cosmetics

### Scope Expansion — Special Rule

Reviewers often point out issues in code adjacent to the PR changes — files or patterns NOT modified in the PR. This is legitimate. The reviewer is examining the broader codebase health, not just the diff.

**You MUST address scope-expansion claims** if:
- The issue exists in the code your PR touches (even if pre-existing)
- The issue could cause bugs, crashes, or data loss
- The issue violates fundamental project architecture rules

**You MAY skip scope-expansion claims** if:
- The issue is cosmetic and pre-existing across dozens of files
- The issue is a known trade-off documented in the project

When skipping, tell the user: "This review point is valid but outside scope. It affects N files and deserves its own PR. I recommend filing it as a follow-up issue."

---

## Step 4 — Present Your Findings

Before making ANY changes, present a summary to the user:

```
## Review Analysis — [PR Title]

### Reviews Found: N comments from M reviewers

#### ❌ Disproven (will not apply)
- [Claim] — reason it's wrong with codebase evidence

#### ⚠️ Skipped (valid but not applying)
- [Claim] — reason for skipping

#### ✅ Will Apply (proceeding to implement)
- [Claim] — brief description of what will change
```

Wait for the user to review before implementing, UNLESS the user explicitly said "just do it" or similar.

---

## Step 5 — Implement Changes

For each "apply" claim, make the change following project conventions:
- Use existing patterns and utilities
- Maintain the same code style as surrounding code


After implementing all changes, run the project's build/lint/typecheck commands to verify nothing is broken.

---

## Anti-Patterns — Never Do These

- **Do NOT** apply every review comment without scrutiny
- **Do NOT** argue with reviewers about style preferences — if it's subjective, apply it
- **Do NOT** silently skip review comments — always explain why
- **Do NOT** refactor unrelated code while addressing reviews (unless the reviewer asked)
- **Do NOT** treat scope-expansion claims as automatically out-of-scope — they are valid
- **Do NOT** make changes without understanding the full context of each claim

---

