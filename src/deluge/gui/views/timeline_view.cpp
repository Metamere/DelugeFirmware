/*
 * Copyright © 2014-2023 Synthstrom Audible Limited
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

#include "gui/views/timeline_view.h"
#include "definitions_cxx.hpp"
#include "extern.h"
#include "gui/ui/load/load_pattern_ui.h"
#include "gui/views/arranger_view.h"
#include "gui/views/audio_clip_view.h"
#include "gui/views/instrument_clip_view.h"
#include "gui/views/session_view.h"
#include "gui/views/view.h"
#include "hid/button.h"
#include "hid/buttons.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "hid/led/indicator_leds.h"
#include "hid/led/pad_leds.h"
#include "lib/printf.h"
#include "model/settings/runtime_feature_settings.h"
#include "model/song/song.h"
#include "processing/engines/audio_engine.h"
#include <algorithm>
#include <playback/playback_handler.h>
#include <string.h>

void TimelineView::scrollFinished() {
	exitUIMode(UI_MODE_HORIZONTAL_SCROLL);
	// Needed because sometimes we initiate a scroll before reverting an Action, so we need to
	// properly render again afterwards
	uiNeedsRendering(getRootUI(), 0xFFFFFFFF, 0);
}

// Virtual function
bool TimelineView::setupScroll(uint32_t oldScroll) {
	memset(PadLEDs::transitionTakingPlaceOnRow, 1, sizeof(PadLEDs::transitionTakingPlaceOnRow));

	renderMainPads(0xFFFFFFFF, PadLEDs::imageStore, &PadLEDs::occupancyMaskStore[kDisplayHeight], true);

	return true;
}

bool TimelineView::calculateZoomPinSquares(uint32_t oldScroll, uint32_t newScroll, uint32_t newZoom, uint32_t oldZoom) {

	int32_t zoomPinSquareBig = ((int64_t)(int32_t)(oldScroll - newScroll) << 16) / (int32_t)(newZoom - oldZoom);

	for (int32_t i = 0; i < kDisplayHeight; i++) {
		PadLEDs::zoomPinSquare[i] = zoomPinSquareBig;
	}

	tellMatrixDriverWhichRowsContainSomethingZoomable();

	return true;
}

ActionResult TimelineView::buttonAction(deluge::hid::Button b, bool on, bool inCardRoutine) {
	using namespace deluge::hid::button;

	// Horizontal encoder button
	if (b == X_ENC) {
		if (on) {
			// Show current zoom level
			if (isNoUIModeActive() && getCurrentUI() != &arrangerView && getCurrentUI() != &sessionView) {
				bool is_clip_view = (getCurrentUI() == &instrumentClipView || getCurrentUI() == &audioClipView);
				displayZoomLevel(false, false, !is_clip_view);
			}

			enterUIMode(UI_MODE_HOLDING_HORIZONTAL_ENCODER_BUTTON);
		}

		else {

			if (isUIModeActive(UI_MODE_HOLDING_HORIZONTAL_ENCODER_BUTTON)) {
				display->cancelPopup();
				exitUIMode(UI_MODE_HOLDING_HORIZONTAL_ENCODER_BUTTON);
			}
		}
	}

	// Triplets button
	else if (b == TRIPLETS) {
		if (on) {
			if (inCardRoutine) {
				return ActionResult::REMIND_ME_OUTSIDE_CARD_ROUTINE;
			}
			tripletsButtonPressed();
		}
	}

#ifdef soloButtonX
	// Solo button
	else if (b == solo) {
		if (on) {
			if (isNoUIModeActive()) {
				enterUIMode(UI_MODE_SOLO_BUTTON_HELD);
				indicator_leds::blinkLed(IndicatorLED::SOLO, 255, 1);
			}
		}
		else {
			exitUIMode(UI_MODE_SOLO_BUTTON_HELD);
			indicator_leds::setLedState(IndicatorLED::SOLO, false);
		}
	}

#endif

	else {
		return view.buttonAction(b, on, inCardRoutine);
	}

	return ActionResult::DEALT_WITH;
}

void TimelineView::displayZoomLevel(bool permanent, bool clear_area, bool centered_notification) {
	DEF_STACK_STRING_BUF(text, 30);
	currentSong->getNoteLengthName(text, currentSong->xZoom[getNavSysId()], "-notes", true);

	if (display->haveOLED()) {
		if (permanent) {
			deluge::hid::display::oled_canvas::Canvas& canvas = hid::display::OLED::main;

			const int32_t yPos = OLED_MAIN_TOPMOST_PIXEL + 32;

			if (clear_area) {
				const int32_t x_margin = kTextSpacingX * 5;
				canvas.clearAreaExact(x_margin, yPos, OLED_MAIN_WIDTH_PIXELS - 1 - x_margin, yPos + kTextSpacingY);
			}

			canvas.drawStringCentred(text.data(), yPos, kTextSpacingX, kTextSpacingY);
			if (display->hasPopupOfType(PopupType::NOTIFICATION)) {
				display->cancelPopup();
			}
			deluge::hid::display::OLED::markChanged();
		}
		else {
			static_cast<deluge::hid::display::OLED*>(display)->displayNotification(text.data(), std::nullopt, true,
			                                                                       centered_notification, false);
		}
	}
	else {
		display->displayPopup(text.data(), 0, true);
	}
}

bool horizontalEncoderActionLock = false;

ActionResult TimelineView::horizontalEncoderAction(int32_t offset) {

	if (sdRoutineLock) {
		return ActionResult::REMIND_ME_OUTSIDE_CARD_ROUTINE;
	}

	// These next two, I had here before adding the actual SD lock check / remind-later above. Maybe they're not still
	// necessary? If either was true, wouldn't sdRoutineLock be true also for us to have gotten here?
	if (pendingUIRenderingLock) {
		return ActionResult::REMIND_ME_OUTSIDE_CARD_ROUTINE; // Would possibly prefer to have this case cause it to
		                                                     // still come back later and do it, but oh well
	}
	if (horizontalEncoderActionLock) {
		return ActionResult::REMIND_ME_OUTSIDE_CARD_ROUTINE; // Really wouldn't want to get in here multiple times,
		                                                     // while pre-rendering the waveforms for the new navigation
	}
	horizontalEncoderActionLock = true;

	int32_t navSysId = getNavSysId();

	// exception to usual checks for zooming and scrolling
	// if you're in the note row editor, UI mode auditioning will always be active
	// in addition, if shift is enabled, we allow zooming and scrolling
	bool inNoteRowEditor = getCurrentUI() == &soundEditor && soundEditor.inNoteRowEditor();

	// Encoder button pressed, zoom.
	if (isUIModeActive(UI_MODE_HOLDING_HORIZONTAL_ENCODER_BUTTON)) {

		// in note row editor we are holding horizontal encoder + auditioning to zoom
		if (isUIModeActiveExclusively(UI_MODE_HOLDING_HORIZONTAL_ENCODER_BUTTON) || inNoteRowEditor) {
			int32_t oldXZoom = currentSong->xZoom[navSysId];
			int32_t zoomMagnitude = -offset;
			uint32_t newZoom = zoomMagnitude == -1 ? oldXZoom >> 1 : oldXZoom << 1;
			uint32_t clipLengthMaxZoom = getMaxZoom();

			// When this timer is set, we may need to wait a bit before allowing zooming
			if (delayHorizontalZoomUntil != 0) {
				bool isWithinZoomDelay = util::infinite_a_lt_b(AudioEngine::audioSampleTimer, delayHorizontalZoomUntil);

				// Prevent further zooming in the delayed direction (see below)
				if (isWithinZoomDelay && (delayHorizontalZoomMagnitude == zoomMagnitude)) {
					goto getOut;
				}

				delayHorizontalZoomUntil = 0;
			}

			// Prevent quickly scrolling past the clip's max zoom level
			if (newZoom == clipLengthMaxZoom && delayHorizontalZoomUntil == 0) {
				delayHorizontalZoomUntil = AudioEngine::audioSampleTimer + kShortPressTime;
				delayHorizontalZoomMagnitude = zoomMagnitude;
			}

			// Constrain to zoom limits
			if ((zoomMagnitude == -1 && oldXZoom <= 3)
			    || (zoomMagnitude == 1 && oldXZoom >= std::min<uint32_t>(clipLengthMaxZoom * 16, kMaxZoom))) {
				goto getOut;
			}

			currentSong->xZoom[navSysId] = newZoom;
			int32_t newScroll = currentSong->xScroll[navSysId] / (newZoom * kDisplayWidth) * (newZoom * kDisplayWidth);

			initiateXZoom(zoomMagnitude, newScroll, oldXZoom);
			const bool is_clip_view = (getCurrentUI() == &instrumentClipView || getCurrentUI() == &audioClipView);
			displayZoomLevel(false, false, !is_clip_view);
			if (display->haveOLED() && is_clip_view) { //&& getCurrentUI() == &instrumentClipView) {
				// update the clip length display to the appropriate precision for the current zoom level
				// it might not need an update with every zoom level change,
				// but it won't be updated too often so no need to track and prevent
				// doesn't work right on bars and beats since it has the file path taking up the bottom row.
				displayNumberOfBarsAndBeats(currentSong->getCurrentClip()->getLoopLength(),
				                            currentSong->xZoom[NAVIGATION_CLIP], false, "LONG", false);
			}
		}
	}

	// Encoder button not pressed, scroll
	else if (isNoUIModeActive() || isUIModeActiveExclusively(UI_MODE_AUDITIONING)) {
		// If shift button not pressed
		// or it's pressed and you're in note row editor
		if (!Buttons::isShiftButtonPressed() || inNoteRowEditor) {

			int32_t newXScroll = currentSong->xScroll[navSysId] + offset * currentSong->xZoom[navSysId] * kDisplayWidth;

			// Make sure we don't scroll too far left
			if (newXScroll < 0) {
				newXScroll = 0;
			}

			// Make sure we don't scroll too far right
			if (newXScroll < getMaxLength() || offset < 0) {
				if (newXScroll != currentSong->xScroll[navSysId]) {
					initiateXScroll(newXScroll);
				}
			}
			displayScrollPos();
		}
	}

getOut:
	horizontalEncoderActionLock = false;
	return ActionResult::DEALT_WITH;
}

void TimelineView::displayScrollPos() {

	int32_t navSysId = getNavSysId();
	uint32_t quantization = currentSong->xZoom[navSysId];
	if (navSysId == NAVIGATION_CLIP) {
		quantization *= kDisplayWidth;
	}

	displayNumberOfBarsAndBeats(currentSong->xScroll[navSysId], quantization, true, "FAR");
}

void TimelineView::displayNumberOfBarsAndBeats(uint32_t number, uint32_t quantization, bool count_from_one,
                                               char const* too_long_text, bool popup) {

	uint32_t one_bar = currentSong->getBarLength();

	uint32_t which_bar = number / one_bar;

	uint32_t pos_within_bar = number - which_bar * one_bar;

	uint32_t which_beat = pos_within_bar / (one_bar >> 2);

	uint32_t pos_within_beat = pos_within_bar - which_beat * (one_bar >> 2);

	uint32_t which_sub_beat = pos_within_beat / (one_bar >> 4);

	if (count_from_one) {
		which_bar++;
		which_beat++;
		which_sub_beat++;
	}

	if (display->haveOLED()) {
		// D_PRINTLN("number: %d, quantization: %d", number, quantization);
		char buffer[11];
		if (popup) {           // for scroll position display notification
			if (number == 0) { // we could have skipped all the calculations for this case, but meh.
				sprintf(buffer, "%s", "START");
			}
			else if (quantization > 192 || which_bar > 999999) { // 32nd-notes, so > half note per screen
				sprintf(buffer, "%d", which_bar);
			}
			else if (quantization > 48 || which_bar > 9999) { // 128th-notes, so > eight note per screen
				sprintf(buffer, "%d:%d", which_bar, which_beat);
			}
			else {
				sprintf(buffer, "%d:%d:%d", which_bar, which_beat, which_sub_beat);
			}
			// if (getCurrentUI() != &arrangerView && getCurrentUI() != &sessionView) {
			if (getCurrentUI() == &instrumentClipView || getCurrentUI() == &audioClipView) { // align left side
				static_cast<deluge::hid::display::OLED*>(display)->displayNotification(buffer, std::nullopt, true,
				                                                                       false, false);
			}
			else { // centered
				static_cast<deluge::hid::display::OLED*>(display)->displayNotification(buffer, std::nullopt, true, true,
				                                                                       false);
			}
		}
		else {                         // for instrument clip view's clip length display
			if (quantization >= 384) { // one bar per pad
				sprintf(buffer, "%d", which_bar);
			}
			else if (which_bar > 999999 || quantization >= 96) { // quarter notes
				sprintf(buffer, "%d:%d", which_bar, which_beat);
			}
			else {
				sprintf(buffer, "%d:%d:%d", which_bar, which_beat, which_sub_beat);
			}
			if (getCurrentUI() == &audioClipView && !playbackHandler.playbackState) {
				// the scrolling file path would interfere
				display->popupTextTemporary(buffer);
				return;
			}
			deluge::hid::display::oled_canvas::Canvas& canvas = hid::display::OLED::main;
			int32_t yPos = OLED_MAIN_TOPMOST_PIXEL + 32;
			canvas.clearAreaExact(OLED_MAIN_WIDTH_PIXELS / 2, yPos, OLED_MAIN_WIDTH_PIXELS - 1, yPos + kTextSpacingY);
			canvas.drawStringAlignRight(buffer, yPos, kTextSpacingX, kTextSpacingY);
			deluge::hid::display::OLED::markChanged();
		}
	}
	else {
		char text[5];

		uint8_t dotMask = 0b10000000;

		if (which_bar >= 10000) {
			strcpy(text, too_long_text);
		}
		else {
			strcpy(text, "    ");

			if (which_bar < 10) {
				intToString(which_bar, &text[1]);
			}
			else {
				intToString(which_bar, &text[0]);
			}

			if (which_bar < 100) {
				dotMask |= 1 << 2;

				if (quantization >= (one_bar >> 2)) {
					text[2] = ' ';
					goto putBeatCountOnFarRight;
				}

				intToString(which_beat, &text[2]);
				dotMask |= 1 << 1;

				intToString(which_sub_beat, &text[3]);
			}
			else if (which_bar < 1000) {
				dotMask |= 1 << 1;

putBeatCountOnFarRight:
				intToString(which_beat, &text[3]);
			}
		}

		display->displayPopup(text, 3, false, dotMask);
	}
}

// Changes the actual xScroll.
void TimelineView::initiateXScroll(uint32_t newXScroll, int32_t numSquaresToScroll) {

	uint32_t oldXScroll = currentSong->xScroll[getNavSysId()];

	int32_t scrollDirection = (newXScroll > currentSong->xScroll[getNavSysId()]) ? 1 : -1;

	currentSong->xScroll[getNavSysId()] = newXScroll;

	bool anyAnimationToDo = setupScroll(oldXScroll);

	if (anyAnimationToDo) {
		currentUIMode |=
		    UI_MODE_HORIZONTAL_SCROLL; // Must set this before calling PadLEDs::setupScroll(), which might then unset it
		PadLEDs::horizontal::setupScroll(scrollDirection, kDisplayWidth, false, numSquaresToScroll);
	}
}

// Returns whether any zooming took place - I think?
bool TimelineView::zoomToMax(bool inOnly) {
	uint32_t maxZoom = getMaxZoom();
	uint32_t oldZoom = currentSong->xZoom[getNavSysId()];
	if (maxZoom != oldZoom && (!inOnly || maxZoom < oldZoom)) {

		// Zoom to view what's new
		currentSong->xZoom[getNavSysId()] = maxZoom;

		int32_t newScroll = currentSong->xScroll[getNavSysId()] / (maxZoom * kDisplayWidth) * (maxZoom * kDisplayWidth);

		initiateXZoom(howMuchMoreMagnitude(maxZoom, oldZoom), newScroll, oldZoom);
		return true;
	}
	else {
		return false;
	}
}

// Puts us into zoom mode. Assumes we've already altered currentSong->xZoom.
void TimelineView::initiateXZoom(int32_t zoomMagnitude, int32_t newScroll, uint32_t oldZoom) {

	memcpy(PadLEDs::imageStore[(zoomMagnitude < 0) ? kDisplayHeight : 0], PadLEDs::image,
	       (kDisplayWidth + kSideBarWidth) * kDisplayHeight * sizeof(RGB));

	uint32_t oldScroll = currentSong->xScroll[getNavSysId()];

	currentSong->xScroll[getNavSysId()] = newScroll;
	bool anyToAnimate = calculateZoomPinSquares(oldScroll, newScroll, currentSong->xZoom[getNavSysId()], oldZoom)
	                    && getCurrentUI() != &loadPatternUI;

	if (anyToAnimate) {

		int32_t storeOffset = (zoomMagnitude < 0) ? 0 : kDisplayHeight;

		renderMainPads(0xFFFFFFFF, &PadLEDs::imageStore[storeOffset], &PadLEDs::occupancyMaskStore[storeOffset], true);

		PadLEDs::zoomingIn = (zoomMagnitude < 0);
		PadLEDs::zoomMagnitude = PadLEDs::zoomingIn ? -zoomMagnitude : zoomMagnitude;

		enterUIMode(UI_MODE_HORIZONTAL_ZOOM);
		PadLEDs::recordTransitionBegin(kZoomSpeed);
		PadLEDs::renderZoom();
	}
}

bool TimelineView::scrollRightToEndOfLengthIfNecessary(int32_t maxLength) {

	// If we're not scrolled all the way to the right, go there now
	if (getPosFromSquare(kDisplayWidth) < maxLength) {

		uint32_t displayLength = currentSong->xZoom[getNavSysId()] * kDisplayWidth;

		initiateXScroll((maxLength - 1) / displayLength * displayLength);
		// displayScrollPos();
		return true;
	}
	return false;
}

bool TimelineView::scrollLeftIfTooFarRight(int32_t maxLength) {

	if (getPosFromSquare(0) >= maxLength) {
		initiateXScroll(currentSong->xScroll[getNavSysId()] - currentSong->xZoom[getNavSysId()] * kDisplayWidth);
		// displayScrollPos();
		return true;
	}
	return false;
}

void TimelineView::tripletsButtonPressed() {

	currentSong->tripletsOn = !currentSong->tripletsOn;
	if (currentSong->tripletsOn) {
		currentSong->tripletsLevel = currentSong->xZoom[getNavSysId()] * 4 / 3;
	}
	uiNeedsRendering(this, 0xFFFFFFFF, 0);
	setTripletsLEDState();
}

void TimelineView::setTripletsLEDState() {
	indicator_leds::setLedState(IndicatorLED::TRIPLETS, inTripletsView());
}

int32_t TimelineView::getPosFromSquare(int32_t square, int32_t xScroll, uint32_t xZoom) const {

	if (inTripletsView()) {
		// If zoomed in just a normal amount...
		if (xZoom < currentSong->tripletsLevel) {
			int32_t prevBlockStart = xScroll / (currentSong->tripletsLevel * 3) * (currentSong->tripletsLevel * 3);
			int32_t blockStartRelativeToScroll = (prevBlockStart - xScroll) / 3 * 4; // Negative or 0

			int32_t posRelativeToBlock =
			    ((int32_t)square) * (xZoom * 4 / 3) - blockStartRelativeToScroll; // Relative to block start pos
			uint32_t numBlocksIn = posRelativeToBlock / (currentSong->tripletsLevel * 4);

			// These two lines affect the resulting "pos" of cols which are "undefined" so they can be detected as such
			uint32_t numTripletsIn = posRelativeToBlock / currentSong->tripletsLevel;
			if (numTripletsIn % 4 == 3) {
				posRelativeToBlock = (numBlocksIn + 1) * currentSong->tripletsLevel * 3;
			}
			else {
				posRelativeToBlock -= numBlocksIn * currentSong->tripletsLevel;
			}

			return posRelativeToBlock + prevBlockStart;
		}
		else if (xZoom < currentSong->tripletsLevel * 2) {
			return xScroll + square * xZoom + (square % 2) * currentSong->tripletsLevel / 2;
		}
	}

	return xScroll + square * xZoom;
}

int32_t TimelineView::getPosFromSquare(int32_t square, int32_t xScroll) const {

	int32_t navSys = getNavSysId();

	if (xScroll == -1) {
		xScroll = currentSong->xScroll[navSys]; // Sets default
	}

	uint32_t xZoom = currentSong->xZoom[navSys];

	return getPosFromSquare(square, xScroll, xZoom);
}

int32_t TimelineView::getSquareFromPos(int32_t pos, bool* rightOnSquare, int32_t xScroll, uint32_t xZoom) {

	int32_t posRelativeToScroll = pos - xScroll;

	if (inTripletsView()) {
		if (xZoom < currentSong->tripletsLevel) {
			int32_t blockStartPos = xScroll / (currentSong->tripletsLevel * 3) * (currentSong->tripletsLevel * 3);
			int32_t blockStartRelativeToScroll = blockStartPos - xScroll; // Will be negative or 0
			int32_t posRelativeToBlockStart = pos - blockStartPos;

			if (rightOnSquare) {
				*rightOnSquare =
				    (posRelativeToBlockStart % (xZoom * 4 / 3) == 0); // Will the % be ok if it's negative? No! :O
			}

			int32_t numBlocksIn =
			    divide_round_negative(posRelativeToBlockStart,
			                          currentSong->tripletsLevel * 3); // Keep as separate step, for rounding purposes

			int32_t semiFinal =
			    posRelativeToBlockStart + blockStartRelativeToScroll * 4 / 3 + numBlocksIn * currentSong->tripletsLevel;
			int32_t final = divide_round_negative(semiFinal, xZoom * 4 / 3);
			return final;
		}
		else if (xZoom < currentSong->tripletsLevel * 2) {
			int32_t posRelativeToTripletsStart =
			    posRelativeToScroll % (currentSong->tripletsLevel * 3); // Will the % be ok if it's negative? No! :O
			if (rightOnSquare) {
				*rightOnSquare =
				    (posRelativeToTripletsStart == 0 || posRelativeToTripletsStart == currentSong->tripletsLevel * 2);
			}

			if (posRelativeToTripletsStart >= currentSong->tripletsLevel * 2) {
				posRelativeToTripletsStart -= currentSong->tripletsLevel * 2;
			}
			return divide_round_negative(posRelativeToScroll - posRelativeToTripletsStart, xZoom);
		}
	}

	if (rightOnSquare) {
		*rightOnSquare = (posRelativeToScroll >= 0
		                  && (posRelativeToScroll % xZoom) == 0); // Will the % be ok if it's negative? No! :O
	}

	// Have to divide the two things separately before subtracting, otherwise negative results get rounded the wrong
	// way!
	return divide_round_negative(pos - xScroll, xZoom);
}

int32_t TimelineView::getSquareFromPos(int32_t pos, bool* rightOnSquare, int32_t xScroll) {

	int32_t navSys = getNavSysId();

	if (xScroll == -1) {
		xScroll = currentSong->xScroll[navSys]; // Defaults to main currentSong->xScroll
	}

	uint32_t xZoom = currentSong->xZoom[navSys];

	return getSquareFromPos(pos, rightOnSquare, xScroll, xZoom);
}

int32_t TimelineView::getSquareEndFromPos(int32_t pos, int32_t localScroll) {
	bool rightOnSquare;
	int32_t square = getSquareFromPos(pos, &rightOnSquare, localScroll);
	if (!rightOnSquare) {
		square++;
	}
	return square;
}

bool TimelineView::isSquareDefined(int32_t square, int32_t xScroll, uint32_t xZoom) {
	if (!inTripletsView()) {
		return true;
	}
	if (xZoom > currentSong->tripletsLevel) {
		return true;
	}
	return (getPosFromSquare(square + 1, xScroll, xZoom) > getPosFromSquare(square, xScroll, xZoom));
}

// Deprecate this.
bool TimelineView::isSquareDefined(int32_t square, int32_t xScroll) {
	if (!inTripletsView()) {
		return true;
	}
	else {
		if (currentSong->xZoom[getNavSysId()] > currentSong->tripletsLevel) {
			return true;
		}
		return (getPosFromSquare(square + 1, xScroll) > getPosFromSquare(square, xScroll));
	}
}

bool TimelineView::inTripletsView() const {
	return (supportsTriplets() && currentSong->tripletsOn);
}

void TimelineView::midiLearnFlash() {
	uiNeedsRendering(this, 0, 0xFFFFFFFF);
}
