/*
 * Copyright (c) 2003-4 Kian Duffy <myob@users.sourceforge.net>
 * Parts Copyright (C) 1998,99 Kazuho Okui and Takashi Murai. 
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files or portions
 * thereof (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice
 *    in the  binary, as well as this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided with
 *    the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "CurPos.h"



/*************************************************************************
 *
 * Constructor and Destructor
 *
 ************************************************************************/

CurPos::CurPos()
{
}


CurPos::CurPos(int32 X, int32 Y)
{
  x = X;
  y = Y;
}


CurPos::CurPos(const CurPos& cp)
{
  x = cp.x;
  y = cp.y;
}

////////////////////////////////////////////////////////////////////////////
// CurPos
//	Operator =
////////////////////////////////////////////////////////////////////////////
CurPos &
CurPos::operator=(const CurPos& from)
{
  x = from.x;
  y = from.y;
  return *this;
}

////////////////////////////////////////////////////////////////////////////
// CurPos
//	Set Function.
////////////////////////////////////////////////////////////////////////////
void
CurPos::Set(int32 X, int32 Y)
{
  x = X;
  y = Y;
}

////////////////////////////////////////////////////////////////////////////
// CurPos
//	Operator !=
////////////////////////////////////////////////////////////////////////////
bool
CurPos::operator!=(const CurPos& from) const
{
  return ((x != from.x) || (y != from.y));
}

////////////////////////////////////////////////////////////////////////////
// CurPos
//	Operator ==
////////////////////////////////////////////////////////////////////////////
bool
CurPos::operator==(const CurPos& from) const
{
  return ((x == from.x) && (y == from.y));
}

////////////////////////////////////////////////////////////////////////////
// CurPos
//	Operator +
////////////////////////////////////////////////////////////////////////////
CurPos
CurPos::operator+(const CurPos& from) const
{
  return CurPos(x + from.x, y + from.y);
}
////////////////////////////////////////////////////////////////////////////
// CurPos
//	Operator -
////////////////////////////////////////////////////////////////////////////
CurPos
CurPos::operator- (const CurPos& from) const
{
  return CurPos(x - from.x, y - from.y);
}

////////////////////////////////////////////////////////////////////////////
// CurPos
//	Operator >
////////////////////////////////////////////////////////////////////////////
bool
CurPos::operator> (const CurPos& from) const
{
  if (y > from.y) {
    return true;
  } else if (y == from.y && x > from.x) {
    return true;
  }
  
  return false;
}

////////////////////////////////////////////////////////////////////////////
// CurPos
//	Operator >=
////////////////////////////////////////////////////////////////////////////
bool
CurPos::operator>= (const CurPos& from) const
{
  if (y > from.y) {
    return true;
  } else if (y == from.y && x >= from.x) {
    return true;
  }
  
  return false;
}

////////////////////////////////////////////////////////////////////////////
// CurPos
//	Operator <
////////////////////////////////////////////////////////////////////////////
bool
CurPos::operator< (const CurPos& from) const
{
  if (y < from.y) {
    return true;
  } else if (y == from.y && x < from.x) {
    return true;
  }
  
  return false;
}

////////////////////////////////////////////////////////////////////////////
// CurPos
//	Operator <=
////////////////////////////////////////////////////////////////////////////
bool
CurPos::operator<= (const CurPos& from) const
{
  if (y < from.y) {
    return true;
  } else if (y == from.y && x <= from.x) {
    return true;
  }
  
  return false;
}
