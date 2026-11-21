# Playability audit, 2026-11-20

**80,667 playable words; zero unresolved entries.** Removed **74** entries from the previous 80,741-word dictionary. No words were added. The browser default dictionary has been rebuilt.

## Scope and findings

Screened the approved list against the pinned ESDB metadata, including all two- and three-letter entries, known shortening families, interjection families, consonant-only spellings, and 295 playable spellings matching abbreviation records (including dotted source forms). These matches are candidates, not automatic exclusions: many ordinary words also have abbreviated senses. This is a targeted metadata and lexical audit, not a claim that every meaning of all 80,741 entries was independently checked.

The user explicitly requested SH and FA be removed, then confirmed removing similar sound spellings and musical syllables while retaining ordinary independent words. SH is a hush interjection; FA is a solfege syllable. These are custom gameplay exclusions, not claims that they are misspellings. See [SH](https://www.merriam-webster.com/dictionary/sh) and [FA](https://www.merriam-webster.com/dictionary/fa).

- Abbreviations and symbols removed include CD, CM, KB, KG, KM, MG, ML, MM, MIN, LBW and VIZ. INC and its documented technical verb forms are also excluded. Legacy ordinary-looking source records had admitted some of these despite abbreviation evidence elsewhere.
- ED is the shortening of EDUCATION: pinned OANC counts are 536 versus 2,140. It fails the existing shorter-form frequency rule. Counts combine senses and capitalization; see [ED](https://www.merriam-webster.com/dictionary/ed).
- Sound forms removed include SH/SHH/SSH, HM/HMM, PSST, BRR, hesitation/exclamation spellings, and documented inflections of those sound forms. FA, LA, MI, SI, musical plural SOS and sound spelling DOH are excluded. ZS/ZZZ sleep notation is excluded.
- DO, SO, RE, ordinary greetings, GYM, PHONE, QI and previously approved ordinary meanings remain playable. TI has an independent plant sense and MIDI a garment sense; neither is rejected solely for its musical-syllable or acronym homograph. See [TI](https://www.merriam-webster.com/dictionary/ti) and [MIDI](https://www.merriam-webster.com/dictionary/midi).
- PFFT was already absent; an explicit exclusion now protects it from later admission. This pass preserves previous resolved shortening decisions and exact queue approvals except for the reviewed exclusions below.

## Exact removals

AAH, ACHOO, AH, AHA, AHCHOO, AHEM, ATISHOO, AW, BAH, BRR, CD, CM, DOH, DUH, ED, EEK, EH, ER, FA, HA, HAH, HM, HMM, HUH, HUMPH, HUMPHED, HUMPHING, HUMPHS, INC, INCED, INCING, INCS, KB, KG, KM, LA, LBW, MEH, MG, MI, MIN, ML, MM, OH, OHO, OHS, OI, OOH, OOHED, OOHING, OOHS, OUCH, OW, PAH, PHEW, PHOOEY, PSHAW, PSHAWS, PSST, RAH, SH, SHH, SI, SOS, SSH, UGH, UH, UM, VIZ, WHEE, WHEW, YOW, ZS, ZZZ.

Every exclusion and its evidence is recorded in [playability_exclusions.tsv](playability_exclusions.tsv).

## Precedence and accounting

The 75-entry exclusion file is a hashed pipeline input applied **after** automatic rules and historical queue approvals. Older accepted/review rows become `superseded`; new `playability_review` rows record exclusion. This prevents the previous blanket approval from reintroducing these words.

The historical 990-entry approval snapshot stays unchanged. Five entries from it are now excluded: FA, LBW, VIZ, ZS and ZZZ. The other 985 remain playable. Historical hard denials remain enforced. No inflections are invented: exclusions list exact spellings.

Of the original 14,618-word baseline, 14,581 remain. Five were excluded by the earlier frequency policy and 32 more by this audit. There are 66,086 playable additions: 14,581 + 66,086 = 80,667. The prior approval report is preserved as [review_v4.md](review_v4.md).

`queue_approval_additions` and the frequency reports describe their historical pipeline stages. `playability_removed` records the 74 subsequent removals. `decisions.csv`, `shortening_review.csv`, `words.txt` and `additions.csv` reflect final membership.

## Verification

- 44 Python tests pass, including precedence over historical approval, exact removals and retained ordinary words.
- Adding back the 74 recorded removals reconstructs the exact prior 80,741-word file and hash; no unrelated vocabulary changed.
- C++ Lexicon loads and checks all 80,667 words, accepts ordinary controls, and rejects the new exclusions.
- All nine generated artifacts, including the manifest, are byte-identical across two offline builds.
- The frontend production build succeeds. All 23 default-vocabulary browser checks pass, including rejection of SH, FA, HM, MI, LA, CD, INC, ED and PFFT and continued acceptance of DO, SO and GYM.

Current word-list SHA-256: `c023861a87bf3758bc5927b56f74ae16bb33b48e647c7d9b339412749bb77b89`.
