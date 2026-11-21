> Historical snapshot of the approved v1 list. See REVIEW.md for the current expanded list.

# Vocabulary review, 2026-11-19

**User approval complete: 14,618 words, zero unresolved entries.** The user approved all 101 entries remaining after the exclusion passes. Those exact spellings are now included, including FURTHEST and OPUSES. `review_queue.csv` is empty except for its header. The manifest status is `reviewed_candidate`, indicating that no entries remain pending; it is not an official game-dictionary certification.

## Final counts

| Stage | Count |
| --- | ---: |
| Ranked source entries selected before exclusions | 10,000 |
| Accepted seeds | 8,581 |
| Excluded seeds | 1,419 |
| Unresolved seeds | 0 |
| Additional distinct validated inflections beyond accepted seeds | 6,034 |
| Explicit allow overrides | 102 |
| Additions outside accepted seeds/automatic inflections | 3 (QI, FURTHEST, OPUSES) |
| Explicit deny overrides | 675 |
| Earlier recorded manual acceptance reviews | 11 |
| Unresolved inflections | 0 |
| Total unresolved spellings | **0** |
| Final unique words | **14,618** |

Accounting: 8,581 + 1,419 = 10,000 selected seeds; 8,581 + 6,034 + 3 = 14,618 final words. The 102 allow overrides comprise QI plus the final 101 approvals. Of those final approvals, 99 are selected seeds and two are disputed inflection spellings. Every accepted entry has a seed, validated family or explicit user-override record in `decisions.csv`.

## User decisions and audit records

The user first explicitly confirmed DOZENS, MARK, MAY, ME, MYSELF, RABBITS, ROSE and WILL. Those existing acceptances remain recorded in `manual_review.csv`; the spelling "doezens" was interpreted as DOZENS.

The exclusion passes resolved 370 organization/abbreviation/shortening cases, 177 names/locations, 69 geographic names and derivatives, 13 religious/holiday-related terms, and 42 individually specified spellings. Together with the original four denials, these account for all 675 deny overrides. The user spelling "yoker" was interpreted as the existing YORKER entry. Explicit individual denials take precedence even where a word has an ordinary meaning. Previously denied words, including PIING, remain excluded.

The user then instructed: "the rest are all approved." [`final_queue_approvals.csv`](final_queue_approvals.csv) preserves the exact 101-entry queue covered by that instruction, its original lexical evidence and review reasons, and the approval decision. The corresponding `allow.txt` rows make the approval reproducible. All 101 spellings were absent from the prior 14,517-word output, so the final count increased by exactly 101.

These approvals are explicit policy exceptions, not claims that missing lexical evidence was independently supplied. They include previously uncertain spellings, calendar words, names or product labels, and non-American spelling variants such as THEATRE, CENTRE, TUMOUR and BEHAVIOUR. Their original reasons remain visible in the approval snapshot. FURTHEST and OPUSES are accepted despite ESDB's disputed variant level; no new suffix-based or derivational relatives are invented. Approving a spelling without an eligible family admits that spelling alone.

Historical review files retain the statuses at the time of each pass:

- `review_samples.csv`: the original 121 distinct spellings sampled.
- `queue_denials.csv`: organization/abbreviation review.
- `name_location_review.csv`: names/location review, including then-pending candidates.
- `geographic_review.csv`: geographic exclusions.
- `religious_review.csv`: religious exclusions and then-pending language words.

Use `words.txt`, `decisions.csv`, `review_queue.csv` and `manifest.json` for current outcomes. All prior approved words remain accepted; all prior deny overrides remain enforced.

## Actual lexical review coverage

The initial 121-spelling sample covered 21 accepted seeds, 20 rejected seeds, 20 added inflections, all 25 entries at ranks 9,976-10,000, 20 initially ambiguous cases, four adjudicated exclusions, nine targeted regular/irregular relationships, three disputed inflections and QI; categories overlap. It did not claim exhaustive reading of every corpus occurrence.

ROSE, MARK, MAY and WILL have independent ordinary family evidence. ME and MYSELF trace to grammatical pronoun lemma I. Review corrected an initially overstrict capitalized-lemma check, usage-dependent plurals such as RABBITS/DOZENS, and American-compatible variants such as SERA. CATS, WENT/GONE, CHILDREN, MICE, BETTER/BEST and HAPPIER/HAPPIEST were checked against explicit family slots. HAPPINESS is independently seeded, not generated from HAPPY. Source punctuation, accents and nonletters were rejected without repair. No profanity filter was applied.

The later categorical reviews used pinned ESDB metadata and inspected OANC contexts, plus selected dictionary references. Only source identifiers/offsets and review evidence are retained; raw corpus caches are ignored. Source attribution and redistribution notices remain in `licenses/` and [README.md](README.md).

## Verification

The focused suite passes **24 tests**, including generated-file format/order/uniqueness/hash checks, exact top-10,000 selection without backfill, seed accounting, provenance, override conflicts, homographs, closed-class words, regular/irregular families, punctuation rejection, common/American expansion gates and absence of derivational expansion. A parser fixture checks case grouping, original surfaces, UTF-16 offsets and malformed unused lemma annotations. A new regression verifies that explicit approvals resolve missing seeds and disputed inflections, report zero pending counts, and do not invent additional forms.

The existing C++ integration executable loaded and checked all **14,618 words** through `bananagrams::Lexicon`. No engine or frontend behavior was changed for this task. Exact rebuild/test/integration commands are in README.

The initial pipeline was verified by reparsing both pinned raw archives. After the final approvals, two builds using the verified cached inputs produced byte-identical `words.txt`, selected seeds, decisions, review queue and manifest. All 101 approved spellings were checked present, all 675 denied spellings absent, and the queue empty. The current vocabulary SHA-256 is:

```text
ba9fd6722c223088bf58f50c0e4992661879068775f844d11c01e67bc9bd3c57
```

## Known limits

OANC is genuinely American-English data, but its older mix of medical, governmental and travel writing is not a neutral conversational-frequency sample. Its ranking counts punctuation/numerical token types before filtering and combines case variants, including proper-name occurrences, for frequency. ESDB's lexical classification and commonness tiers are imperfect. Final user overrides deliberately resolve exceptions to the automatic gates; they do not turn this into a universally validated dictionary. There are no outstanding queue decisions or source-permission blockers for the selected inputs under the documented terms. Preserve the included notices when redistributing the vocabulary.
