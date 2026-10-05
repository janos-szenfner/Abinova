/* AbiSuite
 * Copyright (C) 2003 Dom Lachowicz
 * Copyright (C) 2025-2026 Abinova contributors
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#if !defined(_WIN32)
#include <dlfcn.h>
#include <enchant-provider.h>
#endif

#include "xap_App.h"
#include "xap_Frame.h"
#include "xap_Strings.h"
#include "enchant_checker.h"
#include "ut_string.h"
#include "ut_string_class.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"

/*!
 * Convert a UTF-8 string to a UTF-32 string
 *
 * \param word8 The zero-terminated input string in UTF-8 format
 * \return A zero-terminated UTF-32 string
 */
static UT_UCS4Char *
utf8_to_utf32(const char *word8)
{
	UT_UCS4Char * ucs4 = nullptr;
	UT_UCS4_cloneString (&ucs4, UT_UCS4String (word8).ucs4_str());
	return ucs4;
}

static size_t s_enchant_broker_count = 0;
static EnchantBroker * s_enchant_broker = nullptr;

#if !defined(_WIN32)
/* PACK06: bundled-provider fallback.
 *
 * Enchant dlopen()s its provider modules (enchant_hunspell.so etc.)
 * from a directory compiled into the library; several builds —
 * Debian's among them — do not honour enchant_set_prefix_dir() or any
 * environment variable, so a relocatable bundle cannot place the
 * backends where the broker will find them.  When the broker reports
 * no providers at all we load the bundled backends ourselves through
 * the public EnchantProvider ABI (<enchant-provider.h>) and talk to
 * them directly: dictionaries obtained this way are driven through
 * their function pointers, bypassing the broker/session layer (which
 * expects EnchantDictPrivateData we cannot fabricate).  Windows
 * enchant resolves its provider dir relative to its own DLL, so the
 * fallback is POSIX-only.
 */
struct BundledProvider
{
	EnchantProvider * provider;
	void * module;	// dlopen handle
};
static std::vector<BundledProvider> s_bundled_providers;
static bool s_bundled_providers_scanned = false;

static void
abi_enchant_scan_bundled_providers(void)
{
	if (s_bundled_providers_scanned)
		return;
	s_bundled_providers_scanned = true;

	std::vector<std::string> dirs;
	const char * env = g_getenv("ABINOVA_ENCHANT_MODULE_PATH");
	if (env && *env)
		dirs.push_back(env);
	const char * mr = g_getenv("ABINOVA_MODULE_ROOT");
	if (mr && *mr)
	{
		dirs.push_back(std::string(mr) + "/lib/enchant-2");
		dirs.push_back(std::string(mr) + "/enchant-2");
	}
	if (XAP_App * app = XAP_App::getApp())
	{
		std::string libdir = app->getAbiSuiteLibDir();
		dirs.push_back(libdir + "/lib/enchant-2");	// Linux/Win-style
		dirs.push_back(libdir + "/enchant-2");		// flat .app layout
	}

	for (const auto & dir : dirs)
	{
		GDir * gdir = g_dir_open(dir.c_str(), 0, nullptr);
		if (!gdir)
			continue;
		const gchar * name;
		while ((name = g_dir_read_name(gdir)) != nullptr)
		{
			if (!g_str_has_suffix(name, ".so") &&
				!g_str_has_suffix(name, ".dylib"))
				continue;
			gchar * path = g_build_filename(dir.c_str(), name, nullptr);
			void * module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
			if (!module)
			{
				UT_DEBUGMSG(("enchant: cannot load backend %s: %s\n",
							 path, dlerror()));
				g_free(path);
				continue;
			}
			EnchantProvider *(*initfn)(void) =
				reinterpret_cast<EnchantProvider *(*)(void)>(
					dlsym(module, "init_enchant_provider"));
			EnchantProvider * provider = initfn ? initfn() : nullptr;
			if (!provider || !provider->request_dict)
			{
				dlclose(module);
				g_free(path);
				continue;
			}
			provider->owner = s_enchant_broker;	// for error reporting
			provider->enchant_private_data = module;
			s_bundled_providers.push_back({ provider, module });
			UT_DEBUGMSG(("enchant: loaded bundled provider %s (%s)\n",
						 path,
						 provider->identify ? provider->identify(provider)
											: "?"));
			g_free(path);
		}
		g_dir_close(gdir);
	}
}

static void
abi_enchant_release_bundled_providers(void)
{
	for (auto & bp : s_bundled_providers)
	{
		if (bp.provider->dispose)
			bp.provider->dispose(bp.provider);
		dlclose(bp.module);
	}
	s_bundled_providers.clear();
	s_bundled_providers_scanned = false;
}

static void
abi_enchant_count_provider(const char *, const char *,
						   const char *, void * user_data)
{
	(*static_cast<size_t *>(user_data))++;
}
#endif /* !_WIN32 */

EnchantChecker::EnchantChecker()
	: m_dict(nullptr)
#if !defined(_WIN32)
	, m_bundledProvider(nullptr)
#endif
{
	if (s_enchant_broker_count == 0)
	{
		s_enchant_broker = enchant_broker_init ();
#if !defined(_WIN32)
		/* if the broker found no provider modules at all (relocatable
		 * bundle: enchant's compiled-in provider dir is absent), load
		 * the bundled backends ourselves */
		if (s_enchant_broker)
		{
			size_t n = 0;
			enchant_broker_describe(s_enchant_broker,
									abi_enchant_count_provider, &n);
			if (n == 0)
				abi_enchant_scan_bundled_providers();
		}
#endif
		/* enchant-2 providers locate their own dictionaries; the
		 * enchant-1 broker_set_param() hack that lived here is gone */
	}
	s_enchant_broker_count++;
}

EnchantChecker::~EnchantChecker()
{
	UT_return_if_fail (s_enchant_broker);

	if (m_dict)
	{
#if !defined(_WIN32)
		if (m_bundledProvider)
			m_bundledProvider->dispose_dict(m_bundledProvider, m_dict);
		else
#endif
			enchant_broker_free_dict (s_enchant_broker, m_dict);
		m_dict = nullptr;
	}

	s_enchant_broker_count--;
	if (s_enchant_broker_count == 0) {
#if !defined(_WIN32)
		abi_enchant_release_bundled_providers();
#endif
		enchant_broker_free (s_enchant_broker);
		s_enchant_broker = nullptr;
	}
}

SpellChecker::SpellCheckResult
EnchantChecker::_checkWord (const UT_UCS4Char * ucszWord, size_t len)
{
	UT_return_val_if_fail (m_dict, SpellChecker::LOOKUP_ERROR);
	UT_return_val_if_fail (ucszWord, SpellChecker::LOOKUP_ERROR);
	UT_return_val_if_fail (len, SpellChecker::LOOKUP_ERROR);

	UT_UTF8String utf8 (ucszWord, len);

	int rc;
#if !defined(_WIN32)
	if (m_bundledProvider)
		rc = m_dict->check ? m_dict->check(m_dict, utf8.utf8_str(),
										  utf8.byteLength()) : -1;
	else
#endif
		rc = enchant_dict_check (m_dict, utf8.utf8_str(), utf8.byteLength());

	switch (rc)
	{
	case -1:
		return SpellChecker::LOOKUP_ERROR;
	case 0:
		return SpellChecker::LOOKUP_SUCCEEDED;
	default:
		return SpellChecker::LOOKUP_FAILED;
	}
}

std::vector<UT_UCS4Char*>
EnchantChecker::_suggestWord (const UT_UCS4Char *ucszWord, size_t len)
{
	UT_return_val_if_fail(m_dict, std::vector<UT_UCS4Char*>());
	UT_return_val_if_fail(ucszWord && len, std::vector<UT_UCS4Char*>());

	std::vector<UT_UCS4Char*> pvSugg;

	UT_UTF8String utf8 (ucszWord, len);

	char ** suggestions;
	size_t n_suggestions;

#if !defined(_WIN32)
	if (m_bundledProvider)
	{
		/* provider dicts carry no broker session — drive the vtable
		 * directly; enchant_dict_suggest/_free_string_list would
		 * dereference enchant_private_data we cannot fabricate */
		n_suggestions = 0;
		suggestions = m_dict->suggest
			? m_dict->suggest(m_dict, utf8.utf8_str(), utf8.byteLength(),
							  &n_suggestions)
			: nullptr;
	}
	else
#endif
		suggestions = enchant_dict_suggest (m_dict, utf8.utf8_str(), utf8.byteLength(), &n_suggestions);

	if (suggestions && n_suggestions) {
		for (size_t i = 0; i < n_suggestions; i++) {
			UT_UCS4Char *ucszSugg = utf8_to_utf32(suggestions[i]);
			if (ucszSugg)
				pvSugg.push_back(ucszSugg);
		}

#if !defined(_WIN32)
		if (m_bundledProvider)
			g_strfreev(suggestions);
		else
#endif
			enchant_dict_free_string_list(m_dict, suggestions);
	}

	return pvSugg;
}

bool EnchantChecker::addToCustomDict (const UT_UCS4Char *word, size_t len)
{
	UT_return_val_if_fail (m_dict, false);

	if (word && len) {
		UT_UTF8String utf8 (word, len);
#if !defined(_WIN32)
		if (m_bundledProvider)
		{
			/* most providers leave add_to_personal unset — the broker
			 * normally implements the personal wordlist at the session
			 * layer, which bundled-provider dicts do not have */
			if (m_dict->add_to_personal)
			{
				m_dict->add_to_personal(m_dict, utf8.utf8_str(),
										utf8.byteLength());
				return true;
			}
			return false;
		}
#endif
		enchant_dict_add(m_dict, utf8.utf8_str(), utf8.byteLength());
		return true;
	}
	return false;
}

bool EnchantChecker::isIgnored (const UT_UCS4Char *toCorrect, size_t toCorrectLen) const
{
	UT_return_val_if_fail (m_dict, false);

#if !defined(_WIN32)
	if (m_bundledProvider)
		return false;	// session wordlist lives in the broker
#endif

	UT_UTF8String ignore (toCorrect, toCorrectLen);
	return enchant_dict_is_added(m_dict, ignore.utf8_str(), ignore.byteLength()) != 0;
}

void EnchantChecker::ignoreWord (const UT_UCS4Char *toCorrect, size_t toCorrectLen)
{
	UT_return_if_fail (m_dict);
	UT_return_if_fail (toCorrect && toCorrectLen);

	UT_UTF8String ignore (toCorrect, toCorrectLen);

#if !defined(_WIN32)
	if (m_bundledProvider)
	{
		if (m_dict->add_to_session)
			m_dict->add_to_session(m_dict, ignore.utf8_str(),
								   ignore.byteLength());
		return;
	}
#endif
	enchant_dict_add_to_session (m_dict, ignore.utf8_str(), ignore.byteLength());
}

void EnchantChecker::correctWord (const UT_UCS4Char *toCorrect, size_t toCorrectLen,
								  const UT_UCS4Char *correct, size_t correctLen)
{
	UT_return_if_fail (m_dict);

	UT_return_if_fail (toCorrect && toCorrectLen);
	UT_return_if_fail (correct && correctLen);

	UT_UTF8String bad (toCorrect, toCorrectLen);
	UT_UTF8String good (correct, correctLen);

#if !defined(_WIN32)
	if (m_bundledProvider)
	{
		if (m_dict->store_replacement)
			m_dict->store_replacement(m_dict,
									  bad.utf8_str(), bad.byteLength(),
									  good.utf8_str(), good.byteLength());
		return;
	}
#endif
	enchant_dict_store_replacement (m_dict,
									bad.utf8_str(), bad.byteLength(),
									good.utf8_str(), good.byteLength());
}

bool
EnchantChecker::doesDictionaryExist (const char * szLang)
{
	UT_return_val_if_fail (szLang, false);
	UT_return_val_if_fail (s_enchant_broker, false);

	// Convert the language tag from en-US to en_US form
	char * lang = g_strdup (szLang);
	char * hyphen = strchr (lang, '-');
	if (hyphen)
		*hyphen = '_';

	bool exists = enchant_broker_dict_exists (s_enchant_broker, lang);
#if !defined(_WIN32)
	if (!exists)
	{
		abi_enchant_scan_bundled_providers();
		for (const auto & bp : s_bundled_providers)
		{
			if (bp.provider->dictionary_exists &&
				bp.provider->dictionary_exists(bp.provider, lang))
			{
				exists = true;
				break;
			}
		}
	}
#endif
	FREEP(lang);

	return exists;
}

bool
EnchantChecker::_requestDictionary (const char * szLang)
{
	UT_return_val_if_fail (szLang, false);
	UT_return_val_if_fail (s_enchant_broker, false);

	/* switching dictionaries: release the old one through whichever
	 * channel provided it before requesting the new one — leaking it
	 * would strand the broker session, and a stale m_bundledProvider
	 * would free a broker dict through provider->dispose_dict */
	if (m_dict)
	{
#if !defined(_WIN32)
		if (m_bundledProvider)
			m_bundledProvider->dispose_dict(m_bundledProvider, m_dict);
		else
#endif
			enchant_broker_free_dict (s_enchant_broker, m_dict);
		m_dict = nullptr;
#if !defined(_WIN32)
		m_bundledProvider = nullptr;
#endif
	}

	// Convert the language tag from en-US to en_US form
	char * lang = g_strdup (szLang);
	char * hyphen = strchr (lang, '-');
	if (hyphen)
		*hyphen = '_';

	m_dict = enchant_broker_request_dict(s_enchant_broker, lang);
#if !defined(_WIN32)
	if (!m_dict)
	{
		abi_enchant_scan_bundled_providers();
		for (const auto & bp : s_bundled_providers)
		{
			EnchantProvider * provider = bp.provider;
			if (provider->dictionary_exists &&
				!provider->dictionary_exists(provider, lang))
				continue;
			m_dict = provider->request_dict(provider, lang);
			if (m_dict)
			{
				m_bundledProvider = provider;
				break;
			}
		}
	}
#endif
	FREEP(lang);

	return (m_dict != nullptr);
}
