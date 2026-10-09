# Contributing

## The documentation site

The site at <https://retrocorelabs.github.io/TSS/> is built from this
repository on every push to `main` by `.github/workflows/docs.yml`.

- `docs-site/mkdocs.yml` is the site configuration (theme, extensions).
- `docs-site/gen-site.sh` generates everything else into `build-site/`
  (not tracked): it copies every tracked Markdown and PDF file, rewrites
  links to source files into GitHub links, writes one page per command,
  monitor call and documented routine from the TSS source, and writes the
  page tree. Edit a routine's description in its `%` header comment in
  `src/TSSn.SYMB`, not in a generated page.
- MkDocs is needed only for the site; the repository itself stays
  python-free. `docs-site/requirements.txt` pins the versions.

To build and check the site locally, from the repository root:

```sh
python3 -m venv ~/venvs/tss-site
~/venvs/tss-site/bin/pip install -r docs-site/requirements.txt
./docs-site/gen-site.sh
~/venvs/tss-site/bin/mkdocs build --strict -f build-site/mkdocs.yml -d ~/tss-site-out
# or preview: ~/venvs/tss-site/bin/mkdocs serve -f build-site/mkdocs.yml
```

The build must finish with no warnings: `--strict` turns a broken link
into an error, locally and in CI.
