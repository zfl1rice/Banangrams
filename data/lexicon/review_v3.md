> Historical snapshot before the user approved all 990 remaining queue entries. See REVIEW.md for current membership.

# Vocabulary review: relative-frequency policy, 2026-11-19

The default list now contains **79,751 playable words**. The user instructed that a shortening is valid when it occurs more often than its full form, and invalid when it occurs less often. They explicitly confirmed that this applies to already-playable shortenings too. Equal counts and unresolved expansions remain pending.

## Results

| Base shortening | Count | Listed full form | Count | Result |
| --- | ---: | --- | ---: | --- |
| GYM | 79 | gymnasium | 10 | Eligible; GYMS follows the documented family |
| PHONE | 1,377 | telephone | 646 | Eligible |
| BIZ | 26 | business | 3,977 | Excluded |
| INFO | 80 | information | 8,481 | Excluded; earlier approval superseded |
| LAB | 251 | laboratory | 902 | Excluded; previously playable |
| RHINO | 10 | rhinoceros | 10 | Pending tie |

Counts are raw case-grouped OANC base surfaces, without inflection aggregation or sense disambiguation. Documented family forms share their base comparison. A separate ordinary, non-shortened sense can still admit a spelling. PROP, ADVERT, MINI and TILL have reviewed independent senses; the source links are preserved in the mapping file.

## Accounting

The pre-frequency list contained 79,702 words. This policy/sense review admits **68** spellings and removes **19**, yielding 79,751. Of the removals, eight are excluded and eleven require review because of ties or unresolved expansions. All 675 explicit deny overrides remain absent.

Of the original 14,618-word baseline, 14,604 remain playable and 14 have been superseded by this policy. All 65,147 other playable words appear in `additions.csv`. Thus 14,604 + 65,147 = 79,751. The original baseline can be reconstructed exactly by restoring the removed legacy words from the change report; its original checksum is retained in the manifest.

There are **990 pending spellings** overall. The new-dictionary stage accounts for 981 of them; nine additional pending legacy spellings arise from the global recheck. The dictionary-stage counts retain historical seed accounting separately from the final frequency override.

## Scope and evidence

`shortening_expansions.csv` covers 296 explicit ESDB families: 154 with a literal mapped expansion, 134 whose expansion or shortening classification remains unresolved, and eight with an independently ordinary sense. Of the 154 mapped comparisons, 46 pass, 101 fail and seven tie. These family counts are not counts of unique playable words; all other spelling, category and inflection gates still apply.

The inventory covers every family previously flagged as a possible shortening, plus identified existing words such as LAB, EXAM, PHONE, PHOTO, MATH and ZOO. It does not claim to identify every historical shortening or etymological derivation in the complete dictionary. Unresolved rows explicitly avoid supplying a fabricated full form or interpreting a missing mapping as count zero. Some unresolved rows are abbreviation-only records already excluded by the ordinary-word gate.

Full forms containing spaces are counted from contiguous annotated tokens in the same 8,824 OANC documents. Twelve comparison phrases were counted directly from the pinned archive; their cache is tied to the archive checksum, phrase inventory and parser version. Counts respect punctuation and document boundaries and handle UTF-16 annotation offsets. Examples: public house = 1; situation comedy = 2; zoological garden = 1. Single-word counts use the existing verified frequency cache.

OANC's subject mix and age affect the results. Counts combine senses and capitalization variants, so a ratio is a reproducible game rule, not a claim about current conversational English. The source's grammatical/semantic metadata is imperfect. Unknown mappings, ties and category ambiguities remain reviewable.

## Reports

- [shortening_frequency.csv](shortening_frequency.csv): literal expansions, both counts, comparison decisions and evidence.
- [shortening_review.csv](shortening_review.csv): final per-spelling status for 462 candidates, ordered by corpus count.
- [shortening_word_changes.csv](shortening_word_changes.csv): every admission/removal and whether it came from the original baseline.
- [review_queue.csv](review_queue.csv): all unresolved spellings, none playable.
- [decisions.csv](decisions.csv): complete provenance; overridden earlier acceptances have status `superseded`.
- [review_v2.md](review_v2.md): historical snapshot before this rule; its earlier approval of INFO is no longer current.

## Verification

- **40 Python tests pass**, including greater/less/equal comparisons, unseen/unknown distinctions, phrase boundaries and Unicode offsets, prior approvals being overridden, ordinary homographs, all explicit denials and generated-artifact hashes.
- The exact word changes match the saved pre-policy vocabulary. The reproducibly generated pre-policy vocabulary also matches its SHA-256.
- Restoring removed legacy words reconstructs the original 14,618-word baseline exactly. Every active accepted record matches current membership.
- Two builds from verified inputs produce byte-identical results for all nine generated artifacts, including the new comparisons and word-change report.
- The native C++ loader checked all 79,751 words. The frontend production build succeeds.
- **10 targeted browser tests pass**: default games accept FURTHEST, TOAD, CLOUDY, BANJO, GYM and PHONE; reject INFO, BIZ, LAB and EXAM. Earlier unrelated browser tests are documented in the historical report.

Current word-list SHA-256:

```text
4b383583503f6f4bd916215c3e51cb5a4c64afb0d03076f76e8e905c6fd3db7a
```

Rebuild instructions, source attribution and license notices are in [README.md](README.md).
