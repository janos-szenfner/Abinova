/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode:t -*- */
/* AbiSource Program Utilities
 * Copyright (C) 2002 Dom Lachowicz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#include <stdlib.h>
#include <stdio.h>

#include "ut_string.h"
#include "ut_locale.h"

// don't like XAP in UT, but oh well...
#include "xap_EncodingManager.h"

/********************************************/

/**
 * Class serves to make rolling back locale changes simple, automatic,
 * and transparent
 *
 * USAGE:
 * UT_LocaleTransactor t(LC_NUMERIC, "C");
 * sprintf();
 * sprintf();
 * return; // <-- old locale gets reset transparently for you at the end of
 * // the block
 *
 * Thread-safety contract:
 * - POSIX (default): implemented with newlocale()/uselocale() so the change
 *   is confined to the CALLING THREAD's locale. The process-wide locale is
 *   never touched, and the transactor is safe to use on any thread —
 *   including concurrent with other threads doing locale-sensitive C calls.
 *   Note that setlocale(category, NULL) inside the scope returns the
 *   (unchanged) global locale, NOT the transactor's: observe the effective
 *   locale via localeconv()/nl_langinfo() or formatted output instead.
 * - Windows: _configthreadlocale(_ENABLE_PER_THREAD_LOCALE) makes the
 *   subsequent setlocale() calls thread-local on the calling thread.
 * - Fallback (non-POSIX, non-Windows, or an unmapped category): the legacy
 *   process-wide setlocale() path is kept and MUST only be used on the main
 *   thread.
 *
 * SEE ALSO: man setlocale, man uselocale
 */

#if !defined(G_OS_WIN32) && !defined(UT_NO_USELOCALE)
// Map a setlocale() category to the newlocale() bitmask for it.
// Returns 0 for categories without a mask (nonstandard LC_* values) so the
// caller falls back to the setlocale() path.
static int s_localeMask (int category)
{
	switch (category)
	{
		case LC_CTYPE:     return LC_CTYPE_MASK;
		case LC_NUMERIC:   return LC_NUMERIC_MASK;
		case LC_TIME:      return LC_TIME_MASK;
		case LC_COLLATE:   return LC_COLLATE_MASK;
		case LC_MONETARY:  return LC_MONETARY_MASK;
#ifdef LC_MESSAGES_MASK
		case LC_MESSAGES:  return LC_MESSAGES_MASK;
#endif
		case LC_ALL:       return LC_ALL_MASK;
		default:           return 0;
	}
}
#endif

UT_LocaleTransactor::UT_LocaleTransactor (int category, const char * locale)
  : mCategory(category)
	, mPrevLocale(nullptr)
	, mNewLocale(nullptr)
	, mUseSetlocale(false)
{
#if defined(G_OS_WIN32)
	// Make setlocale() operate per-thread on this thread. If it fails we
	// still take the (process-wide) setlocale path below, which is no worse
	// than the historical behavior.
	_configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
	mUseSetlocale = true;
#elif !defined(UT_NO_USELOCALE)
	int mask = s_localeMask(category);
	if (mask != 0)
	{
		locale_t loc = newlocale(mask, locale, static_cast<locale_t>(nullptr));
		if (loc != nullptr)
		{
			locale_t prev = uselocale(loc);
			if (prev != nullptr)
			{
				mPrevLocale = static_cast<void*>(prev);
				mNewLocale  = static_cast<void*>(loc);
			}
			else
			{
				freelocale(loc);
			}
		}
	}
	if (mNewLocale == nullptr)
	{
		// Unknown locale name (same net result as a failed setlocale) or a
		// category without a uselocale mask: fall back to the legacy path.
		mUseSetlocale = true;
	}
#else
	mUseSetlocale = true;
#endif
	if (mUseSetlocale)
	{
		const char * old = setlocale(category, nullptr);
		mOldLocale = old ? old : "";
		setlocale(category, locale);
	}
}

UT_LocaleTransactor::~UT_LocaleTransactor ()
{
	if (mNewLocale != nullptr)
	{
		uselocale(static_cast<locale_t>(mPrevLocale));
		freelocale(static_cast<locale_t>(mNewLocale));
	}
	else if (mUseSetlocale)
	{
		setlocale(mCategory, mOldLocale.c_str());
	}
}

/********************************************/

/**
 * Essentially identical to UT_LocaleInfo(getenv("LANG")), except
 * works on non-unix platforms too (i.e. win32)
 */
UT_LocaleInfo::UT_LocaleInfo ()
{
	// should work on any platform, as opposed to init(getenv("LANG"))
	XAP_EncodingManager * instance = XAP_EncodingManager::get_instance ();

	if (instance->getLanguageISOName() != nullptr)
		mLanguage = instance->getLanguageISOName();

	if (instance->getLanguageISOTerritory() != nullptr)
		mTerritory = instance->getLanguageISOTerritory();

	if (instance->getNative8BitEncodingName() != nullptr)
		mEncoding = instance->getNative8BitEncodingName();
}

/**
 * Takes in a string of the form "language_TERRITORY.ENCODING" or
 * "language-TERRITORY.ENCODING" and decomposes it. TERRITORY and ENCODING
 * parts are optional
 */
UT_LocaleInfo::UT_LocaleInfo (const char * locale)
{
	init(locale);
}

/* static */const UT_LocaleInfo UT_LocaleInfo::system()
{
	return UT_LocaleInfo();
}

/**
 * True if language field is non-null/non-empty, false if not
 */
bool UT_LocaleInfo::hasLanguage() const
{
	return mLanguage.size() != 0;
}

/**
 * True if territory field is non-null/non-empty, false if not
 */
bool UT_LocaleInfo::hasTerritory() const
{
	return mTerritory.size() != 0;
}

/**
 * True if encoding field is non-null/non-empty, false if not
 */
bool UT_LocaleInfo::hasEncoding() const
{
	return mEncoding.size() != 0;
}

/**
 * Returns empty string or language. Example languages are
 * "en", "wen", "fr", "es"
 */
const std::string& UT_LocaleInfo::getLanguage () const
{
	return mLanguage;
}

/**
 * Returns empty string or territory. Example territories are:
 * "US", "GB", "FR", ...
 */
const std::string& UT_LocaleInfo::getTerritory() const
{
	return mTerritory;
}

/**
 * Returns empty string or encoding. Encoding is like "UTF-8" or
 * "ISO-8859-1"
 */
const std::string& UT_LocaleInfo::getEncoding() const
{
	return mEncoding;
}

void UT_LocaleInfo::init(const std::string & locale)
{
	if (locale.empty())
	{
		return;
	}

	std::string::size_type dot = 0;
	std::string::size_type hyphen = 0;

	// take both hyphen types into account
	hyphen = locale.find('_');
	if (hyphen == std::string::npos)
	{
		hyphen = locale.find('-');
	}

	dot = locale.find('.');

	if (hyphen == std::string::npos && dot == std::string::npos)
	{
		mLanguage = locale;
		return;
	}

	if (hyphen != std::string::npos && dot != std::string::npos)
	{
		if (hyphen < dot)
		{
			mLanguage  = locale.substr(0, hyphen);
			mTerritory = locale.substr(hyphen + 1, dot - (hyphen + 1));
			mEncoding  = locale.substr(dot + 1, locale.size() - (dot + 1));
		}
		else
		{
			mLanguage = locale.substr(0, dot);
			mEncoding = locale.substr(dot + 1, locale.size() - (dot + 1));
		}
	}
	else if (dot != std::string::npos)
	{
		mLanguage = locale.substr(0, dot);
		mEncoding = locale.substr(dot + 1, locale.size() - (dot + 1));
	}
	else if (hyphen != std::string::npos)
	{
		mLanguage = locale.substr(0, hyphen);
		mEncoding = locale.substr(hyphen +1, locale.size() - (hyphen + 1));
	}
}

/**
 * Turns object back into a string of the form language_TERRITORY.ENCODING
 * (eg): en_US.UTF-8
 */
std::string UT_LocaleInfo::toString() const
{
	std::string ret(mLanguage);

	if (hasTerritory())
	{
		ret += "_";
		ret += mTerritory;
	}

	if (hasEncoding())
    {
		ret += ".";
		ret += mEncoding;
    }

	return ret;
}

bool UT_LocaleInfo::operator==(const UT_LocaleInfo & rhs) const
{
	return ((mLanguage == rhs.mLanguage) &&
			(mTerritory == rhs.mTerritory) &&
			(mEncoding == rhs.mEncoding));
}

bool UT_LocaleInfo::operator!=(const UT_LocaleInfo & rhs) const
{
  return (!(*this == rhs));
}


