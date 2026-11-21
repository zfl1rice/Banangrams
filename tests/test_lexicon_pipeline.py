"""Offline policy and generated-artifact checks; no LLM or network required."""
import csv
from dataclasses import replace
import hashlib
import json
from pathlib import Path
import re
import tempfile
import unittest
import zipfile

from tools.lexicon.build_wordlist import (
    DATA, Entry, apply_playability_exclusions, apply_queue_approvals, apply_religion_policy, broaden, classify, compare_shortenings, count_phrases, decision,
    generate, overrides, rank_forms, read_oanc, recheck_playable_shortenings, shape_reason,
)


def entry(word, family=1, **changes):
    return replace(Entry(word, word, family, "n0", "n", "", 35, "_", "", 0, "", "", ""), **changes)


def seed(word, rank=1, count=100):
    return {"form": word.lower(), "spellings": {word: count}, "count": count, "rank": rank}


def run(seeds, entries, allow=None, deny=None, manual=None):
    by_word, families = {}, {}
    for item in entries:
        by_word.setdefault(item.word.lower(), []).append(item)
        families.setdefault(item.family, []).append(item)
    return generate(seeds, by_word, families, allow or {"QI": ("policy", "user")}, deny or {}, manual or {})


class PolicyTests(unittest.TestCase):
    def test_corpus_parser_preserves_surfaces_case_punctuation_and_utf16_offsets(self):
        surfaces = ["\U0001f33b", "Rose", "ROSE", "can't", "caf\u00e9"]
        text = " ".join(surfaces)
        nodes, offset = [], 0
        for surface in surfaces:
            end = offset + len(surface.encode("utf-16-le")) // 2
            nodes.append((f'<struct type="tok" from="{offset}" to="{end}">'
                          '<feat name="msd" value="NN"/>').encode()
                         + b'<feat name="base" value="\xff"/></struct>')
            offset = end + 1
        xml = b'<cesAna xmlns="http://www.xces.org/schema/2003">' + b''.join(nodes) + b'</cesAna>'
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "corpus.zip"
            with zipfile.ZipFile(path, "w") as archive:
                archive.writestr("OANC/data/test.txt", text.encode())
                archive.writestr("OANC/data/test-hepple.xml", xml)
            records, stats = read_oanc(path)
        by_form = {r["form"]: r for r in records}
        self.assertEqual(by_form["rose"]["spellings"], {"ROSE": 1, "Rose": 1})
        self.assertEqual(by_form["rose"]["count"], 2)
        self.assertEqual(set(by_form), {"\U0001f33b", "rose", "can't", "caf\u00e9"})
        self.assertEqual(stats["token_count"], 5)
        self.assertEqual(stats["ignored_invalid_utf8_base_features"], {"OANC/data/test-hepple.xml": 5})

    def test_selects_exactly_ten_thousand_before_exclusion_without_backfill(self):
        records = [seed(f"word{i:05}", count=20000-i) for i in range(10001)]
        selected = rank_forms(records)
        self.assertEqual(len(selected), 10000)
        words, _, _, counts = run(selected, [])
        self.assertEqual(words, {"QI"})
        self.assertEqual(counts["excluded_seeds"], 10000)

    def test_frequency_ties_are_deterministic(self):
        selected = rank_forms([seed("zebra"), seed("apple"), seed("pear")], 2)
        self.assertEqual([row["form"] for row in selected], ["apple", "pear"])

    def test_rejects_punctuation_accents_and_short_words_without_repair(self):
        for word in ["a", "can't", "co-op", "café", "ﬁrst", "A.B.", "AB2", "x y"]:
            self.assertIsNotNone(shape_reason(word))
        words, _, _, _ = run([seed("can't"), seed("co-op")], [entry("cant"), entry("coop")])
        self.assertNotIn("CANT", words)
        self.assertNotIn("COOP", words)

    def test_common_name_homographs_survive_with_ordinary_lexical_evidence(self):
        entries = []
        for i, word in enumerate(["rose", "mark", "may", "will"]):
            entries += [entry(word, i), entry(word.title(), i+10, pos_class="person")]
        words, _, review, _ = run([seed(w.title()) for w in ["rose", "mark", "may", "will"]], entries)
        self.assertTrue({"ROSE", "MARK", "MAY", "WILL"} <= words)
        self.assertFalse(review)

    def test_proper_only_and_uncertain_capitalized_entries_are_not_admitted(self):
        words, rows, review, _ = run([seed("Freda"), seed("London"), seed("NASA")],
            [entry("Freda", pos_class="person"), entry("London", 2, pos_class="upper"), entry("NASA", 3)])
        self.assertEqual(words, {"QI"})
        self.assertEqual(len(review), 2)
        self.assertTrue(any(row["status"] == "excluded" and row["normalized"] == "FREDA" for row in rows))

    def test_abbreviations_and_affixes_require_an_independent_ordinary_sense(self):
        entries = [entry("etc.", base_pos="abbr", pos="abbr"), entry("anti", 2, base_pos="pre", pos="pre"),
                   entry("ism", 3, base_pos="suf", pos="suf"), entry("DNA", 4, pos_class="abbr")]
        words, _, _, _ = run([seed(w) for w in ["etc", "anti", "ism", "DNA"]], entries)
        self.assertEqual(words, {"QI"})
        words, _, _, _ = run([seed("anti")], entries + [entry("anti", 5)])
        self.assertIn("ANTI", words)

    def test_function_words_and_interjections_are_ordinary_entries(self):
        entries = [entry("the", base_pos="d", pos="d"), entry("and", 2, base_pos="c", pos="c"),
                   entry("you", 3, base_pos="pn", pos="pn0"), entry("oh", 4, base_pos="i", pos="i")]
        words, _, review, _ = run([seed(e.word) for e in entries], entries)
        self.assertTrue({"THE", "AND", "YOU", "OH"} <= words)
        self.assertFalse(review)

    def test_capital_I_is_an_explicit_pronoun_lemma_not_a_name(self):
        entries = [entry("me", lemma="I", base_pos="pn", pos="pn1"),
                   entry("myself", lemma="I", base_pos="pn", pos="pnr0")]
        words, _, review, _ = run([seed("me")], entries)
        self.assertTrue({"ME", "MYSELF"} <= words)
        self.assertFalse(review)

    def test_usage_dependent_plurals_and_acceptable_us_variants_are_retained(self):
        entries = [entry("rabbit"), entry("rabbits", lemma="rabbit", pos="ns", entry_rank="*"),
                   entry("serum", 2), entry("sera", 2, lemma="serum", pos="ns", variant=5)]
        words, _, _, _ = run([seed("rabbit"), seed("serum")], entries)
        self.assertTrue({"RABBITS", "SERA"} <= words)

    def test_already_inflected_seed_expands_explicit_irregular_family(self):
        entries = [entry(w, lemma="go", pos=p, base_pos="v") for w, p in
                   [("go", "v0"), ("goes", "vs"), ("went", "vd"), ("gone", "vn"), ("going", "vg")]]
        words, rows, _, _ = run([seed("went")], entries)
        self.assertTrue({"GO", "GOES", "WENT", "GONE", "GOING"} <= words)
        self.assertTrue(all(row["lemma"] == "go" for row in rows if row["stage"] == "inflection"))
        self.assertNotIn("GOED", words)

    def test_regular_plurals_and_comparison_without_derivation(self):
        entries = [entry("cat"), entry("cats", lemma="cat", pos="ns"),
                   entry("happy", 2, base_pos="aj", pos="aj0"),
                   entry("happier", 2, lemma="happy", base_pos="aj", pos="aj1"),
                   entry("happiest", 2, lemma="happy", base_pos="aj", pos="aj2"), entry("happiness", 3)]
        words, _, _, _ = run([seed("cat"), seed("happy")], entries)
        self.assertTrue({"CATS", "HAPPIER", "HAPPIEST"} <= words)
        self.assertNotIn("HAPPINESS", words)
        words, _, _, _ = run([seed("happiness")], entries)
        self.assertIn("HAPPINESS", words)

    def test_homographs_keep_separate_inflection_family_provenance(self):
        entries = [entry("rose"), entry("roses", lemma="rose", pos="ns"),
                   entry("rose", 2, lemma="rise", base_pos="v", pos="vd"),
                   entry("risen", 2, lemma="rise", base_pos="v", pos="vn")]
        words, rows, _, _ = run([seed("rose")], entries)
        self.assertTrue({"ROSES", "RISEN"} <= words)
        self.assertEqual({row["family_id"] for row in rows if row["stage"] == "inflection"}, {1, 2})

    def test_expansion_reapplies_ascii_commonness_and_american_variant_filters(self):
        entries = [entry("cat"), entry("cat's", lemma="cat", pos="np"),
                   entry("cats", lemma="cat", pos="ns", size=70),
                   entry("colour", 2, spelling="B"), entry("color", 3, spelling="A")]
        words, _, _, _ = run([seed("cat"), seed("colour"), seed("color")], entries)
        self.assertEqual(words, {"CAT", "COLOR", "QI"})

    def test_conflicting_allow_and_deny_fails(self):
        with self.assertRaisesRegex(ValueError, "Conflicting"):
            run([], [], deny={"QI": ("conflict", "test")})

    def test_disputed_inflection_is_queued_unless_an_independent_sense_admits_it(self):
        entries = [entry("opus"), entry("opuses", lemma="opus", pos="ns", variant=3)]
        words, _, review, counts = run([seed("opus")], entries)
        self.assertNotIn("OPUSES", words)
        self.assertEqual([r["normalized"] for r in review], ["OPUSES"])
        self.assertEqual(counts["unresolved_inflections"], 1)
        words, _, review, _ = run([seed("opus"), seed("opuses")], entries + [entry("opuses", 2)])
        self.assertIn("OPUSES", words)
        self.assertFalse(review)

    def test_manual_exclusion_cannot_return_via_expansion(self):
        words, _, _, _ = run([seed("cat")], [entry("cat"), entry("cats", lemma="cat", pos="ns")],
            manual={"CATS": {"decision": "exclude", "reason": "test", "evidence": "fixture"}})
        self.assertNotIn("CATS", words)

    def test_explicit_approvals_resolve_missing_seeds_and_disputed_inflections(self):
        entries = [entry("opus"), entry("opuses", lemma="opus", pos="ns", variant=3)]
        allow = {w: ("User approved remaining review entries", "user instruction")
                 for w in ["QI", "MICROARRAY", "OPUSES"]}
        words, rows, review, counts = run([seed("opus"), seed("microarray")], entries, allow=allow)
        self.assertTrue({"MICROARRAY", "OPUSES"} <= words)
        self.assertFalse(review)
        self.assertEqual(counts["unresolved_seeds"], 0)
        self.assertEqual(counts["unresolved_inflections"], 0)
        self.assertEqual(counts["excluded_seeds"], 0)
        self.assertEqual(counts["accepted_seeds"], 2)
        self.assertNotIn("MICROARRAYS", words)
        self.assertTrue(any(r["normalized"] == "OPUSES" and r["status"] == "accepted"
                            and r["manual_override"] == "allow.txt" for r in rows))

    def test_qi_is_only_explicit_q_exception(self):
        words, _, _, _ = run([], [entry("qat"), entry("qis", lemma="qi", pos="ns", size=70)])
        self.assertEqual(words, {"QI"})

    def test_override_requires_reason_and_evidence(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "allow.txt"
            path.write_text("QI\n")
            with self.assertRaises(ValueError):
                overrides(path)


class DictionaryExpansionTests(unittest.TestCase):
    def expand(self, entries, baseline=None, records=None, deny=None, reviews=None):
        by_word = {}
        for item in entries:
            by_word.setdefault(item.word.lower(), []).append(item)
        return broaden(baseline or {"QI"}, by_word, records or [], deny or {}, {}, reviews or {})

    def test_includes_low_frequency_and_unseen_words_and_explicit_inflections(self):
        entries = [entry("toad"), entry("toads", lemma="toad", pos="ns"),
                   entry("cloudy", 2, base_pos="aj", pos="aj0"),
                   entry("cloudier", 2, lemma="cloudy", base_pos="aj", pos="aj1"),
                   entry("cloudiest", 2, lemma="cloudy", base_pos="aj", pos="aj2"),
                   entry("banjo", 3), entry("banjos", 3, lemma="banjo", pos="ns")]
        words, rows, review, additions, counts = self.expand(entries, records=[seed("toad", count=1)])
        self.assertEqual(words, {"QI", "TOAD", "TOADS", "CLOUDY", "CLOUDIER", "CLOUDIEST", "BANJO", "BANJOS"})
        self.assertFalse(review)
        self.assertEqual(counts["common_added_words"], 7)
        self.assertEqual(next(r for r in additions if r["normalized"] == "BANJO")["frequency_count"], 0)
        self.assertNotIn("TOADED", words)

    def test_name_homographs_survive_but_names_abbreviations_and_demonyms_do_not(self):
        entries = [entry("Rose", pos_class="person"), entry("rose", 2),
                   entry("London", 3, pos_class="place"), entry("CIA", 4, base_pos="abbr", pos="abbr"),
                   entry("Australian", 5, pos_class="demonym"), entry("foobar", 6, category="hacker")]
        words, _, review, _, _ = self.expand(entries)
        self.assertEqual(words, {"QI", "ROSE"})
        self.assertFalse(review)

    def test_tier_spelling_and_disputed_variant_filters(self):
        entries = [entry("color", spelling="A"), entry("colour", 2, spelling="B"),
                   entry("qis", 3, size=70), entry("opuses", 4, variant=3)]
        words, _, review, _, _ = self.expand(entries)
        self.assertEqual(words, {"QI", "COLOR"})
        self.assertEqual([r["normalized"] for r in review], ["OPUSES"])

    def test_name_evidence_survives_untagged_duplicate_records_and_keeps_homographs(self):
        entries = [entry("Brady", pos_class="upper", tag="[name]"),
                   entry("Brady", pos_class="upper", size=60),
                   entry("Cheney", 2, pos_class="name?", tag="[name]"),
                   entry("Rose", 3, pos_class="upper", tag="[name]"),
                   entry("rose", 4),
                   entry("azure", 5, base_pos="aj", pos="aj0", variant=3)]
        words, rows, review, _, _ = self.expand(entries)
        self.assertEqual(words, {"QI", "ROSE"})
        self.assertEqual([r["normalized"] for r in review], ["AZURE"])
        for spelling in ("BRADY", "CHENEY"):
            row = next(r for r in rows if r["normalized"] == spelling)
            self.assertEqual(row["status"], "excluded")
            self.assertIn("named_family_without_ordinary_sense", row["reason"])

    def test_likely_abbreviations_are_excluded_without_suppressing_ordinary_senses(self):
        entries = [entry("RR", pos_class="abbr?", base_pos="", pos=""),
                   entry("IN", 2, pos_class="abbr?"),
                   entry("in", 3, base_pos="pp", pos="pp")]
        words, _, review, _, _ = self.expand(entries)
        self.assertEqual(words, {"QI", "IN"})
        self.assertFalse(review)

    def test_named_family_evidence_above_the_admission_tier_is_recorded(self):
        entries = [entry("Brady", pos_class="upper", size=60),
                   entry("Brady", pos_class="upper", tag="[name]", size=70)]
        words, rows, review, _, _ = self.expand(entries)
        self.assertEqual(words, {"QI"})
        self.assertFalse(review)
        self.assertIn("tag=[name]", rows[0]["evidence"])
        self.assertIn("size=70", rows[0]["evidence"])

    def test_capitalization_alone_does_not_reject_uncertain_adjectives_or_nouns(self):
        entries = [entry("Freudian", base_pos="aj", pos="aj0", pos_class="upper"),
                   entry("Example", 2, pos_class="upper")]
        words, _, review, _, _ = self.expand(entries)
        self.assertEqual(words, {"QI"})
        self.assertEqual({r["normalized"] for r in review}, {"FREUDIAN", "EXAMPLE"})

    def test_preserves_prior_approvals_and_applies_semantic_rules_to_new_families(self):
        entries = [entry("nun"), entry("nuns", lemma="nun", pos="ns"), entry("Hebrew", 2),
                   entry("mark", 3), entry("marks", 3, lemma="mark", pos="ns")]
        reviews = {1: {"decision": "exclude", "reason": "religious_term", "evidence": "reviewed family"},
                   2: {"decision": "review", "reason": "ambiguous", "evidence": "reviewed family"}}
        words, _, review, _, _ = self.expand(entries, baseline={"QI", "HEBREW"},
                                             deny={"MARK": ("user", "test")}, reviews=reviews)
        self.assertEqual(words, {"QI", "HEBREW"})
        self.assertEqual([r["normalized"] for r in review], ["MARKS"])

    def test_frequency_orders_review_without_admitting_ambiguous_words(self):
        entries = [entry(w, i, pos_class="upper?") for i, w in enumerate(["Az", "Baz", "Zed"])]
        words, _, review, _, _ = self.expand(entries, records=[seed("Zed", count=1), seed("Baz", count=10)])
        self.assertEqual(words, {"QI"})
        self.assertEqual([r["normalized"] for r in review], ["BAZ", "ZED", "AZ"])

    def test_common_shortening_approval_accepts_only_documented_eligible_forms(self):
        entries = [entry("gym"), entry("gyms", lemma="gym", pos="ns"),
                   entry("gym's", lemma="gym", pos="np"),
                   entry("gymmed", lemma="gym", pos="vd"),
                   entry("app", 2)]
        reviews = {
            1: {"decision": "accept", "reason": "user_approved_common_shortening", "evidence": "user"},
            2: {"decision": "review", "reason": "possible_shortening_or_ordinary_sense", "evidence": "review"},
        }
        words, _, review, _, _ = self.expand(entries, records=[seed("app", count=1000), seed("gym", count=1)], reviews=reviews)
        self.assertEqual(words, {"QI", "GYM", "GYMS"})
        self.assertEqual([r["normalized"] for r in review], ["APP"])
        words, _, _, _, _ = self.expand(entries, deny={"GYM": ("explicit denial", "user")}, reviews=reviews)
        self.assertNotIn("GYM", words)


class ShorteningFrequencyTests(unittest.TestCase):
    def test_later_exclusions_supersede_approvals_and_pending_rows_without_expanding(self):
        rows = [decision("queue_approval", "FA", "accepted", "old approval", "fixture"),
                decision("seed", "SH", "accepted", "ordinary entry", "fixture"),
                decision("dictionary", "SH", "review", "pending", "fixture")]
        exclusions = {w: ("later exclusion", "user") for w in ["FA", "SH", "PFFT"]}
        words, rows, review = apply_playability_exclusions(exclusions, {"FA", "SH", "DO", "SO", "GYM", "FAR"}, rows, [rows[2]])
        self.assertEqual(words, {"DO", "SO", "GYM", "FAR"})
        self.assertEqual(review, [])
        self.assertTrue(all(r["status"] == "superseded" for r in rows[:3]))
        self.assertEqual({r["normalized"] for r in rows if r["status"] == "excluded"}, set(exclusions))

    def test_later_queue_approval_resolves_exact_spelling_without_approving_relatives(self):
        rhino = decision("dictionary", "rhino", "review", "shortening_frequency_tie", "fixture", seed("rhino"), entry("rhino"))
        plural = decision("dictionary", "rhinos", "review", "shortening_frequency_tie", "fixture", seed("rhinos"), entry("rhinos"))
        approval = dict(rhino, approval="accept", approval_evidence="user approves this queue")
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "approvals.csv"
            with path.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.DictWriter(handle, fieldnames=list(approval))
                writer.writeheader()
                writer.writerow(approval)
            words, rows, review, total, added = apply_queue_approvals(path, {"GYM"}, [rhino.copy(), plural.copy()], [rhino.copy(), plural.copy()], set())
            self.assertEqual(words, {"GYM", "RHINO"})
            self.assertEqual((total, added), (1, 1))
            self.assertEqual([r["normalized"] for r in review], ["RHINOS"])
            self.assertTrue(any(r["stage"] == "queue_approval" and r["status"] == "accepted" for r in rows))
            with self.assertRaisesRegex(ValueError, "conflicting"):
                apply_queue_approvals(path, set(), [], [], {"RHINO"})

    def test_gym_passes_biz_info_fail_and_ties_and_unknowns_need_review(self):
        pairs = [("gym", "gymnasium"), ("biz", "business"), ("info", "information"),
                 ("tied", "longer"), ("unseen", "absent"), ("ambiguous", "")]
        mappings = {i: dict(lemma=short, base_pos="n", full_form=full,
                           mapping_status="mapped" if full else "unresolved", evidence="fixture")
                    for i, (short, full) in enumerate(pairs)}
        records = [seed(w, count=c) for w, c in [("gym", 79), ("gymnasium", 10),
                   ("biz", 26), ("business", 3977), ("info", 80), ("information", 8481),
                   ("tied", 3), ("longer", 3), ("ambiguous", 9999)]]
        rules, report = compare_shortenings(mappings, records, {})
        self.assertEqual([rules[i]["decision"] for i in mappings], ["accept", "exclude", "exclude", "review", "review", "review"])
        self.assertEqual(next(r for r in report if r["lemma"] == "gym")["full_form_count"], 10)
        self.assertEqual(next(r for r in report if r["lemma"] == "ambiguous")["full_form_count"], "")

    def test_phrase_counts_use_annotated_tokens_case_folding_and_document_boundaries(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "corpus.zip"
            with zipfile.ZipFile(path, "w") as archive:
                for i, tokens in enumerate([["\U0001f33b", "Public", "house", ",", "public", ",", "house", "public"], ["house"]]):
                    text = " ".join(tokens)
                    nodes, offset = [], 0
                    for token in tokens:
                        end = offset + len(token.encode("utf-16-le")) // 2
                        nodes.append(f'<struct type="tok" from="{offset}" to="{end}"><feat name="msd" value="NN"/></struct>')
                        offset = end + 1
                    archive.writestr(f"OANC/data/{i}.txt", text.encode())
                    archive.writestr(f"OANC/data/{i}-hepple.xml", '<cesAna xmlns="http://www.xces.org/schema/2003">'+''.join(nodes)+'</cesAna>')
            self.assertEqual(count_phrases(path, {"public house", "situation comedy"}), {"public house": 1, "situation comedy": 0})

    def test_previously_playable_forms_are_rechecked_and_ordinary_homographs_survive(self):
        gym, gyms = entry("gym"), entry("gyms", lemma="gym", pos="ns")
        info, prop, ordinary_prop = entry("info", 2), entry("prop", 3), entry("prop", 4)
        by_word = {"gym": [gym], "gyms": [gyms], "info": [info], "prop": [prop, ordinary_prop]}
        rules = {i: dict(decision=d, reason="shortening_frequency_test", evidence="fixture")
                 for i, d in [(1, "accept"), (2, "exclude"), (3, "exclude")]}
        rows = [decision("seed", e.word, "accepted", "old_approval", "fixture", seed(e.word), e) for e in [gym, gyms, info, prop]]
        words, rows, review = recheck_playable_shortenings({"GYM", "GYMS", "INFO", "PROP"}, rows, [], by_word, rules, [])
        self.assertEqual(words, {"GYM", "GYMS", "PROP"})
        self.assertFalse(review)
        self.assertTrue(any(r["normalized"] == "INFO" and r["status"] == "superseded" for r in rows))
        self.assertTrue(any(r["normalized"] == "INFO" and r["status"] == "excluded" for r in rows))


class ReligionPolicyTests(unittest.TestCase):
    def recheck(self, entries, words=None, deny=None, exclusions=None):
        by_word, families = {}, {}
        for e in entries:
            by_word.setdefault(e.word.lower(), []).append(e)
            families.setdefault(e.family, []).append(e)
        reviews = {fid: dict(reason="religious_term") for fid in families}
        rows = [decision("dictionary", e.word, "excluded", "religious_term", e.evidence,
                         seed(e.word), e) for e in entries]
        return apply_religion_policy(words or set(), rows, [], by_word, families,
                                     reviews, deny or {}, {}, exclusions or {})

    def test_restores_ordinary_religious_words_and_documented_inflections_only(self):
        words, rows, pending, _ = self.recheck([
            entry("heresy"), entry("heresies", lemma="heresy", pos="ns"),
            entry("theology", 2), entry("Jesus", 3), entry("Christ", 4), entry("Allah", 5)])
        self.assertEqual(words, {"HERESY", "HERESIES", "THEOLOGY"})
        self.assertEqual(pending, [])
        self.assertTrue(all(r["status"] == "superseded" for r in rows[:6]))

    def test_preserves_independent_homographs_and_explicitly_approved_variants(self):
        words, _, _, _ = self.recheck([
            entry("Noel", 1, pos_class="person"), entry("noel", 2),
            entry("aureolae", 3, lemma="aureola", pos="ns", variant=3),
            entry("heresye", 4, lemma="heresy", variant=3)], {"AUREOLAE"})
        self.assertEqual(words, {"NOEL", "AUREOLAE"})

    def test_does_not_bypass_unrelated_denials_or_name_family_evidence(self):
        words, _, _, _ = self.recheck([
            entry("ordinary", 1), entry("sound", 2), entry("named", 3),
            entry("Named", 3, pos_class="name"), entry("restored", 4)],
            deny={"ORDINARY": ("explicit unrelated exclusion", "user"),
                  "RESTORED": ("User excludes religious terms from the review queue", "old policy")},
            exclusions={"SOUND": ("sound exclusion", "user")})
        self.assertEqual(words, {"RESTORED"})


@unittest.skipUnless((DATA / "manifest.json").exists(), "Run the pipeline to test generated artifacts")
class ArtifactTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw = (DATA / "words.txt").read_bytes()
        cls.words = cls.raw.decode("utf-8").splitlines()
        cls.manifest = json.loads((DATA / "manifest.json").read_text())
        with (DATA / "decisions.csv").open(encoding="utf-8", newline="") as handle:
            cls.decisions = list(csv.DictReader(handle))

    def test_output_shape_order_bytes_and_hash(self):
        self.assertEqual(self.words, sorted(set(self.words)))
        self.assertTrue(all(re.fullmatch(r"[A-Z]{2,}", word) for word in self.words))
        self.assertTrue(self.raw.endswith(b"\n"))
        self.assertNotIn(b"\r", self.raw)
        self.assertEqual(hashlib.sha256(self.raw).hexdigest(), self.manifest["output_sha256"]["words.txt"])

    def test_full_seed_accounting_and_no_unreviewed_seed_admission(self):
        counts = self.manifest["counts"]
        self.assertEqual(counts["selected_source_entries"], 10000)
        self.assertEqual(sum(counts[k] for k in ["accepted_seeds", "excluded_seeds", "unresolved_seeds"]), 10000)
        accepted = {row["normalized"] for row in self.decisions if row["status"] == "accepted"}
        self.assertEqual(set(self.words), accepted)
        with (DATA / "review_queue.csv").open(encoding="utf-8", newline="") as handle:
            pending = list(csv.DictReader(handle))
        self.assertEqual(len(pending), counts["unresolved_entries"])
        self.assertTrue(set(self.words).isdisjoint(r["normalized"] for r in pending))
        for row in self.decisions:
            if row["status"] == "accepted":
                self.assertTrue(row["evidence"])
                self.assertTrue(row["seed"] or row["manual_override"])

    def test_required_homographs_and_named_exclusions(self):
        self.assertTrue({"QI", "ROSE", "MARK", "MAY", "WILL", "TOAD", "TOADS", "CLOUDY", "CLOUDIER", "CLOUDIEST", "BANJO", "BANJOS"} <= set(self.words))
        for word in ["LONDON", "NASA", "FREDA"]:
            self.assertNotIn(word, self.words)

    def test_historical_queue_approval_preserved_except_later_explicit_exclusions(self):
        with (DATA / "queue_approvals_v3.csv").open(encoding="utf-8", newline="") as handle:
            approvals = list(csv.DictReader(handle))
        approved = {r["normalized"] for r in approvals}
        self.assertEqual(len(approvals), 990)
        self.assertEqual(len(approved), 990)
        excluded = set(overrides(DATA / "playability_exclusions.tsv"))
        self.assertTrue(approved - excluded <= set(self.words))
        policy = self.manifest["dictionary_expansion"]
        before_religion = (set(self.words) - set(policy["religion_added_words"])) | set(policy["religion_removed_words"])
        before_audit = before_religion | set(policy["playability_removed_words"])
        previous = "\n".join(sorted(before_audit - approved)) + "\n"
        self.assertEqual(hashlib.sha256(previous.encode()).hexdigest(),
                         "4b383583503f6f4bd916215c3e51cb5a4c64afb0d03076f76e8e905c6fd3db7a")
        self.assertEqual(self.manifest["counts"]["unresolved_entries"], 0)
        self.assertEqual(self.manifest["counts"]["queue_approval_additions"], 990)

    def test_playability_audit_only_removes_reviewed_spellings(self):
        excluded = set(overrides(DATA / "playability_exclusions.tsv"))
        removed = set(self.manifest["dictionary_expansion"]["playability_removed_words"])
        self.assertTrue(excluded.isdisjoint(self.words))
        self.assertTrue(removed <= excluded)
        self.assertEqual(len(removed), self.manifest["counts"]["playability_removed"])
        policy = self.manifest["dictionary_expansion"]
        before_religion = (set(self.words) - set(policy["religion_added_words"])) | set(policy["religion_removed_words"])
        before = "\n".join(sorted(before_religion | removed)) + "\n"
        self.assertEqual(hashlib.sha256(before.encode()).hexdigest(),
                         "aef044a51722763ef790936ecc50ecf8da1c6c6c010c0885cb09809a477c6c88")
        self.assertTrue({"DO", "SO", "TO", "AT", "HI", "HELLO", "GYM", "GYMS", "PHONE", "QI", "TI", "MIDI"} <= set(self.words))
        self.assertTrue({"SH", "SHH", "FA", "LA", "MI", "HM", "PFFT", "CD", "CM", "KB", "INC", "INCED", "ED", "ZS"} <= excluded)

    def test_religion_correction_restores_words_without_unrelated_changes(self):
        policy = self.manifest["dictionary_expansion"]
        added, removed = set(policy["religion_added_words"]), set(policy["religion_removed_words"])
        before = (set(self.words) - added) | removed
        self.assertEqual(hashlib.sha256(("\n".join(sorted(before)) + "\n").encode()).hexdigest(),
                         "c023861a87bf3758bc5927b56f74ae16bb33b48e647c7d9b339412749bb77b89")
        self.assertTrue({"HERESY", "HERESIES", "HERETIC", "THEOLOGY", "BISHOP", "NUN", "HYMN", "RELIGIOUSLY"} <= added)
        self.assertTrue({"JESUS", "CHRIST", "ALLAH", "MUHAMMAD", "JEHOVAH", "SATAN"}.isdisjoint(self.words))
        self.assertTrue({"CATHOLIC", "NOEL", "BIBLE", "AUREOLAE", "ROSE", "MARK"} <= set(self.words))
        with (DATA / "religion_review.csv").open(encoding="utf-8", newline="") as handle:
            audit = list(csv.DictReader(handle))
        self.assertEqual({r["word"] for r in audit if r["status"] == "accepted" and r["previous_status"] != "accepted"}, added)
        self.assertEqual(len(audit), self.manifest["counts"]["religion_reviewed_spellings"])

    def test_obvious_names_and_abbreviations_are_excluded_instead_of_queued(self):
        with (DATA / "review_queue.csv").open(encoding="utf-8", newline="") as handle:
            pending = {r["normalized"] for r in csv.DictReader(handle)}
        examples = {"AZ", "BRADY", "CHENEY", "GANDHI", "CAIRO", "GSA", "FL", "LUTHER"}
        self.assertTrue(examples.isdisjoint(pending))
        self.assertTrue(examples.isdisjoint(self.words))
        excluded = {r["normalized"] for r in self.decisions if r["status"] == "excluded"}
        self.assertTrue(examples <= excluded)

    def test_shortening_report_is_frequency_sorted_and_includes_approved_examples(self):
        with (DATA / "shortening_review.csv").open(encoding="utf-8", newline="") as handle:
            rows = list(csv.DictReader(handle))
        self.assertEqual(rows, sorted(rows, key=lambda r: (-int(r["corpus_count"]), r["word"])))
        by_word = {r["word"]: r for r in rows}
        for word in ("GYM", "GYMS", "PHONE"):
            self.assertIn(word, self.words)
            self.assertEqual(by_word[word]["status"], "accepted")
        for word in ("APP", "BIZ", "INFO", "LAB"):
            self.assertEqual(by_word[word]["status"], "excluded")
            self.assertNotIn(word, self.words)
        self.assertGreater(int(by_word["APP"]["corpus_count"]), int(by_word["GYM"]["corpus_count"]))

    def test_preserves_entire_approved_baseline_and_all_explicit_decisions(self):
        with (DATA / "additions.csv").open(encoding="utf-8", newline="") as handle:
            additions = list(csv.DictReader(handle))
        added = {r["normalized"] for r in additions}
        with (DATA / "shortening_word_changes.csv").open(encoding="utf-8", newline="") as handle:
            changes = list(csv.DictReader(handle))
        removed_legacy = {r["word"] for r in changes if r["was_legacy"] == "True" and r["status"] != "accepted"}
        baseline = ((set(self.words) - added) | removed_legacy |
                    set(self.manifest["dictionary_expansion"]["legacy_playability_removals"]))
        self.assertEqual(len(added), self.manifest["counts"]["common_added_words"])
        self.assertEqual(len(baseline), 14618)
        self.assertEqual(hashlib.sha256(("\n".join(sorted(baseline)) + "\n").encode()).hexdigest(),
                         "ba9fd6722c223088bf58f50c0e4992661879068775f844d11c01e67bc9bd3c57")
        self.assertTrue(set(overrides(DATA / "allow.txt")) - set(overrides(DATA / "playability_exclusions.tsv")) <= set(self.words))
        self.assertTrue(set(overrides(DATA / "deny.txt")).isdisjoint(self.words))
        for name, checksum in self.manifest["output_sha256"].items():
            self.assertEqual(hashlib.sha256((DATA / name).read_bytes()).hexdigest(), checksum)

    def test_inflections_trace_to_an_accepted_seed_family(self):
        families = {(r["seed"], r["family_id"]) for r in self.decisions if r["stage"] == "seed" and r["status"] in {"accepted", "superseded"}}
        for row in self.decisions:
            if row["stage"] == "inflection" and row["status"] == "accepted":
                self.assertIn((row["seed"], row["family_id"]), families)
                self.assertTrue(row["lemma"])
                if row["normalized"] == "HAPPINESS":
                    self.assertNotEqual(row["lemma"], "happy")


if __name__ == "__main__":
    unittest.main()
