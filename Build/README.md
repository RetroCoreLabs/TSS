# `Build/` — output folder

**Everything else in here is generated and disposable.** Delete it freely;
it is rebuilt in a couple of seconds. Nothing else in the repository reads
from it. Only this README is checked in — the build scripts preserve it and
clear the rest.

```bash
cd ../mac-c
./scripts/build/build_tss_assysa.sh    # the 1973 build scripts, end to end
./scripts/build/run_tss.sh             # or: assemble the five parts directly
```

## What lands here

From `build_tss_assysa.sh`:

| file | contents |
|---|---|
| `ASYMB.SYMB` | symbol dump of the version-A build — compare against `../reference/ASYMB.SYMB` |
| `BSYMB.SYMB` | symbol dump of the version-B (`DEBUG`) build |
| `LIST1.SYMB` … `LIST5.SYMB` | the list streams, one per source part |
| `TSS1.SYMB` … `TSS5.SYMB`, `ASSYSA.SYMB`, `ASSYSB.SYMB` | working copies staged under the ND names MAC expects (`)9ASSM TSS1` opens `TSS1:SYMB`) — copies of `../src/`, not edited |
| `write_*.txt` | what `)WRITE` produced — TSS's own `OVERX` macro prints each overlay's size here |
| `err_*.txt` | diagnostics, ending with the `000000 DIAGNOSTICS` count |

From `run_tss.sh`: `asymb.list`, `write.txt`, `err.txt`.

## Checking the result

```bash
cd ../mac-c
./scripts/verify/compare_asymb.sh ../Build/ASYMB.SYMB ../reference/ASYMB.SYMB
```

A good build reports `errors : 0` and **679 of 693** exact symbol matches
(version B: 675 of 689). If that number drops, something regressed — use
`./scripts/verify/first_divergence.sh` to find the first symbol that moved.
