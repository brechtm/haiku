/*
 * Copyright 2004-2007, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Mike Berg <mike@berg-net.us>
 *		Julun <host.haiku@gmx.de>
 */

/*
		Exceptions:
			doesn't calc "Time in" time.

		Issues:
			After experimenting with both Time Prefs, it seems the original
			doesn't use the link file in the users settings file to get the
			current Timezone. Need to find the call it uses to get its
			inital info so I can get exact duplication.
*/

#include "ZoneView.h"
#include "TimeMessages.h"
#include "TZDisplay.h"


#include <Button.h>
#include <Directory.h>
#include <Entry.h>
#include <FindDirectory.h>
#include <ListItem.h>
#include <ListView.h>
#include <MenuField.h>
#include <MenuItem.h>
#include <MenuBar.h>
#include <PopUpMenu.h>
#include <ScrollView.h>
#include <StorageDefs.h>
#include <String.h>
#include <View.h>


#include <stdio.h>
#include <stdlib.h>


class TZoneItem : public BStringItem {
	public:
					TZoneItem(const char *text, const char *zone)
						: BStringItem(text), fZone(new BPath(zone)) { }

					~TZoneItem() {	delete fZone;	}
		
		const char 	*Zone() const	{	return fZone->Leaf();	}
		const char 	*Path() const	{	return fZone->Path();	}
		
	private:
		BPath 		*fZone;
};


TimeZoneView::TimeZoneView(BRect frame)
	: BView(frame, "timeZoneView", B_FOLLOW_NONE, B_WILL_DRAW | B_NAVIGABLE_JUMP),
	  fInitialized(false)
{
	ReadTimeZoneLink();
	InitView();
}


TimeZoneView::~TimeZoneView()
{
}


void
TimeZoneView::AttachedToWindow()
{
	if (Parent())
		SetViewColor(Parent()->ViewColor());

	if (!fInitialized) {
		fInitialized = true;

		fSetZone->SetTarget(this);
		fCityList->SetTarget(this);
		fRegionPopUp->SetTargetForItems(this);

		// update displays	
		BPath parent;
		fCurrentZone.GetParent(&parent);
		int32 czone = FillCityList(parent.Path());
		if (czone > -1) {
			fCityList->Select(czone);
			fCurrent->SetText(((TZoneItem *)fCityList->ItemAt(czone))->Text());
		}
	}
	fCityList->ScrollToSelection();
}


void
TimeZoneView::MessageReceived(BMessage *message)
{
	int32 change;
	switch(message->what) {
		case B_OBSERVER_NOTICE_CHANGE:
			message->FindInt32(B_OBSERVE_WHAT_CHANGE, &change);
			switch(change) {
				case H_TM_CHANGED:
					UpdateDateTime(message);
					break;
				
				default:
					BView::MessageReceived(message);
					break;
			}
			break;
		case H_REGION_CHANGED:
			ChangeRegion(message);
			break;
		
		case H_SET_TIME_ZONE:
			SetTimeZone();
			break;
		
		case H_CITY_CHANGED:
			SetPreview();
			break;	
		
		default:
			BView::MessageReceived(message);
			break;
	}
}


void
TimeZoneView::UpdateDateTime(BMessage *message)
{
	int32 hour;
	int32 minute;

	// only need hour and minute
	if (message->FindInt32("hour", &hour) == B_OK
		&& message->FindInt32("minute", &minute) == B_OK) {
		if (fHour != hour || fMinute != minute) {
			fHour = hour;
			fMinute = minute;
			fCurrent->SetTime(hour, minute);

			// do calc to get other zone time
			if (fCityList->CurrentSelection() > -1)
				SetPreview();
		}
	}
}


void
TimeZoneView::InitView()
{
	// Zone menu
	fRegionPopUp = new BPopUpMenu("", true, true, B_ITEMS_IN_COLUMN);

	BuildRegionMenu();

	// left side
	BRect frameLeft(Bounds());
	frameLeft.right = frameLeft.Width() / 2.0;
	frameLeft.InsetBy(10.0f, 10.0f);
	
	BMenuField *menuField = new BMenuField(frameLeft, "regions", NULL, fRegionPopUp, false);
	AddChild(menuField);
	menuField->ResizeToPreferred();

	frameLeft.top = menuField->Frame().bottom + 10.0;
	frameLeft.right -= B_V_SCROLL_BAR_WIDTH;

	// City Listing
	fCityList = new BListView(frameLeft, "cityList", B_SINGLE_SELECTION_LIST);
	fCityList->SetSelectionMessage(new BMessage(H_CITY_CHANGED));
	fCityList->SetInvocationMessage(new BMessage(H_SET_TIME_ZONE));
	
	BScrollView *scrollList = new BScrollView("scrollList", fCityList,
		B_FOLLOW_ALL, 0, false, true);
	AddChild(scrollList);
	
	// right side
	BRect frameRight(Bounds());
	frameRight.left = frameRight.Width() / 2.0;
	frameRight.InsetBy(10.0f, 10.0f);
	frameRight.top = frameLeft.top;

	// Time Displays
	fCurrent = new TTZDisplay(frameRight, "currentTime", "Current time:");
	AddChild(fCurrent);
	fCurrent->ResizeToPreferred();
	
	frameRight.top = fCurrent->Frame().bottom + 10.0;
	fPreview = new TTZDisplay(frameRight, "previewTime", "Preview time:");
	AddChild(fPreview);
	fPreview->ResizeToPreferred();
	
	// set button
	fSetZone = new BButton(frameRight, "setTimeZone", "Set Time Zone", 
		new BMessage(H_SET_TIME_ZONE));
	AddChild(fSetZone);
	fSetZone->SetEnabled(false);
	fSetZone->ResizeToPreferred();

	fSetZone->MoveTo(frameRight.right - fSetZone->Bounds().Width(),
		scrollList->Frame().bottom - fSetZone->Bounds().Height()); 
}


void
TimeZoneView::BuildRegionMenu()
{
	BPath path;
	if (find_directory(B_BEOS_ETC_DIRECTORY, &path) != B_OK)
		return;

	path.Append("timezones");

	// get current region
	BPath region;
	fCurrentZone.GetParent(&region);
	
	bool markit;
	BEntry entry;
	BMenuItem *item;
	BDirectory dir(path.Path());

	dir.Rewind();

	// walk timezones dir and add entries to menu
	BString itemtext;
	while (dir.GetNextEntry(&entry) == B_NO_ERROR) {
		if (entry.IsDirectory()) {
			path.SetTo(&entry);
			itemtext = path.Leaf();
			
			// skip Etc directory
			if (itemtext.Compare("Etc", 3) == 0)
			 	continue;
			 
			markit = itemtext.Compare(region.Leaf()) == 0;
			
			// add Ocean to oceans :)
			if (itemtext.Compare("Atlantic", 8) == 0
				 ||itemtext.Compare("Pacific", 7) == 0
				 ||itemtext.Compare("Indian", 6) == 0)
				itemtext.Append(" Ocean");

			// underscores are spaces
			itemtext = itemtext.ReplaceAll('_', ' ');
			
			BMessage *msg = new BMessage(H_REGION_CHANGED);
			msg->AddString("region", path.Path());

			item = new BMenuItem(itemtext.String(), msg);
			item->SetMarked(markit);
			fRegionPopUp->AddItem(item);
		}
	}
}


int32
TimeZoneView::FillCityList(const char *area)
{
	// clear list
	int32 count = fCityList->CountItems();
	if (count > 0) {
		for (int32 idx = count; idx >= 0; idx--)
			delete fCityList->RemoveItem(idx);
		fCityList->MakeEmpty();
	}

	// Enter time zones directory. Find subdir with name that matches string
	// stored in area. Enter subdirectory and count the items. For each item,
	// add a StringItem to fCityList Time zones directory 

	BPath path;
	if (find_directory(B_BEOS_ETC_DIRECTORY, &path) != B_OK)
		return 0;

	path.Append("timezones");

	BPath Area(area);
 	BDirectory zoneDir(path.Path()); 
 	BDirectory cityDir;
 	BStringItem *city;
 	BString city_name;
 	BEntry entry;
	int32 index = -1; 
	
	//locate subdirectory:
	if (zoneDir.Contains(Area.Leaf(), B_DIRECTORY_NODE)) {
		cityDir.SetTo(&zoneDir, Area.Leaf());  

		// There is a subdir with a name that matches 'area'. That's the one!!
		// Iterate over the items in the subdir, fill listview with TZoneItems
		while(cityDir.GetNextEntry(&entry) == B_NO_ERROR) {
			if (!entry.IsDirectory()) {
				BPath zone(&entry);
				city_name = zone.Leaf();
				city_name.ReplaceAll("_IN", ", Indiana");
				city_name.ReplaceAll("__Calif", ", Calif");
				city_name.ReplaceAll("__", ", ");
				city_name.ReplaceAll("_", " ");
				city = new TZoneItem(city_name.String(), zone.Path());
				fCityList->AddItem(city);
				if (strcmp(fCurrentZone.Leaf(), zone.Leaf()) == 0)
					index = fCityList->IndexOf(city);
			}
		}
	}
	return index;
}


void
TimeZoneView::ChangeRegion(BMessage *message)
{
	BString area;
	message->FindString("region", &area);

	FillCityList(area.String());
}


void
TimeZoneView::ReadTimeZoneLink()
{
	BEntry tzLink;

#if TARGET_PLATFORM_HAIKU
	extern status_t _kern_get_tzfilename(char *filename, size_t length, bool *_isGMT);
	
	char tzFileName[B_OS_PATH_LENGTH];
	bool isGMT;
	_kern_get_tzfilename(tzFileName, B_OS_PATH_LENGTH, &isGMT);
	tzLink.SetTo(tzFileName);
#else
	/*	reads the timezone symlink from B_USER_SETTINGS_DIRECTORY currently
		this sets fCurrentZone to the symlink value, this is wrong. The 
		original can get users timezone without a timezone symlink present.  
		
		Defaults are set to different values to clue in on what error was returned
	  	GMT is set when the link is invalid EST is set when the settings dir can't 
		be found what should never happen.
	*/
	
 	
 	BPath path;
 	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) == B_OK) {

 		path.Append("timezone");
		tzLink.SetTo(path.Path(), true);

		if (tzLink.InitCheck() != B_OK) {// link is broken
			tzLink.SetTo("/boot/beos/etc/timezones/Pacific/fiji");
			// do something like set to a default GMT???
		}
 	} else {
 		// set tzlink to a default
 		tzLink.SetTo("/boot/beos/etc/timezones/EST");
 	}
#endif
	// we need something in the current zone
	fCurrentZone.SetTo(&tzLink);
}


void
TimeZoneView::SetPreview()
{
	int32 selection = fCityList->CurrentSelection();
	if (selection >= 0) {
		TZoneItem *item = (TZoneItem *)fCityList->ItemAt(selection);

		// set timezone to selection
		SetTimeZone(item->Path());

		// calc preview time
		time_t current = time(0);
		struct tm *ltime = localtime(&current);
		
		// update prview
		fPreview->SetText(item->Text());
		fPreview->SetTime(ltime->tm_hour, ltime->tm_min);

		// set timezone back to current
		SetTimeZone(fCurrentZone.Path());
		
		fSetZone->SetEnabled((strcmp(fCurrent->Text(), item->Text()) != 0));
	}
}


void
TimeZoneView::SetCurrent(const char *text)
{
	SetTimeZone(fCurrentZone.Path());
	
	time_t current = time(0);
	struct tm *ltime = localtime(&current);
	
	fCurrent->SetText(text);
	fCurrent->SetTime(ltime->tm_hour, ltime->tm_min);
}


void
TimeZoneView::SetTimeZone()
{
	/*	set time based on supplied timezone. How to do this?
		1) replace symlink "timezone" in B_USER_SETTINGS_DIR with a link to the new timezone
		2) set TZ environment var
		3) call settz()
		4) call set_timezone from OS.h passing path to timezone file
	*/
	
	// get path to current link
	// update/create timezone symlink in B_USER_SETTINGS_DIRECTORY
	BPath path;
	if (find_directory(B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
		return;
		
	path.Append("timezone");
	
	// build target for new link
	int32 selection = fCityList->CurrentSelection();
	if (selection < 0)
		return;
	
	BPath target(((TZoneItem *)fCityList->ItemAt(selection))->Path());
	
	// remove old
	BEntry entry(path.Path());
	if (entry.Exists())
		entry.Remove();
	
	// create new
	BDirectory dir(target.Path());
	if (dir.CreateSymLink(path.Path(), target.Path(), NULL) != B_OK)
		fprintf(stderr, "timezone not linked\n");

	// update environment
	SetTimeZone(target.Path());

	// update display
	time_t current = time(0);
	struct tm *ltime = localtime(&current);
	
	char tza[B_PATH_NAME_LENGTH];
	sprintf(tza, "%s", target.Path());
	set_timezone(tza);
	
	// disable button
	fSetZone->SetEnabled(false);

	time_t newtime = mktime(ltime);
	ltime = localtime(&newtime);
	stime(&newtime);

	fHour = ltime->tm_hour;
	fMinute = ltime->tm_min;
	fCurrentZone.SetTo(target.Path());
	SetCurrent(((TZoneItem *)fCityList->ItemAt(selection))->Text());
}	


void
TimeZoneView::SetTimeZone(const char *zone)
{
	putenv(BString("TZ=").Append(zone).String());
	tzset();
}

