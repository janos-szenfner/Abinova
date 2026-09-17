#pragma once

#include "ut_misc.h"
#include <limits.h>

#include "ut_types.h"
#include "ut_iconv.h"

// UTF-8 can use up to 6 bytes
#define MY_MB_LEN_MAX 6

// DO NOT USE MB_LEN_MAX -- on win32 it is only 2 bytes!
const size_t iMbLenMax = UT_MAX(MY_MB_LEN_MAX, MB_LEN_MAX);

class ABI_EXPORT UT_UCS2_mbtowc
{
 private:
  class ABI_EXPORT Converter
    {
    public:
      Converter (const char * from_charset);
      ~Converter ();

      void initialize ();

      inline UT_iconv_t cd () const { return m_cd; }

    private:
      UT_iconv_t m_cd;
    };
  Converter * m_converter;

 public:
  void initialize (bool clear = true);

  UT_UCS2_mbtowc (const char * from_charset);
  ~UT_UCS2_mbtowc();

  void setInCharset (const char * from_charset);

  int mbtowc (UT_UCS2Char & wc, char mb);

  // TODO: make me private
  UT_UCS2_mbtowc ();

 private:
  UT_UCS2_mbtowc (const UT_UCS2_mbtowc & v) = delete;
  UT_UCS2_mbtowc& operator=(const UT_UCS2_mbtowc &rhs) = delete;

  size_t m_bufLen;
  char m_buf[iMbLenMax];
};

class ABI_EXPORT UT_UCS4_mbtowc
{
 private:
  class ABI_EXPORT Converter
    {
    public:
      Converter (const char * from_charset);

      ~Converter ();

      void initialize ();

      inline UT_iconv_t cd () const { return m_cd; }

    private:
      UT_iconv_t m_cd;
    };
  Converter * m_converter;

 public:
  void initialize (bool clear = true);

  UT_UCS4_mbtowc (const char * from_charset);
  ~UT_UCS4_mbtowc();

  void setInCharset (const char * from_charset);

  int mbtowc (UT_UCS4Char & wc, char mb);

  // TODO: make me private
  UT_UCS4_mbtowc ();

 private:
  UT_UCS4_mbtowc(const UT_UCS4_mbtowc & v) = delete;
  UT_UCS4_mbtowc& operator=(const UT_UCS4_mbtowc &rhs) = delete;

  size_t m_bufLen;
  char m_buf[iMbLenMax];
};
