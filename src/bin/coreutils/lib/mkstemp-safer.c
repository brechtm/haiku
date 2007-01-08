/* Invoke mkstemp, but avoid some glitches.

   Copyright (C) 2005, 2006 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.  */

/* Written by Paul Eggert.  */

#include <config.h>

#include "stdlib-safer.h"

#include "mkstemp.h"
#include "unistd-safer.h"

/* Like mkstemp, but do not return STDIN_FILENO, STDOUT_FILENO, or
   STDERR_FILENO.  */

int
mkstemp_safer (char *templ)
{
  return fd_safer (mkstemp (templ));
}
