#!/usr/bin/env python3
"""Preserve the approved vocabulary and broaden it with pinned ESDB size 60."""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict, deque
import csv
from dataclasses import dataclass
import hashlib
import io
import json
import os
from pathlib import Path
import re
import sqlite3
import subprocess
import sys
import urllib.request
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "data/lexicon"
SOURCE_FILE = Path(__file__).with_name("sources.json")
POLICY = "bananagrams-esdb60-ordinary-religious-words-v5"
PARSER_VERSION = 1
SEED_LIMIT = 10_000
ASCII_LOWER = str.maketrans("ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz")
SHAPE = re.compile(r"[A-Za-z]{2,}\Z", re.ASCII)
ORDINARY = {"n", "v", "aj", "av", "pn", "c", "pp", "d", "i"}
FORBIDDEN = {"abbr", "pre", "suf", "wp", "we", "x", "s"}
PROPER = {"person", "surname", "place", "name", "trademark"}
UNCERTAIN = {"upper", "upper?", "name?", "abbr?", "demonym"}
INFLECTIONS = {
    "n": {"n0", "ns"},
    "v": {"v0", "vd", "vn", "vg", "vs", "vds", "vss", "vs2", "vs3"},
    "aj": {"aj0", "aj1", "aj2"}, "av": {"av0", "av1", "av2"},
    "pn": {"pn0", "pn1", "pns", "pnd", "pnp", "pnr0", "pnrs"},
    "d": {"d", "ds", "d1", "d2"}, "c": {"c"}, "pp": {"pp"}, "i": {"i"},
}
FIELDS = ["stage", "original_entry", "normalized", "frequency_rank", "frequency_count",
          "status", "reason", "evidence", "seed", "lemma", "family_id", "form_pos",
          "manual_override", "source_spellings"]


def digest(path: Path) -> str:
    with path.open("rb") as handle:
        return hashlib.file_digest(handle, "sha256").hexdigest()


def write_json(path: Path, value) -> None:
    path.write_bytes((json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode())


def ascii_key(word: str) -> str:
    return word.translate(ASCII_LOWER)


def shape_reason(word: str) -> str | None:
    if len(word) < 2:
        return "shorter_than_two_letters"
    if not SHAPE.fullmatch(word):
        return "non_ascii_letter_or_punctuation"
    return None


def fetch(source: dict, path: Path, offline: bool) -> None:
    if not path.exists():
        if offline:
            raise ValueError(f"Missing cached input: {path}; rerun without --offline")
        print(f"Downloading {source['name']}...", flush=True)
        request = urllib.request.Request(source["url"], headers={"User-Agent": "BananagramsLexicon/1"})
        temporary = path.with_suffix(".download")
        with urllib.request.urlopen(request, timeout=120) as response, temporary.open("wb") as output:
            while block := response.read(1024 * 1024):
                output.write(block)
        if digest(temporary) != source["sha256"]:
            raise ValueError(f"Checksum mismatch for {source['name']}")
        temporary.replace(path)
    if digest(path) != source["sha256"]:
        raise ValueError(f"Checksum mismatch for cached {path.name}")


def rank_forms(records: list[dict], limit: int = SEED_LIMIT) -> list[dict]:
    if len(records) < limit:
        raise ValueError(f"Need {limit} source entries, found {len(records)}")
    ordered = sorted(records, key=lambda row: (-row["count"], row["form"]))[:limit]
    return [dict(row, rank=i) for i, row in enumerate(ordered, 1)]


def read_oanc(path: Path) -> tuple[list[dict], dict]:
    """Count annotated surface forms, never the annotation's lowercased lemma."""
    spellings: dict[str, Counter] = defaultdict(Counter)
    tags: dict[str, Counter] = defaultdict(Counter)
    token_count = 0
    damaged_base_features = []
    namespace = "{http://www.xces.org/schema/2003}"
    with zipfile.ZipFile(path) as archive:
        members = set(archive.namelist())
        annotations = sorted(name for name in members if name.endswith("-hepple.xml"))
        missing = []
        for index, name in enumerate(annotations):
            text_name = name.removesuffix("-hepple.xml") + ".txt"
            if text_name not in members:
                raise ValueError(f"Missing annotated text: {text_name}")
            text = archive.read(text_name).decode("utf-8")
            # Java annotation offsets count UTF-16 units; almost all files are BMP.
            encoded = text.encode("utf-16-le") if any(ord(c) > 0xFFFF for c in text) else None
            length = len(encoded) // 2 if encoded is not None else len(text)
            annotation = archive.read(name)
            # Some supplied lemma annotations contain broken UTF-8. We never use
            # these features; omit only base features, leaving surfaces and offsets intact.
            base_features = re.compile(rb'<feat\s+name="base"\s+value="[^"]*"\s*/>')
            for feature in base_features.findall(annotation):
                try:
                    feature.decode("utf-8")
                except UnicodeDecodeError:
                    damaged_base_features.append(name)
            annotation = base_features.sub(b"", annotation)
            try:
                tree = ET.fromstring(annotation)
            except ET.ParseError as error:
                raise ValueError(f"Invalid POS annotation {name}: {error}") from error
            for node in tree.findall(f"{namespace}struct"):
                if node.get("type") != "tok":
                    continue
                start, end = int(node.attrib["from"]), int(node.attrib["to"])
                if not 0 <= start < end <= length:
                    raise ValueError(f"Invalid annotation offsets: {name}:{start}:{end}")
                surface = (encoded[2 * start:2 * end].decode("utf-16-le")
                           if encoded is not None else text[start:end])
                features = {f.get("name"): f.get("value", "") for f in node}
                if "msd" not in features:
                    raise ValueError(f"Missing POS tag: {name}:{start}")
                key = ascii_key(surface)
                spellings[key][surface] += 1
                tags[key][features["msd"]] += 1
                token_count += 1
            if (index + 1) % 1000 == 0:
                print(f"Counted {index + 1}/{len(annotations)} OANC documents", flush=True)
        for name in sorted(members):
            if name.startswith("OANC/data/") and name.endswith(".txt"):
                if name.removesuffix(".txt") + "-hepple.xml" not in members:
                    missing.append(name)
    records = [{"form": form, "count": sum(variants.values()),
                "spellings": dict(sorted(variants.items())), "tags": dict(sorted(tags[form].items()))}
               for form, variants in sorted(spellings.items())]
    return records, {"annotated_documents": len(annotations), "token_count": token_count,
                     "distinct_surface_forms": len(records), "unannotated_text_files": missing,
                     "ignored_invalid_utf8_base_features": dict(sorted(Counter(damaged_base_features).items()))}


def frequencies(archive: Path, cache: Path, rebuild: bool) -> tuple[list[dict], dict]:
    path, meta = cache / "frequency.json", cache / "frequency-meta.json"
    identity = {"source_sha256": digest(archive), "parser_version": PARSER_VERSION}
    if not rebuild and path.exists() and meta.exists():
        info = json.loads(meta.read_text(encoding="utf-8"))
        if all(info.get(k) == v for k, v in identity.items()):
            if digest(path) != info["sha256"]:
                raise ValueError("Frequency cache checksum mismatch; use --rebuild-inputs")
            payload = json.loads(path.read_text(encoding="utf-8"))
            return payload["records"], payload["stats"]
    records, stats = read_oanc(archive)
    write_json(path, {"records": records, "stats": stats})
    write_json(meta, dict(identity, sha256=digest(path)))
    return records, stats


def prepare_esdb(archive: Path, cache: Path, revision: str, rebuild: bool) -> Path:
    destination = cache / f"wordlist-{revision}"
    database = destination / "scowl.db"
    marker = destination / "build-info.json"
    identity = {"source_sha256": digest(archive), "options": "--dont-combine-pos"}
    if not rebuild and database.exists() and marker.exists():
        info = json.loads(marker.read_text())
        if all(info.get(k) == v for k, v in identity.items()):
            if digest(database) != info["sha256"]:
                raise ValueError("ESDB cache checksum mismatch; use --rebuild-inputs")
            return database
    with zipfile.ZipFile(archive) as source:
        for item in source.infolist():
            target = (cache / item.filename).resolve()
            if not target.is_relative_to(destination.resolve()):
                raise ValueError(f"Unexpected ESDB archive path: {item.filename}")
        source.extractall(cache)
    if database.exists():
        database.unlink()  # This exact generated file is inside the checked cache.
    print("Building pinned ESDB database...", flush=True)
    with (cache / "esdb-build.log").open("w", encoding="utf-8") as log:
        subprocess.run([sys.executable, "combine.py", "create-db", "--dont-combine-pos", "scowl.db"],
                       cwd=destination, env=dict(os.environ, PYTHONUTF8="1", PYTHONHASHSEED="0"), stdout=log, stderr=log, check=True)
    write_json(marker, dict(identity, sha256=digest(database)))
    return database


@dataclass(frozen=True)
class Entry:
    word: str
    lemma: str
    family: int
    pos: str
    base_pos: str
    pos_class: str
    size: int
    spelling: str
    region: str
    variant: int
    group_rank: str
    entry_rank: str
    tag: str
    category: str = ""
    definition: str = ""

    @property
    def evidence(self) -> str:
        return (f"ESDB:family={self.family};lemma={self.lemma};pos={self.pos};"
                f"class={self.pos_class};size={self.size};spelling={self.spelling};"
                f"region={self.region};variant={self.variant};tag={self.tag};"
                f"category={self.category};definition={self.definition}")


def load_entries(database: Path) -> tuple[dict[str, list[Entry]], dict[int, list[Entry]]]:
    by_word, families = defaultdict(list), defaultdict(list)
    with sqlite3.connect(database) as connection:
        rows = connection.execute("""
            SELECT DISTINCT s.word, lemma.word, s.lemma_id, s.pos, s.base_pos,
                s.pos_class, s.size, s.spelling, s.region, s.variant_level,
                s.group_rank, s.entry_rank, s.tag, s.category, g.defn_note
            FROM scowl_ s JOIN words lemma ON lemma.word_id = s.lemma_id
            JOIN groups g ON g.group_id = s.group_id
            ORDER BY s.word, s.lemma_id, s.pos, s.size, s.spelling, s.region, s.variant_level, s.tag, s.category
        """)
        for row in rows:
            entry = Entry(*row)
            by_word[ascii_key(entry.word)].append(entry)
            families[entry.family].append(entry)
    return by_word, families


def classify(entry: Entry, expansion: bool = False) -> str | None:
    if reason := shape_reason(entry.word):
        return reason
    if entry.base_pos in FORBIDDEN or entry.pos_class == "abbr":
        return "abbreviation_affix_contraction_or_nonword"
    if entry.pos_class in PROPER:
        return "proper_name_lexical_entry"
    if entry.pos_class in UNCERTAIN:
        return "review_uncertain_lexical_class"
    if entry.base_pos not in ORDINARY:
        return "review_missing_or_ambiguous_part_of_speech"
    if not entry.word.islower() or (not entry.lemma.islower() and not (entry.lemma == "I" and entry.base_pos == "pn")):
        return "review_capitalized_lexical_entry"
    if entry.region not in ("", "US") or entry.spelling not in ("_", "A") or entry.variant > 6:
        return "non_american_or_nonpreferred_variant"
    if entry.group_rank not in ("", "*") or entry.entry_rank not in ("", "*"):
        return "non_common_marked_lexical_entry" if expansion else "review_marked_lexical_entry"
    if entry.size > (60 if expansion else 80):
        return "outside_common_expansion_tier" if expansion else "review_outside_current_word_tier"
    if entry.variant == 3:
        return "review_disputed_variant"
    if entry.pos not in INFLECTIONS[entry.base_pos]:
        return "not_a_permitted_inflection_slot"
    return None


def overrides(path: Path) -> dict[str, tuple[str, str]]:
    result = {}
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or line.startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) != 3 or not all(parts) or parts[0] != parts[0].upper() or shape_reason(parts[0]):
            raise ValueError(f"Invalid override at {path}:{number}")
        word, reason, evidence = parts
        if word in result:
            raise ValueError(f"Duplicate override: {word}")
        result[word] = (reason, evidence)
    return result


def read_manual(path: Path) -> dict[str, dict]:
    with path.open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    result = {}
    for row in rows:
        if row["decision"] not in {"accept", "exclude"} or not row["reason"] or not row["evidence"]:
            raise ValueError("Manual review needs a decision, reason and evidence")
        key = row["spelling"]
        if key in result or shape_reason(key) or key != key.upper():
            raise ValueError(f"Invalid/duplicate manual spelling: {key}")
        result[key] = row
    return result


def decision(stage: str, original: str, status: str, reason: str, evidence: str = "",
             source: dict | None = None, entry: Entry | None = None, manual: str = "") -> dict:
    source = source or {}
    return dict(zip(FIELDS, [stage, original, original.upper(), source.get("rank", ""),
        source.get("count", ""), status, reason, evidence, source.get("form", ""),
        entry.lemma if entry else "", entry.family if entry else "", entry.pos if entry else "",
        manual, json.dumps(source.get("spellings", {}), ensure_ascii=False, sort_keys=True)]))


def generate(selected: list[dict], by_word: dict[str, list[Entry]], families: dict[int, list[Entry]],
             allow: dict, deny: dict, manual: dict) -> tuple[set[str], list[dict], list[dict], dict]:
    if overlap := set(allow) & set(deny):
        raise ValueError(f"Conflicting allow/deny entries: {sorted(overlap)}")
    for word, record in manual.items():
        if (word in allow and record["decision"] == "exclude") or (word in deny and record["decision"] == "accept"):
            raise ValueError(f"Manual decision conflicts with override: {word}")
    rows, review = [], []
    accepted, seeds, additions = set(), set(), set()
    expanded_families: set[tuple[str, int]] = set()
    counts = Counter()

    def expand(source: dict, matched: Entry):
        identity = (source["form"], matched.family)
        if identity in expanded_families:
            return
        expanded_families.add(identity)
        grouped = defaultdict(list)
        for form in families[matched.family]:
            grouped[(form.word, form.pos)].append(form)
        for key, variants in sorted(grouped.items()):
            qualifying = [form for form in variants if classify(form, expansion=True) is None]
            if not qualifying:
                form = variants[0]
                reasons = ";".join(sorted({classify(form, expansion=True) for form in variants}))
                rows.append(decision("inflection", form.word, "excluded", reasons,
                    " | ".join(sorted({form.evidence for form in variants})), source, form))
                continue
            form = qualifying[0]
            word = form.word.upper()
            if word in deny or (word in manual and manual[word]["decision"] == "exclude"):
                rows.append(decision("inflection", form.word, "excluded", "reviewed_denial", form.evidence,
                                     source, form, "deny/manual_review"))
                continue
            accepted.add(word)
            additions.add(word)
            rows.append(decision("inflection", form.word, "accepted", "validated_same_lemma_inflection",
                                 form.evidence, source, form))

    for source in selected:
        original = sorted(source["spellings"], key=lambda s: (-source["spellings"][s], s))[0]
        word = original.upper()
        if reason := shape_reason(original):
            rows.append(decision("seed", original, "excluded", reason, "OANC annotated surface", source))
            counts["excluded_seeds"] += 1
            continue
        if word in deny:
            reason, evidence = deny[word]
            rows.append(decision("seed", original, "excluded", reason, evidence, source, manual="deny.txt"))
            counts["excluded_seeds"] += 1
            continue
        matches = by_word.get(source["form"], [])
        eligible = [entry for entry in matches if classify(entry) is None]
        adjudication = manual.get(word)
        if adjudication:
            status = "accepted" if adjudication["decision"] == "accept" else "excluded"
            rows.append(decision("seed", original, status, adjudication["reason"], adjudication["evidence"], source, manual="manual_review.csv"))
            if status == "excluded":
                counts["excluded_seeds"] += 1
                continue
        elif not eligible and word not in allow:
            explanations = {classify(entry) for entry in matches}
            # A dotted abbreviation is evidence for denial, never a normalized word.
            dotted = by_word.get(source["form"] + ".", [])
            if not matches and dotted and all(e.base_pos == "abbr" or e.pos_class == "abbr" for e in dotted):
                status, reason = "excluded", "dotted_abbreviation"
                matches = dotted
            elif matches and explanations <= {"proper_name_lexical_entry"}:
                status, reason = "excluded", "proper_noun_only_in_lexical_resource"
            elif matches and explanations <= {"abbreviation_affix_contraction_or_nonword", "not_a_permitted_inflection_slot"}:
                status, reason = "excluded", "abbreviation_affix_or_nonword_only"
            else:
                status, reason = "review", ";".join(sorted(explanations)) if explanations else "missing_lexical_entry"
            row = decision("seed", original, status, reason, " | ".join(sorted({e.evidence for e in matches})), source)
            rows.append(row)
            if status == "review":
                review.append(row)
                counts["unresolved_seeds"] += 1
            else:
                counts["excluded_seeds"] += 1
            continue
        accepted.add(word)
        seeds.add(word)
        counts["accepted_seeds"] += 1
        seen_families = set()
        for entry in eligible:
            if entry.family in seen_families:
                continue
            seen_families.add(entry.family)
            rows.append(decision("seed", original, "accepted", "ordinary_lexical_entry", entry.evidence, source, entry))
            expand(source, entry)
        if not eligible and not adjudication:
            rows.append(decision("seed", original, "accepted", allow[word][0], allow[word][1], source, manual="allow.txt"))
    before_overrides = set(accepted)
    for word, (reason, evidence) in sorted(allow.items()):
        accepted.add(word)
        rows.append(decision("override", word, "accepted", reason, evidence, manual="allow.txt"))
    for word, (reason, evidence) in sorted(deny.items()):
        accepted.discard(word)
        rows.append(decision("override", word, "excluded", reason, evidence, manual="deny.txt"))
    # A disputed expansion needs review only if no independent family admits it.
    pending = {row["normalized"] for row in review}
    for row in rows:
        if (row["stage"] == "inflection" and row["status"] == "excluded"
                and "review_" in row["reason"] and row["normalized"] not in accepted
                and row["normalized"] not in deny):
            row["status"] = "review"
            if row["normalized"] not in pending:
                review.append(row)
                pending.add(row["normalized"])
                counts["unresolved_inflections"] += 1
    for name in ("accepted_seeds", "excluded_seeds", "unresolved_seeds", "unresolved_inflections"):
        counts.setdefault(name, 0)
    counts.update({"selected_source_entries": len(selected), "unique_accepted_seed_spellings": len(seeds),
                   "added_inflections": len((additions - seeds) & accepted),
                   "allow_overrides": len(allow), "deny_overrides": len(deny),
                   "override_only_additions": len(accepted - before_overrides),
                   "manual_review_decisions": len(manual), "final_words": len(accepted),
                   "unresolved_entries": len(review)})
    return accepted, rows, review, dict(counts)


def write_csv(path: Path, rows: list[dict], fields=FIELDS):
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def read_family_reviews(path: Path, families: dict[int, list[Entry]]) -> dict[int, dict]:
    with path.open(encoding="utf-8", newline="") as handle:
        records = list(csv.DictReader(handle))
    result = {}
    for row in records:
        family = int(row["family_id"])
        if (family in result or family not in families or row["decision"] not in {"accept", "exclude", "review"}
                or not row["reason"] or not row["evidence"]
                or any(e.lemma != row["lemma"] for e in families[family])):
            raise ValueError(f"Invalid expansion family review: {row}")
        result[family] = row
    return result


def read_shortening_expansions(path: Path, families: dict[int, list[Entry]]) -> dict[int, dict]:
    with path.open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    result = {}
    for row in rows:
        family = int(row["family_id"])
        if (family in result or family not in families
                or row["mapping_status"] not in {"mapped", "unresolved", "ordinary"}
                or not row["evidence"] or any(e.lemma != row["lemma"] or e.base_pos != row["base_pos"] for e in families[family])
                or (row["mapping_status"] == "mapped" and not re.fullmatch(r"[a-z]+(?: [a-z]+)*", row["full_form"]))
                or (row["mapping_status"] != "mapped" and row["full_form"])):
            raise ValueError(f"Invalid shortening expansion: {row}")
        result[family] = row
    return result


def count_phrases(archive_path: Path, phrases: set[str]) -> dict[str, int]:
    """Count exact contiguous annotated token sequences, never across documents."""
    targets = {tuple(p.split()): p for p in phrases}
    counts = dict.fromkeys(phrases, 0)
    if not targets:
        return counts
    lengths = sorted({len(p) for p in targets})
    namespace = "{http://www.xces.org/schema/2003}"
    with zipfile.ZipFile(archive_path) as archive:
        names = sorted(n for n in archive.namelist() if n.endswith("-hepple.xml"))
        for index, name in enumerate(names):
            text = archive.read(name.removesuffix("-hepple.xml") + ".txt").decode("utf-8")
            encoded = text.encode("utf-16-le") if any(ord(c) > 0xFFFF for c in text) else None
            length = len(encoded) // 2 if encoded is not None else len(text)
            xml = re.sub(rb'<feat\s+name="base"\s+value="[^"]*"\s*/>', b"", archive.read(name))
            recent = deque(maxlen=max(lengths))
            for node in ET.fromstring(xml).findall(f"{namespace}struct"):
                if node.get("type") != "tok":
                    continue
                start, end = int(node.attrib["from"]), int(node.attrib["to"])
                if not 0 <= start < end <= length:
                    raise ValueError(f"Invalid annotation offsets: {name}:{start}:{end}")
                surface = encoded[2 * start:2 * end].decode("utf-16-le") if encoded is not None else text[start:end]
                recent.append(ascii_key(surface))
                window = tuple(recent)
                for size in lengths:
                    if len(window) >= size and (phrase := targets.get(window[-size:])):
                        counts[phrase] += 1
            if (index + 1) % 1000 == 0:
                print(f"Counted comparison phrases in {index + 1}/{len(names)} OANC documents", flush=True)
    return counts


def phrase_frequencies(archive: Path, cache: Path, mappings: dict, rebuild: bool) -> dict:
    phrases = {r["full_form"] for r in mappings.values() if " " in r["full_form"]}
    path, meta = cache / "comparison-phrases.json", cache / "comparison-phrases-meta.json"
    identity = {"source_sha256": digest(archive), "parser_version": 1, "phrases": sorted(phrases)}
    if not rebuild and path.exists() and meta.exists():
        info = json.loads(meta.read_text(encoding="utf-8"))
        if all(info.get(k) == v for k, v in identity.items()):
            if digest(path) != info["sha256"]:
                raise ValueError("Comparison phrase cache checksum mismatch; use --rebuild-inputs")
            return json.loads(path.read_text(encoding="utf-8"))
    counts = count_phrases(archive, phrases)
    write_json(path, counts)
    write_json(meta, dict(identity, sha256=digest(path)))
    return counts


def compare_shortenings(mappings: dict[int, dict], records: list[dict], phrases: dict):
    frequencies = {r["form"]: r["count"] for r in records}
    rules, comparisons = {}, []
    for family, mapping in sorted(mappings.items()):
        short_count = frequencies.get(ascii_key(mapping["lemma"]), 0)
        full = mapping["full_form"]
        full_count = (phrases[full] if " " in full else frequencies.get(full, 0)) if full else ""
        if mapping["mapping_status"] == "ordinary":
            decision_, reason = "accept", "independent_nonshortened_sense"
        elif mapping["mapping_status"] == "unresolved":
            decision_, reason = "review", "shortening_full_form_unresolved"
        elif short_count > full_count:
            decision_, reason = "accept", "shortening_more_frequent_than_full_form"
        elif short_count < full_count:
            decision_, reason = "exclude", "shortening_less_frequent_than_full_form"
        else:
            decision_, reason = "review", "shortening_frequency_tie"
        evidence = mapping["evidence"]
        if full:
            evidence += f" | OANC base surfaces: {mapping['lemma']}={short_count}; {full}={full_count}; documented family forms share this comparison."
        rules[family] = dict(lemma=mapping["lemma"], decision=decision_, reason=reason, evidence=evidence)
        comparisons.append(dict(family_id=family, lemma=mapping["lemma"], base_pos=mapping["base_pos"],
            full_form=full, mapping_status=mapping["mapping_status"], shortening_count=short_count,
            full_form_count=full_count, decision=decision_, reason=reason, evidence=evidence))
    comparisons.sort(key=lambda r: (-r["shortening_count"], r["lemma"], r["family_id"]))
    return rules, comparisons


def recheck_playable_shortenings(words: set[str], rows: list[dict], review: list[dict],
                               by_word: dict, rules: dict, records: list[dict]):
    """Apply the new rule to legacy words too, retaining independent ordinary senses."""
    words = set(words)
    sources = {r["form"]: dict(r, rank=i) for i, r in enumerate(
        sorted(records, key=lambda r: (-r["count"], r["form"])), 1)}
    changed = []
    for word in sorted(words):
        eligible = [e for e in by_word.get(ascii_key(word), []) if classify(e, expansion=True) is None]
        if not eligible or any(e.family not in rules or rules[e.family]["decision"] == "accept" for e in eligible):
            continue
        # Every ordinary family is a nonqualifying shortening; earlier acceptance
        # no longer overrides the user's newer frequency policy.
        words.remove(word)
        status = "review" if any(rules[e.family]["decision"] == "review" for e in eligible) else "excluded"
        relevant = [e for e in eligible if (rules[e.family]["decision"] == "review") == (status == "review")]
        chosen = relevant[0]
        evidence = " | ".join(sorted({rules[e.family]["evidence"] + " | " + e.evidence for e in relevant}))
        row = decision("shortening_frequency", chosen.word, status,
            ";".join(sorted({rules[e.family]["reason"] for e in relevant})), evidence,
            sources.get(ascii_key(word), {"form": ascii_key(word), "count": 0}), chosen)
        changed.append(row)
        for previous in rows:
            if previous["normalized"] == word and previous["status"] == "accepted":
                previous["status"] = "superseded"
                previous["reason"] += ";superseded_by_shortening_frequency_policy"
        if status == "review":
            review.append(row)
    rows.extend(changed)
    return words, rows, review


def apply_queue_approvals(path: Path, words: set[str], rows: list[dict], review: list[dict], denied: set[str]):
    """Apply a later explicit approval to exact spellings, without family expansion."""
    with path.open(encoding="utf-8", newline="") as handle:
        approvals = list(csv.DictReader(handle))
    seen = set()
    for approval in approvals:
        word = approval["normalized"]
        if (word in seen or word in denied or shape_reason(word) or word != word.upper()
                or approval["approval"] != "accept" or not approval["approval_evidence"]
                or approval["status"] != "review"):
            raise ValueError(f"Invalid/conflicting queue approval: {word}")
        seen.add(word)
    added = len(seen - words)
    words = words | seen
    for row in rows:
        if row["normalized"] in seen and row["status"] == "review":
            row["status"] = "superseded"
            row["reason"] += ";superseded_by_explicit_queue_approval"
    for approval in approvals:
        row = {field: approval[field] for field in FIELDS}
        row.update(stage="queue_approval", status="accepted", reason="user_approved_remaining_review_queue",
            evidence=approval["approval_evidence"] + " | Previous review: " + approval["reason"] + " | " + approval["evidence"],
            manual_override=path.name)
        rows.append(row)
    review = [r for r in review if r["normalized"] not in seen]
    return words, rows, review, len(seen), added


def apply_playability_exclusions(exclusions: dict, words: set[str], rows: list[dict], review: list[dict]):
    """Later exact-spelling exclusions supersede historical approvals, without expansion."""
    for row in rows:
        if row["normalized"] in exclusions and row["status"] in {"accepted", "review"}:
            row["status"] = "superseded"
            row["reason"] += ";superseded_by_later_playability_review"
    for word, (reason, evidence) in sorted(exclusions.items()):
        rows.append(decision("playability_review", word, "excluded", reason, evidence,
                             manual="playability_exclusions.tsv"))
    return words - exclusions.keys(), rows, [r for r in review if r["normalized"] not in exclusions]


def apply_religion_policy(words: set[str], rows: list[dict], review: list[dict],
                          by_word: dict[str, list[Entry]], families: dict[int, list[Entry]],
                          family_reviews: dict[int, dict], deny: dict, manual: dict,
                          playability_exclusions: dict):
    """Replace religious-topic exclusions with ordinary-sense eligibility.

    Historical inputs remain intact. Recheck every previously religion-screened
    family, including approved review entries, rather than restoring a word just
    because it was once marked religious. Unrelated exclusions still win.
    """
    religion_denied = {w for w, (reason, _) in deny.items()
                       if reason.startswith("User excludes religious terms")}
    family_ids = {fid for fid, r in family_reviews.items() if "religious" in r["reason"]}
    family_ids.update(e.family for w in religion_denied for e in by_word.get(ascii_key(w), []))
    candidates = {e.word.upper() for fid in family_ids for e in families[fid]
                  if shape_reason(e.word) is None and (e.size <= 60 or e.word.upper() in words)}
    blocked = ((set(deny) - religion_denied) | set(playability_exclusions) |
               {w for w, r in manual.items() if r["decision"] == "exclude"})
    named = {e.family for entries in by_word.values() for e in entries
             if e.pos_class in PROPER | {"name?"} or "[name]" in e.tag}
    latest = {r["normalized"]: r for r in rows if r["status"] != "superseded"}
    latest.update({r["normalized"]: r for r in rows if r["status"] == "accepted"})
    result, audit, replacement_rows = set(words), [], []
    for word in sorted(candidates):
        entries = by_word[ascii_key(word)]
        ordinary = [e for e in entries if e.word.islower() and e.lemma.islower()
                    and e.base_pos in ORDINARY and e.pos_class not in PROPER | UNCERTAIN | {"abbr"}
                    and e.family not in named and not e.category]
        eligible = [e for e in ordinary if classify(e, expansion=True) is None]
        # Keep already-approved ordinary variants (e.g. AUREOLAE). This change
        # concerns religious meaning, not a second review of spelling variants.
        if not eligible and word in words:
            eligible = ordinary
        accepted = bool(eligible) and word not in blocked
        status = "accepted" if accepted else "excluded"
        reason = ("ordinary_religious_word" if accepted else
                  "unrelated_explicit_exclusion" if word in blocked else
                  "no_eligible_independent_ordinary_sense")
        chosen = (eligible or entries)[0]
        evidence = ("User 2026-11-20: allow ordinary religious words; exclude proper names such as Jesus and Christ; "
                    "preserve independent ordinary senses. | " + chosen.evidence)
        prior = latest.get(word)
        row = dict(prior) if prior else decision("religion_review", chosen.word, status, reason)
        row.update(stage="religion_review", original_entry=chosen.word, normalized=word,
                   status=status, reason=reason, evidence=evidence, lemma=chosen.lemma,
                   family_id=chosen.family, form_pos=chosen.pos, manual_override="user_policy_2026-11-20")
        replacement_rows.append(row)
        audit.append(dict(word=word, previous_status="accepted" if word in words else
                          prior["status"] if prior else "not_playable",
                          status=status, reason=reason, evidence=evidence))
        if accepted:
            result.add(word)
        else:
            result.discard(word)
    for row in rows:
        if row["normalized"] in candidates and row["status"] != "superseded":
            row["status"] = "superseded"
            row["reason"] += ";superseded_by_religion_policy_correction"
    rows.extend(replacement_rows)
    return result, rows, [r for r in review if r["normalized"] not in candidates], audit


def broaden(baseline: set[str], by_word: dict[str, list[Entry]], records: list[dict],
            deny: dict, manual: dict, family_reviews: dict[int, dict]):
    """Admit explicit dictionary forms independently of corpus rank or presence.

    Rules apply only to additions: the already approved list is preserved. A
    separate ordinary sense can justify a name homograph, but exact denials win.
    """
    frequencies = {r["form"]: dict(r, rank=i) for i, r in enumerate(
        sorted(records, key=lambda r: (-r["count"], r["form"])), 1)}
    denied = set(deny) | {w for w, r in manual.items() if r["decision"] == "exclude"}
    denied_families = {e.family for w in denied for e in by_word.get(ascii_key(w), [])}
    # ESDB can repeat a named family at another tier without its [name] tag.
    # Retain that evidence across records, but never transfer it to an
    # independent ordinary homograph (or infer a name from capitalization alone).
    named_families = {e.family: e.evidence for entries in by_word.values() for e in entries
                      if e.pos_class in PROPER | {"name?"} or "[name]" in e.tag}
    abbreviation_families = {e.family: e.evidence for entries in by_word.values() for e in entries
                            if e.base_pos == "abbr" or e.pos_class in {"abbr", "abbr?"}}
    candidates = defaultdict(list)
    for entries in by_word.values():
        for entry in entries:
            if entry.size <= 60 and shape_reason(entry.word) is None and entry.word.upper() not in baseline:
                candidates[entry.word.upper()].append(entry)
    words, rows, review, additions = set(baseline), [], [], []
    counts = Counter(legacy_approved_words=len(baseline), common_candidates=len(candidates),
                     common_added_words=0, common_excluded_words=0, common_review_words=0)
    for word, entries in sorted(candidates.items()):
        source = frequencies.get(ascii_key(word), {"form": ascii_key(word), "count": 0, "spellings": {}})
        outcomes = []
        for entry in entries:
            if word in denied:
                status, reason = "excluded", "reviewed_denial"
            elif entry.pos_class == "demonym":
                status, reason = "excluded", "geographic_demonym"
            elif entry.category:
                status, reason = "excluded", "special_dictionary_category"
            elif entry.base_pos in FORBIDDEN or entry.pos_class == "abbr":
                # A mapping question must not revive a separately disallowed
                # abbreviation entry after its ordinary clipping sense fails.
                status, reason = "excluded", "abbreviation_affix_contraction_or_nonword"
            elif entry.family in family_reviews:
                record = family_reviews[entry.family]
                # Family approval resolves the category question, not spelling,
                # commonness or inflection eligibility. Never invent relatives.
                if record["decision"] == "accept" and (reason := classify(entry, expansion=True)):
                    status = "review" if reason.startswith("review_") else "excluded"
                else:
                    status = {"accept": "accepted", "exclude": "excluded", "review": "review"}[record["decision"]]
                    reason = record["reason"]
            elif reason := classify(entry, expansion=True):
                status = "review" if reason.startswith("review_") else "excluded"
                if status == "review" and entry.family in named_families:
                    status, reason = "excluded", "named_family_without_ordinary_sense"
                elif status == "review" and entry.family in abbreviation_families:
                    status, reason = "excluded", "abbreviation_family"
            elif entry.family in denied_families:
                status, reason = "review", "relative_of_previously_denied_spelling"
            elif entry.definition == "religious":
                status, reason = "review", "religious_sense_needs_review"
            else:
                status, reason = "accepted", "ordinary_esdb60_entry"
            outcomes.append((status, reason, entry))
        # Prefer an independently eligible ordinary sense over a name/abbreviation.
        status = next(s for s in ("accepted", "review", "excluded") if any(o[0] == s for o in outcomes))
        relevant = [o for o in outcomes if o[0] == status]
        chosen = relevant[0][2]
        evidence = " | ".join(sorted({e.evidence for _, _, e in outcomes}))
        notes = {family_reviews[e.family]["evidence"] for _, _, e in outcomes if e.family in family_reviews}
        notes.update(named_families[e.family] for _, reason, e in outcomes
                     if reason == "named_family_without_ordinary_sense")
        notes.update(abbreviation_families[e.family] for _, reason, e in outcomes
                     if reason == "abbreviation_family")
        if notes:
            evidence += " | " + " | ".join(sorted(notes))
        row = decision("dictionary", chosen.word, status,
                       ";".join(sorted({r for _, r, _ in relevant})), evidence, source, chosen)
        rows.append(row)
        if status == "accepted":
            words.add(word)
            additions.append(row)
            counts["common_added_words"] += 1
        elif status == "review":
            review.append(row)
            counts["common_review_words"] += 1
        else:
            counts["common_excluded_words"] += 1
    review.sort(key=lambda r: (r["frequency_rank"] or float("inf"), r["normalized"]))
    return words, rows, review, additions, dict(counts)


def build(cache: Path, output: Path, offline: bool, rebuild: bool) -> dict:
    sources = json.loads(SOURCE_FILE.read_text(encoding="utf-8"))
    cache.mkdir(parents=True, exist_ok=True)
    for name in ("oanc", "esdb"):
        fetch(sources[name], cache / f"{name}.zip", offline)
    allow, deny = overrides(DATA / "allow.txt"), overrides(DATA / "deny.txt")
    if set(allow) & set(deny):
        raise ValueError("An entry appears in both allow.txt and deny.txt")
    manual = read_manual(DATA / "manual_review.csv")
    records, stats = frequencies(cache / "oanc.zip", cache, rebuild)
    selected = rank_forms(records)
    database = prepare_esdb(cache / "esdb.zip", cache, sources["esdb"]["version"], rebuild)
    by_word, families = load_entries(database)
    words, rows, review, counts = generate(selected, by_word, families, allow, deny, manual)
    legacy_words = set(words)
    legacy_word_sha256 = hashlib.sha256(("\n".join(sorted(words)) + "\n").encode()).hexdigest()
    family_reviews = read_family_reviews(DATA / "expansion_family_reviews.csv", families)
    before_frequency = broaden(words, by_word, records, deny, manual, family_reviews)[0]
    mappings = read_shortening_expansions(DATA / "shortening_expansions.csv", families)
    phrases = phrase_frequencies(cache / "oanc.zip", cache, mappings, rebuild)
    frequency_rules, comparisons = compare_shortenings(mappings, records, phrases)
    family_reviews.update(frequency_rules)
    words, expanded_rows, expanded_review, additions, expanded_counts = broaden(
        words, by_word, records, deny, manual, family_reviews)
    rows.extend(expanded_rows)
    # A dictionary entry may resolve an old pending corpus entry independently.
    review = [r for r in review if r["normalized"] not in words]
    pending = {r["normalized"] for r in review}
    review.extend(r for r in expanded_review if r["normalized"] not in pending)
    words, rows, review = recheck_playable_shortenings(words, rows, review, by_word, frequency_rules, records)
    review = list({r["normalized"]: r for r in review if r["normalized"] not in words}.values())
    review.sort(key=lambda r: (r["frequency_rank"] or float("inf"), r["normalized"]))
    frequency_words = set(words)
    frequency_rows = {r["normalized"]: r for r in rows if r["status"] != "superseded"}
    frequency_pending = {r["normalized"] for r in review}
    word_changes = [dict(word=w, previous_status="accepted" if w in before_frequency else "not_playable",
        status="accepted" if w in words else "review" if w in frequency_pending else "excluded",
        was_legacy=w in legacy_words, reason=frequency_rows[w]["reason"], evidence=frequency_rows[w]["evidence"])
        for w in sorted(words ^ before_frequency)]
    words, rows, review, approval_count, approval_additions = apply_queue_approvals(
        DATA / "queue_approvals_v3.csv", words, rows, review,
        set(deny) | {w for w, r in manual.items() if r["decision"] == "exclude"})
    before_playability = set(words)
    exclusions = overrides(DATA / "playability_exclusions.tsv")
    if "QI" in exclusions:
        raise ValueError("QI must remain an explicit inclusion")
    words, rows, review = apply_playability_exclusions(exclusions, words, rows, review)
    after_playability = set(words)
    words, rows, review, religion_audit = apply_religion_policy(
        words, rows, review, by_word, families, family_reviews, deny, manual, exclusions)
    accepted_rows = {r["normalized"]: r for r in rows if r["status"] == "accepted"}
    additions = [accepted_rows[word] for word in sorted(words - legacy_words)]
    latest_rows = {r["normalized"]: r for r in rows if r["status"] != "superseded"}
    latest_rows.update(accepted_rows)
    counts.update(expanded_counts)
    counts.update(final_words=len(words), unresolved_entries=len(review))
    counts.update(common_added_words=len(additions), legacy_retained_words=len(words & legacy_words),
        common_review_words_before_queue_approvals=expanded_counts["common_review_words"],
        common_review_words=sum(r["normalized"] not in legacy_words for r in review),
        legacy_removed_by_frequency=len(legacy_words - before_playability),
        playability_exclusion_overrides=len(exclusions),
        playability_removed=len(before_playability - after_playability),
        legacy_removed_by_playability=len((before_playability - after_playability) & legacy_words),
        religion_reviewed_spellings=len(religion_audit),
        religion_restored_words=len(words - after_playability),
        religion_removed_words=len(after_playability - words),
        words_before_playability_review=len(before_playability),
        frequency_newly_accepted=len(frequency_words - before_frequency), frequency_removed=len(before_frequency - frequency_words),
        queue_approval_overrides=approval_count, queue_approval_additions=approval_additions,
        shortening_mapped_families=sum(r["mapping_status"] == "mapped" for r in comparisons),
        shortening_unresolved_families=sum(r["mapping_status"] == "unresolved" for r in comparisons))
    if "QI" not in allow or "QI" not in words:
        raise ValueError("QI must be an explicit allow override")
    if any(not re.fullmatch(r"[A-Z]{2,}", word) for word in words):
        raise ValueError("Invalid final vocabulary spelling")
    output.mkdir(parents=True, exist_ok=True)
    (output / "words.txt").write_bytes(("\n".join(sorted(words)) + "\n").encode("utf-8"))
    write_csv(output / "decisions.csv", rows)
    write_csv(output / "review_queue.csv", review)
    write_csv(output / "additions.csv", additions)
    write_csv(output / "religion_review.csv", religion_audit,
              ["word", "previous_status", "status", "reason", "evidence"])
    write_csv(output / "shortening_frequency.csv", comparisons,
        ["family_id", "lemma", "base_pos", "full_form", "mapping_status", "shortening_count", "full_form_count", "decision", "reason", "evidence"])
    write_csv(output / "shortening_word_changes.csv", word_changes,
        ["word", "previous_status", "status", "was_legacy", "reason", "evidence"])
    shortening_words = {e.word.upper() for entries in by_word.values() for e in entries
                        if e.family in frequency_rules and e.size <= 60 and shape_reason(e.word) is None}
    shortening_rows = [dict(word=r["normalized"], lemma=r["lemma"],
        corpus_count=r["frequency_count"], corpus_rank=r["frequency_rank"],
        status=r["status"], reason=r["reason"], evidence=r["evidence"])
        for w, r in latest_rows.items() if w in shortening_words]
    shortening_rows.sort(key=lambda r: (-int(r["corpus_count"]), r["word"]))
    write_csv(output / "shortening_review.csv", shortening_rows,
              ["word", "lemma", "corpus_count", "corpus_rank", "status", "reason", "evidence"])
    write_csv(output / "selected_seeds.csv", [dict(rank=s["rank"], form=s["form"], count=s["count"],
        spellings=json.dumps(s["spellings"], ensure_ascii=False, sort_keys=True),
        corpus_pos_counts=json.dumps(s["tags"], sort_keys=True)) for s in selected],
        ["rank", "form", "count", "spellings", "corpus_pos_counts"])
    manifest = {"policy": POLICY, "status": "candidate_pending_lexical_review" if review else "reviewed_candidate",
        "sources": sources, "corpus": stats, "counts": counts,
        "frequency_selection": {"measure": "raw annotated surface occurrence count, all spoken and written documents",
            "counting": "ASCII case-insensitive surface forms; punctuation and nonletters retained; no vocabulary prefilter",
            "tie_break": "ascending original surface key by Unicode code point", "selected": SEED_LIMIT,
            "cutoff": selected[-1]["form"], "cutoff_count": selected[-1]["count"],
            "description": "Top-10000 selection reproduces the previously approved baseline only; frequency never limits ESDB60 additions"},
        "lexical_policy": {"seed_max_size": 80, "expansion_max_size": 60, "variant_levels": [0, 1, 2, 4, 5, 6],
            "spellings": ["_", "A"], "regions": ["", "US"], "profanity_filter": False,
            "expansion": "same explicit lemma_id and permitted inflection slots only; no clusters, suffix rules or derivation"},
        "dictionary_expansion": {"maximum_size": 60, "frequency_cutoff": None,
            "queue_category_policy": "name/name?/[name] and abbreviation family evidence persists across source records; independent ordinary senses remain eligible; unclassified capitals need explicit family review",
            "shortening_policy": "base shortening count > literal full-form count accepts the category; < excludes; ties/unknown mappings need review. Applies to previously playable forms; independent ordinary senses and explicit denials are preserved.",
            "latest_queue_approval": "queue_approvals_v3.csv overrides automatic review decisions for exact spellings after frequency evaluation; no additional family forms are implied",
            "later_playability_exclusions": "playability_exclusions.tsv supersedes historical approvals for exact spellings and remains binding during the later religion review",
            "legacy_playability_removals": sorted((before_playability - after_playability) & legacy_words),
            "playability_removed_words": sorted(before_playability - after_playability),
            "religion_policy": "Religious subject matter is not an exclusion. Previously screened families are rechecked for independent ordinary lowercase senses; proper/name-derived forms and unrelated exclusions remain screened.",
            "religion_added_words": sorted(words - after_playability),
            "religion_removed_words": sorted(after_playability - words),
            "preserve_legacy_approvals": "except identified shortenings rechecked under the newer frequency policy", "review_order": "corpus rank ascending, then spelling; unseen words last",
            "legacy_word_sha256": legacy_word_sha256,
            "pre_frequency_word_sha256": hashlib.sha256(("\n".join(sorted(before_frequency)) + "\n").encode()).hexdigest(),
            "comparison_phrase_counts": phrases,
            "family_review_count": len(family_reviews), "pending_words_playable": False},
        "input_sha256": {path.name: digest(path) for path in [SOURCE_FILE, DATA / "allow.txt", DATA / "deny.txt", DATA / "manual_review.csv", DATA / "expansion_family_reviews.csv", DATA / "shortening_expansions.csv", DATA / "queue_approvals_v3.csv", DATA / "playability_exclusions.tsv"]},
        "builder_sha256": digest(Path(__file__)),
        "license_notice_sha256": {path.name: digest(path) for path in sorted((DATA / "licenses").glob("*.txt"))},
        "output_sha256": {name: digest(output / name) for name in ["words.txt", "decisions.csv", "review_queue.csv", "selected_seeds.csv", "additions.csv", "shortening_review.csv", "shortening_frequency.csv", "shortening_word_changes.csv", "religion_review.csv"]}}
    write_json(output / "manifest.json", manifest)
    print(json.dumps(counts, indent=2), flush=True)
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, default=ROOT / "build/lexicon-cache")
    parser.add_argument("--output", type=Path, default=DATA)
    parser.add_argument("--offline", action="store_true")
    parser.add_argument("--rebuild-inputs", action="store_true", help="Recount raw corpus and rebuild ESDB instead of using derived caches")
    args = parser.parse_args()
    try:
        build(args.cache.resolve(), args.output.resolve(), args.offline, args.rebuild_inputs)
    except (ValueError, OSError, subprocess.CalledProcessError, zipfile.BadZipFile) as error:
        parser.exit(1, f"Lexicon build failed: {error}\n")


if __name__ == "__main__":
    main()
