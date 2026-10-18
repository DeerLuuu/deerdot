/**************************************************************************/
/*  test_editor_theme_animations.cpp                                      */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_editor_theme_animations)

#include "editor/gui/code_editor.h"
#include "editor/themes/editor_theme_manager.h"
#include "scene/gui/popup.h"

namespace TestEditorThemeAnimations {

using AnimationKind = EditorThemeManager::AnimationKind;
using AnimationParameters = EditorThemeManager::AnimationParameters;
using AnimationStyle = EditorThemeManager::AnimationStyle;

// The Subtle table is the defaults that shipped before the Expressive style existed. It must stay
// byte-for-byte identical, so the full table is pinned here. These values match the v1 probe baseline
// captured before the Expressive amplification (see .workbuddy/anim/implementation-notes-amplify.md,
// values unchanged since the expressive style was introduced).
struct SubtleBaselineEntry {
	AnimationKind kind;
	double duration;
	Tween::TransitionType transition;
	bool animate_rect;
	double exit_duration;
	Tween::TransitionType exit_transition;
};

const SubtleBaselineEntry subtle_baseline[] = {
	{ EditorThemeManager::ANIM_BUTTON_NORMAL, 0.10, Tween::TRANS_SINE, false, 0.0, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_BUTTON_HOVER, 0.12, Tween::TRANS_SINE, false, 0.0, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_BUTTON_PRESSED, 0.08, Tween::TRANS_QUAD, false, 0.0, Tween::TRANS_QUAD },
	{ EditorThemeManager::ANIM_BUTTON_DISABLED, 0.12, Tween::TRANS_SINE, false, 0.0, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_FOCUS_RING, 0.10, Tween::TRANS_SINE, false, 0.10, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_FLAT_HOVER, 0.12, Tween::TRANS_SINE, false, 0.10, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_FLAT_PRESSED, 0.08, Tween::TRANS_QUAD, false, 0.10, Tween::TRANS_QUAD },
	{ EditorThemeManager::ANIM_FLAT_HOVER_PRESSED, 0.10, Tween::TRANS_SINE, false, 0.10, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_TREE_HOVER, 0.10, Tween::TRANS_SINE, false, 0.10, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_TREE_SELECTED, 0.12, Tween::TRANS_SINE, false, 0.14, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_TREE_HOVERED_SELECTED, 0.10, Tween::TRANS_SINE, false, 0.0, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_TREE_CURSOR, 0.10, Tween::TRANS_SINE, false, 0.08, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_TREE_FOCUS, 0.12, Tween::TRANS_SINE, false, 0.14, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_ITEMLIST_HOVER, 0.10, Tween::TRANS_SINE, false, 0.10, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_ITEMLIST_HOVER_SELECTED, 0.10, Tween::TRANS_SINE, false, 0.0, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_ITEMLIST_CURSOR, 0.10, Tween::TRANS_SINE, false, 0.08, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_TAB_STATE, 0.12, Tween::TRANS_SINE, true, 0.0, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_TAB_SMALL_BUTTON, 0.08, Tween::TRANS_SINE, false, 0.08, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_MENU_HOVER, 0.10, Tween::TRANS_SINE, false, 0.08, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_GRABBER, 0.10, Tween::TRANS_SINE, false, 0.0, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_SLIDER_FILL, 0.10, Tween::TRANS_SINE, false, 0.0, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_SPINBOX_HOVER, 0.10, Tween::TRANS_SINE, false, 0.10, Tween::TRANS_SINE },
	{ EditorThemeManager::ANIM_SPINBOX_PRESSED, 0.08, Tween::TRANS_QUAD, false, 0.0, Tween::TRANS_QUAD },
};

TEST_CASE("[Editor][Theme] Subtle animation table matches the v1 baseline") {
	// Every kind must be pinned exactly once; a new kind cannot silently fall back to zero animations.
	for (int kind = 0; kind < EditorThemeManager::ANIM_KIND_MAX; kind++) {
		int matches = 0;
		for (const SubtleBaselineEntry &entry : subtle_baseline) {
			if (int(entry.kind) == kind) {
				matches++;
			}
		}
		CHECK_MESSAGE(matches == 1, "Subtle baseline must cover animation kind ", kind, " exactly once.");
	}

	for (const SubtleBaselineEntry &entry : subtle_baseline) {
		INFO("kind: ", int(entry.kind));
		const AnimationParameters parameters = EditorThemeManager::get_animation_parameters(entry.kind, EditorThemeManager::ANIMATION_STYLE_SUBTLE);
		CHECK(parameters.duration == doctest::Approx(entry.duration));
		CHECK(parameters.transition == entry.transition);
		CHECK(parameters.animate_rect == entry.animate_rect);
		CHECK(parameters.exit_duration == doctest::Approx(entry.exit_duration));
		CHECK(parameters.exit_transition == entry.exit_transition);
		CHECK(parameters.fade == false);
		CHECK(parameters.offset.is_zero_approx());
		CHECK(parameters.scale.is_equal_approx(Size2(1, 1)));
		CHECK(parameters.rotation == 0.0);
	}
}

TEST_CASE("[Editor][Theme] Animation tables keep the perceptibility budget") {
	// Budget from animation-spec-amplify.md, applied to every kind in both styles. These are regression
	// guards: growing any animation past these limits (or adding rotation/bounce) has to be a deliberate
	// change that updates this test together with the spec.
	for (int style_index = 0; style_index < 2; style_index++) {
		const AnimationStyle style = style_index == 0 ? EditorThemeManager::ANIMATION_STYLE_SUBTLE : EditorThemeManager::ANIMATION_STYLE_EXPRESSIVE;
		for (int kind = 0; kind < EditorThemeManager::ANIM_KIND_MAX; kind++) {
			INFO("style: ", style_index, ", kind: ", kind);
			const AnimationParameters parameters = EditorThemeManager::get_animation_parameters(AnimationKind(kind), style);

			CHECK(parameters.duration > 0.0);
			CHECK(parameters.duration <= 0.24);
			CHECK(parameters.exit_duration <= 0.16);

			CHECK(parameters.offset.x >= -10.0f);
			CHECK(parameters.offset.x <= 10.0f);
			CHECK(parameters.offset.y >= -10.0f);
			CHECK(parameters.offset.y <= 10.0f);

			CHECK(parameters.scale.x >= 0.82f);
			CHECK(parameters.scale.x <= 1.06f);
			CHECK(parameters.scale.y >= 0.82f);
			CHECK(parameters.scale.y <= 1.06f);

			CHECK(parameters.transition != Tween::TRANS_BOUNCE);
			CHECK(parameters.exit_transition != Tween::TRANS_BOUNCE);
			if (parameters.transition == Tween::TRANS_ELASTIC) {
				CHECK(kind == EditorThemeManager::ANIM_FLAT_HOVER);
			}
			if (parameters.exit_transition == Tween::TRANS_ELASTIC) {
				CHECK(kind == EditorThemeManager::ANIM_FLAT_HOVER);
			}

			CHECK(parameters.rotation == 0.0);

			// Animating the whole rect makes text and icons slide with the box. Only the tab backgrounds use
			// it; every other kind must keep the box anchored to its layout position.
			const bool expects_rect_animation = kind == EditorThemeManager::ANIM_TAB_STATE;
			CHECK(parameters.animate_rect == expects_rect_animation);
		}
	}
}

TEST_CASE("[Editor][Theme] Effect-specific animations keep the perceptibility budget") {
	// Popup / menu entry: Subtle fades, Expressive adds a small scale-up from the interaction point.
	CHECK(Popup::OPEN_ANIMATION_DURATION_SUBTLE <= 0.24);
	CHECK(Popup::OPEN_ANIMATION_DURATION_EXPRESSIVE <= 0.24);
	CHECK(Popup::OPEN_ANIMATION_SCALE_EXPRESSIVE >= 0.82f);
	CHECK(Popup::OPEN_ANIMATION_SCALE_EXPRESSIVE <= 1.06f);

	// Tooltips are more restrained, never overshoot, and are suppressed when repeated within the window.
	CHECK(Popup::TOOLTIP_ANIMATION_DURATION_SUBTLE <= 0.24);
	CHECK(Popup::TOOLTIP_ANIMATION_DURATION_EXPRESSIVE <= 0.24);
	CHECK(Popup::TOOLTIP_ANIMATION_SCALE_EXPRESSIVE >= 0.82f);
	CHECK(Popup::TOOLTIP_ANIMATION_SCALE_EXPRESSIVE <= 1.06f);
	CHECK(Popup::TOOLTIP_REPLAY_WINDOW_MSEC >= 300);

	// Script reveal: one line stays inside the normal animation budget, while the whole non-blocking
	// reveal (stagger capped at INTRO_MAX_LINES) stays inside the R1 carve-out for content reveals.
	CHECK(CodeTextEditor::INTRO_LINE_DURATION <= 0.24);
	CHECK(CodeTextEditor::INTRO_LINE_STAGGER > 0.0);
	CHECK(CodeTextEditor::INTRO_MAX_LINES > 0);
	CHECK(CodeTextEditor::INTRO_LINE_DURATION + CodeTextEditor::INTRO_MAX_LINES * CodeTextEditor::INTRO_LINE_STAGGER <= CodeTextEditor::INTRO_MAX_TOTAL);
	CHECK(CodeTextEditor::INTRO_MAX_TOTAL <= 0.45);
}

TEST_CASE("[Editor][Theme] Animation presets resolve to the shipped tables") {
	// Golden parity: picking a preset (or being a fresh/legacy user) must reproduce the shipped tables
	// byte for byte. This is the regression clause for the parameterized pipeline.
	for (int style_index = 0; style_index < 2; style_index++) {
		const EditorThemeManager::AnimationStyle style = style_index == 0 ? EditorThemeManager::ANIMATION_STYLE_SUBTLE : EditorThemeManager::ANIMATION_STYLE_EXPRESSIVE;
		INFO("style: ", style_index);
		for (int kind = 0; kind < EditorThemeManager::ANIM_KIND_MAX; kind++) {
			INFO("kind: ", kind);
			const EditorThemeManager::AnimationParameters base = EditorThemeManager::get_animation_parameters(EditorThemeManager::AnimationKind(kind), style);
			const EditorThemeManager::AnimationParameters resolved = EditorThemeManager::resolve_animation_parameters(EditorThemeManager::AnimationKind(kind), style, EditorThemeManager::PRESET_PARAMS[style_index]);
			CHECK(resolved.duration == doctest::Approx(base.duration));
			CHECK(resolved.transition == base.transition);
			CHECK(resolved.animate_rect == base.animate_rect);
			CHECK(resolved.exit_duration == doctest::Approx(base.exit_duration));
			CHECK(resolved.exit_transition == base.exit_transition);
			CHECK(resolved.fade == base.fade);
			CHECK(resolved.offset.is_equal_approx(base.offset));
			CHECK(resolved.scale.is_equal_approx(base.scale));
		}
	}
}

TEST_CASE("[Editor][Theme] Animation parameters stay inside the budget at the range corners") {
	EditorThemeManager::AnimationParams minimum;
	minimum.speed_scale = 0.5;
	minimum.motion_amount = 0.0;
	minimum.bloom_px = 0;
	minimum.press_squash_px = 0;
	minimum.row_slide_px = 0;
	minimum.menu_slide_px = 0;
	minimum.tab_lift_px = 0;

	EditorThemeManager::AnimationParams maximum;
	maximum.speed_scale = 2.0;
	maximum.motion_amount = 1.0;
	maximum.bloom_px = 4;
	maximum.press_squash_px = 6;
	maximum.row_slide_px = 12;
	maximum.menu_slide_px = 12;
	maximum.tab_lift_px = 8;

	const EditorThemeManager::AnimationParams corners[2] = { minimum, maximum };
	for (int corner = 0; corner < 2; corner++) {
		for (int kind = 0; kind < EditorThemeManager::ANIM_KIND_MAX; kind++) {
			INFO("corner: ", corner, ", kind: ", kind);
			const EditorThemeManager::AnimationParameters resolved = EditorThemeManager::resolve_animation_parameters(EditorThemeManager::AnimationKind(kind), EditorThemeManager::ANIMATION_STYLE_EXPRESSIVE, corners[corner]);
			if (resolved.duration > 0.0) {
				CHECK(resolved.duration >= 0.06);
				CHECK(resolved.duration <= 0.26);
			}
			CHECK(resolved.exit_duration <= 0.16);
			CHECK(resolved.offset.x >= -10.0f);
			CHECK(resolved.offset.x <= 10.0f);
			CHECK(resolved.offset.y >= -10.0f);
			CHECK(resolved.offset.y <= 10.0f);
			CHECK(resolved.scale.x >= 0.80f);
			CHECK(resolved.scale.x <= 1.00f);
			CHECK(resolved.scale.y >= 0.80f);
			CHECK(resolved.scale.y <= 1.00f);
			CHECK(resolved.transition != Tween::TRANS_BOUNCE);
			CHECK(resolved.rotation == 0.0);
			const bool expects_rect = kind == EditorThemeManager::ANIM_TAB_STATE;
			CHECK(resolved.animate_rect == expects_rect);
		}
	}
}

TEST_CASE("[Editor][Theme] Derived style reports Custom when parameters diverge") {
	const EditorThemeManager::AnimationParams subtle = EditorThemeManager::PRESET_PARAMS[0];
	const EditorThemeManager::AnimationParams expressive = EditorThemeManager::PRESET_PARAMS[1];

	CHECK(EditorThemeManager::get_derived_animation_style(subtle, "Subtle") == EditorThemeManager::ANIMATION_STYLE_SUBTLE);
	CHECK(EditorThemeManager::get_derived_animation_style(expressive, "Expressive") == EditorThemeManager::ANIMATION_STYLE_EXPRESSIVE);
	CHECK(EditorThemeManager::get_derived_animation_style(subtle, "Expressive") == EditorThemeManager::ANIMATION_STYLE_CUSTOM);
	CHECK(EditorThemeManager::get_derived_animation_style(expressive, "Custom") == EditorThemeManager::ANIMATION_STYLE_CUSTOM);

	EditorThemeManager::AnimationParams near = subtle;
	near.speed_scale = 1.000001; // float tolerance must not report a false Custom
	CHECK(EditorThemeManager::get_derived_animation_style(near, "Subtle") == EditorThemeManager::ANIMATION_STYLE_SUBTLE);

	EditorThemeManager::AnimationParams moved = subtle;
	moved.motion_amount = 0.05;
	CHECK(EditorThemeManager::get_derived_animation_style(moved, "Subtle") == EditorThemeManager::ANIMATION_STYLE_CUSTOM);

	EditorThemeManager::AnimationParams pixel = subtle;
	pixel.tab_lift_px = 1;
	CHECK(EditorThemeManager::get_derived_animation_style(pixel, "Subtle") == EditorThemeManager::ANIMATION_STYLE_CUSTOM);
}

TEST_CASE("[Editor][Theme] Animation parameter reads prefer file values after a restart") {
	const EditorThemeManager::AnimationParams subtle = EditorThemeManager::PRESET_PARAMS[0];
	const EditorThemeManager::AnimationParams expressive = EditorThemeManager::PRESET_PARAMS[1];

	// Old Expressive settings file with missing keys: the value equals the Subtle registration default and
	// the session changed list is empty (it is cleared by save()/restart), so the Expressive preset wins.
	CHECK_FALSE(EditorThemeManager::should_read_animation_param_from_file("Expressive", false, subtle.speed_scale, subtle.speed_scale));
	CHECK_FALSE(EditorThemeManager::should_read_animation_param_from_file("Expressive", false, subtle.popup_open_ms, subtle.popup_open_ms));

	// After a restart of a materialized Custom file the changed list is empty, but the file value differs
	// from the registration default, so it must be read back instead of falling back to the preset.
	CHECK(EditorThemeManager::should_read_animation_param_from_file("Custom", false, 1.4, subtle.speed_scale));
	CHECK(EditorThemeManager::should_read_animation_param_from_file("Expressive", false, 1.4, subtle.speed_scale));
	CHECK(EditorThemeManager::should_read_animation_param_from_file("Subtle", false, 0.85, subtle.popup_open_scale));

	// A key changed in this session is always authoritative, even when it equals the registration default.
	CHECK(EditorThemeManager::should_read_animation_param_from_file("Expressive", true, subtle.speed_scale, subtle.speed_scale));

	// stored=Custom + file values -> derived Custom, Expressive base table and the file parameters.
	EditorThemeManager::AnimationParams file = expressive;
	file.speed_scale = 1.4;
	CHECK(EditorThemeManager::get_derived_animation_style(file, "Custom") == EditorThemeManager::ANIMATION_STYLE_CUSTOM);
	const AnimationParameters custom = EditorThemeManager::resolve_animation_parameters(EditorThemeManager::ANIM_BUTTON_NORMAL, EditorThemeManager::ANIMATION_STYLE_CUSTOM, file);
	const AnimationParameters expressive_base = EditorThemeManager::resolve_animation_parameters(EditorThemeManager::ANIM_BUTTON_NORMAL, EditorThemeManager::ANIMATION_STYLE_EXPRESSIVE, file);
	const AnimationParameters table = EditorThemeManager::get_animation_parameters(EditorThemeManager::ANIM_BUTTON_NORMAL, EditorThemeManager::ANIMATION_STYLE_EXPRESSIVE);
	CHECK(custom.duration == doctest::Approx(expressive_base.duration));
	CHECK(custom.duration == doctest::Approx(CLAMP(table.duration * 1.4, 0.06, 0.26)));
	CHECK(custom.scale == expressive_base.scale);
}

} // namespace TestEditorThemeAnimations
