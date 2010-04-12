/*
 * Copyright 2010, Oliver Tappe, zooey@hirschkaefer.de.
 * Distributed under the terms of the MIT License.
 */


#include "LibbeLocaleBackend.h"

#include "Catalog.h"
#include "Locale.h"
#include "LocaleRoster.h"

#include <new>


namespace BPrivate {


extern "C" LocaleBackend*
CreateLocaleBackendInstance()
{
	return new(std::nothrow) LibbeLocaleBackend();
}


LibbeLocaleBackend::LibbeLocaleBackend()
{
	be_locale_roster->GetSystemCatalog(&systemCatalog);
}


LibbeLocaleBackend::~LibbeLocaleBackend()
{
}


const char*
LibbeLocaleBackend::GetString(const char* string, const char* context,
	const char* comment)
{
	return systemCatalog->GetString(string, context, comment);
}


}	// namespace BPrivate
