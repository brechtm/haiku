/*
 * Copyright 2001-2006, Haiku Inc.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Marc Flerackers (mflerackers@androme.be)
 *		Stephan Aßmus <superstippi@gmx.de>
 */

#include <stdio.h>

#include <BMCPrivate.h>
#include <LayoutUtils.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <Message.h>
#include <MessageRunner.h>
#include <Region.h>
#include <Window.h>


_BMCFilter_::_BMCFilter_(BMenuField *menuField, uint32 what)
	:
	BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE, what),
	fMenuField(menuField)
{
}


_BMCFilter_::~_BMCFilter_()
{
}


filter_result
_BMCFilter_::Filter(BMessage *message, BHandler **handler)
{
	if (message->what == B_MOUSE_DOWN) {
		if (BView *view = dynamic_cast<BView *>(*handler)) {
			BPoint point;
			message->FindPoint("be:view_where", &point);
			view->ConvertToParent(&point);
			message->ReplacePoint("be:view_where", point);
			*handler = fMenuField;
		}
	}

	return B_DISPATCH_MESSAGE;
}


_BMCFilter_ &
_BMCFilter_::operator=(const _BMCFilter_ &)
{
	return *this;
}


_BMCMenuBar_::_BMCMenuBar_(BRect frame, bool fixedSize, BMenuField *menuField)
	: BMenuBar(frame, "_mc_mb_", B_FOLLOW_LEFT | B_FOLLOW_TOP, B_ITEMS_IN_ROW,
		!fixedSize),
	fMenuField(menuField),
	fFixedSize(fixedSize),
	fRunner(NULL),
	fShowPopUpMarker(true)
{
	SetFlags(Flags() | B_FRAME_EVENTS);
	SetBorder(B_BORDER_CONTENTS);

	float left, top, right, bottom;
	GetItemMargins(&left, &top, &right, &bottom);
	// give a bit more space to draw the small thumb
	left -= 1;
	right += 3;
	SetItemMargins(left, top, right, bottom);

	SetMaxContentWidth(frame.Width() - (left + right));

	fPreviousWidth = frame.Width();
}


_BMCMenuBar_::_BMCMenuBar_(BMessage *data)
	:	BMenuBar(data),
	fMenuField(NULL),
	fFixedSize(true),
	fRunner(NULL),
	fShowPopUpMarker(true)
{
	SetFlags(Flags() | B_FRAME_EVENTS);

	bool resizeToFit;
	if (data->FindBool("_rsize_to_fit", &resizeToFit) == B_OK)
		fFixedSize = !resizeToFit;
}


_BMCMenuBar_::~_BMCMenuBar_()
{
	delete fRunner;
}


BArchivable *
_BMCMenuBar_::Instantiate(BMessage *data)
{
	if (validate_instantiation(data, "_BMCMenuBar_"))
		return new _BMCMenuBar_(data);

	return NULL;
}


void
_BMCMenuBar_::AttachedToWindow()
{
	fMenuField = static_cast<BMenuField *>(Parent());

	// Don't cause the KeyMenuBar to change by being attached
	BMenuBar *menuBar = Window()->KeyMenuBar();
	BMenuBar::AttachedToWindow();
	if (fFixedSize)
		SetResizingMode(B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP);
	Window()->SetKeyMenuBar(menuBar);
}


void
_BMCMenuBar_::Draw(BRect updateRect)
{
	if (!fShowPopUpMarker) {
		BMenuBar::Draw(updateRect);
		return;
	}

	// draw the right side with the popup marker

	// prevent the original BMenuBar's Draw from
	// drawing in those parts
	BRect bounds(Bounds());
	bounds.right -= 10.0;
	bounds.bottom -= 1.0;

	BRegion clipping(bounds);
	ConstrainClippingRegion(&clipping);

	BMenuBar::Draw(updateRect);

	// restore clipping
	ConstrainClippingRegion(NULL);
	bounds.right += 10.0;
	bounds.bottom += 1.0;

	// prepare some colors
	rgb_color normalNoTint = LowColor();
	rgb_color noTint = tint_color(normalNoTint, 0.74);
	rgb_color darken4;
	rgb_color normalDarken4;
	rgb_color darken1;
	rgb_color lighten1;
	rgb_color lighten2;

	if (IsEnabled()) {
		darken4 = tint_color(noTint, B_DARKEN_4_TINT);
		normalDarken4 = tint_color(normalNoTint, B_DARKEN_4_TINT);
		darken1 = tint_color(noTint, B_DARKEN_1_TINT);
		lighten1 = tint_color(noTint, B_LIGHTEN_1_TINT);
		lighten2 = tint_color(noTint, B_LIGHTEN_2_TINT);
	} else {
		darken4 = tint_color(noTint, B_DARKEN_2_TINT);
		normalDarken4 = tint_color(normalNoTint, B_DARKEN_2_TINT);
		darken1 = tint_color(noTint, (B_NO_TINT + B_DARKEN_1_TINT) / 2.0);
		lighten1 = tint_color(noTint, (B_NO_TINT + B_LIGHTEN_1_TINT) / 2.0);
		lighten2 = tint_color(noTint, B_LIGHTEN_1_TINT);
	}

	BRect r(bounds);
	r.left = r.right - 10.0;	

	BeginLineArray(6);
		// bottom below item text, darker then BMenuBar
		// would normaly draw it
		AddLine(BPoint(bounds.left, r.bottom),
				BPoint(r.left - 1.0, r.bottom), normalDarken4);

		// bottom below popup marker
		AddLine(BPoint(r.left, r.bottom),
				BPoint(r.right, r.bottom), darken4);
		// right of popup marker
		AddLine(BPoint(r.right, r.bottom - 1),
				BPoint(r.right, r.top), darken4);
		// top above popup marker
		AddLine(BPoint(r.left, r.top),
				BPoint(r.right - 2, r.top), lighten2);

		r.top += 1;
		r.bottom -= 1;
		r.right -= 1;

		// bottom below popup marker
		AddLine(BPoint(r.left, r.bottom),
				BPoint(r.right, r.bottom), darken1);
		// right of popup marker
		AddLine(BPoint(r.right, r.bottom - 1),
				BPoint(r.right, r.top), darken1);
	EndLineArray();

	r.bottom -= 1;
	r.right -= 1;
	SetHighColor(noTint);
	FillRect(r);

	// popup marker
	BPoint center(roundf((r.left + r.right) / 2.0),
				  roundf((r.top + r.bottom) / 2.0));
	BPoint triangle[3];
	triangle[0] = center + BPoint(-2.5, -0.5);
	triangle[1] = center + BPoint(2.5, -0.5);
	triangle[2] = center + BPoint(0.0, 2.0);

	uint32 flags = Flags();
	SetFlags(flags | B_SUBPIXEL_PRECISE);

	SetHighColor(normalDarken4);
	FillTriangle(triangle[0], triangle[1], triangle[2]);

	SetFlags(flags);
}


void
_BMCMenuBar_::FrameResized(float width, float height)
{
	// we need to take care of resizing and cleaning up
	// the parent menu field
	float diff;
	if (fFixedSize)
		diff = width - fPreviousWidth;
	else
		diff = Frame().right - (fMenuField->Bounds().right - 2);

	fPreviousWidth = width;

	if (Window()) {
		if (diff > 0) {
			// clean up the dirty right border of
			// the menu field when enlarging
			BRect dirty(fMenuField->Bounds());
			dirty.left = dirty.right - diff - 2;
			fMenuField->Invalidate(dirty);
			
			// clean up the arrow part
			dirty = Bounds();
			dirty.left = dirty.right - diff - 12;
			Invalidate(dirty);

		} else if (diff < 0) {
			// clean up the dirty right line of
			// the menu field when shrinking
			BRect dirty(fMenuField->Bounds());
			dirty.left = dirty.right - 2;
			fMenuField->Invalidate(dirty);
			
			// clean up the arrow part
			dirty = Bounds();
			dirty.left = dirty.right - 12;
			Invalidate(dirty);
		}
	}

	if (!fFixedSize && ResizingMode() == (B_FOLLOW_LEFT | B_FOLLOW_TOP)) {
		// we have been shrinked or enlarged and need to take care
		// of the size of the parent menu field as well
		// NOTE: no worries about follow mode, we follow left and top
		// in autosize mode
		fMenuField->ResizeBy(diff, 0.0);
	}
	BMenuBar::FrameResized(width, height);
}


void
_BMCMenuBar_::MessageReceived(BMessage *msg)
{
	switch (msg->what) {
		case 'TICK':
		{
			BMenuItem *item = ItemAt(0);

			if (item && item->Submenu() &&  item->Submenu()->Window()) {
				BMessage message(B_KEY_DOWN);
	
				message.AddInt8("byte", B_ESCAPE);
				message.AddInt8("key", B_ESCAPE);
				message.AddInt32("modifiers", 0);
				message.AddInt8("raw_char", B_ESCAPE);
	
				Window()->PostMessage(&message, this, NULL);
			}
		}
		// fall through	
		default:
			BMenuBar::MessageReceived(msg);
			break;
	}
}


void
_BMCMenuBar_::MakeFocus(bool focused)
{
	if (IsFocus() == focused)
		return;

	BMenuBar::MakeFocus(focused);

	if (focused) {
		BMessage message('TICK');
		//fRunner = new BMessageRunner(BMessenger(this, NULL, NULL), &message,
		//	50000, -1);
	} else if (fRunner) {
		//delete fRunner;
		fRunner = NULL;
	}

	if (focused)
		return;

	fMenuField->fSelected = false;
	fMenuField->fTransition = true;

	BRect bounds(fMenuField->Bounds());

	fMenuField->Invalidate(BRect(bounds.left, bounds.top, fMenuField->fDivider,
		bounds.bottom));
}


BSize
_BMCMenuBar_::MaxSize()
{
	// The maximum width of a normal BMenuBar is unlimited, but we want it
	// limited.
	BSize size;
	BMenuBar::GetPreferredSize(&size.width, &size.height);
	return BLayoutUtils::ComposeSize(ExplicitMaxSize(), size);
}


_BMCMenuBar_ 
&_BMCMenuBar_::operator=(const _BMCMenuBar_ &)
{
	return *this;
}
