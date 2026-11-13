/* Dynamic GTK4 / Libadwaita / GTK3 palette synced from current Quattro theme */
@define-color accent_color {{ accent }};
@define-color accent_bg_color {{ accent }};
@define-color accent_fg_color {{ background }};

@define-color window_bg_color {{ background }};
@define-color window_fg_color {{ foreground }};

@define-color view_bg_color {{ background }};
@define-color view_fg_color {{ foreground }};

@define-color headerbar_bg_color {{ mix background foreground 4% }};
@define-color headerbar_fg_color {{ foreground }};
@define-color headerbar_border_color {{ mix background foreground 12% }};
@define-color headerbar_backdrop_color {{ background }};
@define-color headerbar_shade_color rgba(0, 0, 0, 0.24);

@define-color sidebar_bg_color {{ mix background foreground 3% }};
@define-color sidebar_fg_color {{ foreground }};
@define-color sidebar_backdrop_color {{ mix background foreground 2% }};
@define-color sidebar_shade_color rgba(0, 0, 0, 0.16);
@define-color sidebar_border_color {{ mix background foreground 10% }};

@define-color secondary_sidebar_bg_color {{ mix background foreground 4% }};
@define-color secondary_sidebar_fg_color {{ foreground }};
@define-color secondary_sidebar_backdrop_color {{ mix background foreground 2% }};
@define-color secondary_sidebar_shade_color rgba(0, 0, 0, 0.14);

@define-color card_bg_color {{ mix background foreground 6% }};
@define-color card_fg_color {{ foreground }};
@define-color card_shade_color rgba(0, 0, 0, 0.18);

@define-color dialog_bg_color {{ mix background foreground 4% }};
@define-color dialog_fg_color {{ foreground }};

@define-color popover_bg_color {{ mix background foreground 5% }};
@define-color popover_fg_color {{ foreground }};
@define-color popover_shade_color rgba(0, 0, 0, 0.2);

@define-color thumbnail_bg_color {{ mix background foreground 4% }};
@define-color thumbnail_fg_color {{ foreground }};

@define-color shade_color rgba(0, 0, 0, 0.24);
@define-color scrollbar_outline_color {{ mix background foreground 15% }};

@define-color destructive_color {{ red }};
@define-color destructive_bg_color {{ red }};
@define-color destructive_fg_color {{ background }};

@define-color warning_color {{ yellow }};
@define-color warning_bg_color {{ yellow }};
@define-color warning_fg_color {{ background }};

@define-color success_color {{ green }};
@define-color success_bg_color {{ green }};
@define-color success_fg_color {{ background }};

/* GTK3 fallback named colors */
@define-color theme_bg_color {{ background }};
@define-color theme_fg_color {{ foreground }};
@define-color theme_base_color {{ background }};
@define-color theme_text_color {{ foreground }};
@define-color theme_selected_bg_color {{ accent }};
@define-color theme_selected_fg_color {{ background }};
@define-color insensitive_bg_color {{ mix background foreground 3% }};
@define-color insensitive_fg_color {{ muted }};
@define-color borders {{ mix background foreground 14% }};
