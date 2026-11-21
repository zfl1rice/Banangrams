> Historical snapshot before the user applied the relative-frequency rule to all identified shortenings. See REVIEW.md for current outcomes.

# Expanded vocabulary review, 2026-11-19

The playable list now contains **79,702 words**, including **TOAD, CLOUDY and BANJO** and their documented inflections. All **14,618 previously approved words** remain present; all **675 explicit denials** remain absent. The browser bundles the expanded `words.txt` by default.

This generation adds **65,084 words**. It also produces a **new 1,195-spelling review queue**, whose entries are excluded from play. The user's earlier approval covered the 101 entries in `final_queue_approvals.csv`; it does not imply approval of this new queue.

## Accounting

| Stage | Unique spellings |
| --- | ---: |
| Previously approved baseline, retained unchanged | 14,618 |
| New size-60 ASCII dictionary candidates outside that baseline | 80,453 |
| Newly accepted | 65,084 |
| Newly excluded | 14,174 |
| Pending review, not playable | 1,195 |
| Final playable words | **79,702** |

80,453 = 65,084 + 14,174 + 1,195. Final size is 14,618 + 65,084.
Of the additions, 40,782 occur below rank 10,000 in OANC and 24,302 have no occurrence in that corpus. Both groups can now qualify on dictionary evidence. The frequency cutoff is used only to reproduce the historical baseline, never to limit new dictionary entries.

## Policy and review coverage

New entries use explicit ESDB size-60 forms with ordinary grammatical classes and supported inflection slots, American/shared spellings and the established acceptable variant levels. The source database, archive checksum and licenses are unchanged. No new web corpus or externally generated words were imported. Source punctuation and diacritics are not stripped to create words. No inflections are guessed.

Proper names, explicit abbreviations/affixes/nonwords, demonyms, nonpreferred variants and special dictionary categories are screened using lexical metadata. Name/likely-name classes and `[name]` tags now resolve uncertain duplicate records in the same family; abbreviation/likely-abbreviation classes are handled likewise. Positive evidence persists across source tiers and is recorded with the exclusion. An independently eligible ordinary sense can justify a name homograph. Existing spelling-wide denials always win.

`expansion_family_reviews.csv` records **2,165 exact-family category decisions**. This is additional agent review under the user's existing category instructions, not an assertion that the user individually approved these rows. Clear names, locations, organizations, abbreviations and religious/geographic families are excluded. Possible shortenings, religious/ordinary homographs, and geographic-origin/common-use ambiguities are queued. For example, AZ, BRADY, CHENEY, BAPTISM, NUN/NUNS and EASTER are excluded; ambiguous ordinary uses are left for review. Family decisions apply only to newly considered words and never revoke earlier approvals.

The initial expansion queue contained 6,907 entries. The name/abbreviation cleanup excluded **5,709** of them, leaving **1,198** pending at that stage. The playable vocabulary remained byte-identical. `queue_cleanup_review.csv` records every changed decision and its evidence. AZ was verified as a postal abbreviation against USPS Publication 28 Appendix B; Brady and Cheney have explicit name evidence in ESDB.

The user subsequently approved GYM and INFO as reasonable everyday shortenings. Exact family approvals also admit the documented plural GYMS, adding three playable words and reducing the queue to 1,195. All previously playable words remain present. The other shortening candidates remain pending: frequency supports judgment but is not a threshold. In OANC, APP occurs 96 times, INFO 80, GYM 79 and GYMS 8; counts mix cases and senses, so they do not establish ordinary-use frequency on their own.

The remaining queue mostly concerns ordinary versus religious uses and possible shortenings. It also includes uncertain lexical classes, disputed variants and geographic-origin ambiguities. Unclassified capitals are not blindly discarded: adjectives such as FREUDIAN remain reviewable when the source does not resolve their ordinary use. The queue is ordered by corpus frequency rank, with unseen spellings alphabetically last. Each row gives its reason and lexical evidence.

**Scope limit:** ESDB is a spelling resource, not a complete semantic taxonomy. The extra category screening is targeted, not an exhaustive dictionary-sense review of every new word. Religious associations, geographic etymologies and current shortenings are not consistently labeled by the source; additional exceptions can be recorded as they are found. Existing approved words with those associations remain deliberately preserved. This list is not an official game dictionary.

## Files

- [words.txt](words.txt): the playable vocabulary.
- [additions.csv](additions.csv): one row per new playable spelling, with family evidence and corpus rank/count when available.
- [shortening_review.csv](shortening_review.csv): 398 shortening candidates sorted by corpus count, with accepted/pending status and evidence.
- [review_queue.csv](review_queue.csv): pending spellings; none are playable.
- [expansion_family_reviews.csv](expansion_family_reviews.csv): reproducible category review inputs tied to pinned lemma families.
- [queue_cleanup_review.csv](queue_cleanup_review.csv): the 5,709 queue entries now excluded, with their previous reasons and new evidence.
- [decisions.csv](decisions.csv): accepted/excluded/review provenance for the legacy and dictionary stages.
- [manifest.json](manifest.json): source pins, policy, counts, input/builder/output checksums.
- [review_v1.md](review_v1.md): historical approval report and its original scope.

Use `allow.txt` or `deny.txt` with a reason and evidence to adjudicate exact spellings, then rebuild. A spelling approval alone does not approve additional family members. The source-specific family review file can resolve categories for explicit dictionary forms in the same family.

## Verification

- **37 Python tests pass**, covering source parsing, lexical rules, no-frequency-cutoff additions, unseen words, explicit inflections, category gates, review ordering and generated artifacts. New regression cases cover name-tagged and untagged duplicate records, likely abbreviations, positive evidence from higher tiers, ordinary homographs, preservation of unresolved adjectives and the reported AZ/BRADY/CHENEY cases.
- Subtracting `additions.csv` from the new word list reconstructs the original 14,618-word file byte-for-byte by its SHA-256. Every explicit allow/deny override is checked.
- All generated output hashes match the manifest. Two builds from verified local source caches produce byte-identical words, decisions, queue, legacy seeds, additions, shortening report and manifest.
- The native C++ loader constructed a `bananagrams::Lexicon` and checked every one of the 79,702 words, including the three reported missing words.
- The expansion's frontend production build and **13 browser integration tests passed**, including full games using FURTHEST, TOAD, CLOUDY and BANJO without a file upload, custom vocabulary behavior, nested static hosting and Backspace navigation. The subsequent shortening approval was verified with a fresh production build and all six default-vocabulary browser cases, including GYM and INFO. The earlier queue cleanup itself did not change playable words.

Current vocabulary SHA-256:

```text
b9f59e8059e303407d946e5ea04b325019f6b429f839a116cb994e79b1f4353e
```

Rebuild commands and source attribution are in [README.md](README.md). Rebuild the frontend after regenerating the list so its bundled default stays current.
