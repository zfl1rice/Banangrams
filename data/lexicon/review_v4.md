# Final expanded vocabulary approval, 2026-11-19

**80,741 playable words; zero unresolved entries.** The user approved everything in the current review queue. The exact 990-spelling queue is preserved in `queue_approvals_v3.csv`, including each entry's prior reason and evidence. Every one of those spellings is now playable, and no other word changed.

## Accounting and precedence

The preceding list contained 79,751 words. Adding the 990 approved spellings gives 80,741. `review_queue.csv` now contains only its header. All 675 explicit denials remain absent.

The approval covers only this queue snapshot. It does not undo exclusions that were already resolved: INFO, BIZ, LAB, EXAM, AZ, BRADY and CHENEY remain excluded. It does approve former ties and unresolved entries such as RHINO and TRANS. These are exact-spelling exceptions; no additional family members are inferred.

The explicit approval runs after the automatic frequency/category rules. Earlier review rows are retained as `superseded` in `decisions.csv`, with a new accepted `queue_approval` row. The snapshot is a hashed pipeline input, so the same approvals persist across rebuilds. A conflicting explicit denial fails validation rather than silently changing precedence.

Of the original 14,618-word baseline, 14,613 are playable. The five still excluded by the frequency rule are EXAM, EXAMS, LAB, LABS and MO. `additions.csv` contains the other 66,128 playable words. Thus 14,613 + 66,128 = 80,741. Nine approved queue spellings restore earlier baseline membership; the other 981 are dictionary additions.

`common_review_words_before_queue_approvals` records the historical automatic-stage count of 981; the final `common_review_words` and `unresolved_entries` are both zero. The original source-frequency methodology, dictionary pins and exclusion rules remain documented in README.

## Reports

- [words.txt](words.txt): current playable membership.
- [queue_approvals_v3.csv](queue_approvals_v3.csv): all 990 exact spellings and their approval evidence.
- [review_queue.csv](review_queue.csv): empty apart from its header.
- [decisions.csv](decisions.csv): active and superseded provenance.
- [shortening_review.csv](shortening_review.csv): current per-spelling status after approvals.
- [shortening_frequency.csv](shortening_frequency.csv): underlying automatic comparisons, before explicit exceptions.
- [shortening_word_changes.csv](shortening_word_changes.csv): historical automatic frequency-stage effects, before this approval.
- [review_v3.md](review_v3.md): the prior 79,751-word report and its review scope.

Approvals resolve playability, not dictionary metadata or etymological uncertainty. The list remains a user-approved custom vocabulary; it is not an official game dictionary or a claim that each pending lexical issue was independently resolved.

## Verification

- **42 Python tests pass.** A new regression verifies exact-spelling approval after frequency review, preservation of unapproved relatives and rejection of conflicts with explicit denials.
- All 990 snapshot spellings are present and the queue is empty. Subtracting them reconstructs the exact previous 79,751-word file and its SHA-256. No other words were added or removed.
- All output hashes match the manifest; two builds produce byte-identical results for all nine generated artifacts.
- The C++ loader checked all **80,741 words**.
- The frontend production build succeeds. **12 targeted browser tests pass**, including newly approved RHINO and TRANS and continued rejection of INFO, BIZ, LAB and EXAM.

Current word-list SHA-256:

```text
aef044a51722763ef790936ecc50ecf8da1c6c6c010c0885cb09809a477c6c88
```

See [README.md](README.md) for reproducible build commands and source notices.
