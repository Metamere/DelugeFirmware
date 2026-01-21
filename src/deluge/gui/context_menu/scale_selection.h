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

#pragma once

#include "gui/context_menu/context_menu.h"
#include "model/scale/preset_scales.h"
#include <cstddef>
#include <span>

namespace deluge::gui::context_menu {

class ScaleSelection : public ContextMenu {
public:
	ScaleSelection() = default;

	char const* getTitle() override;
	std::span<char const*> getOptions() override;
	bool setupAndCheckAvailability() override;
	bool acceptCurrentOption() override;
	bool isCurrentOptionAvailable() override;
	void selectEncoderAction(int8_t offset) override;
	ActionResult buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) override;
	void renderOLED(deluge::hid::display::oled_canvas::Canvas& canvas) override;

private:
	void updateSelectedOption();
	Scale getScaleFromOption(int32_t option);
	int32_t getOptionFromScale(Scale scale);
	void buildAvailableScales();

	static constexpr size_t kMaxScales = NUM_PRESET_SCALES + 1; // +1 for USER_SCALE
	char const* availableScaleNames[kMaxScales];
	Scale availableScales[kMaxScales];
	int32_t availableScaleNoteCounts[kMaxScales]; // Store note counts for display
	size_t numAvailableScales = 0;
	Scale currentScale = NO_SCALE;
};

extern ScaleSelection scaleSelection;

} // namespace deluge::gui::context_menu