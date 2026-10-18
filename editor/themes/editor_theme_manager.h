/**************************************************************************/
/*  editor_theme_manager.h                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "editor/themes/editor_theme.h"
#include "scene/animation/tween.h"

class StyleBoxFlat;
class StyleBoxLine;
class StyleBoxTexture;

class EditorThemeManager {
	static int benchmark_run;
	static inline bool outdated_cache = false;
	static inline bool outdated_cache_dirty = true;

	static String get_benchmark_key();

	enum ColorMode {
		AUTO_COLOR,
		DARK_COLOR,
		LIGHT_COLOR,
	};

public:
	struct ThemeConfiguration {
		// Basic properties.

		String style;
		String preset;
		String spacing_preset;

		Color base_color;
		Color accent_color;
		float contrast = 1.0;
		float icon_saturation = 1.0;

		// Extra properties.

		int base_spacing = 4;
		int extra_spacing = 0;
		Size2 dialogs_buttons_min_size = Size2(105, 34);
		int border_width = 0;
		int corner_radius = 3;

		bool draw_extra_borders = false;
		int draw_relationship_lines;
		float relationship_line_opacity = 1.0;
		int thumb_size = 16;
		int class_icon_size = 16;
		bool enable_touch_optimizations = false;
		float gizmo_handle_scale = 1.0;
		int inspector_property_height = 28;
		float subresource_hue_tint = 0.0;
		float dragging_hover_wait_msec = 0;
		int max_sticky_tree_items = 5;

		// Make sure to keep those in sync with the definitions in the editor settings.
		const float default_icon_saturation = 2.0;
		const int default_relationship_lines = RELATIONSHIP_SELECTED_ONLY;
		const float default_contrast = 0.3;
		const int default_corner_radius = 4;

		// Generated properties.

		bool dark_theme = false;
		bool dark_icon_and_font = false;

		int base_margin = 4;
		int increased_margin = 4;
		int separation_margin = 4;
		int popup_margin = 12;
		int window_border_margin = 8;
		int top_bar_separation = 8;
		int forced_even_separation = 0;

		Color mono_color;
		Color mono_color_font;
		Color mono_color_inv;
		Color dark_color_1;
		Color dark_color_2;
		Color dark_color_3;
		Color contrast_color_1;
		Color contrast_color_2;
		Color highlight_color;
		Color highlight_disabled_color;
		Color info_color;
		Color success_color;
		Color warning_color;
		Color error_color;
		Color extra_border_color_1;
		Color extra_border_color_2;

		Color font_color;
		Color font_secondary_color;
		Color font_focus_color;
		Color font_hover_color;
		Color font_pressed_color;
		Color font_hover_pressed_color;
		Color font_disabled_color;
		Color font_readonly_color;
		Color font_placeholder_color;
		Color font_outline_color;

		Color font_dark_background_color;
		Color font_dark_background_focus_color;
		Color font_dark_background_hover_color;
		Color font_dark_background_pressed_color;
		Color font_dark_background_hover_pressed_color;

		Color icon_normal_color;
		Color icon_secondary_color;
		Color icon_focus_color;
		Color icon_hover_color;
		Color icon_pressed_color;
		Color icon_disabled_color;

		Color surface_popup_color;
		Color surface_lowest_color;
		Color surface_lower_color;
		Color surface_low_color;
		Color surface_base_color;
		Color surface_high_color;
		Color surface_higher_color;
		Color surface_highest_color;

		Color button_normal_color;
		Color button_hover_color;
		Color button_pressed_color;
		Color button_disabled_color;
		Color button_border_normal_color;
		Color button_border_hover_color;
		Color button_border_pressed_color;

		Color flat_button_pressed_color;
		Color flat_button_hover_pressed_color;
		Color flat_button_hover_color;

		Color shadow_color;
		Color selection_color;
		Color disabled_border_color;
		Color disabled_bg_color;
		Color separator_color;

		Ref<StyleBoxFlat> base_style;
		Ref<StyleBoxFlat> focus_style;
		Ref<StyleBoxEmpty> base_empty_style;
		Ref<StyleBoxEmpty> base_empty_wide_style;

		Ref<StyleBoxFlat> button_style;
		Ref<StyleBoxFlat> button_style_disabled;
		Ref<StyleBoxFlat> button_style_focus;
		Ref<StyleBoxFlat> button_style_pressed;
		Ref<StyleBoxFlat> button_style_hover;

		Ref<StyleBoxFlat> flat_button;
		Ref<StyleBoxFlat> flat_button_pressed;
		Ref<StyleBoxFlat> flat_button_hover_pressed;
		Ref<StyleBoxFlat> flat_button_hover;

		Ref<StyleBoxFlat> popup_style;
		Ref<StyleBoxFlat> popup_border_style;
		Ref<StyleBoxFlat> popup_panel_style;
		Ref<StyleBoxFlat> window_style;
		Ref<StyleBoxFlat> window_complex_style;
		Ref<StyleBoxFlat> dialog_style;
		Ref<StyleBoxFlat> panel_container_style;
		Ref<StyleBoxFlat> content_panel_style;
		Ref<StyleBoxFlat> tree_panel_style;
		Ref<StyleBoxFlat> tab_container_style;
		Ref<StyleBoxFlat> foreground_panel;

		Vector2 widget_margin;

		uint32_t hash();
		uint32_t hash_fonts();
		uint32_t hash_icons();
	};

	enum RelationshipLinesMode {
		RELATIONSHIP_NONE,
		RELATIONSHIP_SELECTED_ONLY,
		RELATIONSHIP_ALL,
	};

private:
	static Ref<EditorTheme> _create_base_theme(const Ref<EditorTheme> &p_old_theme = nullptr);
	static ThemeConfiguration _create_theme_config();

	static void _populate_text_editor_styles(const Ref<EditorTheme> &p_theme, ThemeConfiguration &p_config);
	static void _populate_visual_shader_styles(const Ref<EditorTheme> &p_theme, ThemeConfiguration &p_config);

	static void _reset_dirty_flag();

public:
	// Style of the editor UI animations, controlled by `interface/theme/ui_animations_style`.
	enum AnimationStyle {
		ANIMATION_STYLE_SUBTLE,
		ANIMATION_STYLE_EXPRESSIVE,
		// Derived, never stored by the manager: the stored preset no longer matches the parameter values.
		ANIMATION_STYLE_CUSTOM,
	};

	// The user-tunable animation knobs (interface/theme/ui_animations/*). The preset tables above stay the
	// source of the per-kind curves; these parameters scale or override them.
	struct AnimationParams {
		double speed_scale = 1.0;
		double motion_amount = 0.0;
		int bloom_px = 0;
		int press_squash_px = 0;
		int row_slide_px = 0;
		int menu_slide_px = 0;
		int tab_lift_px = 0;
		double popup_open_scale = 1.0;
		int popup_open_ms = 100;
		int reveal_delay_ms = 0;
	};

	// Indexed by ANIMATION_STYLE_SUBTLE / ANIMATION_STYLE_EXPRESSIVE.
	static const AnimationParams PRESET_PARAMS[2];
	// Values the user currently has, with the legacy fallback: a missing parameter reads the preset of the
	// *stored* style, so an old Expressive settings file does not silently get Subtle values.
	static AnimationParams get_user_animation_params();
	// True when the value stored in the settings file must win over the stored-style preset: the style is
	// Custom, the key changed this session, or the file value differs from the registered default. The file
	// value is what survives a restart, so this must not depend on the session-only changed list alone.
	static bool should_read_animation_param_from_file(const String &p_stored_style, bool p_changed, double p_value, double p_registered_default);
	// Expressive semantics for geometry resets and bloom: Custom uses the Expressive base table too.
	static bool uses_expressive_base_table();
	// Pure derived style: Custom when the stored preset no longer matches the parameters. Never writes.
	static AnimationStyle get_derived_animation_style(const AnimationParams &p_params, const String &p_stored_style);
	static bool animation_params_match(const AnimationParams &p_a, const AnimationParams &p_b);

	// Convenience readers for the geometry/effect consumers. All are clamped to the documented ranges.
	static int get_button_bloom_px();
	static int get_button_press_squash_px();
	static int get_tab_lift_px();
	static double get_popup_open_scale();
	static int get_popup_open_ms();
	static int get_script_reveal_delay_ms();

	// Semantic animation targets. Every animated StyleBox picks a kind, and the manager resolves the
	// actual animation values from the table of the active style (see editor_theme_manager.cpp).
	enum AnimationKind {
		ANIM_BUTTON_NORMAL,
		ANIM_BUTTON_HOVER,
		ANIM_BUTTON_PRESSED,
		ANIM_BUTTON_DISABLED,
		ANIM_FOCUS_RING,
		ANIM_FLAT_HOVER,
		ANIM_FLAT_PRESSED,
		ANIM_FLAT_HOVER_PRESSED,
		ANIM_TREE_HOVER,
		ANIM_TREE_SELECTED,
		ANIM_TREE_HOVERED_SELECTED,
		ANIM_TREE_CURSOR,
		ANIM_TREE_FOCUS,
		ANIM_ITEMLIST_HOVER,
		ANIM_ITEMLIST_HOVER_SELECTED,
		ANIM_ITEMLIST_CURSOR,
		ANIM_TAB_STATE,
		ANIM_TAB_SMALL_BUTTON,
		ANIM_MENU_HOVER,
		ANIM_GRABBER,
		ANIM_SLIDER_FILL,
		ANIM_SPINBOX_HOVER,
		ANIM_SPINBOX_PRESSED,
		ANIM_KIND_MAX,
	};

	static Ref<StyleBoxFlat> make_flat_stylebox(Color p_color, float p_margin_left = -1, float p_margin_top = -1, float p_margin_right = -1, float p_margin_bottom = -1, int p_corner_width = 0);
	static Ref<StyleBoxEmpty> make_empty_stylebox(float p_margin_left = -1, float p_margin_top = -1, float p_margin_right = -1, float p_margin_bottom = -1);
	static Ref<StyleBoxTexture> make_stylebox(Ref<Texture2D> p_texture, float p_left, float p_top, float p_right, float p_bottom, float p_margin_left = -1, float p_margin_top = -1, float p_margin_right = -1, float p_margin_bottom = -1, bool p_draw_center = true);
	static Ref<StyleBoxLine> make_line_stylebox(Color p_color, int p_thickness = 1, float p_grow_begin = 1, float p_grow_end = 1, bool p_vertical = false);

	// Read-only copy of one animation table entry. The tables themselves stay private; this accessor exists
	// so tests can keep the perceptibility budget and the Subtle baseline under regression coverage.
	struct AnimationParameters {
		double duration = 0.0;
		Tween::TransitionType transition = Tween::TRANS_SINE;
		bool animate_rect = false;
		double exit_duration = 0.0;
		Tween::TransitionType exit_transition = Tween::TRANS_SINE;
		bool fade = false;
		Point2 offset;
		Size2 scale = Size2(1, 1);
		// The table has no rotation data: animations created from it never rotate. Exposed so the budget
		// test can pin this down explicitly instead of relying on the struct layout.
		double rotation = 0.0;
	};
	static AnimationParameters get_animation_parameters(AnimationKind p_kind, AnimationStyle p_style);

	// Pure resolution of one kind for the given stored style/parameters. Exposed for the golden tests.
	static AnimationParameters resolve_animation_parameters(AnimationKind p_kind, AnimationStyle p_stored_style, const AnimationParams &p_params);
	// Resolved parameters for the active settings (uses get_user_animation_params()).
	static AnimationParameters get_resolved_animation_parameters(AnimationKind p_kind);

	// Sets up animation parameters for an editor StyleBox based on the active animation style. Does nothing if
	// the `interface/theme/ui_animations` setting is disabled, so the editor stays fully static in that case.
	// The exit animation also fades the box out, for styleboxes that are only drawn conditionally.
	static void animate_stylebox(const Ref<StyleBox> &p_style, AnimationKind p_kind);
	// Returns the animation style. This is not the same as whether animations are enabled: use
	// is_expressive_animation_style() for geometry that should only apply to the Expressive style.
	static AnimationStyle get_animation_style();
	// True only when animations are enabled and the Expressive style is selected.
	static bool is_expressive_animation_style();

	static Ref<EditorTheme> generate_theme(const Ref<EditorTheme> &p_old_theme = nullptr);
	static bool is_generated_theme_outdated();
	static void set_theme_outdated();

	static bool is_dark_theme();
	static bool is_dark_icon_and_font();

	static void initialize();
	static void finalize();
};
