/*
 * Copyright (c) 2025
 *
 * This file is part of The Synthstrom Audible Deluge Firmware.
 *
 * The Synthstrom Audible Deluge Firmware is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include "gui/context_menu/scale_selection.h"
#include "gui/l10n/l10n.h"
#include "gui/ui/root_ui.h"
#include "gui/ui/ui.h"
#include "gui/views/instrument_clip_view.h"
#include "hid/button.h"
#include "hid/buttons.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "model/song/song.h"
#include "util/cfunctions.h"
#include <cstring>

namespace deluge::gui::context_menu {

ScaleSelection scaleSelection{};

char const* ScaleSelection::getTitle() {
	static char const* title = "Select Scale";
	return title;
}

std::span<char const*> ScaleSelection::getOptions() {
	return {availableScaleNames, numAvailableScales};
}

bool ScaleSelection::setupAndCheckAvailability() {
	currentScale = currentSong->getCurrentScale();

	// Build list of available scales (excluding those that result in "NONE")
	buildAvailableScales();

	if (numAvailableScales == 0) {
		return false; // No scales available
	}

	// Find current scale in the list and set as selected option
	updateSelectedOption();

	if (display->haveOLED()) {
		scrollPos = currentOption - 1;
		// Handle wraparound for initial scroll position
		if (scrollPos < 0) {
			scrollPos = static_cast<int32_t>(numAvailableScales) - 1;
		}
	}

	return true;
}

void ScaleSelection::buildAvailableScales() {
	numAvailableScales = 0;

	// Helper lambda to get the number of notes in a scale
	auto getScaleNoteCount = [&](Scale scale) -> int32_t {
		if (scale == USER_SCALE) {
			// For USER_SCALE, we need to get the count from the current key's mode notes
			// if it's a user scale, or return 0 if no user scale is defined
			if (currentSong->hasUserScale()) {
				return currentSong->key.modeNotes.count();
			}
			return 0;
		}
		if (scale >= NUM_PRESET_SCALES) {
			return 0;
		}
		return presetScaleNotes[scale].count();
	};

	// Helper lambda to check if a scale is valid (not "NONE") and can be applied
	auto isValidScale = [&](Scale scale) -> bool {
		if (scale == USER_SCALE) {
			if (!currentSong->hasUserScale()) {
				return false; // USER_SCALE is only valid if user has defined one
			}
		}
		else if (scale >= NUM_PRESET_SCALES) {
			return false; // Out of bounds
		}
		else if (currentSong->disabledPresetScales[scale]) {
			return false; // Scale is disabled
		}
		else {
			// Check if the scale name is "NONE" - this is simpler and safer
			const char* scale_name = getScaleName(scale);
			if (!scale_name || strcmp(scale_name, "NONE") == 0) {
				return false;
			}
		}

		// Check if we can actually change to this scale based on notes currently in use
		NoteSet notes_within_octave_present = currentSong->notesInScaleModeClips();
		notes_within_octave_present.add(0); // Always count the root note as present

		NoteSet target_scale;
		if (scale == USER_SCALE) {
			target_scale = currentSong->key.modeNotes;
		}
		else {
			target_scale = presetScaleNotes[scale];
		}
		target_scale.add(0); // Ensure root

		// If the target scale cannot fit the notes from the current clips, we can't change to it
		return notes_within_octave_present.scaleSize() <= target_scale.scaleSize();
	};

	// Add USER_SCALE first if valid
	if (isValidScale(USER_SCALE)) {
		availableScales[numAvailableScales] = USER_SCALE;
		availableScaleNames[numAvailableScales] = getScaleName(USER_SCALE);
		availableScaleNoteCounts[numAvailableScales] = getScaleNoteCount(USER_SCALE);
		numAvailableScales++;
	}

	// Add all valid preset scales
	for (int32_t i = 0; i < NUM_PRESET_SCALES; i++) {
		Scale scale = static_cast<Scale>(i);
		if (isValidScale(scale)) {
			availableScales[numAvailableScales] = scale;
			availableScaleNames[numAvailableScales] = getScaleName(scale);
			availableScaleNoteCounts[numAvailableScales] = getScaleNoteCount(scale);
			numAvailableScales++;
		}
	}
}

void ScaleSelection::updateSelectedOption() {
	currentOption = 0; // Default to first option

	// Find the current scale in our available scales list
	for (size_t i = 0; i < numAvailableScales; i++) {
		if (availableScales[i] == currentScale) {
			currentOption = static_cast<int32_t>(i);
			break;
		}
	}
}

Scale ScaleSelection::getScaleFromOption(int32_t option) {
	if (option >= 0 && option < static_cast<int32_t>(numAvailableScales)) {
		return availableScales[option];
	}
	return NO_SCALE;
}

int32_t ScaleSelection::getOptionFromScale(Scale scale) {
	for (size_t i = 0; i < numAvailableScales; i++) {
		if (availableScales[i] == scale) {
			return static_cast<int32_t>(i);
		}
	}
	return 0; // Default to first option
}

bool ScaleSelection::isCurrentOptionAvailable() {
	return (currentOption >= 0 && currentOption < static_cast<int32_t>(numAvailableScales));
}

bool ScaleSelection::acceptCurrentOption() {
	Scale selectedScale = getScaleFromOption(currentOption);
	if (selectedScale != NO_SCALE) {
		// Apply the selected scale
		Scale result = currentSong->setScale(selectedScale);
		if (result != NO_SCALE) {
			// Update the instrument clip view
			instrumentClipView.recalculateColours();
			uiNeedsRendering(&instrumentClipView);
			// Display the scale name as confirmation
			display->displayPopup(getScaleName(result));
			return false; // Return false to close the menu
		}
		else {
			// Scale change failed
			display->displayPopup(deluge::l10n::get(deluge::l10n::String::STRING_FOR_CANT_CHANGE_SCALE));
			return true; // Return true to keep menu open
		}
	}
	return true; // Keep menu open if no scale selected
}

void ScaleSelection::selectEncoderAction(int8_t offset) {
	int32_t oldOption = currentOption;

	// Implement wraparound scrolling
	currentOption += offset;

	// Wrap around to end if going below 0
	if (currentOption < 0) {
		currentOption = static_cast<int32_t>(numAvailableScales) - 1;
	}
	// Wrap around to start if going beyond last option
	else if (currentOption >= static_cast<int32_t>(numAvailableScales)) {
		currentOption = 0;
	}

	// Update scroll position for OLED display - keep selection at position 1 (second line)
	if (display->haveOLED()) {
		scrollPos = currentOption - 1;
		// Handle wraparound for scroll position
		if (scrollPos < 0) {
			scrollPos = static_cast<int32_t>(numAvailableScales) - 1;
		}
	}

	// Trigger display update
	if (currentOption != oldOption) {
		renderUIsForOled();
	}
}

ActionResult ScaleSelection::buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) {
	using namespace deluge::hid::button;

	// Allow scale button to exit the context menu
	if (b == BACK && on) {
		display->setNextTransitionDirection(-1);
		close();
		if (instrumentClipView.entered_context_menu_from_outside_scale_mode) {
			instrumentClipView.commandExitScaleMode();
		}
		return ActionResult::DEALT_WITH;
	}

	// Handle other buttons with the base class
	return ContextMenu::buttonAction(b, on, inCardRoutine);
}

void ScaleSelection::renderOLED(deluge::hid::display::oled_canvas::Canvas& canvas) {
	const auto options = getOptions();

	// int32_t window_width = OLED_MAIN_WIDTH_PIXELS;
	int32_t window_height = 40;

	int32_t window_min_x = 0;                          //(OLED_MAIN_WIDTH_PIXELS - window_width) >> 1;
	int32_t window_max_x = OLED_MAIN_WIDTH_PIXELS - 1; // - window_min_x;

	int32_t window_min_y = (OLED_MAIN_HEIGHT_PIXELS - window_height) >> 1;
	int32_t window_max_y = OLED_MAIN_HEIGHT_PIXELS - window_min_y;

	canvas.clearAreaExact(window_min_x + 1, window_min_y + 1, window_max_x - 1, window_max_y - 1);

	canvas.drawRectangle(window_min_x, window_min_y, window_max_x, window_max_y);

	int32_t text_pixel_y = window_min_y + 2;
	int32_t actual_current_option = currentOption;

	currentOption = scrollPos;
	int32_t i = 0;
	int32_t text_pixel_x = window_min_x + 4;
	int32_t max_text_width = window_max_x - window_min_x - 3;
	int32_t divider_x = text_pixel_x + kTextSpacingX + 2;

	canvas.drawVerticalLine(divider_x, window_min_y + 1, window_max_y - 1);

	int32_t items_drawn = 0;
	int32_t max_items_to_draw = 4; // Maximum items that fit on screen

	while (items_drawn < max_items_to_draw && items_drawn < static_cast<int32_t>(numAvailableScales)) {
		// Wrap around when we reach the end
		if (currentOption >= static_cast<int32_t>(options.size())) {
			currentOption = 0;
		}
		if (!isCurrentOptionAvailable()) {
			currentOption++;
			continue;
		}

		const int32_t note_count = availableScaleNoteCounts[currentOption];
		char note_count_str[3];
		intToString(note_count, note_count_str);
		if (note_count > 9) {
			canvas.drawString(note_count_str, text_pixel_x - 1, text_pixel_y + 2, 3,
			                  5); // use small text for double digits.
		}
		else {
			canvas.drawString(note_count_str, text_pixel_x, text_pixel_y, kTextSpacingX, kTextSpacingY);
		}
		// ------

		// Draw scale name
		const int32_t scale_name_x = divider_x + 4;
		canvas.drawString(options[currentOption], scale_name_x, text_pixel_y, kTextSpacingX, kTextSpacingY, 0,
		                  window_max_x);

		if (currentOption == actual_current_option) {
			canvas.invertLeftEdgeForMenuHighlighting(window_min_x + 2, max_text_width, text_pixel_y, text_pixel_y + 8);
		}

		text_pixel_y += kTextSpacingY;
		currentOption++;
		items_drawn++;
	}

	currentOption = actual_current_option;
}

} // namespace deluge::gui::context_menu