#pragma once

#include <limits.h>

#include "ut_types.h"
#include "ut_iconv.h"

class ABI_EXPORT UT_Wctomb
{
public:

  void initialize();
  UT_Wctomb(const char* to_charset);
  ~UT_Wctomb();
  int wctomb(char * pC,int &length,UT_UCS4Char wc, int max_len = 100) const;
  void wctomb_or_fallback(char * pC,int &length,UT_UCS4Char wc, int max_len = 100) const;

  void setOutCharset(const char* charset);

  // TODO: make me private
  UT_Wctomb();

 private:
  UT_Wctomb(const UT_Wctomb& v) = delete;
  UT_Wctomb& operator=(const UT_Wctomb &rhs) = delete;

  UT_iconv_t cd;
};
