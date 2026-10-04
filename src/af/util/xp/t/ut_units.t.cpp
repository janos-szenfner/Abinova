// Copyright (C) 2023 Hubert Figuière
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "tf_test.h"
#include "ut_units.h"
#include "ut_debugmsg.h"

#define TFSUITE "core.af.util.units"

TFTEST_MAIN("UT_convertInchesToDimensionString works")
{
  double res = 72.0f;

  // defaults
  auto dim = UT_convertInchesToDimensionString(DIM_IN, 0.0);
  TFPASSEQ(dim, "0.0000in");
  dim = UT_convertInchesToDimensionString(DIM_IN, 1.0);
  TFPASSEQ(dim, "1.0000in");
  dim = UT_convertInchesToDimensionString(DIM_CM, 0.0);
  TFPASSEQ(dim, "0.00cm");
  dim = UT_convertInchesToDimensionString(DIM_CM, 1.0);
  TFPASSEQ(dim, "2.54cm");
  dim = UT_convertInchesToDimensionString(DIM_MM, 0.0);
  TFPASSEQ(dim, "0.0mm");
  dim = UT_convertInchesToDimensionString(DIM_MM, 1.0);
  TFPASSEQ(dim, "25.4mm");
  dim = UT_convertInchesToDimensionString(DIM_PI, 0.0);
  TFPASSEQ(dim, "0pi");
  dim = UT_convertInchesToDimensionString(DIM_PI, 1.0);
  TFPASSEQ(dim, "6pi");
  dim = UT_convertInchesToDimensionString(DIM_PT, 0.0);
  TFPASSEQ(dim, "0pt");
  dim = UT_convertInchesToDimensionString(DIM_PT, 1.0);
  TFPASSEQ(dim, "72pt");
  dim = UT_convertInchesToDimensionString(DIM_PX, 0.0);
  TFPASSEQ(dim, "0px");
  dim = UT_convertInchesToDimensionString(DIM_PX, 1.0);
  TFPASSEQ(dim, "72px");
  dim = UT_convertInchesToDimensionString(DIM_PERCENT, 0.0);
  TFPASSEQ(dim, "0.000000%");
  dim = UT_convertInchesToDimensionString(DIM_PERCENT, 1.0);
  TFPASSEQ(dim, "1.000000%");
  dim = UT_convertInchesToDimensionString(DIM_none, 0.0);
  TFPASSEQ(dim, "0.000000");
  dim = UT_convertInchesToDimensionString(DIM_none, 1.0);
  TFPASSEQ(dim, "1.000000");

  dim = UT_convertInchesToDimensionString(DIM_IN, 162.4/res, "3.2");
  TFPASSEQ(dim, "2.26in");

  dim = UT_convertInchesToDimensionString(DIM_IN, 12.0, nullptr);
  TFPASSEQ(dim, "12.0000in");

  dim = UT_convertInchesToDimensionString(DIM_MM, 10, ".2");
  TFPASSEQ(dim, "254.00mm");
  dim = UT_convertInchesToDimensionString(DIM_PERCENT, 1.0, ".0");
  TFPASSEQ(dim, "1%");
}

TFTEST_MAIN("UT_formatDimensionString works")
{
  double res = 72.0f;

  // defaults
  auto dim = UT_formatDimensionString(DIM_IN, 0.0);
  TFPASSEQ(dim, "0.0000in");
  dim = UT_formatDimensionString(DIM_IN, 1.0);
  TFPASSEQ(dim, "1.0000in");
  dim = UT_formatDimensionString(DIM_CM, 0.0);
  TFPASSEQ(dim, "0.00cm");
  dim = UT_formatDimensionString(DIM_CM, 1.0);
  TFPASSEQ(dim, "1.00cm");
  dim = UT_formatDimensionString(DIM_MM, 0.0);
  TFPASSEQ(dim, "0.0mm");
  dim = UT_formatDimensionString(DIM_MM, 1.0);
  TFPASSEQ(dim, "1.0mm");
  dim = UT_formatDimensionString(DIM_PI, 0.0);
  TFPASSEQ(dim, "0pi");
  dim = UT_formatDimensionString(DIM_PI, 1.0);
  TFPASSEQ(dim, "1pi");
  dim = UT_formatDimensionString(DIM_PT, 0.0);
  TFPASSEQ(dim, "0pt");
  dim = UT_formatDimensionString(DIM_PT, 1.0);
  TFPASSEQ(dim, "1pt");
  dim = UT_formatDimensionString(DIM_PX, 0.0);
  TFPASSEQ(dim, "0px");
  dim = UT_formatDimensionString(DIM_PX, 1.0);
  TFPASSEQ(dim, "1px");
  dim = UT_formatDimensionString(DIM_PERCENT, 0.0);
  TFPASSEQ(dim, "0.000000%");
  dim = UT_formatDimensionString(DIM_PERCENT, 1.0);
  TFPASSEQ(dim, "1.000000%");
  dim = UT_formatDimensionString(DIM_none, 0.0);
  TFPASSEQ(dim, "0.000000");
  dim = UT_formatDimensionString(DIM_none, 1.0);
  TFPASSEQ(dim, "1.000000");

  dim = UT_formatDimensionString(DIM_IN, 162.4/res, "3.2");
  TFPASSEQ(dim, "2.26in");

  dim = UT_formatDimensionString(DIM_IN, 12.0, nullptr);
  TFPASSEQ(dim, "12.0000in");

  dim = UT_formatDimensionString(DIM_MM, 10, ".2");
  TFPASSEQ(dim, "10.00mm");
  dim = UT_formatDimensionString(DIM_PERCENT, 1.0, ".0");
  TFPASSEQ(dim, "1%");
}

TFTEST_MAIN("UT_determineDimension")
{
	TFPASS(UT_determineDimension("1in") == DIM_IN);
	TFPASS(UT_determineDimension("1.5inch") == DIM_IN);
	TFPASS(UT_determineDimension("2cm") == DIM_CM);
	TFPASS(UT_determineDimension("25.4mm") == DIM_MM);
	TFPASS(UT_determineDimension("6pi") == DIM_PI);
	TFPASS(UT_determineDimension("12pt") == DIM_PT);
	TFPASS(UT_determineDimension("96px") == DIM_PX);
	TFPASS(UT_determineDimension("50%") == DIM_PERCENT);

	// case-insensitive and space-separated unit
	TFPASS(UT_determineDimension("10 CM") == DIM_CM);
	TFPASS(UT_determineDimension("3  in") == DIM_IN);

	// no unit -> caller's fallback (default DIM_IN)
	TFPASS(UT_determineDimension("42") == DIM_IN);
	TFPASS(UT_determineDimension("42", DIM_PT) == DIM_PT);

	// unknown unit -> fallback
	TFPASS(UT_determineDimension("5furlongs", DIM_MM) == DIM_MM);

	// a single decimal comma is normalised to a point
	TFPASS(UT_determineDimension("1,5cm") == DIM_CM);
	TFPASS(UT_convertDimensionless("1,5") > 1.49 && UT_convertDimensionless("1,5") < 1.51);
}

TFTEST_MAIN("UT_units conversions")
{
	// convertToInches across units
	TFPASS(UT_convertToInches("72pt") > 0.99 && UT_convertToInches("72pt") < 1.01);
	TFPASS(UT_convertToInches("2.54cm") > 0.99 && UT_convertToInches("2.54cm") < 1.01);
	TFPASS(UT_convertToInches("25.4mm") > 0.99 && UT_convertToInches("25.4mm") < 1.01);
	TFPASS(UT_convertToInches("6pi") > 0.99 && UT_convertToInches("6pi") < 1.01);
	TFPASS(UT_convertToInches("72px") > 0.99 && UT_convertToInches("72px") < 1.01);
	TFPASS(UT_convertToInches("") == 0.0);
	TFPASS(UT_convertToInches(nullptr) == 0.0);

	// convertToPoints
	TFPASS(UT_convertToPoints("1in") > 71.9 && UT_convertToPoints("1in") < 72.1);
	TFPASS(UT_convertToPoints("72pt") > 71.9 && UT_convertToPoints("72pt") < 72.1);

	// logical units = twips
	TFPASS(UT_convertToLogicalUnits("1in") == 1440);
	TFPASS(UT_convertToLogicalUnits("72pt") == 1440);

	// convertDimensions: 1 inch = 72 pt
	TFPASS(UT_convertDimensions(1.0, DIM_IN, DIM_PT) > 71.9 &&
		   UT_convertDimensions(1.0, DIM_IN, DIM_PT) < 72.1);

	// convertToDimension with/without conversion
	TFPASS(UT_convertToDimension("1in", DIM_CM) > 2.53 &&
		   UT_convertToDimension("1in", DIM_CM) < 2.55);
	TFPASS(UT_convertToDimension("2.54cm", DIM_CM) > 2.53 &&
		   UT_convertToDimension("2.54cm", DIM_CM) < 2.55);

	// convertDimensionless just reads the number
	TFPASS(UT_convertDimensionless("12.5pt") > 12.49 &&
		   UT_convertDimensionless("12.5pt") < 12.51);
}

TFTEST_MAIN("UT_units dimensioned strings")
{
	// reformat into a different unit
	TFPASSEQ(UT_reformatDimensionString(DIM_CM, "1in", ".2"), "2.54cm");
	TFPASSEQ(UT_reformatDimensionString(DIM_PT, "72pt", ".0"), "72pt");

	// increment/multiply keep the unit
	{
		std::string s = UT_incrementDimString("1.0in", 0.5);
		TFPASS(s == "1.5000in");
	}
	{
		std::string s = UT_multiplyDimString("2.0cm", 2.0);
		TFPASS(s == "4.00cm");
	}
	{
		std::string s = UT_formatDimensionedValue(1.5, "in", ".1");
		TFPASS(s == "1.5in");
	}
}

TFTEST_MAIN("UT_units misc helpers")
{
	// hasDimensionComponent
	TFPASS(UT_hasDimensionComponent("1.5in"));
	TFPASS(UT_hasDimensionComponent("42pt"));
	TFPASS(!UT_hasDimensionComponent("42"));
	TFPASS(!UT_hasDimensionComponent(nullptr));

	// paper units = 1/100 inch
	TFPASS(UT_paperUnits("1in") == 100);
	TFPASS(UT_paperUnits("8.5in") == 850);
	TFPASS(UT_paperUnits(nullptr) == 0);
	TFPASS(UT_paperUnits("") == 0);
	TFPASS(UT_paperUnitsFromInches(1.0) == 100);
	TFPASS(UT_inchesFromPaperUnits(100) > 0.99 &&
		   UT_inchesFromPaperUnits(100) < 1.01);

	// validity check on the numeric part (deliberately loose:
	// only the leading numeric run is validated)
	TFPASS(UT_isValidDimensionString("1.5in", 0));
	TFPASS(UT_isValidDimensionString("42", 0));
	TFPASS(!UT_isValidDimensionString("abc", 0));
	TFPASS(!UT_isValidDimensionString("in", 0));
	TFPASS(!UT_isValidDimensionString("", 0));
	// max_length is honoured
	TFPASS(!UT_isValidDimensionString("12345in", 4));
	TFPASS(UT_isValidDimensionString("1in", 4));

	// precision + resolution per unit
	TFPASS(UT_getDimensionPrecision(DIM_IN) == 2);
	TFPASS(UT_getDimensionPrecision(DIM_CM) == 1);
	TFPASS(UT_getDimensionPrecision(DIM_MM) == 0);
	TFPASS(UT_getDimensionPrecision(DIM_PI) == 0);
	TFPASS(UT_getDimensionPrecision(DIM_PT) == 0);
	TFPASS(UT_getDimensionPrecision(DIM_PX) == 0);

	TFPASS(UT_getDimensionResolution(DIM_IN) == 0.1);
	TFPASS(UT_getDimensionResolution(DIM_CM) == 0.5);
	TFPASS(UT_getDimensionResolution(DIM_PI) == 1);
	TFPASS(UT_getDimensionResolution(DIM_MM) == 5);
	TFPASS(UT_getDimensionResolution(DIM_PT) == 10);
	TFPASS(UT_getDimensionResolution(DIM_PX) == 10);
	TFPASS(UT_getDimensionResolution(DIM_PERCENT) == 1);

	// fractions: "50%" -> 0.5, plain numbers pass through
	TFPASS(UT_convertFraction("50%") > 0.49 && UT_convertFraction("50%") < 0.51);
	TFPASS(UT_convertFraction("0.25") > 0.24 && UT_convertFraction("0.25") < 0.26);
}
