/* AbiWord
 * Copyright (C) 2025 AbiWord contributors
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
#include "../xp/AbiGrammarUtil.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <string>

#include <hunspell.hxx>

#include "HunspellWrap.h"

static bool fileExists(const std::string & path)
{
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
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

static bool findEnglishDictionary(Hunspell ** ppHS)
{
  static const char * dirs[] = {
    "/usr/share/hunspell",
    "/usr/share/myspell",
    "/usr/local/share/hunspell",
    "/usr/local/share/myspell",
    nullptr
  };
  static const char * langs[] = {
    "en_US", "en_GB", "en", nullptr
  };

  for(int d = 0; dirs[d]; d++)
  {
    for(int l = 0; langs[l]; l++)
    {
      std::string base = std::string(dirs[d]) + "/" + langs[l];
      if(loadDictionaryFrom(ppHS, base))
        return true;
    }
  }

  /* Fall back to the first en_* dictionary in the standard dirs */
  for(int d = 0; dirs[d]; d++)
  {
    DIR * pDir = opendir(dirs[d]);
    if(!pDir)
      continue;
    struct dirent * pEnt = nullptr;
    std::string found;
    while((pEnt = readdir(pDir)) != nullptr)
    {
      std::string name = pEnt->d_name;
      if(name.size() > 7 && name.compare(0, 3, "en_") == 0 &&
         name.compare(name.size() - 4, 4, ".dic") == 0)
      {
        found = std::string(dirs[d]) + "/" + name.substr(0, name.size() - 4);
        break;
      }
    }
    closedir(pDir);
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
      pT->m_vecGrammarErrors.addItem(pErr);
    }
  }

  pT->m_bGrammarChecked = true;
  pT->m_bGrammarOK = bOK;
  return bOK;
}
