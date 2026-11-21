# Religious-word policy correction, 2026-11-20

**81,236 playable words; zero unresolved entries.** Replaced blanket religious-topic exclusion with the agreed ordinary-word/proper-name distinction. Restored **569 spellings**, including HERESY, HERESIES, THEOLOGY, BISHOP, NUN, HYMN, PRAYERFUL, CLERGY, BAPTISM and RELIGIOUSLY. No previously playable word was removed.

## Review scope

Rechecked **1,182 spellings** across all religion-screened families: `religious_term`, `confirmed_religious_term` and `religious_or_ordinary_sense`, plus the families attached to the 13 old religious-topic deny entries. This includes earlier queue approvals, not just the originally reported 710 exclusions. The results are in [religion_review.csv](religion_review.csv).

The final audit accepts 1,028 spellings (459 already playable and 569 restored) and excludes 154. These are dictionary spellings and documented inflections, not mechanically generated suffixes.

- Religious meaning alone never excludes a word. Ordinary nouns, verbs, adjectives and adverbs are eligible under the established dictionary, spelling, variant and inflection rules.
- JESUS, CHRIST, ALLAH, MUHAMMAD, JEHOVAH and SATAN remain excluded. Capitalized religious names and name-derived forms without independent ordinary lowercase senses remain screened, consistently with the existing proper-name policy.
- Independent ordinary senses remain playable, including NOEL, CATHOLIC and BIBLE. A separately named person, religious institution or text does not invalidate a supported ordinary sense.
- Existing approved ordinary variants such as AUREOLAE remain approved. This correction does not reopen unrelated spelling decisions.
- The SH/FA sound and abbreviation cleanup remains binding: all 75 recorded exclusions stay absent. GYM, PHONE, DO, SO, QI and the required ordinary name homographs remain playable.

## Precedence and accounting

The original review files are retained for provenance. The pipeline now re-evaluates religious decisions after the historical expansion, frequency, queue-approval and playability stages. It supersedes those earlier per-spelling rows with `religion_review` outcomes in `decisions.csv`. Unrelated explicit denials and the later playability exclusions retain priority within this recheck.

80,667 previous words + 569 restored words = 81,236. The original 14,618-word baseline still contributes 14,581 playable words; final additions now total 66,655. The review queue remains empty. The preceding sound/abbreviation audit is preserved in [review_v5.md](review_v5.md).

Earlier counters describe their historical stages. The new `religion_restored_words`, `religion_removed_words` and `religion_reviewed_spellings` counters describe this correction. `religion_added_words` and `religion_removed_words` in the manifest record the exact membership delta. `words.txt`, `additions.csv`, `decisions.csv` and `shortening_review.csv` reflect final membership.

## Verification

- **48 Python tests pass**, including ordinary religious words, proper-name exclusions, independent homographs, previously approved variants, and precedence of unrelated exclusions.
- Removing the 569 additions reconstructs the exact previous 80,667-word dictionary and its SHA-256. No other membership changes occurred.
- C++ Lexicon loads and checks all **81,236 words**, accepts restored words and rejects JESUS, CHRIST, ALLAH and SATAN as well as the recent sound/abbreviation exclusions.
- Production build succeeds. All **34 default-vocabulary browser checks** pass, including restored ordinary religious words, proper-name exclusions and the recent sound/abbreviation exclusions.
- All **10 generated artifacts**, including the manifest and religion audit, are byte-identical across two offline builds.

Word-list SHA-256: `c3dd133b4e3fc6ebba7f65df38d2fb950a9caababa85c3396f7b872affa6d967`.

See [README.md](README.md) for rebuild commands and source notices. The list remains a custom game vocabulary.
