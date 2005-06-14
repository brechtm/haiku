//------------------------------------------------------------------------------
//	Copyright (c) 2001-2005, Haiku
//
//	Permission is hereby granted, free of charge, to any person obtaining a
//	copy of this software and associated documentation files (the "Software"),
//	to deal in the Software without restriction, including without limitation
//	the rights to use, copy, modify, merge, publish, distribute, sublicense,
//	and/or sell copies of the Software, and to permit persons to whom the
//	Software is furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in
//	all copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//	DEALINGS IN THE SOFTWARE.
//
//	File Name:		Font.cpp
//	Author:			DarkWyrm (bpmagic@columbus.rr.com)
//	Description:	Class to manage font-handling capabilities
//------------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>

#include <AppServerLink.h>
#include <Message.h>
#include <PortLink.h>
#include <Rect.h>
#include <ServerProtocol.h>
#include <Shape.h>
#include <String.h>

#include <moreUTF8.h>
#include <truncate_string.h>

#include <Font.h>

//----------------------------------------------------------------------------------------
//		Globals
//----------------------------------------------------------------------------------------

// The actual objects which the globals point to
static BFont sPlainFont;
static BFont sBoldFont;
static BFont sFixedFont;

const BFont *be_plain_font = &sPlainFont;
const BFont *be_bold_font = &sBoldFont;
const BFont *be_fixed_font = &sFixedFont;


extern "C" void
_init_global_fonts()
{
	_font_control_(&sPlainFont,AS_SET_SYSFONT_PLAIN,NULL);
	_font_control_(&sBoldFont,AS_SET_SYSFONT_BOLD,NULL);
	_font_control_(&sFixedFont,AS_SET_SYSFONT_FIXED,NULL);
}


/*!
	\brief Private function originally used by Be. Now used for initialization
	\param font The font to initialize
	\param cmd message code to send to the app_server
	\param data unused
	
	While it is not known what Be used it for, Haiku uses it to initialize the
	three system fonts when the interface kit is initialized when an app starts.
*/

void
_font_control_(BFont *font, int32 cmd, void *data)
{
	if (!font
		|| (cmd != AS_SET_SYSFONT_PLAIN && cmd != AS_SET_SYSFONT_BOLD
			&& cmd != AS_SET_SYSFONT_FIXED)) {
		// this shouldn't ever happen, but just in case....
		printf("DEBUG: Bad parameters in _font_control_()\n");
		return;
	}

	int32 code;
	BPrivate::AppServerLink link;

	link.StartMessage(cmd);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE) {
		// Once again, this shouldn't ever happen, but I want to know about it
		// if it does
		printf("DEBUG: Couldn't initialize font in _font_control()\n");
		return;
	}

	// there really isn't that much data that we need to set for such cases -- most
	// of them need to be set to the defaults. The stuff that can change are family,
	// style/face, size, and height.
	link.Read<uint16>(&font->fFamilyID);
	link.Read<uint16>(&font->fStyleID);
	link.Read<float>(&font->fSize);
	link.Read<uint16>(&font->fFace);
	link.Read<uint32>(&font->fFlags);
}

/*!
	\brief Private function used to replace the R5 hack which sets a system font
	\param which string denoting which font to set
	\param family the new family for the system font
	\param style the new style for the system font
	\param size the size for the system font to have
	
	R5 used a global area offset table to set the system fonts in the Font
	preferences panel. Bleah.
*/
void
_set_system_font_(const char *which, font_family family, font_style style, 
					float size)
{
	if (!which)
		return;
	
	if (!strcmp(which,"plain")
		|| !strcmp(which,"bold")
		|| !strcmp(which,"fixed")) {
		BPrivate::AppServerLink link;

		link.StartMessage(AS_SET_SYSTEM_FONT);
		link.AttachString(which);
		link.AttachString(family);
		link.AttachString(style);
		link.Attach<float>(size);
		link.Flush();
	}
}


/*!
	\brief Returns the number of installed font families
	\return The number of installed font families
*/

int32
count_font_families(void)
{
	int32 code, count;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_COUNT_FONT_FAMILIES);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return -1;

	link.Read<int32>(&count);
	return count;
}


/*!
	\brief Returns the number of styles available for a font family
	\return The number of styles available for a font family
*/

int32
count_font_styles(font_family name)
{
	int32 code, count;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_COUNT_FONT_STYLES);

	if (link.FlushWithReply(code) != B_OK
		&& code != SERVER_TRUE)
		return -1;

	link.Read<int32>(&count);
	return count;
}


/*!
	\brief Retrieves the family name at the specified index
	\param index Unique font identifier code.
	\param name font_family string to receive the name of the family
	\param flags iF non-NULL, the values of the flags IS_FIXED and B_HAS_TUNED_FONT are returned
	\return B_ERROR if the index does not correspond to a font family
*/

status_t
get_font_family(int32 index, font_family *name, uint32 *flags)
{
	// Fix over R5, which does not check for NULL font family names - it just crashes
	if (!name)
		return B_ERROR;

	int32 code;
	BPrivate::AppServerLink link;
	
	link.StartMessage(AS_GET_FAMILY_NAME);
	link.Attach<int32>(index);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return B_ERROR;

	link.Read<font_family>(name);
	
	if (flags)
		link.Read<uint32>(flags);

	return B_OK;
}


/*!
	\brief Retrieves the family name at the specified index
	\param index Unique font identifier code.
	\param name font_family string to receive the name of the family
	\param flags iF non-NULL, the values of the flags IS_FIXED and B_HAS_TUNED_FONT are returned
	\return B_ERROR if the index does not correspond to a font style
*/

status_t
get_font_style(font_family family, int32 index, font_style *name,
	uint32 *flags)
{
	if(!name)
		return B_ERROR;

	int32 code;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_GET_STYLE_NAME);
	link.Attach(family,sizeof(font_family));
	link.Attach<int32>(index);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return B_ERROR;

	link.Read<font_style>(name);
	if (flags) {
		uint16 face;
		link.Read<uint16>(&face);
		link.Read<uint32>(flags);
	}
	return B_OK;
}


/*!
	\brief Retrieves the family name at the specified index
	\param index Unique font identifier code.
	\param name font_family string to receive the name of the family
	\param face recipient of font face value, such as B_REGULAR_FACE
	\param flags iF non-NULL, the values of the flags IS_FIXED and B_HAS_TUNED_FONT are returned
	\return B_ERROR if the index does not correspond to a font style
	
	The face value returned by this function is not very reliable. At the same time, the value
	returned should be fairly reliable, returning the proper flag for 90%-99% of font names.
*/

status_t
get_font_style(font_family family, int32 index, font_style *name,
	uint16 *face, uint32 *flags)
{
	if(!name || !face)
		return B_ERROR;

	int32 code;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_GET_STYLE_NAME);
	link.Attach(family, sizeof(font_family));
	link.Attach<int32>(index);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return B_ERROR;

	link.Read<font_style>(name);
	link.Read<uint16>(face);
	if (flags)
		link.Read<uint32>(flags);

	return B_OK;
}


/*!
	\brief Updates the font family list
	\param check_only If true, the function only checks to see if the font list has changed
	\return true if the font list has changed, false if not.
*/

bool
update_font_families(bool check_only)
{
	int32 code;
	bool value;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_QUERY_FONTS_CHANGED);
	link.Attach<bool>(check_only);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return false;

	link.Read<bool>(&value);
	return value;
}


status_t
get_font_cache_info(uint32 id, void *set)
{
	// TODO: Implement

	// Note that the only reliable data from this function will probably be the cache size
	// Depending on how the font cache is implemented, this function and the corresponding
	// set function will either see major revision or completely disappear in R2.
	return B_ERROR;
}


status_t
set_font_cache_info(uint32 id, void *set)
{
	// TODO: Implement

	// Note that this function will likely only set the cache size in our implementation
	// because of (a) the lack of knowledge on R5's font system and (b) the fact that it
	// is a completely different font engine.
	return B_ERROR;
}


//----------------------------------------------------------------------------------------
//		BFont Class Definition
//----------------------------------------------------------------------------------------


BFont::BFont(void) 
	:
	// initialise for be_plain_font (avoid circular definition)
	fFamilyID(0),
	fStyleID(0),
	fSize(10.0),
	fShear(90.0),
	fRotation(0.0),
	fSpacing(0),
	fEncoding(0),
	fFace(0),
	fFlags(0)
{
	fHeight.ascent = 7.0;
	fHeight.descent = 2.0;
	fHeight.leading = 13.0;
 	
	fFamilyID = be_plain_font->fFamilyID;
	fStyleID = be_plain_font->fStyleID;
	fSize = be_plain_font->fSize;
}


BFont::BFont(const BFont &font)
{
	fFamilyID = font.fFamilyID;
	fStyleID = font.fStyleID;
	fSize = font.fSize;
	fShear = font.fShear;
	fRotation = font.fRotation;
	fSpacing = font.fSpacing;
	fEncoding = font.fEncoding;
	fFace = font.fFace;
	fHeight = font.fHeight;
}


BFont::BFont(const BFont *font)
{
	if (font) {
		fFamilyID = font->fFamilyID;
		fStyleID = font->fStyleID;
		fSize = font->fSize;
		fShear = font->fShear;
		fRotation = font->fRotation;
		fSpacing = font->fSpacing;
		fEncoding = font->fEncoding;
		fFace = font->fFace;
		fHeight = font->fHeight;
	} else {
		fFamilyID = be_plain_font->fFamilyID;
		fStyleID = be_plain_font->fStyleID;
		fSize = be_plain_font->fSize;
		fShear = be_plain_font->fShear;
		fRotation = be_plain_font->fRotation;
		fSpacing = be_plain_font->fSpacing;
		fEncoding = be_plain_font->fEncoding;
		fFace = be_plain_font->fFace;
		fHeight = be_plain_font->fHeight;
	}
}


/*!
	\brief Sets the font's family and style all at once
	\param family Font family to set
	\param style Font style to set
	\return B_ERROR if family or style do not exist or if style does not belong to family.
*/

status_t
BFont::SetFamilyAndStyle(const font_family family, const font_style style)
{
	// R5 version always returns B_OK. That's a problem...
	if (!family)
		return B_ERROR;

	int32 code;
	BPrivate::AppServerLink link;

	if (!style) {
		// The BeBook states that a NULL style means set only the family
		link.StartMessage(AS_SET_FAMILY_NAME);
		link.Attach(family, sizeof(font_family));

		if (link.FlushWithReply(code) != B_OK
			|| code != SERVER_TRUE)
			return B_ERROR;

		link.Read<uint16>(&fFamilyID);
	} else {
		link.StartMessage(AS_SET_FAMILY_AND_STYLE);
		link.Attach(family, sizeof(font_family));
		link.Attach(style, sizeof(font_style));

		if (link.FlushWithReply(code) != B_OK
			|| code != SERVER_TRUE)
			return B_ERROR;

		link.Read<uint16>(&fFamilyID);
		link.Read<uint16>(&fStyleID);
	}

	return B_OK;
}


/*!
	\brief Sets the font's family and style all at once
	\param code Unique font identifier obtained from the server.
*/

void
BFont::SetFamilyAndStyle(uint32 fontcode)
{
	// R5 has a bug here: the face is not updated even though the IDs are set. This
	// is a problem because the face flag includes Regular/Bold/Italic information in
	// addition to stuff like underlining and strikethrough. As a result, this will
	// need a trip to the server and, thus, be slower than R5's in order to be correct
	
	uint16 family,style,face;
	int32 code;
	BPrivate::AppServerLink link;

	style = fontcode & 0xFFFF;
	family = (fontcode & 0xFFFF0000) >> 16;

	link.StartMessage(AS_SET_FAMILY_AND_STYLE_FROM_ID);
	link.Attach<uint16>(family);
	link.Attach<uint16>(style);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return;

	link.Read<uint16>(&face);

	fStyleID = style;
	fFamilyID = family;
	
	// Mask off any references in the face to Bold/Normal/Italic and set the face
	// value to reflect the new font style
	fFace &= B_UNDERSCORE_FACE | B_NEGATIVE_FACE | B_OUTLINED_FACE | B_STRIKEOUT_FACE;
	fFace |= face;
}


/*!
	\brief Sets the font's family and face all at once
	\param family Font family to set
	\param face Font face to set.
	\return B_ERROR if family does not exists or face is an invalid value.
	
	To comply with the BeBook, this function will only set valid values - i.e. passing a 
	nonexistent family will cause only the face to be set. Additionally, if a particular 
	face does not exist in a family, the closest match will be chosen.
*/

status_t
BFont::SetFamilyAndFace(const font_family family, uint16 face)
{
	if (face & (B_ITALIC_FACE | B_UNDERSCORE_FACE | B_NEGATIVE_FACE | B_OUTLINED_FACE
			| B_STRIKEOUT_FACE | B_BOLD_FACE | B_REGULAR_FACE) != 0)
		fFace = face;
	
	if (family) {
		int32 code;
		BPrivate::AppServerLink link;

		link.StartMessage(AS_SET_FAMILY_AND_FACE);
		link.Attach(family, sizeof(font_family));
		link.Attach<uint16>(face);

		if (link.FlushWithReply(code) != B_OK
			|| code != SERVER_TRUE)
			return B_ERROR;

		link.Read<uint16>(&fFamilyID);
		link.Read<uint16>(&fStyleID);
	} else
		fFace = face;

	return B_OK;
}


void
BFont::SetSize(float size)
{
	fSize = size;
}


void
BFont::SetShear(float shear)
{
	fShear = shear;
}


void
BFont::SetRotation(float rotation)
{
	fRotation = rotation;
}


void
BFont::SetSpacing(uint8 spacing)
{
	fSpacing = spacing;
}


void
BFont::SetEncoding(uint8 encoding)
{
	fEncoding = encoding;
}


void
BFont::SetFace(uint16 face)
{
	fFace = face;
}


void
BFont::SetFlags(uint32 flags)
{
	fFlags = flags;
}


void
BFont::GetFamilyAndStyle(font_family *family, font_style *style) const
{
	if (!family || !style)
		return;

	int32 code;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_GET_FAMILY_AND_STYLE);
	link.Attach<uint16>(fFamilyID);
	link.Attach<uint16>(fStyleID);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return;

	link.Read<font_family>(family);
	link.Read<font_style>(style);
}


uint32
BFont::FamilyAndStyle(void) const
{
	uint32 token = (fFamilyID << 16) | fStyleID;
	return token;
}


float
BFont::Size(void) const
{
	return fSize;
}


float
BFont::Shear(void) const
{
	return fShear;
}


float
BFont::Rotation(void) const
{
	return fRotation;
}


uint8
BFont::Spacing(void) const
{
	return fSpacing;
}


uint8
BFont::Encoding(void) const
{
	return fEncoding;
}


uint16
BFont::Face(void) const
{
	return fFace;
}


uint32
BFont::Flags(void) const
{
	return fFlags;
}


font_direction
BFont::Direction(void) const
{
	int32 code;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_GET_FONT_DIRECTION);
	link.Attach<uint16>(fFamilyID);
	link.Attach<uint16>(fStyleID);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return B_FONT_LEFT_TO_RIGHT;

	font_direction fdir;
	link.Read<font_direction>(&fdir);
	return fdir;
}
 

bool
BFont::IsFixed(void) const
{
	int32 code;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_QUERY_FONT_FIXED);
	link.Attach<uint16>(fFamilyID);
	link.Attach<uint16>(fStyleID);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return false;

	bool fixed;
	link.Read<bool>(&fixed);
	return fixed;
}


/*!
	\brief Returns true if the font is fixed-width and contains both full and half-width characters
	
	This was left unimplemented as of R5. It was a way to work with both Kanji and Roman 
	characters in the same fixed-width font.
*/

bool
BFont::IsFullAndHalfFixed(void) const
{
	return false;
}


BRect
BFont::BoundingBox(void) const
{
	int32 code;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_GET_FONT_BOUNDING_BOX);
	link.Attach<uint16>(fFamilyID);
	link.Attach<uint16>(fStyleID);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return BRect(0, 0, 0 ,0);

	BRect box;
	link.Read<BRect>(&box);
	return box;
}


unicode_block
BFont::Blocks(void) const
{
	// TODO: Add Block support
	return unicode_block();
}


font_file_format
BFont::FileFormat(void) const
{
	// TODO: this will not work until I extend FreeType to handle this kind of call
	return B_TRUETYPE_WINDOWS;
}


int32
BFont::CountTuned(void) const
{
	int32 code;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_GET_TUNED_COUNT);
	link.Attach<uint16>(fFamilyID);
	link.Attach<uint16>(fStyleID);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return -1;

	int32 count;
	link.Read<int32>(&count);
	return count;
}


void
BFont::GetTunedInfo(int32 index, tuned_font_info *info) const
{
	if (!info)
		return;

	int32 code;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_GET_TUNED_INFO);
	link.Attach<uint16>(fFamilyID);
	link.Attach<uint16>(fStyleID);
	link.Attach<uint32>(index);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return;

	link.Read<tuned_font_info>(info);
}

// TruncateString
void
BFont::TruncateString(BString *inOut, uint32 mode, float width) const
{
	// NOTE: Careful, we cannot directly use "inOut->String()" as result
	// array, because the string length increases by 3 bytes in the worst case scenario.
	const char* array[1];
	array[0] = inOut->String();
	GetTruncatedStrings(array, 1, mode, width, inOut);
}

// GetTruncatedStrings
void
BFont::GetTruncatedStrings(const char *stringArray[], int32 numStrings, 
	uint32 mode, float width, BString resultArray[]) const
{
	if (stringArray && resultArray && numStrings > 0) {
		// allocate storage, see BeBook for "+ 3"
		char** truncatedStrings = new char*[numStrings];
		for (int32 i = 0; i < numStrings; i++) {
			truncatedStrings[i] = new char[strlen(stringArray[i]) + 3];
		}
	
		GetTruncatedStrings(stringArray, numStrings, mode, width, truncatedStrings);
	
		// copy the strings into the BString array and free each one
		for (int32 i = 0; i < numStrings; i++) {
			resultArray[i].SetTo(truncatedStrings[i]);
			delete[] truncatedStrings[i];
		}
		delete[] truncatedStrings;
	}
}

// GetTruncatedStrings
void
BFont::GetTruncatedStrings(const char *stringArray[], int32 numStrings, 
	uint32 mode, float width, char *resultArray[]) const
{
	if (stringArray && numStrings > 0) {
		// the width of the "…" glyph
		float ellipsisWidth = StringWidth(B_UTF8_ELLIPSIS);
		for (int32 i = 0; i < numStrings; i++) {
			int32 length = strlen(stringArray[i]);
			// count the individual glyphs
			int32 numChars = UTF8CountChars(stringArray[i], length);
			// get the escapement of each glyph in font units
			float* escapementArray = new float[numChars];
			GetEscapements(stringArray[i], numChars, NULL, escapementArray);

			truncate_string(stringArray[i], mode, width, resultArray[i],
							escapementArray, fSize, ellipsisWidth, length, numChars);

			delete[] escapementArray;
		}
	}
}

// StringWidth
float
BFont::StringWidth(const char *string) const
{
	int32 length = strlen(string);
 	return StringWidth(string, length);
}


float
BFont::StringWidth(const char *string, int32 length) const
{
	if (!string || length < 1)
		return 0.0;
	
	int32 code;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_GET_STRING_WIDTH);
	link.AttachString(string);
	link.Attach<int32>(length);
	link.Attach<uint16>(fFamilyID);
	link.Attach<uint16>(fStyleID);
	link.Attach<float>(fSize);
	link.Attach<uint8>(fSpacing);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return 0.0;

	float width;
	link.Read<float>(&width);
	return width;
}


void
BFont::GetStringWidths(const char *stringArray[], const int32 lengthArray[], 
	int32 numStrings, float widthArray[]) const
{
	if (!stringArray || !lengthArray || numStrings < 1 || !widthArray)
		return;

	int32 code;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_GET_STRING_WIDTHS);
	link.Attach<int32>(numStrings);

	for (int32 i = 0; i < numStrings; i++) {
		link.AttachString(stringArray[i]);
		link.Attach<int32>(lengthArray[i]);
	}

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return;

	link.Read(widthArray, sizeof(float) * numStrings);
}


void
BFont::GetEscapements(const char charArray[], int32 numChars, float escapementArray[]) const
{
	GetEscapements(charArray, numChars, NULL, escapementArray);
}


void
BFont::GetEscapements(const char charArray[], int32 numChars, escapement_delta *delta, 
	float escapementArray[]) const
{
	if (!charArray ||  numChars < 1 || !escapementArray)
		return;

	// NOTE: The R5 implementation crashes if delta == NULL!

	int32 code;
	BPrivate::AppServerLink link;
	
	link.StartMessage(AS_GET_ESCAPEMENTS_AS_FLOATS);
	link.Attach<uint16>(fFamilyID);
	link.Attach<uint16>(fStyleID);
	link.Attach<float>(fSize);
	link.Attach<float>(fRotation);
	link.Attach<uint32>(fFlags);

	link.Attach<float>(delta ? delta->nonspace : 0.0);
	link.Attach<float>(delta ? delta->space : 0.0);
	
	// TODO: Should we not worry about the port capacity here?!?
	link.Attach<int32>(numChars);

	uint32 bytesInBuffer = UTF8CountBytes(charArray, numChars);
	link.Attach<int32>(bytesInBuffer);
	link.Attach(charArray, bytesInBuffer);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return;

	link.Read(escapementArray, numChars * sizeof(float));
}


void
BFont::GetEscapements(const char charArray[], int32 numChars, escapement_delta *delta, 
	BPoint escapementArray[]) const
{
	GetEscapements(charArray, numChars, delta, escapementArray, NULL);
}


void
BFont::GetEscapements(const char charArray[], int32 numChars, escapement_delta *delta, 
	BPoint escapementArray[], BPoint offsetArray[]) const
{
	if (!charArray ||  numChars<1 || !escapementArray)
		return;

	int32 code;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_GET_ESCAPEMENTS);
	link.Attach<uint16>(fFamilyID);
	link.Attach<uint16>(fStyleID);
	link.Attach<float>(fSize);
	link.Attach<float>(fRotation);
	link.Attach<uint32>(fFlags);

	link.Attach<int32>(numChars);

	// TODO: Support UTF8 characters
	if (offsetArray) {
		for (int32 i = 0; i < numChars; i++) {
			link.Attach<char>(charArray[i]);
			link.Attach<BPoint>(offsetArray[i]);
		}
	} else {
		BPoint dummypt(0, 0);

		for (int32 i = 0; i < numChars; i++) {
			link.Attach<char>(charArray[i]);
			link.Attach<BPoint>(dummypt);
		}
	}

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return;

	link.Read(escapementArray, sizeof(BPoint) * numChars);
}


void
BFont::GetEdges(const char charArray[], int32 numBytes, edge_info edgeArray[]) const
{
	if (!charArray || numBytes < 1 || !edgeArray)
		return;

	int32 code;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_GET_EDGES);
	link.Attach<int32>(numBytes);
	link.Attach(charArray, numBytes);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return;

	link.Read(edgeArray, sizeof(edge_info) * numBytes);
}


void
BFont::GetHeight(font_height *height) const
{
	if (height) {
		// R5's version actually contacts the server in this call. The more and more
		// I work with this class, the more and more I can't wait for R2 to fix it. Yeesh.
		int32 code;
		BPrivate::AppServerLink link;

		link.StartMessage(AS_GET_FONT_HEIGHT);
		link.Attach<uint16>(fFamilyID);
		link.Attach<uint16>(fStyleID);
		link.Attach<float>(fSize);

		if (link.FlushWithReply(code) != B_OK
			|| code != SERVER_TRUE)
			return;

		link.Read<font_height>(height);
	}
}


void
BFont::GetBoundingBoxesAsGlyphs(const char charArray[], int32 numChars, font_metric_mode mode,
	BRect boundingBoxArray[]) const
{
	GetBoundingBoxesAsString(charArray,numChars,mode,NULL,boundingBoxArray);
}


void
BFont::GetBoundingBoxesAsString(const char charArray[], int32 numChars, font_metric_mode mode,
	escapement_delta *delta, BRect boundingBoxArray[]) const
{
	if (!charArray || numChars < 1 || !boundingBoxArray)
		return;

	int32 code;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_GET_BOUNDINGBOXES_CHARS);
	link.Attach<font_metric_mode>(mode);

	if (delta) {
		link.Attach<escapement_delta>(*delta);
	} else {
		escapement_delta emptyDelta = {0, 0};
		link.Attach<escapement_delta>(emptyDelta);
	}

	link.Attach<int32>(numChars);
	link.Attach(charArray, numChars);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return;

	link.Read(boundingBoxArray, sizeof(BRect) * numChars);
}


void
BFont::GetBoundingBoxesForStrings(const char *stringArray[], int32 numStrings,
	font_metric_mode mode, escapement_delta deltas[], BRect boundingBoxArray[]) const
{
	if (!stringArray || numStrings < 1 || !boundingBoxArray)
		return;

	int32 code;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_GET_BOUNDINGBOXES_STRINGS);
	link.Attach<font_metric_mode>(mode);
	link.Attach<int32>(numStrings);

	if (deltas) {
		for (int32 i = 0; i < numStrings; i++) {
			link.AttachString(stringArray[i]);
			link.Attach<escapement_delta>(deltas[i]);
		}
	} else {
		escapement_delta emptyDelta = {0, 0};

		for (int32 i = 0; i < numStrings; i++) {
			link.AttachString(stringArray[i]);
			link.Attach<escapement_delta>(emptyDelta);
		}
	}

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return;

	link.Read(boundingBoxArray, sizeof(BRect) * numStrings);
}


void
BFont::GetGlyphShapes(const char charArray[], int32 numChars, BShape *glyphShapeArray[]) const
{
	// TODO: implement code specifically for passing BShapes to and from the server
	if (!charArray || numChars < 1 || !glyphShapeArray)
		return;
	
	int32 code;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_GET_GLYPH_SHAPES);
	link.Attach<uint16>(fFamilyID);
	link.Attach<uint16>(fStyleID);
	link.Attach<float>(fSize);
	link.Attach<float>(fShear);
	link.Attach<float>(fRotation);
	link.Attach<uint32>(fFlags);
	
	link.Attach<int32>(numChars);
	link.Attach(charArray, numChars);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return;

	for (int32 i = 0; i < numChars; i++)
		link.ReadShape(glyphShapeArray[i]);
}


void
BFont::GetHasGlyphs(const char charArray[], int32 numChars, bool hasArray[]) const
{
	if (!charArray || numChars < 1 || !hasArray)
		return;
	
	int32 code;
	BPrivate::AppServerLink link;

	link.StartMessage(AS_GET_HAS_GLYPHS);
	link.Attach<int32>(numChars);
	link.Attach(charArray, numChars);

	if (link.FlushWithReply(code) != B_OK
		|| code != SERVER_TRUE)
		return;

	link.Read(hasArray, sizeof(bool) * numChars);
}


BFont
&BFont::operator=(const BFont &font)
{
	fFamilyID = font.fFamilyID;
	fStyleID = font.fStyleID;
	fSize = font.fSize;
	fShear = font.fShear;
	fRotation = font.fRotation;
	fSpacing = font.fSpacing;
	fEncoding = font.fEncoding;
	fFace = font.fFace;
	fHeight = font.fHeight;
	return *this;
}


bool
BFont::operator==(const BFont &font) const
{
	return fFamilyID == font.fFamilyID
		&& fStyleID == font.fStyleID
		&& fSize == font.fSize
		&& fShear == font.fShear
		&& fRotation == font.fRotation
		&& fSpacing == font.fSpacing
		&& fEncoding == font.fEncoding
		&& fFace == font.fFace;
}


bool
BFont::operator!=(const BFont &font) const
{
	return fFamilyID != font.fFamilyID
		|| fStyleID != font.fStyleID
		|| fSize != font.fSize
		|| fShear != font.fShear
		|| fRotation != font.fRotation
		|| fSpacing != font.fSpacing
		|| fEncoding != font.fEncoding
		|| fFace != font.fFace;
}


void
BFont::PrintToStream(void) const
{
	printf("FAMILY STYLE %f %f %f %f %f %f\n", fSize, fShear, fRotation, fHeight.ascent,
		fHeight.descent, fHeight.leading);
}

