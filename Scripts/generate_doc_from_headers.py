import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "Source"
OUT = ROOT / "Documentation" / "source" / "params"
NO_PRIORITY = 10**9

CLASS_RE = re.compile(r'\s*(?:class|struct)\s+(?:\w+_API\s+)?(\w+)(?:\s*:\s*public\s+(\w+))?')
ENUM_RE = re.compile(r'UENUM\s*\(.*?\)\s*enum\s+class\s+(\w+)\s*(?::\s*\w+)?\s*\{(.*?)\}\s*;', re.S)
ENTRY_RE = re.compile(
    r'((?:\s*(?:/\*.*?\*/|//[^\n]*))*)\s*(\w+)\s*(?:=[^,\n]*?)?\s*(?:UMETA\s*\(([^)]*)\))?\s*(?:,|\Z)',
    re.S,
)


def prettify(name):
    name = re.sub(r'^b(?=[A-Z])', '', name)
    name = name.replace("_", " ")
    name = re.sub(r'(?<=[a-z0-9])(?=[A-Z])', ' ', name)
    name = re.sub(r'(?<=[A-Z])(?=[A-Z][a-z])', ' ', name)
    return re.sub(r'\s+', ' ', name).strip()


def match_paren(text, start):
    depth = 0
    in_str = False
    i = start
    while i < len(text):
        c = text[i]
        if in_str:
            if c == "\\":
                i += 1
            elif c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def read_decl(text, i):
    depth = 0
    in_str = False
    j = i
    while j < len(text):
        c = text[j]
        if in_str:
            if c == "\\":
                j += 1
            elif c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c in "({[":
            depth += 1
        elif c in ")}]":
            depth -= 1
        elif c == ";" and depth == 0:
            return text[i:j], j
        j += 1
    return None, i


def split_default(decl):
    depth = 0
    in_str = False
    for k, c in enumerate(decl):
        if in_str:
            if c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c in "({[<":
            depth += 1
        elif c in ")}]>":
            depth -= 1
        elif c == "=" and depth == 0:
            prev = decl[k - 1] if k else ""
            nxt = decl[k + 1] if k + 1 < len(decl) else ""
            if nxt != "=" and prev not in "!<>=":
                return decl[:k], decl[k + 1:]
    return decl, None


def clean_comment(raw):
    raw = raw.strip()
    if not raw:
        return ""
    if raw.startswith("/*"):
        raw = re.sub(r'^/\*+', '', raw)
        raw = re.sub(r'\*+/\s*$', '', raw)
        lines = [re.sub(r'^\s*\*+ ?', '', l) for l in raw.split("\n")]
    else:
        lines = [re.sub(r'^\s*//+ ?', '', l) for l in raw.split("\n")]
    paras = []
    cur = []
    for l in lines:
        if l.strip():
            cur.append(l.strip())
        elif cur:
            paras.append(" ".join(cur))
            cur = []
    if cur:
        paras.append(" ".join(cur))
    text = "\n\n".join(paras)
    return re.sub(r'(?<!`)`([^`\n]+)`(?!`)', r'``\1``', text)


def pre_comment(text, pos):
    lines = text[:pos].split("\n")
    lines.pop()
    out = []
    if lines and lines[-1].strip().endswith("*/"):
        while lines:
            l = lines.pop()
            out.append(l)
            if "/*" in l:
                break
        block = "\n".join(reversed(out))
        if block.lstrip().startswith("/***"):
            return ""
        return block
    while lines and lines[-1].strip().startswith("//"):
        out.append(lines.pop())
    return "\n".join(reversed(out))


def post_comments(text, i):
    out = []
    while True:
        m = re.compile(r'\s*').match(text, i)
        i = m.end()
        if text.startswith("/*", i):
            e = text.find("*/", i)
            if e < 0:
                break
            out.append(text[i:e + 2])
            i = e + 2
        elif text.startswith("//", i):
            e = text.find("\n", i)
            e = len(text) if e < 0 else e
            out.append(text[i:e])
            i = e
        else:
            break
    return "\n".join(out), i


def simple_type(t):
    t = " ".join(t.split())
    t = re.sub(r'TEnumAsByte<\s*([\w:]+?)(?:::Type)?\s*>', r'\1', t)
    t = re.sub(r'^TObjectPtr<\s*(.+)\s*>$', r'\1', t)
    return t


def parse_enums(text, enums):
    for m in ENUM_RE.finditer(text):
        entries = []
        for e in ENTRY_RE.finditer(m.group(2)):
            name = e.group(2)
            meta = e.group(3) or ""
            dn = re.search(r'DisplayName\s*=\s*"([^"]*)"', meta)
            entries.append({
                "name": name,
                "display": dn.group(1) if dn else prettify(name),
                "doc": " ".join(clean_comment(e.group(1)).split()),
            })
        enums[m.group(1)] = entries


def parse_file(text):
    classes = []
    for m in re.finditer(r'\b(UCLASS|USTRUCT)\s*\(', text):
        end = match_paren(text, m.end() - 1)
        if end < 0:
            continue
        cm = CLASS_RE.match(text, end + 1)
        if cm:
            classes.append({"start": m.start(), "name": cm.group(1), "base": cm.group(2), "props": []})
    classes.sort(key=lambda c: c["start"])

    for m in re.finditer(r'\bUPROPERTY\s*\(', text):
        end = match_paren(text, m.end() - 1)
        if end < 0:
            continue
        args = text[m.end():end]
        if not re.search(r'\b(Edit|Visible)(Anywhere|DefaultsOnly|InstanceOnly)\b', args):
            continue
        cond = re.search(r'EditCondition\s*=\s*"([^"]*)"', args)
        if cond and cond.group(1).strip() == "false":
            continue
        post, after = post_comments(text, end + 1)
        decl, _ = read_decl(text, after)
        if decl is None:
            continue
        left, default = split_default(decl)
        dm = re.match(r'(.+?)\s+[*&]*(\w+)\s*(?:\[.*\])?\s*$', left.strip(), re.S)
        if not dm:
            continue
        cat = re.search(r'Category\s*=\s*"([^"]*)"', args)
        pri = re.search(r'DisplayPriority\s*=\s*"(-?\d+)"', args)
        dn = re.search(r'DisplayName\s*=\s*"([^"]*)"', args)
        name = dm.group(2)
        doc = clean_comment("\n\n".join(x for x in (pre_comment(text, m.start()), post) if x.strip()))
        prop = {
            "name": name,
            "type": simple_type(dm.group(1)),
            "default": " ".join(default.split()) if default is not None else None,
            "category": cat.group(1) if cat else "Default",
            "priority": int(pri.group(1)) if pri else NO_PRIORITY,
            "display": dn.group(1) if dn else prettify(name),
            "doc": doc,
        }
        owner = None
        for c in classes:
            if c["start"] <= m.start():
                owner = c
        if owner:
            owner["props"].append(prop)
    return classes


def format_default(prop, enums):
    d = prop["default"]
    if d is None:
        return None
    if prop["type"] in enums:
        key = d.split("::")[-1]
        for e in enums[prop["type"]]:
            if e["name"] == key:
                return e["display"]
        return None
    if d in ("", '""', "{}", "nullptr") or "Cast<" in d or "StaticLoad" in d or len(d) > 40 or "`" in d:
        return None
    return d


def cat_title(cat):
    return " > ".join(prettify(p) for p in cat.split("|"))


def all_props(name, index, seen=None):
    seen = seen or set()
    if name not in index or name in seen:
        return []
    seen.add(name)
    c = index[name]
    inherited = all_props(c["base"], index, seen) if c["base"] else []
    own_names = {p["name"] for p in c["props"]}
    return [p for p in inherited if p["name"] not in own_names] + c["props"]


def render(props, enums):
    cats = {}
    for p in props:
        cats.setdefault(p["category"], []).append(p)
    lines = []
    for cat, ps in cats.items():
        ps = sorted(ps, key=lambda p: p["priority"])
        lines += [f".. rubric:: {cat_title(cat)}", ""]
        for p in ps:
            head = f"* **{p['display']}** (``{p['type']}``"
            d = format_default(p, enums)
            if d is not None:
                head += f", default ``{d}``"
            head += ")"
            if p["doc"]:
                head += ":"
            lines.append(head)
            if p["doc"]:
                paras = p["doc"].split("\n\n")
                lines.append("  " + paras[0])
                for para in paras[1:]:
                    lines += ["", "  " + para]
            if p["type"] in enums:
                lines.append("")
                for e in enums[p["type"]]:
                    item = f"  * ``{e['display']}``"
                    if e["doc"]:
                        item += f": {e['doc']}"
                    lines.append(item)
            lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def main():
    enums = {}
    index = {}
    for h in SOURCE.rglob("*.h"):
        if "ThirdParty" in h.parts:
            continue
        text = h.read_text(encoding="utf-8", errors="replace")
        parse_enums(text, enums)
        for c in parse_file(text):
            index[c["name"]] = c
    OUT.mkdir(parents=True, exist_ok=True)
    for name in sorted(index):
        props = all_props(name, index)
        if not props:
            continue
        name_out = re.sub(r'^[AUF](?=[A-Z])', '', name)
        path = OUT / f"{name_out}.inc"
        path.write_text(render(props, enums), encoding="utf-8", newline="\n")
        print(path)


if __name__ == "__main__":
    main()