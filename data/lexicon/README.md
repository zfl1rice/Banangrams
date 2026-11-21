# User-approved expanded game vocabulary

`words.txt` contains **81,236 playable words**. The corrected religious-word policy restored 569 ordinary spellings, including HERESY, THEOLOGY, BISHOP, NUN and HYMN. Proper names such as JESUS, CHRIST and ALLAH remain excluded. The browser bundles the updated file as its default dictionary.

**The review queue is empty.** [queue_approvals_v3.csv](queue_approvals_v3.csv) preserves the exact approved snapshot, prior reasons, lexical evidence and user instruction. The historical deny file is preserved. Religious-topic denials are rechecked under the newer ordinary-word policy; unrelated explicit denials remain binding. The 75 later exclusions in [playability_exclusions.tsv](playability_exclusions.tsv) apply after all older approvals; 74 remove previously playable entries and PFFT was already absent. Words already rejected before the approval, including INFO, BIZ, LAB and EXAM, were outside that queue and remain excluded.

See [REVIEW.md](REVIEW.md) for current accounting and verification. The `review_v1.md`, `review_v2.md`, `review_v3.md`, `review_v4.md` and `review_v5.md` files are historical snapshots. This remains a custom game list, not an official Scrabble or Bananagrams dictionary.

## Rebuild

Run from the repository root with Python 3.11 or later. The pipeline and pinned ESDB builder use the Python standard library; no pip packages or LLM API are required. The recorded run used Python 3.14 on Windows.

```powershell
python -X utf8 tools/lexicon/build_wordlist.py
python -X utf8 -m unittest discover -s tests -p test_lexicon_pipeline.py -v
```

The first command downloads the two pinned archives (approximately 336 MB total) to the ignored `build/lexicon-cache/`, verifies SHA-256, counts OANC, builds the ESDB SQLite database, and writes the artifacts here. It does not use a moving branch. Cached archive and derived-file hashes are checked on later runs. A mismatch fails the build rather than silently accepting changed inputs.

```powershell
# Reuse verified local inputs without network access.
python -X utf8 tools/lexicon/build_wordlist.py --offline

# Reparse both original archives, ignoring the derived frequency/database caches.
python -X utf8 tools/lexicon/build_wordlist.py --offline --rebuild-inputs --output build/lexicon-rebuild
python -c "from pathlib import Path; assert Path('data/lexicon/words.txt').read_bytes() == Path('build/lexicon-rebuild/words.txt').read_bytes(); print('Byte-identical words.txt')"
```

The exact native integration commands used in this workspace were:

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
& 'C:\msys64\ucrt64\bin\g++.exe' -std=c++20 -Wall -Wextra -pedantic -Iinclude tests/lexicon_file_check.cpp src/lexicon.cpp -o build/lexicon_file_check.exe
& .\build\lexicon_file_check.exe data/lexicon/words.txt
```

On other systems use the installed C++20 compiler with the same source files and flags. This standalone check reads the file and constructs the existing `bananagrams::Lexicon`; it checks every word and required homographs. Dictionary generation is offline. A game using this artifact must load this identical list for humans and bots and perform membership checks. The engine rules are unchanged. The browser imports this file at build time.

## Selection and inflection policy

Policy identifier: `bananagrams-esdb60-ordinary-religious-words-v5`.

1. **Reproduce the historical baseline.** The original OANC top-10,000 surface selection and ESDB family expansion reproduce the approved 14,618-word v1 list and its evidence. That absolute cutoff never limits additional ESDB60 entries. The new relative-frequency policy is then applied to identified shortenings throughout the dictionary, including previously playable words. Historical acceptance rows superseded by this policy remain in `decisions.csv` with status `superseded`.
2. **Use documented dictionary forms.** Consider explicit ESDB spellings through size 60, regardless of corpus rank or presence. Require ASCII letters, at least two characters, ordinary grammatical classes and permitted inflection slots; use American/shared spellings, blank/US regions and the established acceptable variant levels 0, 1, 2, 4, 5, 6. Never generate suffixes or invent relatives. A frequency comparison cannot turn an abbreviation-only record or an invalid spelling into an ordinary word.
3. **Keep the category filters.** Proper names, abbreviations/affixes/nonwords, demonyms and special dictionary categories remain screened. Name/likely-name classes, `[name]` tags and abbreviation evidence persist across records in the same family. Independent ordinary senses remain eligible; capitalization alone is not proof of exclusion. `expansion_family_reviews.csv` preserves earlier category decisions. `shortening_expansions.csv` supplies newer shortening comparisons and documented ordinary-sense exceptions, which supersede earlier shortening-specific approvals such as INFO.
4. **Compare base forms.** A mapped shortening is eligible when its base-form count is strictly greater than its listed full form's count; it is excluded when lower. The result applies to documented inflections in that family. Equal counts, including 0/0, normally require review. The later explicit queue approval resolves the exact spellings in `queue_approvals_v3.csv`. An unresolved expansion has no numeric full-form count: it must not be treated as an unseen full form with count zero. GYM is compared with GYMNASIUM, rather than requiring each inflected surface to pass separately. Exact deny overrides retain precedence.
5. **Count consistently.** Use the pinned OANC case-grouped annotated surfaces. Single-word full forms use the existing count cache. Multiword full forms are counted as contiguous annotated-token sequences in the same 8,824 documents, case-insensitively, without crossing document boundaries or skipping punctuation. Those counts are separately cached and checksum-verified. Source counts combine grammatical senses and proper-name occurrences; no sense-specific frequency is claimed. Comparison tables list literal expansions and evidence, making contested mappings editable and reproducible.
6. **Preserve independent ordinary meanings.** A spelling supported by an ordinary non-shortened sense is not rejected just because a separate shortened sense fails. Reviewed examples include PROP as a support, ADVERT as a verb, MINI as an adjective and TILL. Other unclear mappings normally require review; exact queue approvals can admit their spellings without supplying a new expansion. This is an inventory of identified candidates, not a claim to have classified the etymology of every dictionary entry.
7. **Apply later explicit queue approvals.** After automatic category and frequency evaluation, apply `queue_approvals_v3.csv` as exact-spelling overrides. These user approvals supersede earlier review outcomes, including ties and unresolved mappings. They do not approve unlisted relatives or invent inflections. Conflicts with explicit deny overrides fail the build.
8. **Apply the later playability review.** `playability_exclusions.tsv` removes exact spellings after the historical approvals, including SH, FA, similar sound spellings/musical syllables, reviewed abbreviations and ED (536 occurrences versus 2,140 for EDUCATION). It supersedes historical accepted/review rows without rewriting the approval snapshots. Independent ordinary meanings such as DO/SO, TI (a plant) and MIDI (a garment) remain eligible. This is a curated audit, not a blanket ban on short words, interjections, or words with any abbreviated sense.
9. **Correct the religious-topic filter.** Religion is not itself a reason for exclusion. Re-evaluate all previously religion-screened families, including accepted queue entries and explicit religious denials, against ordinary lowercase senses and the existing spelling/inflection requirements. Proper and name-derived capitalized entries remain excluded unless an independently eligible ordinary sense supports the spelling. Previously approved ordinary variants stay approved. Unrelated denials and all later sound/abbreviation exclusions remain binding. The generated `religion_review.csv` records every result; original review files are historical inputs, not final membership.
10. **Export deterministically.** Uppercase ASCII, sorted and deduplicated, UTF-8/LF with a final newline. All playable words have active accepted provenance. QI remains explicitly allowed. No profanity filter is applied. Review rows are ordered by source frequency rank; unseen words follow alphabetically. Rebuild the frontend after regenerating the list.

OANC's unused `base` annotation features contain malformed UTF-8 in some files. The parser removes those unused XML features before parsing, preserving original text, token offsets and POS features. It never uses the annotated lemmas for counting. The manifest enumerates affected files and counts; other XML, text decoding or offset errors fail. UTF-16 annotation offsets are handled explicitly. ESDB's builder warnings about unmatched compound corrections are preserved in the ignored cache log; those corrections are not silently treated as game vocabulary.

## Sources and reuse

Retrieved and reviewed **2026-11-19**. Exact archive URLs, repository commits and SHA-256 hashes are pinned in [`tools/lexicon/sources.json`](../../tools/lexicon/sources.json) and copied into `manifest.json`.

| Source | Pinned version | Purpose |
| --- | --- | --- |
| [Open American National Corpus](https://anc.org/OANC/index.html) | OANC 1.0.1 UTF-8; official mirror release `datasets-2026-09-25`, website repository commit `ecc70bb29a8f5ee5907e903491e5f75462e46921` | Original American-English corpus used to derive the frequency table |
| [English Speller Database, formerly SCOWL](https://github.com/en-wl/wordlist) | `1e5b7d3a72f47a71da5d28686c1dd4b397178485` | Lexical POS/classes, explicit inflection families, commonness tiers and spelling variants |

OANC archive SHA-256: `1a5c37b0227c176d2c5a00f23721e70e7489782e31ba14e58e47f56eff04d24f`.

ESDB archive SHA-256: `4a9acd18d0c16ed595d11458a2dbc226c8a91f93c89970c0f4506444a8f4b161`.

The [OANC project's open-data statement](https://anc.org/data/oanc/) explicitly permits use and redistribution, including commercial use. The original archive also contains a custom agreement; it is **not labeled CC0 or public domain here**. Preserve [the complete agreement and Appendix I author/publisher attributions](licenses/OANC-LICENSE.txt). The agreement permits reproduction and transparent modifications subject to attribution, carrying the agreement, identifying changes and their maker/date/origin, and avoiding implied endorsement. It does not grant unrestricted permission to rewrite the semantic content of source passages. This project extracts statistical token counts and lexical forms; it does not rewrite or publish corpus passages. No endorsement by corpus authors, publishers or the ANC project is claimed. Ship these notices and this attribution with redistributed vocabulary artifacts.

**OANC attribution and change notice:** Ide, Nancy and Suderman, Keith (2007), *The Open American National Corpus (OANC)*. Original corpus: <https://anc.org/OANC/index.html>, exact download in `sources.json`. Original text authors/publishers retain their respective copyrights, acknowledged individually in Appendix I of the included agreement. Transparently modified statistical derivative prepared by Codex for this Bananagrams repository on 2026-11-19: counted original annotated surfaces, grouped case for frequency, selected 10,000 ranks for the legacy baseline, applied the documented exclusions, joined ESDB inflections, then broadened coverage with independently eligible size-60 dictionary forms, applied reviewed shortening/full-form frequency comparisons, and exported uppercase forms. The included OANC license was transcoded from the archive's Windows-1252 to UTF-8 and its line endings normalized; its full text and attributions are retained. Original license-member SHA-256: `5c1b2a90dccea5875d4c2d702d5a39106cb90419beeae5c26a0f4d4045bbdfb8`.

ESDB's [own Copyright file](https://github.com/en-wl/wordlist/blob/1e5b7d3a72f47a71da5d28686c1dd4b397178485/Copyright) permits using, modifying, distributing and selling wordlists while retaining its notices. Copyright 2000-2026 Kevin Atkinson, with upstream contributors and terms listed there. Preserve [the complete copied notice](licenses/ESDB-Copyright.txt), including incorporated-source notices, with redistribution. This is permission for the ESDB wordlist; it is not a claim that underlying datasets such as COCA are independently open. ESDB data and code are both taken from the pinned archive.

[SUBTLEX-US](https://www.ugent.be/pp/experimentele-psychologie/en/research/documents/subtlexus) was investigated but not used: its primary download documentation did not establish clear redistribution terms for this project. A wrapper package's license or another author's permission is not a dataset license. OANC was selected as an explicitly American alternative with primary reuse documentation, rather than substituting a general or British ranking. No SUBTLEX, COCA, or restricted source files are included.

Manual exclusions cite the pinned OANC/ESDB data and selected Merriam-Webster, NHGRI or Oxford journal references with access dates in `deny.txt`; these are factual review references, not bulk lexical imports. Their page text is not redistributed or fetched during rebuild. The reproducible review inputs are the checked-in decisions, hashed in the manifest.

## Artifacts and further review

- `words.txt`: the 81,236-word playable vocabulary.
- `selected_seeds.csv`: the historical baseline's 10,000 ranks, not an admission limit for dictionary additions.
- `decisions.csv`: active accepted/excluded/review outcomes and superseded historical approvals, with source evidence.
- `review_queue.csv`: currently header-only; zero unresolved spellings.
- `queue_approvals_v3.csv`: immutable historical approval snapshot of all 990 entries. The later review supersedes five exact spellings: FA, LBW, VIZ, ZS and ZZZ.
- `playability_exclusions.tsv`: 75 later exact-spelling exclusions, with per-word reasons and evidence; still enforced.
- `religion_review.csv`: the final recheck of 1,182 religion-screened spellings, with previous/final status, reason and dictionary evidence.
- `additions.csv`: 66,655 currently playable spellings outside the original v1 baseline.
- `shortening_expansions.csv`: 296 exact family mappings: 154 mapped comparisons, 134 unresolved mappings and eight independently ordinary family exceptions.
- `shortening_frequency.csv`: one row per mapped/reviewed family, with base count, full form, full-form count and comparison result. This is the automatic category decision before explicit queue approvals; final per-spelling status is in `words.txt`, `decisions.csv` and `shortening_review.csv`.
- `shortening_review.csv`: 462 candidate spellings with final playable/pending/excluded status and source frequency, including previously playable entries.
- `shortening_word_changes.csv`: automatic frequency-stage changes before later queue approvals. Its earlier review/removal status is superseded for spellings in `queue_approvals_v3.csv`.
- `allow.txt`, `deny.txt`, `manual_review.csv`: earlier spelling-wide decisions. Exact denials still win; the user authorized rechecking prior shortening approvals under the newer rule.
- `expansion_family_reviews.csv`: earlier exact-family category review inputs. New comparison results overlay shortening-specific records during generation rather than destroying historical approvals.
- `queue_cleanup_review.csv`: historical audit of 5,709 names/abbreviations/category entries removed from the initial expansion queue.
- `final_queue_approvals.csv`, `queue_denials.csv`, `name_location_review.csv`, `geographic_review.csv`, `religious_review.csv` and `review_samples.csv`: historical review records; current outcomes are in the generated files above.
- `REVIEW.md`: current accounting and verification. `review_v1.md`, `review_v2.md`, `review_v3.md`, `review_v4.md` and `review_v5.md` preserve earlier snapshots.
- `manifest.json`: source pins, policy, counts, phrase counts and checksums for inputs, builder, licenses and generated outputs.

To resolve a shortening, inspect its ordinary senses, record a literal expansion or an independent ordinary-sense decision in `shortening_expansions.csv`, rebuild and rerun checks. Do not guess an expansion merely to obtain a convenient frequency ratio. The source-specific family records do not authorize invented inflections. Keep raw archives, databases and caches under ignored `build/`.
