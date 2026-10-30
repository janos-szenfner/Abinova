/* AbiWord
 * Copyright (C) 2026 AbiSource, Inc.
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

#pragma once

#include <cstring>

/* Word's Insert > Equation built-in gallery: stable id, display name
 * and the LaTeX source inserted.  Shared between the ribbon gallery
 * popover (ap_UnixRibbon.cpp, GTK side) and the insertEquation edit
 * method's "builtin:<id>" selector (ap_EditMethods.cpp, XP side). */
struct AP_EquationBuiltin
{
	const char * id;
	const char * name;
	const char * latex;
};

static const AP_EquationBuiltin AP_EQUATION_BUILTINS[] =
{
	{ "area",       "Area of Circle",
	  "A = \\pi r^2" },
	{ "binomial",   "Binomial Theorem",
	  "(x+a)^n = \\sum_{k=0}^{n} \\binom{n}{k} x^k a^{n-k}" },
	{ "expansion",  "Expansion of a Sum",
	  "(1+x)^n = 1 + \\frac{nx}{1!} + \\frac{n(n-1)x^2}{2!} + \\cdots" },
	{ "fourier",    "Fourier Series",
	  "f(x) = a_0 + \\sum_{n=1}^{\\infty} \\left( a_n \\cos \\frac{n\\pi x}{L} + b_n \\sin \\frac{n\\pi x}{L} \\right)" },
	{ "pythagoras", "Pythagorean Theorem",
	  "a^2 + b^2 = c^2" },
	{ "quadratic",  "Quadratic Formula",
	  "x = \\frac{-b \\pm \\sqrt{b^2 - 4ac}}{2a}" },
	{ "taylor",     "Taylor Expansion",
	  "e^x = \\sum_{n=0}^{\\infty} \\frac{x^n}{n!}" },
	{ "trig1",      "Trigonometric Identity",
	  "\\sin \\alpha \\pm \\sin \\beta = 2 \\sin \\frac{\\alpha \\pm \\beta}{2} \\cos \\frac{\\alpha \\mp \\beta}{2}" },
	{ "trig2",      "Trigonometric Identity 2",
	  "\\cos \\alpha + \\cos \\beta = 2 \\cos \\frac{\\alpha + \\beta}{2} \\cos \\frac{\\alpha - \\beta}{2}" },
	{ "gaussian",   "Gaussian Integral",
	  "\\int_0^{\\infty} e^{-x^2} dx = \\frac{\\sqrt{\\pi}}{2}" },
	{ "euler",      "Euler's Identity",
	  "e^{i\\pi} + 1 = 0" },
	{ "derivative", "Derivative as a Limit",
	  "f^{\\prime}(x) = \\lim_{h \\to 0} \\frac{f(x+h) - f(x)}{h}" },
	{ "normal",     "Normal Distribution",
	  "f(x) = \\frac{1}{\\sigma \\sqrt{2\\pi}} e^{-\\frac{(x-\\mu)^2}{2\\sigma^2}}" },
	{ "bayes",      "Bayes Theorem",
	  "P(A|B) = \\frac{P(B|A) \\, P(A)}{P(B)}" },
};

inline const AP_EquationBuiltin * ap_equationBuiltin(const char * szId)
{
	if (!szId)
		return nullptr;
	for (const AP_EquationBuiltin & b : AP_EQUATION_BUILTINS)
		if (!strcmp(szId, b.id))
			return &b;
	return nullptr;
}
