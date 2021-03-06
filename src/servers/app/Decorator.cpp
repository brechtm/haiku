/*
 * Copyright 2001-2010, Haiku.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		DarkWyrm <bpmagic@columbus.rr.com>
 *		Stephan Aßmus <superstippi@gmx.de>
 *		Clemens Zeidler <haiku@clemens-zeidler.de>
 *		Ingo Weinhold <ingo_weinhold@gmx.de>
 */


/*!	Base class for window decorators */


#include "Decorator.h"

#include <stdio.h>

#include <Region.h>

#include "DrawingEngine.h"


/*!	\brief Constructor

	Does general initialization of internal data members and creates a colorset
	object.

	\param rect Size of client area
	\param wlook style of window look. See Window.h
	\param wfeel style of window feel. See Window.h
	\param wflags various window flags. See Window.h
*/
Decorator::Decorator(DesktopSettings& settings, BRect rect, window_look look,
		uint32 flags)
	:
	fDrawingEngine(NULL),
	fDrawState(),

	fLook(look),
	fFlags(flags),

	fZoomRect(),
	fCloseRect(),
	fMinimizeRect(),
	fTabRect(),
	fFrame(rect),
	fResizeRect(),
	fBorderRect(),

	fClosePressed(false),
	fZoomPressed(false),
	fMinimizePressed(false),
	fIsFocused(false),
	fTitle(""),

	fFootprintValid(false)
{
	memset(&fRegionHighlights, HIGHLIGHT_NONE, sizeof(fRegionHighlights));
}


/*!
	\brief Destructor

	Frees the color set and the title string
*/
Decorator::~Decorator()
{
}


/*!	\brief Assigns a display driver to the decorator
	\param driver A valid DrawingEngine object
*/
void
Decorator::SetDrawingEngine(DrawingEngine* engine)
{
	fDrawingEngine = engine;
	// lots of subclasses will depend on the driver for text support, so call
	// _DoLayout() after we have it
	if (fDrawingEngine)
		_DoLayout();
}


/*!	\brief Sets the decorator's window flags

	While this call will not update the screen, it will affect how future
	updates work and immediately affects input handling.

	\param flags New value for the flags
*/
void
Decorator::SetFlags(uint32 flags, BRegion* updateRegion)
{
	// we're nice to our subclasses - we make sure B_NOT_{H|V|}_RESIZABLE
	// are in sync (it's only a semantical simplification, not a necessity)
	if ((flags & (B_NOT_H_RESIZABLE | B_NOT_V_RESIZABLE))
			== (B_NOT_H_RESIZABLE | B_NOT_V_RESIZABLE))
		flags |= B_NOT_RESIZABLE;
	if (flags & B_NOT_RESIZABLE)
		flags |= B_NOT_H_RESIZABLE | B_NOT_V_RESIZABLE;

	_SetFlags(flags, updateRegion);
	_InvalidateFootprint();
		// the border might have changed (smaller/larger tab)
}


/*!	\brief Called whenever the system fonts are changed.
*/
void
Decorator::FontsChanged(DesktopSettings& settings, BRegion* updateRegion)
{
	_FontsChanged(settings, updateRegion);
	_InvalidateFootprint();
}


/*!	\brief Sets the decorator's window look
	\param look New value for the look
*/
void
Decorator::SetLook(DesktopSettings& settings, window_look look,
	BRegion* updateRect)
{
	_SetLook(settings, look, updateRect);
	_InvalidateFootprint();
		// the border very likely changed
}


/*!	\brief Sets the close button's value.

	Note that this does not update the button's look - it just updates the
	internal button value

	\param is_down Whether the button is down or not
*/
void
Decorator::SetClose(bool pressed)
{
	if (pressed != fClosePressed) {
		fClosePressed = pressed;
		DrawClose();
	}
}

/*!	\brief Sets the minimize button's value.

	Note that this does not update the button's look - it just updates the
	internal button value

	\param is_down Whether the button is down or not
*/
void
Decorator::SetMinimize(bool pressed)
{
	if (pressed != fMinimizePressed) {
		fMinimizePressed = pressed;
		DrawMinimize();
	}
}

/*!	\brief Sets the zoom button's value.

	Note that this does not update the button's look - it just updates the
	internal button value

	\param is_down Whether the button is down or not
*/
void
Decorator::SetZoom(bool pressed)
{
	if (pressed != fZoomPressed) {
		fZoomPressed = pressed;
		DrawZoom();
	}
}


/*!	\brief Updates the value of the decorator title
	\param string New title value
*/
void
Decorator::SetTitle(const char* string, BRegion* updateRegion)
{
	fTitle.SetTo(string);
	_SetTitle(string, updateRegion);

	_InvalidateFootprint();
		// the border very likely changed

	// TODO: redraw?
}


/*!	\brief Returns the decorator's window look
	\return the decorator's window look
*/
window_look
Decorator::Look() const
{
	return fLook;
}


/*!	\brief Returns the decorator's window flags
	\return the decorator's window flags
*/
uint32
Decorator::Flags() const
{
	return fFlags;
}


/*!	\brief Returns the decorator's title
	\return the decorator's title
*/
const char*
Decorator::Title() const
{
	return fTitle.String();
}


/*!	\brief Returns the decorator's border rectangle
	\return the decorator's border rectangle
*/
BRect
Decorator::BorderRect() const
{
	return fBorderRect;
}


/*!	\brief Returns the decorator's tab rectangle
	\return the decorator's tab rectangle
*/
BRect
Decorator::TabRect() const
{
	return fTabRect;
}


/*!	\brief Returns the value of the close button
	\return true if down, false if up
*/
bool
Decorator::GetClose()
{
	return fClosePressed;
}


/*!	\brief Returns the value of the minimize button
	\return true if down, false if up
*/
bool
Decorator::GetMinimize()
{
	return fMinimizePressed;
}


/*!	\brief Returns the value of the zoom button
	\return true if down, false if up
*/
bool
Decorator::GetZoom()
{
	return fZoomPressed;
}


void
Decorator::GetSizeLimits(int32* minWidth, int32* minHeight, int32* maxWidth,
	int32* maxHeight) const
{
}


/*!	\brief Changes the focus value of the decorator

	While this call will not update the screen, it will affect how future
	updates work.

	\param active True if active, false if not
*/
void
Decorator::SetFocus(bool active)
{
	fIsFocused = active;
	_SetFocus();
	// TODO: maybe it would be cleaner to handle the redraw here.
}


//	#pragma mark - virtual methods


/*!	\brief Returns a cached footprint if available otherwise recalculate it
*/
const BRegion&
Decorator::GetFootprint()
{
	if (!fFootprintValid) {
		_GetFootprint(&fFootprint);
		fFootprintValid = true;
	}
	return fFootprint;
}


/*!	\brief Performs hit-testing for the decorator.

	The base class provides a basic implementation, recognizing only button and
	tab hits. Derived classes must override/enhance it to handle borders and
	corners correctly.

	\param where The point to be tested.
	\return Either of the following, depending on what was hit:
		- \c REGION_NONE: None of the decorator regions.
		- \c REGION_TAB: The window tab (but none of the buttons embedded).
		- \c REGION_CLOSE_BUTTON: The close button.
		- \c REGION_ZOOM_BUTTON: The zoom button.
		- \c REGION_MINIMIZE_BUTTON: The minimize button.
		- \c REGION_LEFT_BORDER: The left border.
		- \c REGION_RIGHT_BORDER: The right border.
		- \c REGION_TOP_BORDER: The top border.
		- \c REGION_BOTTOM_BORDER: The bottom border.
		- \c REGION_LEFT_TOP_CORNER: The left-top corner.
		- \c REGION_LEFT_BOTTOM_CORNER: The left-bottom corner.
		- \c REGION_RIGHT_TOP_CORNER: The right-top corner.
		- \c REGION_RIGHT_BOTTOM_CORNER The right-bottom corner.
*/
Decorator::Region
Decorator::RegionAt(BPoint where) const
{
	if (fCloseRect.Contains(where))
		return REGION_CLOSE_BUTTON;
	if (fZoomRect.Contains(where))
		return REGION_ZOOM_BUTTON;
	if (fTabRect.Contains(where))
		return REGION_TAB;

	return REGION_NONE;
}


/*!	\brief Moves the decorator frame and all default rectangles

	If a subclass implements this method, be sure to call Decorator::MoveBy
	to ensure that internal members are also updated. All members of the
	Decorator class are automatically moved in this method

	\param x X Offset
	\param y y Offset
*/
void
Decorator::MoveBy(float x, float y)
{
	MoveBy(BPoint(x, y));
}


/*!	\brief Moves the decorator frame and all default rectangles

	If a subclass implements this method, be sure to call Decorator::MoveBy
	to ensure that internal members are also updated. All members of the
	Decorator class are automatically moved in this method

	\param offset BPoint containing the offsets
*/
void
Decorator::MoveBy(BPoint offset)
{
	if (fFootprintValid)
		fFootprint.OffsetBy(offset.x, offset.y);

	_MoveBy(offset);
}


/*!	\brief Resizes the decorator frame

	This is a required function for subclasses to implement - the default does
	nothing. Note that window resize flags should be followed and fFrame should
	be resized accordingly. It would also be a wise idea to ensure that the
	window's rectangles are not inverted.

	\param x x offset
	\param y y offset
*/
void
Decorator::ResizeBy(float x, float y, BRegion* dirty)
{
	ResizeBy(BPoint(x, y), dirty);
}


void
Decorator::ResizeBy(BPoint offset, BRegion* dirty)
{
	_ResizeBy(offset, dirty);
	_InvalidateFootprint();
}


bool
Decorator::SetTabLocation(float location, BRegion* updateRegion)
{
	if (_SetTabLocation(location, updateRegion)) {
		_InvalidateFootprint();
		return true;
	}
	return false;
}


/*!	\brief Sets a specific highlight for a decorator region.

	Can be overridden by derived classes, but the base class version must be
	called, if the highlight shall be applied.

	\param region The decorator region.
	\param highlight The value identifying the kind of highlight.
	\param dirty The dirty region to be extended, if the highlight changes. Can
		be \c NULL.
	\return \c true, if the highlight could be applied.
*/
bool
Decorator::SetRegionHighlight(Region region, uint8 highlight, BRegion* dirty)
{
	int32 index = (int32)region - 1;
	if (index < 0 || index >= REGION_COUNT - 1)
		return false;

	if (fRegionHighlights[index] == highlight)
		return true;
	fRegionHighlights[index] = highlight;

	if (dirty != NULL)
		ExtendDirtyRegion(region, *dirty);

	return true;
}


bool
Decorator::SetSettings(const BMessage& settings, BRegion* updateRegion)
{
	if (_SetSettings(settings, updateRegion)) {
		_InvalidateFootprint();
		return true;
	}
	return false;
}


bool
Decorator::GetSettings(BMessage* settings) const
{
	return false;
}


/*!	\brief Updates the decorator's look in the area \a rect

	The default version updates all areas which intersect the frame and tab.

	\param rect The area to update.
*/
void
Decorator::Draw(BRect rect)
{
	_DrawFrame(rect & fFrame);
	_DrawTab(rect & fTabRect);
}


//! Forces a complete decorator update
void
Decorator::Draw()
{
	_DrawFrame(fFrame);
	_DrawTab(fTabRect);
}


//! Draws the close button
void
Decorator::DrawClose()
{
	_DrawClose(fCloseRect);
}


//! draws the frame
void
Decorator::DrawFrame()
{
	_DrawFrame(fFrame);
}


//! draws the minimize button
void
Decorator::DrawMinimize()
{
	_DrawTab(fMinimizeRect);
}


//! draws the tab, title, and buttons
void
Decorator::DrawTab()
{
	_DrawTab(fTabRect);
	_DrawZoom(fZoomRect);
	_DrawMinimize(fMinimizeRect);
	_DrawTitle(fTabRect);
	_DrawClose(fCloseRect);
}


//! draws the title
void
Decorator::DrawTitle()
{
	_DrawTitle(fTabRect);
}


//! draws the zoom button
void
Decorator::DrawZoom()
{
	_DrawZoom(fZoomRect);
}


rgb_color
Decorator::UIColor(color_which which)
{
	// TODO: for now - calling ui_color() from within the app_server
	//	will always return the default colors (as there is no be_app)
	return ui_color(which);
}


/*!	\brief Extends a dirty region by a decorator region.

	Must be implemented by derived classes.

	\param region The decorator region.
	\param dirty The dirty region to be extended.
*/
void
Decorator::ExtendDirtyRegion(Region region, BRegion& dirty)
{
}


//! Function for calculating layout for the decorator
void
Decorator::_DoLayout()
{
}


/*!	\brief Actually draws the frame
	\param rect Area of the frame to update
*/
void
Decorator::_DrawFrame(BRect rect)
{
}


/*!	\brief Actually draws the tab

	This function is called when the tab itself needs drawn. Other items,
	like the window title or buttons, should not be drawn here.

	\param rect Area of the tab to update
*/
void
Decorator::_DrawTab(BRect rect)
{
}


/*!	\brief Actually draws the close button

	Unless a subclass has a particularly large button, it is probably
	unnecessary to check the update rectangle.

	\param rect Area of the button to update
*/
void
Decorator::_DrawClose(BRect rect)
{
}


/*!	\brief Actually draws the title

	The main tasks for this function are to ensure that the decorator draws
	the title only in its own area and drawing the title itself.
	Using B_OP_COPY for drawing the title is recommended because of the marked
	performance hit of the other drawing modes, but it is not a requirement.

	\param rect area of the title to update
*/
void
Decorator::_DrawTitle(BRect rect)
{
}


/*!	\brief Actually draws the zoom button

	Unless a subclass has a particularly large button, it is probably
	unnecessary to check the update rectangle.

	\param rect Area of the button to update
*/
void
Decorator::_DrawZoom(BRect rect)
{
}

/*!
	\brief Actually draws the minimize button

	Unless a subclass has a particularly large button, it is probably
	unnecessary to check the update rectangle.

	\param rect Area of the button to update
*/
void
Decorator::_DrawMinimize(BRect rect)
{
}


//! Hook function called when the decorator changes focus
void
Decorator::_SetFocus()
{
}


void
Decorator::_FontsChanged(DesktopSettings& settings, BRegion* updateRegion)
{
}


void
Decorator::_SetLook(DesktopSettings& settings, window_look look,
	BRegion* updateRect)
{
	fLook = look;
}


void
Decorator::_SetFlags(uint32 flags, BRegion* updateRegion)
{
	fFlags = flags;
}


void
Decorator::_MoveBy(BPoint offset)
{
	fZoomRect.OffsetBy(offset);
	fCloseRect.OffsetBy(offset);
	fMinimizeRect.OffsetBy(offset);
	fMinimizeRect.OffsetBy(offset);
	fTabRect.OffsetBy(offset);
	fFrame.OffsetBy(offset);
	fResizeRect.OffsetBy(offset);
	fBorderRect.OffsetBy(offset);
}


bool
Decorator::_SetSettings(const BMessage& settings, BRegion* updateRegion)
{
	return false;
}


/*!	\brief Returns the "footprint" of the entire window, including decorator

	This function is required by all subclasses.

	\param region Region to be changed to represent the window's screen
		footprint
*/
void
Decorator::_GetFootprint(BRegion *region)
{
}


void
Decorator::_InvalidateFootprint()
{
	fFootprintValid = false;
}
