# OmaZettel colours for the current Omarchy theme.
#
# Omarchy renders this template from the theme's colors.toml, like it does
# for Neovim, VS Code and the rest. To change how OmaZettel is coloured in
# every theme, copy it to ~/.config/omarchy/themed/omazettel.toml.tpl and
# edit it (OmaZettel picks up the change at once). A theme can also ship its
# own finished omazettel.toml, which wins.
#
# Values are colours: {{ name }} is a theme colour (background, foreground,
# accent, red, blue, lighter_background, selection_background, color0..15,
# ...), {{ mix a b 30% }} mixes two of them, or write "#rrggbb" directly.
# Leave a key out to use OmaZettel's default for it.

[editor]
background = "{{ background }}"
foreground = "{{ foreground }}"
cursor = "{{ cursor }}"
selection = "{{ selection_background }}"
selection_foreground = "{{ selection_foreground }}"
heading = "{{ bright_foreground }}"
bold = "{{ bright_foreground }}"
italic = "{{ foreground }}"
link = "{{ blue }}"
wikilink = "{{ accent }}"
tag = "{{ magenta }}"
citation = "{{ yellow }}"
code = "{{ cyan }}"
code_background = "{{ lighter_background }}"
quote = "{{ light_foreground }}"
marker = "{{ dark_foreground }}"
search = "{{ mix background yellow 35% }}"
search_current = "{{ mix background orange 55% }}"

[ui]
accent = "{{ accent }}"
panel = "{{ dark_background }}"
raised = "{{ lighter_background }}"
field = "{{ mix background foreground 8% }}"
border = "{{ mix background foreground 18% }}"
muted = "{{ light_foreground }}"
