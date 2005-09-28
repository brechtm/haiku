/*
 * Copyright 2005, Jérôme DUVAL. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#include <fs_attr.h>
#include <Directory.h>
#include <ScrollBar.h>
#include <String.h>
#include <stdio.h>
#include <View.h>
#include "PackageViews.h"

#define ICON_ATTRIBUTE "INSTALLER PACKAGE: ICON"

static void
SizeAsString(int32 size, char *string)
{
	float kb = size / 1024.0;
	if (kb < 1.0) {
		sprintf(string, "%ld B", size);
		return;
	}
	float mb = kb / 1024.0;
	if (mb < 1.0) {
		sprintf(string, "%3.1f KB", kb);
		return;
	}
	sprintf(string, "%3.1f MB", mb);
}


Package::Package()
	: Group(),
	fName(NULL),
	fDescription(NULL),
	fSize(0),
	fIcon(NULL)
{

}


Package::~Package()
{
	free(fName);
	free(fDescription);
	delete fIcon;	
}


Package *
Package::PackageFromEntry(BEntry &entry)
{
	BDirectory directory(&entry);
	if (directory.InitCheck()!=B_OK)
		return NULL;
	Package *package = new Package();
	char name[255];
	char group[255];
	char description[255];
	bool alwaysOn;
	bool onByDefault;
	int32 size;
	if (directory.ReadAttr("INSTALLER PACKAGE: NAME", B_STRING_TYPE, 0, name, 255)<0)
		goto err;
	if (directory.ReadAttr("INSTALLER PACKAGE: GROUP", B_STRING_TYPE, 0, group, 255)<0)
		goto err;
	if (directory.ReadAttr("INSTALLER PACKAGE: DESCRIPTION", B_STRING_TYPE, 0, description, 255)<0)
		goto err;
	if (directory.ReadAttr("INSTALLER PACKAGE: ON_BY_DEFAULT", B_BOOL_TYPE, 0, &onByDefault, sizeof(onByDefault))<0)
		goto err;
	if (directory.ReadAttr("INSTALLER PACKAGE: ALWAYS_ON", B_BOOL_TYPE, 0, &alwaysOn, sizeof(alwaysOn))<0)
		goto err;
	if (directory.ReadAttr("INSTALLER PACKAGE: SIZE", B_INT32_TYPE, 0, &size, sizeof(size))<0)
		goto err;
	package->SetName(name);
	package->SetGroupName(group);
	package->SetDescription(description);
	package->SetSize(size);
	package->SetAlwaysOn(alwaysOn);
	package->SetOnByDefault(onByDefault);

	attr_info info;
	if (directory.GetAttrInfo(ICON_ATTRIBUTE, &info) == B_OK) {
		char buffer[info.size];
		BMessage msg;
		if ((directory.ReadAttr(ICON_ATTRIBUTE, info.type, 0, buffer, info.size) == info.size)
			&& (msg.Unflatten(buffer) == B_OK)) {
			package->SetIcon(new BBitmap(&msg));
		}
	}
	return package;
err:
	delete package;
	return NULL;
}


void 
Package::GetSizeAsString(char *string)
{
	SizeAsString(fSize, string);
}


Group::Group()
	: fGroup(NULL)
{

}

Group::~Group()
{
	free(fGroup);
}


PackageCheckBox::PackageCheckBox(BRect rect, Package &item) 
	: BCheckBox(rect.OffsetBySelf(7,0), "pack_cb", item.Name(), NULL),
	fPackage(item)
{
}


PackageCheckBox::~PackageCheckBox()
{
}


void 
PackageCheckBox::Draw(BRect update)
{
	BCheckBox::Draw(update);
	char string[15];
	fPackage.GetSizeAsString(string);
	float width = StringWidth(string);
	DrawString(string, BPoint(Bounds().right - width - 8, 11));

	const BBitmap *icon = fPackage.Icon();
	if (icon)
		DrawBitmap(icon, BPoint(Bounds().right - 92, 0));
}

GroupView::GroupView(BRect rect, Group &group)
	: BStringView(rect, "group", group.GroupName()),
	fGroup(group)
{
	SetFont(be_bold_font);
}


GroupView::~GroupView()
{
}


PackagesView::PackagesView(BRect rect, const char* name)
	: BView(rect, name, B_FOLLOW_ALL_SIDES, B_WILL_DRAW|B_FRAME_EVENTS)
{
}


PackagesView::~PackagesView()
{

}


void
PackagesView::Clean()
{
	
}


void
PackagesView::AddPackages(BList &packages, BMessage *msg)
{
	int32 count = packages.CountItems();
	BRect rect = Bounds();
	BRect bounds = rect;
	rect.left = 1;
	rect.bottom = 15;
	BString lastGroup = "";
	for (int32 i=0; i<count; i++) {
		void *item = packages.ItemAt(i);
		Package *package = static_cast<Package *> (item);
		if (lastGroup != BString(package->GroupName())) {
			rect.OffsetBy(0, 1);
			lastGroup = package->GroupName();
			Group *group = new Group();
			group->SetGroupName(package->GroupName());
			GroupView *view = new GroupView(rect, *group);
			AddChild(view);
			rect.OffsetBy(0, 17);
		}
		PackageCheckBox *checkBox = new PackageCheckBox(rect, *package);
		checkBox->SetValue(package->OnByDefault() ? B_CONTROL_ON : B_CONTROL_OFF);
		checkBox->SetEnabled(!package->AlwaysOn());
		checkBox->SetMessage(msg);
		AddChild(checkBox);
		rect.OffsetBy(0, 20);
	}
	ResizeTo(bounds.Width(), rect.bottom);

	BScrollBar *vertScroller = ScrollBar(B_VERTICAL);

	if (bounds.Height() > rect.top) {
		vertScroller->SetRange(0.0f, 0.0f);
		vertScroller->SetValue(0.0f);
	} else {
		vertScroller->SetRange(0.0f, rect.top - bounds.Height());
		vertScroller->SetProportion(bounds.Height () / rect.top);
        }

	vertScroller->SetSteps(15, bounds.Height());
	
	Invalidate();
}


void 
PackagesView::GetTotalSizeAsString(char *string)
{
	int32 count = CountChildren();
	int32 size = 0;
	for (int32 i=0; i<count; i++) {
		PackageCheckBox *cb = dynamic_cast<PackageCheckBox*>(ChildAt(i));
		if (cb && cb->Value())
			size += cb->GetPackage()->Size();
	}
	SizeAsString(size, string);
}
