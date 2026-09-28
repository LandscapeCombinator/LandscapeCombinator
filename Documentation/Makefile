# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line, and also
# from the environment for the first two.
SPHINXOPTS    ?=
SPHINXBUILD   ?= sphinx-build
SOURCEDIR     = source
BUILDDIR      = build
SCRIPTS       = ../Scripts

# Put it first so that "make" without argument is like "make help".
help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

# Generate documentation files from sources (runs before any build)
_headers: $(SCRIPTS)/generate_doc_from_headers.py $(SCRIPTS)/generate_credit_from_readme.py
	@echo "Generating documentation from sources..."
	@python $(SCRIPTS)/generate_doc_from_headers.py
	@python $(SCRIPTS)/generate_credit_from_readme.py

.PHONY: help _headers

# Catch-all target: route all unknown targets to Sphinx using the new
# "make mode" option.  $(O) is meant as a shortcut for $(SPHINXOPTS).
%: _headers Makefile
	@$(SPHINXBUILD) -M $@ "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)
