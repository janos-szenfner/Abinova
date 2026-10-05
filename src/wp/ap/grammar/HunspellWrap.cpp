/* Abinova
 * Copyright (C) 2025 Abinova contributors
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xap_App.h"
#include "ut_string_class.h"
#include "ut_types.h"
#include "ut_debugmsg.h"
#include "ut_vector.h"
#include "ut_string.h"
#include "AbiGrammarUtil.h"

#include <ctype.h>
#include <algorithm>
#include <string>
#include <vector>

#include <glib.h>
#ifdef G_OS_WIN32
#include <glib/gwin32.h>
#endif

#include <hunspell.hxx>

#include "HunspellWrap.h"

static bool fileExists(const std::string & path)
{
  return g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR);
}

static bool loadDictionaryFrom(Hunspell ** ppHS, const std::string & base)
{
  std::string aff = base + ".aff";
  std::string dic = base + ".dic";
  if(!fileExists(aff) || !fileExists(dic))
    return false;
  *ppHS = new Hunspell(aff.c_str(), dic.c_str());
  return true;
}

/*!
 * Ordered list of directories searched for hunspell dictionaries,
 * mirroring the search path hunspell 1.7.4 itself uses:
 *
 *   1. $DICPATH entries (platform search-path separator)
 *   2. $XDG_DATA_HOME/hunspell (default ~/.local/share/hunspell)
 *   3. <each $XDG_DATA_DIRS entry>/hunspell
 *      (default /usr/local/share:/usr/share, relative entries ignored)
 *   4. legacy distro dirs (/usr/{share,local/share}/myspell*)
 *   5. macOS: ~/Library/Spelling, /Library/Spelling
 *   6. Windows: %APPDATA%\hunspell, %LOCALAPPDATA%\hunspell and
 *      <install-dir>\hunspell + <install-dir>\share\hunspell
 */
static std::vector<std::string> dictionaryDirs(void)
{
  std::vector<std::string> dirs;
  auto addDir = [&dirs](const std::string & dir) {
    if(!dir.empty() &&
       std::find(dirs.begin(), dirs.end(), dir) == dirs.end())
    {
      dirs.push_back(dir);
    }
  };

  const char * dicpath = getenv("DICPATH");
  if(dicpath && *dicpath)
  {
    gchar ** pPaths = g_strsplit(dicpath, G_SEARCHPATH_SEPARATOR_S, -1);
    for(gchar ** pp = pPaths; pp && *pp; pp++)
    {
      addDir(*pp);
    }
    g_strfreev(pPaths);
  }

#ifdef G_OS_WIN32
  const char * appdata = getenv("APPDATA");
  if(appdata && *appdata)
  {
    addDir(std::string(appdata) + "/hunspell");
  }
  const char * localappdata = getenv("LOCALAPPDATA");
  if(localappdata && *localappdata)
  {
    addDir(std::string(localappdata) + "/hunspell");
  }
  gchar * pInstallDir = g_win32_get_package_installation_directory_of_module(nullptr);
  if(pInstallDir)
  {
    addDir(std::string(pInstallDir) + "/hunspell");
    addDir(std::string(pInstallDir) + "/share/hunspell");
    g_free(pInstallDir);
  }
#else
  addDir(std::string(g_get_user_data_dir()) + "/hunspell");

  for(const gchar * const * pp = g_get_system_data_dirs(); pp && *pp; pp++)
  {
    if(**pp == '/')
    {
      addDir(std::string(*pp) + "/hunspell");
    }
  }

  addDir("/usr/share/myspell");
  addDir("/usr/share/myspell/dicts");
  addDir("/usr/local/share/myspell");

#ifdef __APPLE__
  const gchar * home = g_get_home_dir();
  if(home)
  {
    addDir(std::string(home) + "/Library/Spelling");
  }
  addDir("/Library/Spelling");
#endif
#endif

  /* PACK07: dictionaries staged inside a relocatable bundle land at
   * <AbiSuiteLibDir>/hunspell (the bundle root on Linux/Windows,
   * Contents/Resources on macOS).  _setBundleModulePaths also appends
   * the bundle root to XDG_DATA_DIRS, which already reaches this list
   * via the loop above — these explicit entries keep the fallback
   * alive where that setup never ran (tests, embedded AbiWidget).
   * Appended last: a system or user dictionary still wins, the bundle
   * only guarantees a baseline. */
  if(XAP_App * app = XAP_App::getApp())
  {
    const std::string libdir = app->getAbiSuiteLibDir();
    addDir(libdir + "/hunspell");
    addDir(libdir + "/share/hunspell");
    addDir(libdir + "/dictionary/myspell");
  }

  return dirs;
}

static bool findEnglishDictionary(Hunspell ** ppHS)
{
  const std::vector<std::string> dirs = dictionaryDirs();
  static const char * langs[] = {
    "en_US", "en_GB", "en", nullptr
  };

  for(const std::string & dir : dirs)
  {
    for(int l = 0; langs[l]; l++)
    {
      if(loadDictionaryFrom(ppHS, dir + "/" + langs[l]))
        return true;
    }
  }

  /* Fall back to the first en_* dictionary in the search dirs */
  for(const std::string & dir : dirs)
  {
    GDir * pDir = g_dir_open(dir.c_str(), 0, nullptr);
    if(!pDir)
      continue;
    std::string found;
    const gchar * pEnt = nullptr;
    while((pEnt = g_dir_read_name(pDir)) != nullptr)
    {
      std::string name = pEnt;
      if(name.size() > 7 && name.compare(0, 3, "en_") == 0 &&
         name.compare(name.size() - 4, 4, ".dic") == 0)
      {
        found = dir + "/" + name.substr(0, name.size() - 4);
        break;
      }
    }
    g_dir_close(pDir);
    if(!found.empty() && loadDictionaryFrom(ppHS, found))
      return true;
  }

  return false;
}

HunspellWrap::HunspellWrap(void) :
  m_pHS(nullptr)
{
  if(!findEnglishDictionary(&m_pHS))
  {
    UT_DEBUGMSG(("HunspellWrap: no English hunspell dictionary found\n"));
  }
}

HunspellWrap::~HunspellWrap(void)
{
  delete m_pHS;
}

bool HunspellWrap::clear(void)
{
  return true;
}

/*!
 * Check each word of the sentence with hunspell. Every misspelled
 * word is reported as an AbiGrammarError so the block squiggles
 * the offending region. Returns true when no errors were found.
 */
bool HunspellWrap::parseSentence(PieceOfText * pT)
{
  if(!m_pHS || !pT)
  {
    return true; // no dictionary: default to no checking
  }

  const std::string sent = pT->sText.utf8_str();
  const size_t totlen = sent.size();
  UT_sint32 iWord = 0;
  bool bOK = true;

  size_t i = 0;
  while(i < totlen)
  {
    /* words are runs of letters (plus internal ' and -) */
    if(!isalpha(static_cast<unsigned char>(sent[i])))
    {
      i++;
      continue;
    }

    size_t start = i;
    while(i < totlen &&
          (isalpha(static_cast<unsigned char>(sent[i])) ||
           ((sent[i] == '\'' || sent[i] == '-') &&
            i + 1 < totlen &&
            isalpha(static_cast<unsigned char>(sent[i + 1])))))
    {
      i++;
    }
    size_t len = i - start;
    iWord++;

    std::string word = sent.substr(start, len);
    if(m_pHS->spell(word) == 0)
    {
      bOK = false;
      AbiGrammarError * pErr = new AbiGrammarError();
      pErr->m_iErrLow  = pT->iInLow + static_cast<UT_sint32>(start);
      pErr->m_iErrHigh = pT->iInLow + static_cast<UT_sint32>(start + len) - 1;
      pErr->m_iWordNum = iWord;
      pErr->m_sErrorDesc = "misspelled word";
      pT->m_vecGrammarErrors.push_back(pErr);
    }
  }

  pT->m_bGrammarChecked = true;
  pT->m_bGrammarOK = bOK;
  return bOK;
}
