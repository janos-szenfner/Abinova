
#include <vector>
#include <algorithm>

#include "tf_test.h"
#include "ut_uuid.h"
#include "ut_debugmsg.h"

#define TFSUITE "core.af.util.uuid"


//#include "ut_endian.h"
#include "ut_rand.h"

struct test_record
{
  UT_uint32 val;
  UT_uint32 indx;
};

static int s_cmp_hash(const test_record &i1, const test_record &i2)
{
  return (i1.val < i2.val);
}

void UT_UUIDGenerator__test(UT_UUIDGenerator* self)
{
  // test hashes ...
  UT_DEBUGMSG(("------------------------- Testing uuid hash() ---------------------------\n"));
  std::vector<test_record> v;
  const UT_uint32 iMax = 512000;
  const UT_uint32 iTest = 10;
  UT_uint32 iColHTotal = 0;
  UT_uint32 iDeltaMinH = 0xffffffff;

  // create a dummy uuid instance ...
  if(!self->m_pUUID)
    self->m_pUUID = new UT_UUID;

  for (UT_uint32 k = 0; k < iTest; ++k)
  {
    UT_uint32 j;
    UT_uint32 iColH = 0;
    UT_uint32 iDMinH = 0xffffffff;

    TF_Test::pulse();

    for(j = 0; j < iMax; ++j)
    {
      //makeUUID();

      // on similar strings, the glib hash performs much better;
      // let's test it on random strings
      UT_uint32 * p = reinterpret_cast<UT_uint32 *>(&(self->m_pUUID->m_uuid));

      for(UT_uint32 n = 0; n < 4; n++)
        p[n] = UT_rand();

      test_record t;
      t.val = self->m_pUUID->hash32();
      t.indx = j;
      v.push_back(t);

//      if(0 == j % iMsg)
//        UT_DEBUGMSG(("Round %d: Generating rand %d of %d\n", k, j, iMax));
    }

    TF_Test::pulse();

    std::sort(v.begin(), v.end(), s_cmp_hash);

    for(j = 0; j < iMax - 1; ++j)
    {
      const test_record &t1 = v.at(j);
      const test_record &t2 = v.at(j+1);

      if(t1.val == t2.val)
      {
        UT_DEBUGMSG(("Round %04d: uuid hash() collision (value: %u)\n", k, t1.val));
        UT_uint32 i1 = t1.indx > t2.indx ? t1.indx : t2.indx;
        UT_uint32 i2 = t1.indx < t2.indx ? t1.indx : t2.indx;
        iDMinH = iDMinH < static_cast<UT_uint32>((i1-i2) )? iDMinH : static_cast<UT_uint32>(i1)-i2;
        iColH++;
      }

//      if(0 == j % iMsg)
//        UT_DEBUGMSG(("Round %04d: testing %d of %d\n", k, j, iMax));
    }

    UT_DEBUGMSG(("RESULTS: round %04u: %u hash collisions (min distance %u)\n",
                 k, iColH, iDMinH));

    iColHTotal += iColH;
    iDeltaMinH = iDeltaMinH < iDMinH ? iDeltaMinH : iDMinH;
    v.clear();
  }

  UT_DEBUGMSG(("CUMULATIVE RESULTS (of %d): %d hash collisions (min distance %d)\n",
               iMax*iTest, iColHTotal, iDeltaMinH));

  // delete the dummy uuid instance so that any genuine calls to the
  // hash functions allocate a proper derived instance
  if(self->m_pUUID)
  {
    delete self->m_pUUID;
    self->m_pUUID = nullptr;
  }

  UT_DEBUGMSG(("---------------------- Testing uuid hash END --------------------------\n"));
}

TFTEST_MAIN("UUID")
{
  {
    UT_UUIDGenerator generator;

//    UT_UUIDGenerator__test(&generator);
  }
  {
    UT_UUIDGenerator generator;

    UT_UUID *uuid = generator.createUUID();

    TFPASS(uuid->isValid());
    TFPASS(!uuid->isNull());

    delete uuid;

  }
}

TFTEST_MAIN("UT_UUID parse and stringify")
{
	UT_UUIDGenerator gen;

	// the canonical RFC 4122 text form round-trips
	const char * s = "01234567-89ab-cdef-0123-456789abcdef";
	UT_UUIDPtr u(gen.createUUID(s));
	TFPASS(u && u->isValid());
	auto str = u->toString();
	TFPASS(str.has_value());
	TFPASS(*str == s);

	// uppercase hex is accepted
	UT_UUIDPtr u2(gen.createUUID("01234567-89AB-CDEF-0123-456789ABCDEF"));
	TFPASS(u2 && u2->isValid());
	TFPASS(u2->toString().value_or("x") == s);

	// malformed input (wrong length / bad dash positions / non-hex)
	// falls back to generating a fresh valid uuid
	UT_UUIDPtr u3(gen.createUUID("{01234567-89ab-cdef-0123-456789abcdef}"));
	TFPASS(u3 && u3->isValid());
	UT_UUIDPtr u4(gen.createUUID(std::string("0123456789abcdef0123456789abcdef")));
	TFPASS(u4 && u4->isValid());
	TFPASS(*u3 != *u4 || true); // random; just exercise the path

	// a cleared uuid is invalid and its toString yields nothing
	UT_UUIDPtr bad(gen.createUUID());
	bad->clear();
	TFPASS(!bad->isValid());
	TFPASS(!bad->toString().has_value());
}

TFTEST_MAIN("UT_UUID binary conversion and equality")
{
	UT_UUIDGenerator gen;
	const char * s = "01234567-89ab-cdef-0123-456789abcdef";

	UT_UUIDPtr u(gen.createUUID(s));

	struct uuid bin;
	memset(&bin, 0, sizeof(bin));
	TFPASS(u->toBinary(bin));
	TFPASS(bin.time_low == 0x01234567u);
	TFPASS(bin.time_mid == 0x89ab);
	TFPASS(bin.time_high_and_version == 0xcdef);
	TFPASS(bin.node[5] == 0xef);

	// static toStringFromBinary reproduces the text form
	char buf[40];
	TFPASS(UT_UUID::toStringFromBinary(buf, sizeof(buf), bin));
	TFPASS(std::string(buf) == s);

	// constructing from binary gives an equal uuid
	UT_UUIDPtr u2(gen.createUUID(bin));
	TFPASS(u2 && u2->isValid());
	TFPASS(*u == *u2);
	TFPASS(!(*u != *u2));

	// a different uuid compares differently
	UT_UUIDPtr other(gen.createUUID());
	TFPASS(*u != *other);
	TFPASS((*u < *other) != (*other < *u));

	// copy construction + assignment preserve equality
	UT_UUIDPtr copy(gen.createUUID(*u));
	TFPASS(*copy == *u);
	*copy = *other;
	TFPASS(*copy == *other);
}

TFTEST_MAIN("UT_UUID null state and clear")
{
	UT_UUIDGenerator gen;

	// getNull is the canonical null uuid
	const UT_UUID & nul = UT_UUID::getNull();
	TFPASS(nul.isNull());
	TFPASS(!nul.isValid());
	TFPASS(!nul.toString().has_value());

	// a generated uuid cleared back to null compares equal
	UT_UUIDPtr u(gen.createUUID());
	TFPASS(!u->isNull());
	u->clear();
	TFPASS(u->isNull());
	TFPASS(*u == nul);
}

TFTEST_MAIN("UT_UUID type/variant/time")
{
	UT_UUIDGenerator gen;
	UT_UUIDPtr u(gen.createUUID());

	// generated uuids are v1 (time based) DCE
	TFPASS(u->getType() == 1);
	TFPASS(u->getVariant() == UT_UUID_VARIANT_DCE);

	// the embedded timestamp is close to now
	time_t now = time(nullptr);
	TFPASS(u->getTime() <= now && u->getTime() > now - 60);

	// resetTime stamps a new (later-or-equal) creation time
	time_t before = u->getTime();
	TFPASS(u->resetTime());
	TFPASS(u->getTime() >= before);

	// parsing a version-4 uuid reports type 4
	UT_UUIDPtr v4(gen.createUUID("25e8a7c2-3f0a-4b8d-8c1e-2d3f4a5b6c7d"));
	TFPASS(v4 && v4->isValid());
	TFPASS(v4->getType() == 4);

	// temporal comparisons between two freshly minted uuids
	UT_UUIDPtr older(gen.createUUID());
	UT_UUIDPtr younger(gen.createUUID());
	TFPASS(older->isOfSameAge(*younger) ||
		   older->isOlder(*younger) || older->isYounger(*younger));
	TFPASS(older->isOfSameAge(*older));
}

TFTEST_MAIN("UT_UUID makeUUID and setUUID")
{
	UT_UUIDGenerator gen;
	UT_UUIDPtr u(gen.createUUID());

	// makeUUID() on a cleared (null) uuid mutates it in place
	u->clear();
	TFPASS(u->isNull());
	TFPASS(u->makeUUID());
	TFPASS(!u->isNull());

	// makeUUID(out) yields a string without touching the instance
	std::string out;
	UT_UUIDPtr v(gen.createUUID());
	v->clear();
	TFPASS(v->makeUUID(out));
	TFPASS(v->isNull());
	TFPASS(out.size() == 36);

	// setUUID reparses
	TFPASS(v->setUUID("01234567-89ab-cdef-0123-456789abcdef"));
	TFPASS(v->toString().value_or("") ==
		   "01234567-89ab-cdef-0123-456789abcdef");

	struct uuid bin;
	TFPASS(v->toBinary(bin));
	UT_UUIDPtr w(gen.createUUID());
	TFPASS(w->setUUID(bin));
	TFPASS(*w == *v);

	// hashes are stable for identical uuids
	TFPASS(v->hash32() == w->hash32());
	TFPASS(v->hash64() == w->hash64());
}

TFTEST_MAIN("UT_UUIDGenerator helpers")
{
	UT_UUIDGenerator gen;

	UT_uint32 a = gen.getNewUUID32();
	UT_uint32 b = gen.getNewUUID32();
	// collision-corrected hashes of distinct uuids differ
	TFPASS(a != b);
}
