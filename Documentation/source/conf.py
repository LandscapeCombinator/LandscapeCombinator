from pathlib import Path

project = 'Landscape Combinator'
copyright = '2026, Landscape Combinator'
author = ''
version = '0.9.0'
release = '0.9.0'

extensions = [
    'sphinx.ext.autosectionlabel',
    'sphinx.ext.viewcode',
    'sphinx_copybutton'
]

templates_path = ['_templates']
exclude_patterns = []

Path(__file__).parent.joinpath('_static', 'version.css').write_text(
    '.navbar-brand .logo__title::after {\n'
    f'  content: "v{release}";\n'
    '  display: block;\n'
    '  font-size: 0.75rem;\n'
    '  font-weight: normal;\n'
    '  color: var(--pst-color-text-muted);\n'
    '}\n'
)

html_theme = "sphinx_book_theme"
html_title = "Landscape Combinator"
html_static_path = ['_static']
html_css_files = ['custom.css', 'version.css']
html_favicon = "_static/logo-lc.svg"

html_theme_options = {
    'repository_url': 'https://github.com/LandscapeCombinator/LandscapeCombinator',
    'use_repository_button': True,
    'show_toc_level': 1
}
