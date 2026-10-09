#!/bin/sh
# Generate the documentation site's sources into build-site/ (not tracked).
#
#   ./docs-site/gen-site.sh            (from anywhere; works on the repo root)
#
# Then build with MkDocs (installed only for the site, see CONTRIBUTING.md):
#   mkdocs build --strict -f build-site/mkdocs.yml -d <output folder>
#
# What it does, all in POSIX sh + awk:
#   1. copies every tracked Markdown and PDF file into build-site/docs/,
#      keeping its path, so relative links between pages keep working;
#   2. rewrites links that point at other tracked files (source, scripts,
#      folders without a README) to their page on GitHub;
#   3. writes reference pages from the TSS source: one per command (the
#      CMD/SCn tables in TSS5), one per monitor call (MCTBL in TSS1) and one
#      per documented routine (a "%NAME" header whose NAME is defined in the
#      same part), with "calls" / "called by" links from JPL / JMP I (NAME;
#   4. writes build-site/mkdocs.yml: INHERIT the committed base config plus
#      the generated nav.
# Only files git tracks are used, so the result matches a clean checkout.
set -eu
cd "$(dirname "$0")/.."

REPO_URL=https://github.com/RetroCoreLabs/TSS
OUT=build-site
D=$OUT/docs
rm -rf "$OUT"
mkdir -p "$D"

git ls-files > "$OUT/tracked.txt"

# ---------------------------------------------------------------- 1. copy
grep -E '\.(md|pdf)$' "$OUT/tracked.txt" | grep -v '^docs-site/' | grep -v '^CLAUDE\.md$' > "$OUT/pages.txt" || true
while IFS= read -r f; do
    mkdir -p "$D/$(dirname "$f")"
    cp "$f" "$D/$f"
done < "$OUT/pages.txt"

# ------------------------------------------------- 2. rewrite non-page links
# A link target that resolves to a tracked .md or .pdf stays relative. A
# folder with a tracked README.md points at that README. Any other tracked
# file or folder points at GitHub. Links inside ``` fences are left alone.
rewrite() {   # $1 = page path relative to the repo root
    awk -v page="$1" -v url="$REPO_URL" -v tracked="$OUT/tracked.txt" '
    function norm(p,   n, a, i, o, k) {
        n = split(p, a, "/"); k = 0
        for (i = 1; i <= n; i++) {
            if (a[i] == "" || a[i] == ".") continue
            if (a[i] == "..") { if (k > 0) k--; continue }
            o[++k] = a[i]
        }
        p = ""; for (i = 1; i <= k; i++) p = p (i > 1 ? "/" : "") o[i]
        return p
    }
    function fix(t,   anchor, h, path, dir, r) {
        if (t ~ /^(https?:|mailto:|#)/ || t ~ /^</) return t
        anchor = ""; h = index(t, "#")
        if (h) { anchor = substr(t, h); t = substr(t, 1, h - 1) }
        if (t == "") return anchor
        dir = page; sub(/[^\/]*$/, "", dir)
        path = norm(dir t)
        if (path in pg) return t anchor
        if ((path "/README.md") in pg) { r = t; sub(/\/$/, "", r); return r "/README.md" anchor }
        if (path in tf) return url "/blob/main/" path anchor
        if (path in td || path == "") return url "/tree/main/" path
        return t anchor
    }
    BEGIN {
        while ((getline l < tracked) > 0) {
            tf[l] = 1
            if (l ~ /\.(md|pdf)$/) pg[l] = 1
            n = split(l, a, "/"); p = ""
            for (i = 1; i < n; i++) { p = p (i > 1 ? "/" : "") a[i]; td[p] = 1 }
        }
    }
    /^[ \t]*```/ { fence = !fence; print; next }
    fence { print; next }
    {
        out = ""; s = $0
        while (match(s, /\]\([^) ]+\)/)) {
            t = substr(s, RSTART + 2, RLENGTH - 3)
            out = out substr(s, 1, RSTART + 1) fix(t) ")"
            s = substr(s, RSTART + RLENGTH)
        }
        print out s
    }' "$D/$1" > "$D/$1.tmp" && mv "$D/$1.tmp" "$D/$1"
}
grep '\.md$' "$OUT/pages.txt" | while IFS= read -r f; do rewrite "$f"; done

# The home page links the reference and the history in its first screen.
awk 'NR == 1 { print; print ""; print "**Reference:** [commands](code-reference/commands/index.md) · [monitor calls](code-reference/monitor-calls/index.md) · [routines](code-reference/routines/index.md) · **History:** [NORD TSS and SINTRAN III](docs/TSS-AND-SINTRAN.md)"; next } { print }' \
    "$D/README.md" > "$D/README.md.tmp" && mv "$D/README.md.tmp" "$D/README.md"

# ------------------------------------------------- 3. reference pages
R=$D/code-reference
mkdir -p "$R/commands" "$R/monitor-calls" "$R/routines"
awk -v url="$REPO_URL" -v R="$R" '
function esc(s) { gsub(/\\/, "\\\\", s); gsub(/[*_`<>\[\]|]/, "\\\\&", s); return s }
function slug(s) { s = tolower(s); gsub(/[^a-z0-9]+/, "-", s); return s }
function rlink(k, from) { return "[" rname[k] "](" from "routines/" rpart[k] "/" rfile[k] ".md)" }
FNR == 1 { part = FILENAME; sub(/^.*\//, "", part); sub(/\.SYMB$/, "", part); nparts++; parts[nparts] = part }
{
    sub(/\r$/, "")
    line[part, FNR] = $0; nl[part] = FNR
    if (match($0, /^[A-Z0-9][A-Z0-9]*,/)) def[part, substr($0, 1, RLENGTH - 1)] = FNR
    if (match($0, /^[A-Z0-9][A-Z0-9]*=\*/)) def[part, substr($0, 1, RLENGTH - 2)] = FNR
    if ($0 ~ /^[ \t]*GOVER /) { n = split($2, gv, ","); def[part, gv[n]] = FNR; gover[part, gv[n]] = 1 }
    if ($0 ~ /^%[A-Z0-9][A-Z0-9]*([ \t]+-[ \t].*)?[ \t]*$/) {
        h = $0; sub(/^%/, "", h); il = ""
        if (h ~ /[ \t]+-[ \t]/) { il = h; sub(/^[A-Z0-9]+[ \t]+-[ \t]+/, "", il); sub(/[ \t]+-[ \t].*$/, "", h) }
        sub(/[ \t]*$/, "", h); hdr[part, ++nh[part]] = h; hln[part, nh[part]] = FNR; hil[part, nh[part]] = il
    }
}
END {
    # routines: a header whose name is defined in the same part
    for (p = 1; p <= nparts; p++) {
        P = parts[p]
        for (i = 1; i <= nh[P]; i++) {
            n = hdr[P, i]
            if (!((P, n) in def)) continue
            k = ++nr; rname[k] = n; rpart[k] = P; rstart[k] = hln[P, i]; rinl[k] = hil[P, i]
            f = n; if ((f, P) in seen) f = f "-" (++dup[f, P]); seen[f, P] = 1; rfile[k] = f
            ncand[n]++; cand[n, ncand[n]] = k
            if (!(n in byname) || ((P, n) in gover)) byname[n] = k
        }
    }
    for (k = 1; k <= nr; k++) {   # extent: up to the next routine in the same part
        e = nl[rpart[k]]
        for (j = k + 1; j <= nr; j++) if (rpart[j] == rpart[k]) { e = rstart[j] - 1; break }
        while (e > rstart[k] && line[rpart[k], e] ~ /^[ \t]*$/) e--
        rend[k] = e
        desc[k] = rinl[k]
        for (l = rstart[k] + 1; l <= e && line[rpart[k], l] ~ /^%/; l++) {
            d = line[rpart[k], l]; sub(/^%[ \t]*/, "", d)
            desc[k] = desc[k] (desc[k] == "" ? "" : "\n") d
        }
        first[k] = desc[k]; sub(/\n.*/, "", first[k])
        for (l = rstart[k]; l <= e; l++) {   # calls
            s = line[rpart[k], l]; sub(/%.*/, "", s)
            while (match(s, /(JPL I \(|JPL |JMP I \()[A-Z0-9]+/)) {
                t = substr(s, RSTART, RLENGTH); sub(/^(JPL I \(|JPL |JMP I \()/, "", t)
                s = substr(s, RSTART + RLENGTH)
                if (!(t in byname)) continue
                tk = byname[t]
                for (c = 1; c <= ncand[t]; c++) if (rpart[cand[t, c]] == rpart[k]) tk = cand[t, c]
                if (tk != k && !((k, tk) in calls)) {
                    calls[k, tk] = 1; clist[k] = clist[k] " " tk
                    cby[tk] = cby[tk] " " k
                }
            }
        }
    }
    for (n in ncand) if (ncand[n] > 1) { s = ""; for (c = 1; c <= ncand[n]; c++) s = s " " rpart[cand[n, c]]; printf "note: routine name %s is defined in more than one place:%s\n", n, s > "/dev/stderr" }
    # monitor calls: MCTBL in TSS1
    nm = 0; inm = 0
    for (l = 1; l <= nl["TSS1"]; l++) {
        s = line["TSS1", l]
        if (s ~ /^MCTBL,/) { inm = 1; sub(/^MCTBL,/, "", s) }
        else if (s ~ /^MCSIZ=/) inm = 0
        if (!inm) continue
        sub(/%.*/, "", s); n = split(s, a, ";")
        for (i = 1; i <= n; i++) { t = a[i]; gsub(/[ \t]/, "", t); if (t != "") { mc[nm] = t; mcl[nm] = l; nm++ } }
    }
    # commands: SCn names and the CMD routine list in TSS5
    nc = 0; inc = 0
    for (l = 1; l <= nl["TSS5"]; l++) {
        s = line["TSS5", l]
        if (match(s, /^SC[0-9]+,/)) { i = substr(s, 3, RLENGTH - 3) + 0; c = s; sub(/^[^\x27]*\x27/, "", c); sub(/\\.*$/, "", c); cname[i] = c }
        if (s ~ /^XCMD,/) { inx = 1; sub(/^XCMD,/, "", s) } else if (inx && s ~ /^NCMD=/) inx = 0
        if (inx) { t2 = s; sub(/%.*/, "", t2); n = split(t2, a, ";"); for (i = 1; i <= n; i++) { t = a[i]; gsub(/[ \t]/, "", t); if (t ~ /^XC[0-9]+$/) xord[++nx] = substr(t, 3) + 0 } }
        if (s ~ /^CMD,/) { inc = 1; sub(/^CMD,/, "", s) } else if (inc && s ~ /^[ \t]*$/) inc = 0
        if (!inc) continue
        sub(/%.*/, "", s); n = split(s, a, ";")
        for (i = 1; i <= n; i++) { t = a[i]; gsub(/[ \t]/, "", t); if (t != "") clst[++ncl] = t }
    }
    # command i: name from SC<n> where XC<n> is the i-th entry of XCMD,
    # routine from the i-th entry of CMD (both tables skip XC31/SC31)
    for (i = 2; i <= ncl; i++) { nc++; crout[nc] = clst[i]; cmdof[clst[i]] = nc }
    if (nx != nc) printf "WARNING: XCMD has %d entries, CMD has %d\n", nx, nc > "/dev/stderr"
    for (i = 1; i <= nc; i++) cnm[i] = cname[xord[i]]
    for (i = 1; i <= nc; i++) cname[i] = cnm[i]
    # user-manual anchors, from the real "#### NAME ..." headings
    while ((getline um < "docs/TSS-USER-MANUAL.md") > 0) {
        sub(/\r$/, "", um)
        if (um !~ /^#### /) continue
        h = um; sub(/^#### /, "", h); nm2 = h; sub(/[ \t].*$/, "", nm2)
        an = tolower(h); gsub(/[^a-z0-9 -]/, "", an); gsub(/ +/, "-", an); sub(/-$/, "", an)
        anchor[nm2] = an
    }
    for (i = 0; i < nm; i++) mcof[mc[i]] = i

    # ---- routine pages
    for (k = 1; k <= nr; k++) {
        P = rpart[k]; f = R "/routines/" P "/" rfile[k] ".md"
        system("mkdir -p \"" R "/routines/" P "\"")
        print "# " rname[k] > f
        print "" > f
        if (desc[k] != "") { m = split(desc[k], dl, "\n"); for (i = 1; i <= m; i++) print esc(dl[i]) "  " > f; print "" > f }
        print "| | |" > f; print "|---|---|" > f
        print "| source | [src/" P ".SYMB line " rstart[k] "](" url "/blob/main/src/" P ".SYMB#L" rstart[k] ") |" > f
        if (rname[k] in cmdof) print "| command | [" cname[cmdof[rname[k]]] "](../../commands/" slug(cname[cmdof[rname[k]]]) ".md) |" > f
        if (rname[k] in mcof) printf "| monitor call | [%o](../../monitor-calls/%o.md) |\n", mcof[rname[k]], mcof[rname[k]] > f
        if (clist[k] != "") { m = split(clist[k], cl, " "); s = ""; for (i = 1; i <= m; i++) s = s (i > 1 ? ", " : "") rlink(cl[i], "../../"); print "| calls | " s " |" > f }
        if (cby[k] != "") { m = split(cby[k], cl, " "); s = ""; for (i = 1; i <= m; i++) s = s (i > 1 ? ", " : "") rlink(cl[i], "../../"); print "| called by | " s " |" > f }
        print "" > f
        print "??? abstract \"Source: src/" P ".SYMB lines " rstart[k] "-" rend[k] "\"" > f
        print "    ```text" > f
        for (l = rstart[k]; l <= rend[k]; l++) { s = line[P, l]; gsub(/\t/, "    ", s); print "    " s > f }
        print "    ```" > f
        close(f)
    }
    # ---- routine indexes
    f = R "/routines/index.md"
    print "# Routines" > f; print "" > f
    print "Every routine in the TSS 3.0 source that has a `%NAME` header comment, grouped by source part. The description is the routine\x27s own header comment." > f; print "" > f
    for (p = 1; p <= nparts; p++) print "- [" parts[p] "](" parts[p] "/index.md)" > f
    close(f)
    for (p = 1; p <= nparts; p++) {
        P = parts[p]; f = R "/routines/" P "/index.md"
        system("mkdir -p \"" R "/routines/" P "\"")
        print "# " P > f; print "" > f
        print "Routines in [src/" P ".SYMB](" url "/blob/main/src/" P ".SYMB), in source order." > f; print "" > f
        print "| routine | line | description |" > f; print "|---|---|---|" > f
        for (k = 1; k <= nr; k++) if (rpart[k] == P) print "| [" rname[k] "](" rfile[k] ".md) | " rstart[k] " | " esc(first[k]) " |" > f
        close(f)
    }
    # ---- monitor-call pages
    f = R "/monitor-calls/index.md"
    print "# Monitor calls" > f; print "" > f
    print "The table `MCTBL` in [src/TSS1.SYMB](" url "/blob/main/src/TSS1.SYMB#L" mcl[0] "): call number (octal) and the routine it runs. A user program calls them with `MON n`." > f; print "" > f
    print "| call | routine | description |" > f; print "|---|---|---|" > f
    for (i = 0; i < nm; i++) {
        t = mc[i]; k = (t in byname) ? byname[t] : 0
        printf "| [%o](%o.md) | %s | %s |\n", i, i, t, (k ? esc(first[k]) : "") > f
        g = sprintf("%s/monitor-calls/%o.md", R, i)
        printf "# Monitor call %o: %s\n\n", i, t > g
        print "| | |" > g; print "|---|---|" > g
        printf "| number | %o (octal) |\n", i > g
        print "| table entry | [src/TSS1.SYMB line " mcl[i] "](" url "/blob/main/src/TSS1.SYMB#L" mcl[i] ") |" > g
        if (k) { print "| routine | " rlink(k, "../") " |" > g; print "" > g; m = split(desc[k], dl, "\n"); for (j = 1; j <= m; j++) print esc(dl[j]) "  " > g }
        else print "| routine | " t ", no documented routine page |" > g
        close(g)
    }
    close(f)
    # ---- command pages
    f = R "/commands/index.md"
    print "# Commands" > f; print "" > f
    print "The 60 commands of the command processor, in the order of the table `CMD` in [src/TSS5.SYMB](" url "/blob/main/src/TSS5.SYMB). Usage is described in the [user manual](../../docs/TSS-USER-MANUAL.md)." > f; print "" > f
    print "| command | routine | description |" > f; print "|---|---|---|" > f
    for (i = 1; i <= nc; i++) {
        t = crout[i]; k = (t in byname) ? byname[t] : 0
        print "| [" cname[i] "](" slug(cname[i]) ".md) | " t " | " (k ? esc(first[k]) : "") " |" > f
        g = R "/commands/" slug(cname[i]) ".md"
        print "# " cname[i] > g; print "" > g
        print "| | |" > g; print "|---|---|" > g
        print "| user manual | [" cname[i] "](../../docs/TSS-USER-MANUAL.md" ((cname[i] in anchor) ? "#" anchor[cname[i]] : "") ") |" > g
        if (k) { print "| routine | " rlink(k, "../") " |" > g; print "" > g; m = split(desc[k], dl, "\n"); for (j = 1; j <= m; j++) print esc(dl[j]) "  " > g }
        else print "| routine | " t ", no documented routine page |" > g
        close(g)
    }
    close(f)
    f = R "/index.md"
    print "# Reference" > f; print "" > f
    print "Generated from the TSS 3.0 source on every site build." > f; print "" > f
    print "- [Commands](commands/index.md): the " nc " commands of the command processor" > f
    print "- [Monitor calls](monitor-calls/index.md): the " nm " entries of `MCTBL`" > f
    print "- [Routines](routines/index.md): " nr " documented routines in TSS1-TSS5" > f
    close(f)
    printf "reference: %d commands, %d monitor calls, %d routines\n", nc, nm, nr > "/dev/stderr"
}' src/TSS1.SYMB src/TSS2.SYMB src/TSS3.SYMB src/TSS4.SYMB src/TSS5.SYMB

# ------------------------------------------------- 4. mkdocs.yml with the nav
{
    echo "# generated by docs-site/gen-site.sh; edit that script or docs-site/mkdocs.yml"
    echo "INHERIT: ../docs-site/mkdocs.yml"
    echo "docs_dir: docs"
    echo "nav:"
    echo "  - Home: README.md"
    section() { echo "  - $1:"; shift; for p in "$@"; do [ -f "$D/$p" ] && echo "      - $p"; done; }
    section "Getting started" dist/QUICKSTART.md docs/TSS-BRINGUP.md docs/TSS-USER-MANUAL.md
    section "History" docs/TSS-AND-SINTRAN.md docs/TSS-PROGRAMS.md docs/PROJECT-DESCRIPTION.md \
        docs/ND-60.039.01_Reference_Manual_for_the_NORD_Timesharing_System_16_February_1973.md
    section "How TSS works" docs/TSS-ARCHITECTURE.md docs/TSS-PSEUDOCODE.md docs/TSS-SOURCE-FILES.md docs/TSS-FLOAT-FORMAT.md
    section "Testing" docs/TSS-COMMAND-VALIDATION.md docs/OPEN-QUESTIONS.md
    section "The MAC assembler" docs/MAC-ASSEMBLER.md mac-c/README.md
    echo "  - Source reference:"
    echo "      - code-reference/index.md"
    echo "      - Commands:"
    echo "          - code-reference/commands/index.md"
    (cd "$D" && ls code-reference/commands/*.md | grep -v index.md) | sed 's/^/          - /'
    echo "      - Monitor calls:"
    echo "          - code-reference/monitor-calls/index.md"
    (cd "$D" && ls code-reference/monitor-calls/*.md | grep -v index.md | sort -t/ -k3 -n) | sed 's/^/          - /'
    echo "      - Routines:"
    echo "          - code-reference/routines/index.md"
    for P in TSS1 TSS2 TSS3 TSS4 TSS5; do
        echo "          - $P:"
        echo "              - code-reference/routines/$P/index.md"
        (cd "$D" && ls code-reference/routines/$P/*.md | grep -v index.md) | sed 's/^/              - /'
    done
    echo "  - Repository:"
    grep '/README\.md$' "$OUT/pages.txt" | sort | sed 's/^/      - /'
} > "$OUT/mkdocs.yml"

rm -f "$OUT/tracked.txt" "$OUT/pages.txt"
echo "site sources in $OUT/; build with: mkdocs build --strict -f $OUT/mkdocs.yml -d <output>"
