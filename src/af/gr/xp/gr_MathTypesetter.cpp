/* AbiWord
 * Copyright (C) 2025 AbiSource
 *
 * Self-contained mathematical typesetter for AbiWord equations.
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

#include "gr_MathTypesetter.h"

#include <cstring>
#include <cstdlib>
#include <cmath>
#include <libxml/parser.h>
#include <libxml/tree.h>

/* ------------------------------------------------------------------ */
/* symbol tables                                                       */
/* ------------------------------------------------------------------ */

struct MSym {
	const char *name;   /*!< command name without backslash, or entity name */
	const char *utf8;
	unsigned fl;
};

/* keep grouped; lookup is linear which is fine at this size */
static const MSym s_syms[] = {
	/* greek lowercase */
	{ "alpha", "\xce\xb1", MF_ITALIC }, { "beta", "\xce\xb2", MF_ITALIC },
	{ "gamma", "\xce\xb3", MF_ITALIC }, { "delta", "\xce\xb4", MF_ITALIC },
	{ "epsilon", "\xcf\xb5", MF_ITALIC }, { "varepsilon", "\xce\xb5", MF_ITALIC },
	{ "zeta", "\xce\xb6", MF_ITALIC }, { "eta", "\xce\xb7", MF_ITALIC },
	{ "theta", "\xce\xb8", MF_ITALIC }, { "vartheta", "\xcf\x91", MF_ITALIC },
	{ "iota", "\xce\xb9", MF_ITALIC }, { "kappa", "\xce\xba", MF_ITALIC },
	{ "lambda", "\xce\xbb", MF_ITALIC }, { "mu", "\xce\xbc", MF_ITALIC },
	{ "nu", "\xce\xbd", MF_ITALIC }, { "xi", "\xce\xbe", MF_ITALIC },
	{ "pi", "\xcf\x80", MF_ITALIC }, { "varpi", "\xcf\x96", MF_ITALIC },
	{ "rho", "\xcf\x81", MF_ITALIC }, { "varrho", "\xcf\xb1", MF_ITALIC },
	{ "sigma", "\xcf\x83", MF_ITALIC }, { "varsigma", "\xcf\x82", MF_ITALIC },
	{ "tau", "\xcf\x84", MF_ITALIC }, { "upsilon", "\xcf\x85", MF_ITALIC },
	{ "phi", "\xcf\x95", MF_ITALIC }, { "varphi", "\xcf\x86", MF_ITALIC },
	{ "chi", "\xcf\x87", MF_ITALIC }, { "psi", "\xcf\x88", MF_ITALIC },
	{ "omega", "\xcf\x89", MF_ITALIC },
	/* greek uppercase */
	{ "Gamma", "\xce\x93", 0 }, { "Delta", "\xce\x94", 0 },
	{ "Theta", "\xce\x98", 0 }, { "Lambda", "\xce\x9b", 0 },
	{ "Xi", "\xce\x9e", 0 }, { "Pi", "\xce\xa0", 0 },
	{ "Sigma", "\xce\xa3", 0 }, { "Upsilon", "\xce\xa5", 0 },
	{ "Phi", "\xce\xa6", 0 }, { "Psi", "\xce\xa8", 0 },
	{ "Omega", "\xce\xa9", 0 },
	/* binary operators */
	{ "pm", "\xc2\xb1", MF_BIN }, { "mp", "\xe2\x88\x93", MF_BIN },
	{ "times", "\xc3\x97", MF_BIN }, { "div", "\xc3\xb7", MF_BIN },
	{ "cdot", "\xe2\x8b\x85", MF_BIN }, { "ast", "\xe2\x88\x97", MF_BIN },
	{ "star", "\xe2\x8b\x86", MF_BIN }, { "circ", "\xe2\x88\x98", MF_BIN },
	{ "bullet", "\xe2\x88\x99", MF_BIN }, { "oplus", "\xe2\x8a\x95", MF_BIN },
	{ "ominus", "\xe2\x8a\x96", MF_BIN }, { "otimes", "\xe2\x8a\x97", MF_BIN },
	{ "oslash", "\xe2\x8a\x98", MF_BIN }, { "odot", "\xe2\x8a\x99", MF_BIN },
	{ "cap", "\xe2\x88\xa9", MF_BIN }, { "cup", "\xe2\x88\xaa", MF_BIN },
	{ "uplus", "\xe2\x8a\x8e", MF_BIN }, { "sqcap", "\xe2\x8a\x93", MF_BIN },
	{ "sqcup", "\xe2\x8a\x94", MF_BIN }, { "vee", "\xe2\x88\xa8", MF_BIN },
	{ "wedge", "\xe2\x88\xa7", MF_BIN }, { "setminus", "\xe2\x88\x96", MF_BIN },
	{ "wr", "\xe2\x89\x80", MF_BIN }, { "diamond", "\xe2\x8b\x84", MF_BIN },
	{ "bigtriangleup", "\xe2\x96\xb3", MF_BIN },
	{ "bigtriangledown", "\xe2\x96\xbd", MF_BIN },
	{ "triangleleft", "\xe2\x97\x83", MF_BIN },
	{ "triangleright", "\xe2\x96\xb9", MF_BIN },
	{ "lhd", "\xe2\x97\x81", MF_BIN }, { "rhd", "\xe2\x96\xb7", MF_BIN },
	{ "unlhd", "\xe2\x8a\xb4", MF_BIN }, { "unrhd", "\xe2\x8a\xb5", MF_BIN },
	{ "amalg", "\xe2\xa8\xbf", MF_BIN },
	/* relations */
	{ "leq", "\xe2\x89\xa4", MF_REL }, { "le", "\xe2\x89\xa4", MF_REL },
	{ "geq", "\xe2\x89\xa5", MF_REL }, { "ge", "\xe2\x89\xa5", MF_REL },
	{ "neq", "\xe2\x89\xa0", MF_REL }, { "ne", "\xe2\x89\xa0", MF_REL },
	{ "equiv", "\xe2\x89\xa1", MF_REL }, { "approx", "\xe2\x89\x88", MF_REL },
	{ "cong", "\xe2\x89\x85", MF_REL }, { "sim", "\xe2\x88\xbc", MF_REL },
	{ "simeq", "\xe2\x89\x83", MF_REL }, { "propto", "\xe2\x88\x9d", MF_REL },
	{ "models", "\xe2\x8a\xa8", MF_REL }, { "perp", "\xe2\x8a\xa5", MF_REL },
	{ "mid", "\xe2\x88\xa3", MF_REL }, { "parallel", "\xe2\x88\xa5", MF_REL },
	{ "nmid", "\xe2\x88\xa4", MF_REL }, { "bowtie", "\xe2\x8b\x88", MF_REL },
	{ "Join", "\xe2\x8b\x88", MF_REL }, { "smile", "\xe2\x8c\xa3", MF_REL },
	{ "frown", "\xe2\x8c\xa2", MF_REL }, { "prec", "\xe2\x89\xba", MF_REL },
	{ "succ", "\xe2\x89\xbb", MF_REL }, { "preceq", "\xe2\xaa\xaf", MF_REL },
	{ "succeq", "\xe2\xaa\xb0", MF_REL }, { "ll", "\xe2\x89\xaa", MF_REL },
	{ "gg", "\xe2\x89\xab", MF_REL }, { "subset", "\xe2\x8a\x82", MF_REL },
	{ "supset", "\xe2\x8a\x83", MF_REL }, { "subseteq", "\xe2\x8a\x86", MF_REL },
	{ "supseteq", "\xe2\x8a\x87", MF_REL }, { "nsubseteq", "\xe2\x8a\x88", MF_REL },
	{ "nsupseteq", "\xe2\x8a\x89", MF_REL }, { "sqsubseteq", "\xe2\x8a\x91", MF_REL },
	{ "sqsupseteq", "\xe2\x8a\x92", MF_REL }, { "in", "\xe2\x88\x88", MF_REL },
	{ "ni", "\xe2\x88\x8b", MF_REL }, { "notin", "\xe2\x88\x89", MF_REL },
	{ "vdash", "\xe2\x8a\xa2", MF_REL }, { "dashv", "\xe2\x8a\xa3", MF_REL },
	{ "asymp", "\xe2\x89\x8d", MF_REL }, { "doteq", "\xe2\x89\x90", MF_REL },
	{ "lt", "<", MF_REL }, { "gt", ">", MF_REL },
	/* arrows */
	{ "leftarrow", "\xe2\x86\x90", MF_REL }, { "gets", "\xe2\x86\x90", MF_REL },
	{ "rightarrow", "\xe2\x86\x92", MF_REL }, { "to", "\xe2\x86\x92", MF_REL },
	{ "uparrow", "\xe2\x86\x91", MF_REL }, { "downarrow", "\xe2\x86\x93", MF_REL },
	{ "leftrightarrow", "\xe2\x86\x94", MF_REL },
	{ "updownarrow", "\xe2\x86\x95", MF_REL },
	{ "Leftarrow", "\xe2\x87\x90", MF_REL }, { "Rightarrow", "\xe2\x87\x92", MF_REL },
	{ "Uparrow", "\xe2\x87\x91", MF_REL }, { "Downarrow", "\xe2\x87\x93", MF_REL },
	{ "Leftrightarrow", "\xe2\x87\x94", MF_REL },
	{ "Updownarrow", "\xe2\x87\x95", MF_REL },
	{ "mapsto", "\xe2\x86\xa6", MF_REL },
	{ "hookleftarrow", "\xe2\x86\xa9", MF_REL },
	{ "hookrightarrow", "\xe2\x86\xaa", MF_REL },
	{ "leftharpoonup", "\xe2\x86\xbc", MF_REL },
	{ "rightharpoonup", "\xe2\x87\x80", MF_REL },
	{ "rightleftharpoons", "\xe2\x87\x8c", MF_REL },
	{ "longleftarrow", "\xe2\x9f\xb5", MF_REL },
	{ "longrightarrow", "\xe2\x9f\xb6", MF_REL },
	{ "longleftrightarrow", "\xe2\x9f\xb7", MF_REL },
	{ "Longrightarrow", "\xe2\x9f\xb9", MF_REL },
	{ "Longleftarrow", "\xe2\x9f\xb8", MF_REL },
	{ "Longleftrightarrow", "\xe2\x9f\xba", MF_REL },
	{ "nearrow", "\xe2\x86\x97", MF_REL }, { "searrow", "\xe2\x86\x98", MF_REL },
	{ "swarrow", "\xe2\x86\x99", MF_REL }, { "nwarrow", "\xe2\x86\x96", MF_REL },
	/* misc symbols */
	{ "infty", "\xe2\x88\x9e", 0 }, { "partial", "\xe2\x88\x82", MF_ITALIC },
	{ "nabla", "\xe2\x88\x87", 0 }, { "forall", "\xe2\x88\x80", 0 },
	{ "exists", "\xe2\x88\x83", 0 }, { "nexists", "\xe2\x88\x84", 0 },
	{ "emptyset", "\xe2\x88\x85", 0 }, { "varnothing", "\xe2\x88\x85", 0 },
	{ "neg", "\xc2\xac", 0 }, { "lnot", "\xc2\xac", 0 },
	{ "flat", "\xe2\x99\xad", 0 }, { "natural", "\xe2\x99\xae", 0 },
	{ "sharp", "\xe2\x99\xaf", 0 }, { "aleph", "\xe2\x84\xb5", 0 },
	{ "hbar", "\xe2\x84\x8f", 0 }, { "imath", "\xc4\xb1", MF_ITALIC },
	{ "jmath", "\xc8\xb7", MF_ITALIC }, { "ell", "\xe2\x84\x93", MF_ITALIC },
	{ "wp", "\xe2\x84\x98", MF_ITALIC }, { "Re", "\xe2\x84\x9c", 0 },
	{ "Im", "\xe2\x84\x91", 0 }, { "prime", "\xe2\x80\xb2", 0 },
	{ "angle", "\xe2\x88\xa0", 0 }, { "measuredangle", "\xe2\x88\xa1", 0 },
	{ "triangle", "\xe2\x96\xb5", 0 }, { "square", "\xe2\x96\xa1", 0 },
	{ "blacksquare", "\xe2\x96\xa0", 0 }, { "lozenge", "\xe2\x97\x8a", 0 },
	{ "cdots", "\xe2\x8b\xaf", 0 }, { "ldots", "\xe2\x80\xa6", 0 },
	{ "dots", "\xe2\x80\xa6", 0 }, { "vdots", "\xe2\x8b\xae", 0 },
	{ "ddots", "\xe2\x8b\xb1", 0 }, { "degree", "\xc2\xb0", 0 },
	{ "top", "\xe2\x8a\xa4", 0 }, { "bot", "\xe2\x8a\xa5", 0 },
	{ "S", "\xc2\xa7", 0 }, { "P", "\xc2\xb6", 0 },
	{ "copyright", "\xc2\xa9", 0 }, { "therefore", "\xe2\x88\xb4", 0 },
	{ "because", "\xe2\x88\xb5", 0 }, { "surd", "\xe2\x88\x9a", 0 },
	{ "checkmark", "\xe2\x9c\x93", 0 }, { "maltese", "\xe2\x9c\xa0", 0 },
	{ "circledR", "\xc2\xae", 0 }, { "circledS", "\xe2\x93\xa2", 0 },
	{ "complement", "\xe2\x88\x81", 0 }, { "hslash", "\xe2\x84\x8f", 0 },
	{ "mho", "\xe2\x84\xa7", 0 }, { "Finv", "\xe2\x84\xb2", 0 },
	{ "Game", "\xe2\x84\x81", 0 }, { "eth", "\xc3\xb0", MF_ITALIC },
	{ "Bbbk", "\xf0\x9d\x95\x9c", 0 },
	/* large operators (limits where noted) */
	{ "sum", "\xe2\x88\x91", MF_BIGOP | MF_LIMITS },
	{ "prod", "\xe2\x88\x8f", MF_BIGOP | MF_LIMITS },
	{ "coprod", "\xe2\x88\x90", MF_BIGOP | MF_LIMITS },
	{ "int", "\xe2\x88\xab", MF_BIGOP },
	{ "iint", "\xe2\x88\xac", MF_BIGOP },
	{ "iiint", "\xe2\x88\xad", MF_BIGOP },
	{ "oint", "\xe2\x88\xae", MF_BIGOP },
	{ "bigcup", "\xe2\x8b\x83", MF_BIGOP | MF_LIMITS },
	{ "bigcap", "\xe2\x8b\x82", MF_BIGOP | MF_LIMITS },
	{ "bigvee", "\xe2\x8b\x81", MF_BIGOP | MF_LIMITS },
	{ "bigwedge", "\xe2\x8b\x80", MF_BIGOP | MF_LIMITS },
	{ "bigoplus", "\xe2\xa8\x81", MF_BIGOP | MF_LIMITS },
	{ "bigotimes", "\xe2\xa8\x82", MF_BIGOP | MF_LIMITS },
	{ "bigodot", "\xe2\xa8\x99", MF_BIGOP | MF_LIMITS },
	{ "biguplus", "\xe2\xa8\x84", MF_BIGOP | MF_LIMITS },
	{ "bigsqcup", "\xe2\xa8\x86", MF_BIGOP | MF_LIMITS },
	{ "lim", "lim", MF_UPRIGHT | MF_BIGOP | MF_LIMITS },
	{ "limsup", "lim sup", MF_UPRIGHT | MF_BIGOP | MF_LIMITS },
	{ "liminf", "lim inf", MF_UPRIGHT | MF_BIGOP | MF_LIMITS },
	{ "sup", "sup", MF_UPRIGHT | MF_BIGOP | MF_LIMITS },
	{ "inf", "inf", MF_UPRIGHT | MF_BIGOP | MF_LIMITS },
	{ "max", "max", MF_UPRIGHT | MF_BIGOP | MF_LIMITS },
	{ "min", "min", MF_UPRIGHT | MF_BIGOP | MF_LIMITS },
	{ "det", "det", MF_UPRIGHT | MF_BIGOP | MF_LIMITS },
	{ "gcd", "gcd", MF_UPRIGHT | MF_BIGOP | MF_LIMITS },
	/* named functions: upright, treated as inner so no bin spacing */
	{ "sin", "sin", MF_UPRIGHT | MF_INNER }, { "cos", "cos", MF_UPRIGHT | MF_INNER },
	{ "tan", "tan", MF_UPRIGHT | MF_INNER }, { "sec", "sec", MF_UPRIGHT | MF_INNER },
	{ "csc", "csc", MF_UPRIGHT | MF_INNER }, { "cot", "cot", MF_UPRIGHT | MF_INNER },
	{ "arcsin", "arcsin", MF_UPRIGHT | MF_INNER },
	{ "arccos", "arccos", MF_UPRIGHT | MF_INNER },
	{ "arctan", "arctan", MF_UPRIGHT | MF_INNER },
	{ "sinh", "sinh", MF_UPRIGHT | MF_INNER }, { "cosh", "cosh", MF_UPRIGHT | MF_INNER },
	{ "tanh", "tanh", MF_UPRIGHT | MF_INNER }, { "coth", "coth", MF_UPRIGHT | MF_INNER },
	{ "ln", "ln", MF_UPRIGHT | MF_INNER }, { "lg", "lg", MF_UPRIGHT | MF_INNER },
	{ "log", "log", MF_UPRIGHT | MF_INNER }, { "exp", "exp", MF_UPRIGHT | MF_INNER },
	{ "dim", "dim", MF_UPRIGHT | MF_INNER }, { "mod", "mod", MF_UPRIGHT | MF_INNER },
	{ "arg", "arg", MF_UPRIGHT | MF_INNER }, { "hom", "hom", MF_UPRIGHT | MF_INNER },
	{ "ker", "ker", MF_UPRIGHT | MF_INNER }, { "deg", "deg", MF_UPRIGHT | MF_INNER },
	{ "Pr", "Pr", MF_UPRIGHT | MF_INNER }, { "bmod", "mod", MF_UPRIGHT | MF_BIN },
	{ "pmod", "mod", MF_UPRIGHT | MF_INNER },
};

static const MSym *_findSym(const std::string &name)
{
	for (size_t i = 0; i < G_N_ELEMENTS(s_syms); ++i)
		if (name == s_syms[i].name)
			return &s_syms[i];
	return nullptr;
}

/* double-struck letters for \mathbb */
static const char *_bbChar(char c)
{
	switch (c) {
	case 'C': return "\xe2\x84\x82"; case 'H': return "\xe2\x84\x8d";
	case 'N': return "\xe2\x84\x95"; case 'P': return "\xe2\x84\x99";
	case 'Q': return "\xe2\x84\x9a"; case 'R': return "\xe2\x84\x9d";
	case 'Z': return "\xe2\x84\xa4";
	default: return nullptr;
	}
}

/* ------------------------------------------------------------------ */
/* LaTeX parser                                                        */
/* ------------------------------------------------------------------ */

struct MLatexParser {
	const char *s;
	size_t n, i;
	GR_MathTypesetter *ts;
	unsigned style;         /*!< inherited MF_ flags (BOLD/UPRIGHT/ITALIC) */
	std::string err;

	MLatexParser(const char *str, GR_MathTypesetter *t)
		: s(str), n(str ? strlen(str) : 0), i(0), ts(t), style(0) {}

	char peek()  { return i < n ? s[i] : 0; }
	char get()   { return i < n ? s[i++] : 0; }
	void skipws(){ while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n')) ++i; }

	bool atCmd(const char *word) {
		size_t l = strlen(word);
		return i + l <= n && s[i] == '\\' && strncmp(s + i + 1, word, l) == 0 &&
		       (i + 1 + l >= n || !isalpha((unsigned char)s[i + 1 + l]));
	}
	void eatCmd(const char *word) { i += 1 + strlen(word); }

	std::string cmdName() {
		/* cursor is at '\\' */
		++i;
		std::string nm;
		if (i < n && isalpha((unsigned char)s[i])) {
			while (i < n && isalpha((unsigned char)s[i])) nm += s[i++];
		} else if (i < n) {
			nm += s[i++];          /* single-char command: \{ \\ \, etc */
		}
		return nm;
	}

	/* parse a {...} group or a single token into a node */
	MNode *arg() {
		skipws();
		if (peek() == '{') {
			++i;
			MNode *r = row('}');
			if (peek() == '}') ++i;
			return r;
		}
		if (peek() == '\\') {
			return command();
		}
		if (peek()) {
			return charAtom(get());
		}
		return new MNode(MNode::ROW);
	}

	MNode *charAtom(char c) {
		MNode *a = new MNode(MNode::ATOM);
		char buf[8];
		if (c < 0x80) {
			buf[0] = c; buf[1] = 0;
		} else {
			buf[0] = c; buf[1] = 0; /* raw byte; latex input is utf-8 so copy through */
		}
		a->t = buf;
		if (isalpha((unsigned char)c)) a->fl |= MF_ITALIC;
		a->fl |= style;
		return a;
	}

	MNode *command() {
		std::string nm = cmdName();
		if (nm.empty()) return new MNode(MNode::ROW);

		const MSym *sym = _findSym(nm);
		if (sym) {
			MNode *a = new MNode(MNode::ATOM);
			a->t = sym->utf8;
			a->fl = sym->fl | style;
			return a;
		}

		if (nm == "frac" || nm == "dfrac" || nm == "tfrac" || nm == "cfrac") {
			MNode *f = new MNode(MNode::FRAC);
			f->k.push_back(arg());
			f->k.push_back(arg());
			f->fl = style;
			return f;
		}
		if (nm == "binom" || nm == "dbinom" || nm == "tbinom") {
			MNode *f = new MNode(MNode::FRAC);
			f->fl = MF_NOBAR | style;
			f->k.push_back(arg());
			f->k.push_back(arg());
			MNode *d = new MNode(MNode::FENCE);
			d->t = "("; d->t2 = ")";
			d->k.push_back(f);
			return d;
		}
		if (nm == "sqrt") {
			MNode *r = new MNode(MNode::RADICAL);
			skipws();
			if (peek() == '[') {
				++i;
				r->k.push_back(row(']'));   /* index stored second */
				if (peek() == ']') ++i;
				r->k.push_back(arg());
				std::swap(r->k[0], r->k[1]);
			} else {
				r->k.push_back(arg());
			}
			return r;
		}
		if (nm == "left") {
			return fence();
		}
		if (nm == "right") {      /* stray \right: skip it */
			skipws(); get();
			return new MNode(MNode::ROW);
		}
		if (nm == "overline" || nm == "underline" || nm == "bar" ||
		    nm == "vec" || nm == "hat" || nm == "widehat" ||
		    nm == "tilde" || nm == "widetilde" || nm == "dot" ||
		    nm == "ddot" || nm == "breve" || nm == "check" ||
		    nm == "acute" || nm == "grave") {
			MNode *ac = new MNode(MNode::ACCENT);
			if (nm == "widehat") nm = "hat";
			if (nm == "widetilde") nm = "tilde";
			ac->t = nm.c_str();
			ac->k.push_back(arg());
			return ac;
		}
		if (nm == "overset" || nm == "stackrel") {
			MNode *l = new MNode(MNode::LIMITS);
			MNode *over = arg();
			l->k.push_back(arg());   /* base */
			l->k.push_back(over);
			l->aux = 1;              /* has over */
			return l;
		}
		if (nm == "underset") {
			MNode *l = new MNode(MNode::LIMITS);
			MNode *under = arg();
			l->k.push_back(arg());
			l->k.push_back(under);
			l->aux = 2;              /* has under */
			return l;
		}
		if (nm == "boxed") {
			MNode *d = new MNode(MNode::FENCE);
			d->aux = 1;
			d->k.push_back(arg());
			return d;
		}
		if (nm == "text" || nm == "textrm" || nm == "textbf" ||
		    nm == "textit" || nm == "textsf" || nm == "texttt" ||
		    nm == "operatorname" || nm == "mathrm" || nm == "mathbf" ||
		    nm == "mathit" || nm == "mathsf" || nm == "mathtt" ||
		    nm == "mathcal" || nm == "mathbb" || nm == "mathfrak" ||
		    nm == "boldsymbol" || nm == "mathnormal" || nm == "hbox" ||
		    nm == "mbox") {
			unsigned st = MF_UPRIGHT;
			if (nm == "mathbf" || nm == "textbf" || nm == "boldsymbol") st = MF_BOLD;
			if (nm == "mathit" || nm == "textit") st = MF_ITALIC;
			if (nm == "operatorname" || nm == "mathrm" || nm == "textrm" ||
			    nm == "hbox" || nm == "mbox") st = MF_UPRIGHT;
			return styledGroup(st | style, nm == "mathbb");
		}
		if (nm == "begin") {
			return matrix();
		}
		if (nm == "left" /* handled above */) {}
		if (nm == "big" || nm == "Big" || nm == "bigl" || nm == "Bigl" ||
		    nm == "bigr" || nm == "Bigr" || nm == "bigg" || nm == "Bigg" ||
		    nm == "biggl" || nm == "Biggl" || nm == "biggr" || nm == "Biggr" ||
		    nm == "bigm" || nm == "Bigm" || nm == "biggm" || nm == "Biggm") {
			return arg();            /* v1: delimiter at natural size */
		}
		if (nm == "displaystyle" || nm == "textstyle" ||
		    nm == "scriptstyle" || nm == "scriptscriptstyle" ||
		    nm == "limits" || nm == "nolimits" || nm == "nonumber" ||
		    nm == "displaystyle" || nm == "medspace" || nm == "thickspace") {
			return new MNode(MNode::ROW);   /* styling hint: ignore */
		}
		if (nm == "," || nm == "thinspace")  return spaceNode(17);
		if (nm == ";" || nm == "thickspace") return spaceNode(22);
		if (nm == ":" || nm == "medspace")   return spaceNode(22);
		if (nm == "!")   return spaceNode(-17);
		if (nm == " " || nm == "~" || nm == "nbsp") return spaceNode(30);
		if (nm == "quad")  return spaceNode(100);
		if (nm == "qquad") return spaceNode(200);
		if (nm == "enspace") return spaceNode(50);
		if (nm == "hspace") { arg(); return spaceNode(100); }
		if (nm == "color" || nm == "colorbox" || nm == "fcolorbox" ||
		    nm == "definecolor" || nm == "pagecolor") {
			arg();                    /* swallow the colour spec */
			if (nm == "colorbox" || nm == "fcolorbox") return arg();
			return new MNode(MNode::ROW);
		}
		if (nm == "phantom") {       /* reserve space, draw nothing */
			MNode *in = arg();
			MNode *sp = new MNode(MNode::SPACE);
			sp->k.push_back(in);     /* measured via child, not drawn */
			return sp;
		}
		if (nm == "over") {          /* infix fraction */
			MNode *a = new MNode(MNode::ATOM);
			a->t = "/"; a->fl = MF_BIN | style;
			return a;
		}
		if (nm == "{" || nm == "lbrace") { MNode *a = charAtom('{'); a->fl = MF_OPEN | style; return a; }
		if (nm == "}" || nm == "rbrace") { MNode *a = charAtom('}'); a->fl = MF_CLOSE | style; return a; }
		if (nm == "lbrack") { MNode *a = charAtom('['); a->fl = MF_OPEN | style; return a; }
		if (nm == "rbrack") { MNode *a = charAtom(']'); a->fl = MF_CLOSE | style; return a; }
		if (nm == "lvert" || nm == "vert" || nm == "|" || nm == "rvert") {
			MNode *a = charAtom('|'); a->fl = style; return a;
		}
		if (nm == "lVert" || nm == "Vert" || nm == "rVert" || nm == "parallel") {
			MNode *a = new MNode(MNode::ATOM); a->t = "\xe2\x88\xa5"; a->fl = style; return a;
		}
		if (nm == "langle") { MNode *a = new MNode(MNode::ATOM); a->t = "\xe2\x9f\xa8"; a->fl = MF_OPEN | style; return a; }
		if (nm == "rangle") { MNode *a = new MNode(MNode::ATOM); a->t = "\xe2\x9f\xa9"; a->fl = MF_CLOSE | style; return a; }
		if (nm == "lceil")  { MNode *a = new MNode(MNode::ATOM); a->t = "\xe2\x8c\x88"; a->fl = MF_OPEN | style; return a; }
		if (nm == "rceil")  { MNode *a = new MNode(MNode::ATOM); a->t = "\xe2\x8c\x89"; a->fl = MF_CLOSE | style; return a; }
		if (nm == "lfloor") { MNode *a = new MNode(MNode::ATOM); a->t = "\xe2\x8c\x8a"; a->fl = MF_OPEN | style; return a; }
		if (nm == "rfloor") { MNode *a = new MNode(MNode::ATOM); a->t = "\xe2\x8c\x8b"; a->fl = MF_CLOSE | style; return a; }
		if (nm == "backslash") { MNode *a = charAtom('\\'); a->fl = style; return a; }
		if (nm == "%") { MNode *a = charAtom('%'); a->fl = style; return a; }
		if (nm == "$") { MNode *a = charAtom('$'); a->fl = style; return a; }
		if (nm == "#") { MNode *a = charAtom('#'); a->fl = style; return a; }
		if (nm == "&") { MNode *a = charAtom('&'); a->fl = style; return a; }
		if (nm == "_") { MNode *a = charAtom('_'); a->fl = style; return a; }
		if (nm == "newline" || nm == "\\" ) return new MNode(MNode::ROW);
		if (nm == "leftroot" || nm == "uproot") { arg(); return new MNode(MNode::ROW); }

		/* unknown command: render its name so nothing silently vanishes */
		MNode *a = new MNode(MNode::ATOM);
		a->t = nm.c_str();
		a->fl = MF_UPRIGHT | style;
		return a;
	}

	MNode *spaceNode(int em100) {
		MNode *sp = new MNode(MNode::SPACE);
		sp->aux = em100;
		return sp;
	}

	/* group whose atoms get the given style flags instead of defaults */
	MNode *styledGroup(unsigned st, bool bb) {
		skipws();
		if (peek() != '{') return arg();
		++i;
		MNode *r = new MNode(MNode::ROW);
		while (i < n && peek() != '}') {
			char c = peek();
			if (c == ' ' || c == '\t' || c == '\n') {
				++i;
				r->k.push_back(spaceNode(30));
				continue;
			}
			if (c == '\\') {
				unsigned old = style;
				style = st;
				r->k.push_back(command());
				style = old;
				continue;
			}
			if (!c) break;
			MNode *a = new MNode(MNode::ATOM);
			if (bb && isalpha((unsigned char)c) && _bbChar(c)) {
				a->t = _bbChar(c);
			} else {
				char buf[2] = { c, 0 };
				a->t = buf;
			}
			a->fl = st;
			++i;
			r->k.push_back(a);
		}
		if (peek() == '}') ++i;
		return r;
	}

	/* \left X <row> \right Y */
	MNode *fence() {
		skipws();
		char open = get();            /* delimiter char or '.' */
		if (open == '\\') {           /* \{, \langle, \| ... */
			std::string nm = cmdName();
			if (nm == "{") open = '{';
			else if (nm == "langle") open = '<';
			else if (nm == "|" || nm == "Vert") open = 'B';   /* double bar */
			else if (nm == "vert") open = '|';
			else if (nm == "lceil") open = 'L';
			else if (nm == "lfloor") open = 'F';
			else if (nm == "rbrace") open = '}';
			else open = '(';
		}
		MNode *inner = row(-1);       /* row() stops at \right */
		skipws();
		char close = '.';
		if (atCmd("right")) {
			eatCmd("right");
			skipws();
			close = get();
			if (close == '\\') {
				std::string nm = cmdName();
				if (nm == "}") close = '}';
				else if (nm == "rangle") close = '>';
				else if (nm == "|" || nm == "Vert") close = 'B';
				else if (nm == "vert") close = '|';
				else if (nm == "rceil") close = 'R';
				else if (nm == "rfloor") close = 'G';
				else close = ')';
			}
		}
		MNode *d = new MNode(MNode::FENCE);
		char b[2] = { open, 0 };
		d->t = b;
		b[0] = close; b[1] = 0;
		d->t2 = b;
		d->k.push_back(inner);
		return d;
	}

	/* \begin{env} rows & cols \end{env} */
	MNode *matrix() {
		skipws();
		std::string env;
		if (peek() == '{') {
			++i;
			while (i < n && peek() != '}') env += s[i++];
			if (peek() == '}') ++i;
		}
		std::string colspec;
		if (env == "array") {         /* \begin{array}{cc} */
			skipws();
			if (peek() == '{') {
				++i;
				while (i < n && peek() != '}') colspec += s[i++];
				if (peek() == '}') ++i;
			}
		}
		MNode *m = new MNode(MNode::MATRIX);
		if (env == "pmatrix")      { m->t = "("; m->t2 = ")"; }
		else if (env == "bmatrix") { m->t = "["; m->t2 = "]"; }
		else if (env == "Bmatrix") { m->t = "{"; m->t2 = "}"; }
		else if (env == "vmatrix") { m->t = "|"; m->t2 = "|"; }
		else if (env == "Vmatrix") { m->t = "B"; m->t2 = "B"; }
		else if (env == "cases")   { m->t = "{"; m->t2 = "."; m->fl |= 0x10000; /*left-align*/ }
		else if (env == "aligned" || env == "align" || env == "alignedat" ||
		         env == "array" || env == "matrix" || env == "smallmatrix") {}
		else                       { m->t = "("; m->t2 = ")"; }

		/* parse cells: rows separated by \\, cols by & */
		int ncol = 0, cur = 0;
		while (i < n) {
			skipws();
			if (atCmd("end")) break;
			if (peek() == '&') { ++i; continue; }
			if (atCmd("\\") || atCmd("newline")) { eatCmd("\\"); cur = 0;
				continue; }
			MNode *cell = new MNode(MNode::ROW);
			while (i < n && peek() != '&' && !atCmd("\\") && !atCmd("end")) {
				skipws();
				if (peek() == '&' || atCmd("\\") || atCmd("end")) break;
				cell->k.push_back(atomWithScripts());
			}
			m->k.push_back(cell);
			++cur;
			if (cur > ncol) ncol = cur;
		}
		if (atCmd("end")) {
			eatCmd("end");
			skipws();
			if (peek() == '{') {      /* swallow env name */
				while (i < n && get() != '}') {}
			}
		}
		m->aux = ncol > 0 ? ncol : 1;
		return m;
	}

	/* atom + following ^ _ scripts */
	MNode *atomWithScripts() {
		skipws();
		MNode *base;
		if (peek() == '{') { ++i; base = row('}'); if (peek() == '}') ++i; }
		else if (peek() == '\\') base = command();
		else if (peek() == '(' || peek() == '[') {
			base = charAtom(get()); base->fl |= MF_OPEN;
		}
		else if (peek() == ')' || peek() == ']') {
			base = charAtom(get()); base->fl |= MF_CLOSE;
		}
		else if (peek() == '^' || peek() == '_') {
			base = new MNode(MNode::ROW);   /* scripts with empty base */
		}
		else if (peek()) base = charAtom(get());
		else return new MNode(MNode::ROW);

		MNode *sup = nullptr, *sub = nullptr;
		for (;;) {
			skipws();
			if (peek() == '^') { ++i; if (!sup) sup = arg(); else delete arg(); }
			else if (peek() == '_') { ++i; if (!sub) sub = arg(); else delete arg(); }
			else break;
		}
		if (!sup && !sub) return base;

		bool lim = (base->kind == MNode::ATOM) && (base->fl & MF_LIMITS) &&
		           !(base->fl & MF_INNER);
		if (lim) {
			MNode *l = new MNode(MNode::LIMITS);
			l->k.push_back(base);
			if (sup) { l->k.push_back(sup); l->aux |= 1; }
			if (sub) { l->k.push_back(sub); l->aux |= 2; }
			return l;
		}
		MNode *sc = new MNode(MNode::SCRIPT);
		sc->k.push_back(base);
		if (sup) { sc->k.push_back(sup); sc->aux |= 1; }
		if (sub) { sc->k.push_back(sub); sc->aux |= 2; }
		return sc;
	}

	/* parse a row until 'stop' char, or -1 for \right, or EOF */
	MNode *row(int stop) {
		MNode *r = new MNode(MNode::ROW);
		while (i < n) {
			skipws();
			char c = peek();
			if (!c) break;
			if (stop >= 0 && c == (char)stop) break;
			if (stop == -1 && atCmd("right")) break;
			if (c == '&' || atCmd("\\") || atCmd("end")) break;   /* matrix context */
			if (c == '{' ) { ++i; MNode *g = row('}'); if (peek() == '}') ++i;
				g = attachScripts(g); r->k.push_back(g); continue; }
			r->k.push_back(atomWithScripts());
		}
		return r;
	}

	MNode *attachScripts(MNode *base) {
		MNode *sup = nullptr, *sub = nullptr;
		for (;;) {
			skipws();
			if (peek() == '^') { ++i; if (!sup) sup = arg(); else delete arg(); }
			else if (peek() == '_') { ++i; if (!sub) sub = arg(); else delete arg(); }
			else break;
		}
		if (!sup && !sub) return base;
		MNode *sc = new MNode(MNode::SCRIPT);
		sc->k.push_back(base);
		if (sup) { sc->k.push_back(sup); sc->aux |= 1; }
		if (sub) { sc->k.push_back(sub); sc->aux |= 2; }
		return sc;
	}
};

/* ------------------------------------------------------------------ */
/* MathML parser                                                       */
/* ------------------------------------------------------------------ */

struct MMLParser {
	GR_MathTypesetter *ts;
	unsigned style;

	MMLParser(GR_MathTypesetter *t) : ts(t), style(0) {}

	static std::string text(xmlNode *e) {
		std::string out;
		for (xmlNode *c = e->children; c; c = c->next) {
			if (c->type == XML_TEXT_NODE || c->type == XML_CDATA_SECTION_NODE)
				out += (const char *)c->content;
		}
		/* collapse whitespace */
		std::string r;
		bool sp = false;
		for (char ch : out) {
			if (isspace((unsigned char)ch)) { sp = true; continue; }
			if (sp && !r.empty()) r += ' ';
			sp = false; r += ch;
		}
		return r;
	}

	static const char *attr(xmlNode *e, const char *nm) {
		xmlChar *v = xmlGetProp(e, BAD_CAST nm);
		if (!v) return nullptr;
		static thread_local std::string s;
		s = (const char *)v;
		xmlFree(v);
		return s.c_str();
	}

	MNode *atom(xmlNode *e, unsigned fl) {
		MNode *a = new MNode(MNode::ATOM);
		std::string t = text(e);
		a->t = t.c_str();
		a->fl = fl | style;
		const char *mv = attr(e, "mathvariant");
		if (mv) {
			if (!strcmp(mv, "bold")) a->fl = (a->fl & ~MF_ITALIC) | MF_BOLD;
			else if (!strcmp(mv, "italic")) a->fl |= MF_ITALIC;
			else if (!strcmp(mv, "bold-italic")) a->fl |= MF_BOLD | MF_ITALIC;
			else if (!strcmp(mv, "normal")) a->fl &= ~MF_ITALIC;
			else if (!strcmp(mv, "double-struck")) a->fl &= ~MF_ITALIC;
		}
		return a;
	}

	bool bigopChar(const std::string &t) {
		static const char *ops[] = {
			"\xe2\x88\x91", "\xe2\x88\x8f", "\xe2\x88\x90", "\xe2\x8b\x83",
			"\xe2\x8b\x82", "\xe2\x8b\x81", "\xe2\x8b\x80", "\xe2\xa8\x81",
			"\xe2\xa8\x82", "\xe2\x8b\x84", nullptr };
		for (int i = 0; ops[i]; ++i)
			if (t == ops[i]) return true;
		return false;
	}

	MNode *elem(xmlNode *e) {
		if (!e || e->type != XML_ELEMENT_NODE) return new MNode(MNode::ROW);
		const char *nm = (const char *)e->name;

		if (!strcmp(nm, "math") || !strcmp(nm, "mrow") ||
		    !strcmp(nm, "mstyle") || !strcmp(nm, "mpadded") ||
		    !strcmp(nm, "merror") || !strcmp(nm, "semantics") ||
		    !strcmp(nm, "maction")) {
			unsigned old = style;
			if (!strcmp(nm, "mstyle")) {
				const char *mv = attr(e, "mathvariant");
				if (mv && !strcmp(mv, "bold")) style |= MF_BOLD;
			}
			MNode *r = new MNode(MNode::ROW);
			for (xmlNode *c = e->children; c; c = c->next)
				if (c->type == XML_ELEMENT_NODE)
					r->k.push_back(elem(c));
			style = old;
			return r;
		}
		if (!strcmp(nm, "mi")) {
			std::string t = text(e);
			return atom(e, t.size() == 1 || (t.size() >= 2 && (unsigned char)t[0] >= 0x80)
			            ? MF_ITALIC : MF_UPRIGHT);
		}
		if (!strcmp(nm, "mn")) return atom(e, MF_UPRIGHT);
		if (!strcmp(nm, "ms") || !strcmp(nm, "mtext")) return atom(e, MF_UPRIGHT);
		if (!strcmp(nm, "mo")) {
			MNode *a = atom(e, 0);
			std::string t = a->t.utf8_str();
			if (t == "+" || t == "-" || t == "\xc2\xb1" || t == "\xc3\x97" ||
			    t == "\xc3\xb7" || t == "\xe2\x88\x97" || t == "\xe2\x88\x92" ||
			    t == "\xe2\x8b\x85" || t == "\xe2\x88\x98")
				a->fl |= MF_BIN;
			else if (t == "=" || t == "<" || t == ">" || t == "\xe2\x89\xa4" ||
			         t == "\xe2\x89\xa5" || t == "\xe2\x89\xa0" || t == "\xe2\x89\x88" ||
			         t == "\xe2\x89\xa1" || t == "\xe2\x88\x88" || t == "\xe2\x8a\x82" ||
			         t == "\xe2\x8a\x83" || t == "\xe2\x88\x9d" || t == "\xe2\x88\xbc" ||
			         t == "\xe2\x86\x92" || t == "\xe2\x86\x90" || t == "\xe2\x87\x92" ||
			         t == "\xe2\x87\x90" || t == "\xe2\x86\x94")
				a->fl |= MF_REL;
			else if (bigopChar(t) || t == "\xe2\x88\xab" || t == "\xe2\x88\xae")
				a->fl |= MF_BIGOP | MF_LIMITS;
			else if (t == "(" || t == "[" || t == "{" || t == "\xe2\x9f\xa8")
				a->fl |= MF_OPEN;
			else if (t == ")" || t == "]" || t == "}" || t == "\xe2\x9f\xa9")
				a->fl |= MF_CLOSE;
			if (!strcmp(text(e).c_str(), "lim") || !strcmp(text(e).c_str(), "sup") ||
			    !strcmp(text(e).c_str(), "inf") || !strcmp(text(e).c_str(), "max") ||
			    !strcmp(text(e).c_str(), "min"))
				a->fl |= MF_UPRIGHT | MF_BIGOP | MF_LIMITS;
			else if (t.size() > 1 && isalpha((unsigned char)t[0]) && t[0] < 0x80)
				a->fl |= MF_UPRIGHT | MF_INNER;
			return a;
		}
		if (!strcmp(nm, "msub") || !strcmp(nm, "msup") || !strcmp(nm, "msubsup")) {
			MNode *sc = new MNode(MNode::SCRIPT);
			std::vector<MNode*> ch;
			for (xmlNode *c = e->children; c; c = c->next)
				if (c->type == XML_ELEMENT_NODE) ch.push_back(elem(c));
			if (ch.empty()) return sc;
			sc->k.push_back(ch[0]);
			if (!strcmp(nm, "msup")) {
				if (ch.size() > 1) { sc->k.push_back(ch[1]); sc->aux |= 1; }
			} else if (!strcmp(nm, "msub")) {
				if (ch.size() > 1) { sc->k.push_back(ch[1]); sc->aux |= 2; }
			} else {
				/* msubsup order is base,sub,sup; keep k=[base,sup,sub] */
				if (ch.size() > 2) { sc->k.push_back(ch[2]); sc->aux |= 1; }
				if (ch.size() > 1) { sc->k.push_back(ch[1]); sc->aux |= 2; }
			}
			for (size_t i = sc->k.size(); i < ch.size(); ++i) delete ch[i];
			/* upgrade to limits for bigops */
			if (sc->k[0]->kind == MNode::ATOM && (sc->k[0]->fl & MF_LIMITS)) {
				MNode *l = new MNode(MNode::LIMITS);
				l->k = sc->k; l->aux = sc->aux;
				sc->k.clear(); sc->aux = 0;
				delete sc;
				return l;
			}
			return sc;
		}
		if (!strcmp(nm, "munder") || !strcmp(nm, "mover") || !strcmp(nm, "munderover")) {
			std::vector<MNode*> ch;
			for (xmlNode *c = e->children; c; c = c->next)
				if (c->type == XML_ELEMENT_NODE) ch.push_back(elem(c));
			if (ch.empty()) return new MNode(MNode::ROW);
			/* accent? */
			if (!strcmp(nm, "mover") && ch.size() > 1 &&
			    ch[1]->kind == MNode::ATOM) {
				std::string t = ch[1]->t.utf8_str();
				const char *id = nullptr;
				if (t == "\xe2\x86\x92" || t == "\xe2\x87\x80") id = "vec";
				else if (t == "^" || t == "\xcc\x82" || t == "\xe2\x88\xa7") id = "hat";
				else if (t == "~" || t == "\xcc\x83" || t == "\xe2\x88\xbc") id = "tilde";
				else if (t == "\xc2\xaf" || t == "\xe2\x80\xbe" || t == "_") id = "overline";
				else if (t == "\xcb\x99" || t == ".") id = "dot";
				else if (t == "\xc2\xa8") id = "ddot";
				if (id) {
					MNode *ac = new MNode(MNode::ACCENT);
					ac->t = id;
					ac->k.push_back(ch[0]);
					for (size_t i = 1; i < ch.size(); ++i) delete ch[i];
					return ac;
				}
			}
			MNode *l = new MNode(MNode::LIMITS);
			l->k.push_back(ch[0]);
			if (!strcmp(nm, "munder")) {
				if (ch.size() > 1) { l->k.push_back(ch[1]); l->aux |= 2; }
			} else if (!strcmp(nm, "mover")) {
				if (ch.size() > 1) { l->k.push_back(ch[1]); l->aux |= 1; }
			} else {
				/* munderover order is base,under,over; keep k=[base,over,under] */
				if (ch.size() > 2) { l->k.push_back(ch[2]); l->aux |= 1; }
				if (ch.size() > 1) { l->k.push_back(ch[1]); l->aux |= 2; }
			}
			for (size_t i = l->k.size(); i < ch.size(); ++i) delete ch[i];
			return l;
		}
		if (!strcmp(nm, "mfrac")) {
			MNode *f = new MNode(MNode::FRAC);
			const char *lt = attr(e, "linethickness");
			if (lt && (!strcmp(lt, "0") || !strcmp(lt, "0pt"))) f->fl |= MF_NOBAR;
			for (xmlNode *c = e->children; c; c = c->next)
				if (c->type == XML_ELEMENT_NODE && f->k.size() < 2)
					f->k.push_back(elem(c));
			return f;
		}
		if (!strcmp(nm, "msqrt") || !strcmp(nm, "mroot")) {
			MNode *r = new MNode(MNode::RADICAL);
			std::vector<MNode*> ch;
			for (xmlNode *c = e->children; c; c = c->next)
				if (c->type == XML_ELEMENT_NODE) ch.push_back(elem(c));
			/* radicand = all children for msqrt, all but last for mroot
			 * (implicit mrow when more than one) */
			size_t nrad = !strcmp(nm, "msqrt") ? ch.size()
			                                 : (ch.size() ? ch.size() - 1 : 0);
			if (nrad == 1)
				r->k.push_back(ch[0]);
			else if (nrad > 1) {
				MNode *row = new MNode(MNode::ROW);
				row->k.assign(ch.begin(), ch.begin() + nrad);
				r->k.push_back(row);
			}
			if (nrad < ch.size())
				r->k.push_back(ch.back());   /* mroot index */
			return r;
		}
		if (!strcmp(nm, "mfenced")) {
			const char *o = attr(e, "open"), *c = attr(e, "close");
			MNode *d = new MNode(MNode::FENCE);
			d->t = (o && *o) ? o : "(";
			d->t2 = (c && *c) ? c : ")";
			/* map utf8 delimiters back to our drawn ids */
			mapDelim(d->t); mapDelim(d->t2);
			MNode *inner = new MNode(MNode::ROW);
			for (xmlNode *x = e->children; x; x = x->next)
				if (x->type == XML_ELEMENT_NODE) inner->k.push_back(elem(x));
			d->k.push_back(inner);
			return d;
		}
		if (!strcmp(nm, "menclose")) {
			MNode *d = new MNode(MNode::FENCE);
			d->aux = 1;
			MNode *inner = new MNode(MNode::ROW);
			for (xmlNode *x = e->children; x; x = x->next)
				if (x->type == XML_ELEMENT_NODE) inner->k.push_back(elem(x));
			d->k.push_back(inner);
			return d;
		}
		if (!strcmp(nm, "mtable") || !strcmp(nm, "mlabeledtr")) {
			MNode *m = new MNode(MNode::MATRIX);
			int ncol = 0;
			for (xmlNode *r = e->children; r; r = r->next) {
				if (r->type != XML_ELEMENT_NODE || strcmp((const char *)r->name, "mtr"))
					continue;
				int cur = 0;
				for (xmlNode *c = r->children; c; c = c->next) {
					if (c->type != XML_ELEMENT_NODE ||
					    strcmp((const char *)c->name, "mtd")) continue;
					MNode *cell = new MNode(MNode::ROW);
					for (xmlNode *x = c->children; x; x = x->next)
						if (x->type == XML_ELEMENT_NODE) cell->k.push_back(elem(x));
					m->k.push_back(cell);
					++cur;
				}
				if (cur > ncol) ncol = cur;
			}
			m->aux = ncol > 0 ? ncol : 1;
			return m;
		}
		if (!strcmp(nm, "mspace")) {
			MNode *sp = new MNode(MNode::SPACE);
			const char *w = attr(e, "width");
			sp->aux = 30;
			if (w) {
				double v = strtod(w, nullptr);
				if (strstr(w, "em")) sp->aux = (int)(v * 100);
				else if (strstr(w, "pt")) sp->aux = (int)(v * 100 / 10.0);
			}
			return sp;
		}
		if (!strcmp(nm, "mphantom")) {
			MNode *sp = new MNode(MNode::SPACE);
			MNode *inner = new MNode(MNode::ROW);
			for (xmlNode *x = e->children; x; x = x->next)
				if (x->type == XML_ELEMENT_NODE) inner->k.push_back(elem(x));
			sp->k.push_back(inner);
			return sp;
		}
		/* unknown element: wrap children */
		MNode *r = new MNode(MNode::ROW);
		for (xmlNode *c = e->children; c; c = c->next)
			if (c->type == XML_ELEMENT_NODE) r->k.push_back(elem(c));
		return r;
	}

	void mapDelim(UT_UTF8String &d) {
		std::string s = d.utf8_str();
		const char *r = nullptr;
		if (s == "(") r = "("; else if (s == ")") r = ")";
		else if (s == "[") r = "["; else if (s == "]") r = "]";
		else if (s == "{") r = "{"; else if (s == "}") r = "}";
		else if (s == "|") r = "|";
		else if (s == "\xe2\x88\xa5") r = "B";
		else if (s == "\xe2\x9f\xa8") r = "<"; else if (s == "\xe2\x9f\xa9") r = ">";
		else if (s == "\xe2\x8c\x88") r = "L"; else if (s == "\xe2\x8c\x89") r = "R";
		else if (s == "\xe2\x8c\x8a") r = "F"; else if (s == "\xe2\x8c\x8b") r = "G";
		else if (s.empty() || s == ".") r = ".";
		if (r) d = r;
	}
};

/* ------------------------------------------------------------------ */
/* typesetter                                                          */
/* ------------------------------------------------------------------ */

GR_MathTypesetter::GR_MathTypesetter()
	: m_root(nullptr), m_w(0), m_a(0), m_d(0),
	  m_r(0), m_g(0), m_b(0), m_bError(false),
	  m_cr(nullptr), m_baseSize(12), m_display(true)
{
}

GR_MathTypesetter::~GR_MathTypesetter()
{
	delete m_root;
}

bool GR_MathTypesetter::parseLaTeX(const char *sz)
{
	delete m_root; m_root = nullptr; m_bError = false; m_sErr.clear();
	MLatexParser p(sz, this);
	m_root = p.row(-2);
	if (!m_root) m_root = new MNode(MNode::ROW);
	return true;
}

bool GR_MathTypesetter::parseMathML(const char *sz, int len)
{
	delete m_root; m_root = nullptr; m_bError = false; m_sErr.clear();
	if (!sz || !*sz) {
		m_root = new MNode(MNode::ROW);
		return false;
	}
	if (len < 0) len = (int)strlen(sz);
	xmlDoc *doc = xmlReadMemory(sz, len, "mathml", "UTF-8",
	                            XML_PARSE_RECOVER | XML_PARSE_NOERROR |
	                            XML_PARSE_NOWARNING | XML_PARSE_NONET);
	if (!doc) {
		m_bError = true;
		m_sErr = "unparsable MathML";
		m_root = new MNode(MNode::ROW);
		return false;
	}
	MMLParser p(this);
	xmlNode *rootEl = xmlDocGetRootElement(doc);
	if (rootEl) {
		/* find the <math> element if the root isn't it */
		if (strcmp((const char *)rootEl->name, "math")) {
			xmlNode *e = rootEl;
			rootEl = nullptr;
			for (xmlNode *c = e->children; c && !rootEl; c = c->next)
				if (c->type == XML_ELEMENT_NODE &&
				    !strcmp((const char *)c->name, "math"))
					rootEl = c;
			if (!rootEl) rootEl = e;
		}
		m_root = p.elem(rootEl);
	}
	xmlFreeDoc(doc);
	if (!m_root) m_root = new MNode(MNode::ROW);
	return true;
}

/* ------------------------------------------------------------------ */
/* measurement                                                         */
/* ------------------------------------------------------------------ */

void GR_MathTypesetter::_selectFont(double size, bool italic, bool bold)
{
	cairo_select_font_face(m_cr,
		m_family.empty() ? "DejaVu Serif" : m_family.c_str(),
		italic ? CAIRO_FONT_SLANT_ITALIC : CAIRO_FONT_SLANT_NORMAL,
		bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(m_cr, size);
}

void GR_MathTypesetter::_atomExtents(const char *txt, double size, bool italic,
                                   bool bold, double &w, double &a, double &d)
{
	_selectFont(size, italic, bold);
	cairo_text_extents_t ex;
	cairo_text_extents(m_cr, txt, &ex);
	w = ex.x_advance > 0 ? ex.x_advance : ex.width;
	a = -ex.y_bearing;
	if (a < 0) a = 0;
	d = ex.height + ex.y_bearing;
	if (d < 0) d = 0;
	/* ensure a sane minimum so empty/extentless glyphs still take room */
	if (w <= 0) w = size * 0.3;
	if (a <= 0) a = size * 0.7;
}

static double s_scriptSize(double s)
{
	if (s > 6.5) return s * 0.72;
	return s * 0.85;
}

double GR_MathTypesetter::_scriptShiftUp(MNode *base, double size) const
{
	double u = size * 0.52;
	if (base->a - size * 0.12 > u) u = base->a - size * 0.12;
	return u;
}

double GR_MathTypesetter::_scriptShiftDown(MNode *base, double size) const
{
	double u = size * 0.30;
	if (base->d + size * 0.06 > u) u = base->d + size * 0.06;
	return u;
}

void GR_MathTypesetter::_measure(MNode *n, double size, unsigned inherit)
{
	if (!n) return;
	n->sz = size;
	n->kx.assign(n->k.size(), 0);
	n->ky.assign(n->k.size(), 0);
	unsigned fl = n->fl | inherit;

	switch (n->kind) {
	case MNode::ATOM: {
		if (n->t.size() == 0) { n->w = 0; n->a = n->d = 0; break; }
		bool big = (fl & MF_BIGOP) != 0;
		double sz = big && m_display ? size * 1.35 : size;
		n->aux = (int)(sz * 64);          /* remembered for _drawAtom */
		_atomExtents(n->t.utf8_str(), sz,
		             (fl & MF_ITALIC) && !(fl & MF_UPRIGHT),
		             (fl & MF_BOLD) != 0, n->w, n->a, n->d);
		if (big && m_display) {           /* center big ops on the math axis */
			double axis = size * 0.28;
			double h = n->a + n->d;
			n->a = h / 2 + axis * 0.4;
			n->d = h - n->a;
		}
		break;
	}
	case MNode::SPACE: {
		if (!n->k.empty()) {             /* phantom: adopt child box */
			_measure(n->k[0], size, fl);
			n->w = n->k[0]->w; n->a = n->k[0]->a; n->d = n->k[0]->d;
		} else {
			n->w = size * n->aux / 100.0;
			n->a = n->d = 0;
		}
		break;
	}
	case MNode::ROW: {
		double x = 0, a = 0, d = 0;
		size_t cnt = n->k.size();
		for (size_t i = 0; i < cnt; ++i) {
			MNode *c = n->k[i];
			_measure(c, size, fl);
			/* inter-atom spacing */
			double gap = 0;
			if (i > 0) {
				MNode *p = n->k[i - 1];
				unsigned lf = p->fl, rf = c->fl;
				if (!((lf | rf) & (MF_OPEN | MF_CLOSE | MF_INNER)) &&
				    !(lf & MF_CLOSE) && !(rf & MF_OPEN)) {
					if ((lf | rf) & MF_REL) gap = size * 0.24;
					else if ((lf | rf) & MF_BIN) gap = size * 0.16;
				}
			}
			x += gap;
			n->kx[i] = x;
			n->ky[i] = c->a;              /* placeholder; baselines aligned */
			x += c->w;
			if (c->a > a) a = c->a;
			if (c->d > d) d = c->d;
		}
		n->w = x; n->a = a; n->d = d;
		/* ky[i] = baseline of child relative to row top = row.a */
		for (size_t i = 0; i < cnt; ++i) n->ky[i] = n->a;
		break;
	}
	case MNode::SCRIPT: {
		MNode *base = n->k[0];
		_measure(base, size, fl);
		double ss = s_scriptSize(size);
		MNode *sup = (n->aux & 1) ? n->k[1] : nullptr;
		MNode *sub = (n->aux & 2) ? n->k[(n->aux & 1) ? 2 : 1] : nullptr;
		if (sup) _measure(sup, ss, fl);
		if (sub) _measure(sub, ss, fl);
		double up = _scriptShiftUp(base, size);
		double dn = _scriptShiftDown(base, size);
		double sx = base->w + size * 0.04;
		double sw = 0;
		if (sup) sw = sup->w;
		if (sub && sub->w > sw) sw = sub->w;
		/* B = base baseline offset from node top. It must leave room for
		 * the superscript above (sup baseline sits 'up' above base
		 * baseline) and the subscript below. */
		double B = base->a;
		if (sup && up + sup->a > B) B = up + sup->a;
		double bot = B + base->d;
		if (sub && B + dn + sub->d > bot) bot = B + dn + sub->d;
		n->w = sx + sw;
		n->a = B;
		n->d = bot - B;
		n->ky[0] = B;
		if (sup) { n->kx[1] = sx; n->ky[1] = B - up; }
		if (sub) {
			size_t si = (n->aux & 1) ? 2 : 1;
			n->kx[si] = sx; n->ky[si] = B + dn;
		}
		break;
	}
	case MNode::LIMITS: {
		MNode *base = n->k[0];
		_measure(base, size, fl);
		double ss = s_scriptSize(size);
		MNode *over = (n->aux & 1) ? n->k[1] : nullptr;
		MNode *under = (n->aux & 2) ? n->k[(n->aux & 1) ? 2 : 1] : nullptr;
		if (over) _measure(over, ss, fl);
		if (under) _measure(under, ss, fl);
		double gap = size * 0.14;
		double overH = over ? over->a + over->d + gap : 0;
		double undH = under ? under->a + under->d + gap : 0;
		double W = base->w;
		if (over && over->w > W) W = over->w;
		if (under && under->w > W) W = under->w;
		n->w = W;
		n->a = base->a + overH;
		n->d = base->d + undH;
		n->kx[0] = (W - base->w) / 2;
		n->ky[0] = n->a;
		if (over) {
			n->kx[1] = (W - over->w) / 2;
			n->ky[1] = over->a;          /* over sits at the very top */
		}
		if (under) {
			size_t si = (n->aux & 1) ? 2 : 1;
			n->kx[si] = (W - under->w) / 2;
			n->ky[si] = n->a + base->d + gap + under->a;
		}
		break;
	}
	case MNode::FRAC: {
		double fs = m_display ? size : size * 0.9;
		MNode *num = n->k[0], *den = n->k.size() > 1 ? n->k[1] : nullptr;
		_measure(num, fs, fl);
		if (den) _measure(den, fs, fl);
		double W = num->w;
		if (den && den->w > W) W = den->w;
		W += size * 0.35;                 /* pad around the rule */
		double axis = size * 0.26;
		double rule = (n->fl & MF_NOBAR) ? 0 : size * 0.05;
		double gapN = (n->fl & MF_NOBAR) ? size * 0.12 : size * 0.16;
		double gapD = gapN;
		n->w = W;
		n->a = axis + gapN + num->a + num->d;
		n->d = (den ? den->a + den->d : 0) + gapD - axis;
		n->kx[0] = (W - num->w) / 2;
		n->ky[0] = num->a;               /* num baseline (top of node) */
		if (den) {
			n->kx[1] = (W - den->w) / 2;
			n->ky[1] = n->a - axis + gapD + den->a;
		}
		(void)rule;
		break;
	}
	case MNode::RADICAL: {
		MNode *in = n->k[0];
		_measure(in, size, fl);
		MNode *idx = n->k.size() > 1 ? n->k[1] : nullptr;
		if (idx) _measure(idx, s_scriptSize(size), fl);
		double rule = size * 0.05;
		double pad = size * 0.12;
		double surdW = size * 0.55;
		n->a = in->a + rule + pad;
		n->d = in->d + size * 0.04;
		n->w = surdW + in->w + size * 0.08;
		if (idx) {
			n->kx[1] = 0;
			n->ky[1] = n->a - idx->d;
			double need = idx->w + size * 0.05;
			if (need > surdW) n->w += need - surdW;
			n->kx[0] = (need > surdW ? need : surdW);
		} else {
			n->kx[0] = surdW;
		}
		/* radicand baseline coincides with the node baseline; its ascent
		 * ends pad+rule below the node top */
		n->ky[0] = n->a;
		break;
	}
	case MNode::FENCE: {
		MNode *in = n->k[0];
		_measure(in, size, fl);
		double pad = size * 0.10;
		char o = n->t.size() ? n->t.utf8_str()[0] : '.';
		char c = n->t2.size() ? n->t2.utf8_str()[0] : '.';
		if (n->aux == 1) {               /* boxed */
			double t = size * 0.045;
			n->w = in->w + 2 * pad + 2 * t + size * 0.16;
			n->a = in->a + pad + t;
			n->d = in->d + pad + t;
			n->kx[0] = t + pad + size * 0.08;
			n->ky[0] = n->a;
			break;
		}
		double ow = _delimWidth(o, size);
		double cw = _delimWidth(c, size);
		n->w = ow + cw + in->w + 2 * pad;
		n->a = in->a + pad * 0.6;
		n->d = in->d + pad * 0.6;
		n->kx[0] = ow + pad;
		n->ky[0] = n->a;
		break;
	}
	case MNode::ACCENT: {
		MNode *in = n->k[0];
		_measure(in, size, fl);
		std::string id = n->t.utf8_str();
		bool under = (id == "underline");
		double mh = (id == "dot" || id == "ddot") ? size * 0.16 :
		            (id == "hat" || id == "tilde" || id == "breve" ||
		             id == "check") ? size * 0.24 : size * 0.14;
		double gap = size * 0.10;
		n->w = in->w;
		if (under) { n->a = in->a; n->d = in->d + gap + mh; }
		else       { n->a = in->a + gap + mh; n->d = in->d; }
		n->kx[0] = 0;
		n->ky[0] = n->a;
		break;
	}
	case MNode::MATRIX: {
		int ncol = n->aux > 0 ? n->aux : 1;
		size_t cells = n->k.size();
		int nrow = (int)((cells + ncol - 1) / ncol);
		std::vector<double> colW(ncol, 0), rowA(nrow, 0), rowD(nrow, 0);
		for (size_t i = 0; i < cells; ++i) {
			MNode *c = n->k[i];
			_measure(c, size, fl);
			int r = (int)i / ncol, cc = (int)i % ncol;
			if (c->w > colW[cc]) colW[cc] = c->w;
			if (c->a > rowA[r]) rowA[r] = c->a;
			if (c->d > rowD[r]) rowD[r] = c->d;
		}
		double colGap = size * 0.9, rowGap = size * 0.45;
		double totW = 0;
		for (int j = 0; j < ncol; ++j) totW += colW[j];
		totW += colGap * (ncol - 1);
		double totH = 0;
		for (int r = 0; r < nrow; ++r) totH += rowA[r] + rowD[r];
		totH += rowGap * (nrow - 1);
		bool leftAlign = (n->fl & 0x10000) != 0;
		double y = 0;
		for (int r = 0; r < nrow; ++r) {
			double x = 0;
			for (int cc = 0; cc < ncol; ++cc) {
				size_t i = (size_t)r * ncol + cc;
				if (i < cells) {
					MNode *c = n->k[i];
					n->kx[i] = leftAlign ? x : x + (colW[cc] - c->w) / 2;
					n->ky[i] = y + rowA[r];
				}
				x += colW[cc] + colGap;
			}
			y += rowA[r] + rowD[r] + rowGap;
		}
		/* fence wrapping like FENCE */
		double pad = size * 0.10;
		char o = n->t.size() ? n->t.utf8_str()[0] : '.';
		char c = n->t2.size() ? n->t2.utf8_str()[0] : '.';
		double ow = _delimWidth(o, size), cw = _delimWidth(c, size);
		double mid = totH / 2;            /* centre matrix on math axis */
		double innerA = mid + size * 0.28;
		double innerD = totH - innerA;
		n->w = totW + ow + cw + 2 * pad;
		n->a = innerA + pad * 0.6;
		n->d = innerD + pad * 0.6;
		for (size_t i = 0; i < cells; ++i) {
			n->kx[i] += ow + pad;
			n->ky[i] += pad * 0.6;
		}
		break;
	}
	}
}

void GR_MathTypesetter::layout(cairo_t *cr, const char *family,
                               double baseSizePt, bool displayStyle)
{
	m_cr = cr;
	m_family = family ? family : "";
	m_baseSize = baseSizePt;
	m_display = displayStyle;
	m_w = m_a = m_d = 0;
	if (!m_root) return;
	_measure(m_root, baseSizePt, 0);
	m_w = m_root->w;
	m_a = m_root->a;
	m_d = m_root->d;
	if (m_w <= 0) m_w = baseSizePt * 0.4;
	if (m_a <= 0) m_a = baseSizePt * 0.7;
}

/* ------------------------------------------------------------------ */
/* drawing                                                             */
/* ------------------------------------------------------------------ */

double GR_MathTypesetter::_delimWidth(char d, double size) const
{
	switch (d) {
	case '.': return 0;
	case '|': case 'B': return size * 0.28;
	case '<': case '>': return size * 0.34;
	case 'L': case 'R': case 'F': case 'G': return size * 0.34;
	default: return size * 0.40;
	}
}

void GR_MathTypesetter::_drawDelim(cairo_t *cr, char d, double x,
                                 double yTop, double h, double size)
{
	if (d == '.') return;
	double w = _delimWidth(d, size);
	double lw = size * 0.045;
	if (lw < 0.6) lw = 0.6;
	cairo_set_line_width(cr, lw);
	cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
	double mid = yTop + h / 2;
	switch (d) {
	case '(':
		cairo_move_to(cr, x + w * 0.9, yTop);
		cairo_curve_to(cr, x + w * 0.15, yTop + h * 0.18,
		               x + w * 0.15, yTop + h * 0.82, x + w * 0.9, yTop + h);
		cairo_stroke(cr);
		break;
	case ')':
		cairo_move_to(cr, x + w * 0.1, yTop);
		cairo_curve_to(cr, x + w * 0.85, yTop + h * 0.18,
		               x + w * 0.85, yTop + h * 0.82, x + w * 0.1, yTop + h);
		cairo_stroke(cr);
		break;
	case '[':
		cairo_move_to(cr, x + w * 0.85, yTop);
		cairo_line_to(cr, x + w * 0.3, yTop);
		cairo_line_to(cr, x + w * 0.3, yTop + h);
		cairo_line_to(cr, x + w * 0.85, yTop + h);
		cairo_stroke(cr);
		break;
	case ']':
		cairo_move_to(cr, x + w * 0.15, yTop);
		cairo_line_to(cr, x + w * 0.7, yTop);
		cairo_line_to(cr, x + w * 0.7, yTop + h);
		cairo_line_to(cr, x + w * 0.15, yTop + h);
		cairo_stroke(cr);
		break;
	case '{': {
		double sw = w * 0.55;
		cairo_move_to(cr, x + w, yTop);
		cairo_curve_to(cr, x + sw, yTop + h * 0.05, x + sw, mid - h * 0.12,
		               x + w * 0.15, mid);
		cairo_curve_to(cr, x + sw, mid + h * 0.12, x + sw, yTop + h * 0.95,
		               x + w, yTop + h);
		cairo_stroke(cr);
		break;
	}
	case '}': {
		double sw = w * 0.45;
		cairo_move_to(cr, x, yTop);
		cairo_curve_to(cr, x + sw, yTop + h * 0.05, x + sw, mid - h * 0.12,
		               x + w * 0.85, mid);
		cairo_curve_to(cr, x + sw, mid + h * 0.12, x + sw, yTop + h * 0.95,
		               x, yTop + h);
		cairo_stroke(cr);
		break;
	}
	case '|':
		cairo_move_to(cr, x + w / 2, yTop);
		cairo_line_to(cr, x + w / 2, yTop + h);
		cairo_stroke(cr);
		break;
	case 'B':                        /* double bar */
		cairo_move_to(cr, x + w * 0.3, yTop);
		cairo_line_to(cr, x + w * 0.3, yTop + h);
		cairo_move_to(cr, x + w * 0.7, yTop);
		cairo_line_to(cr, x + w * 0.7, yTop + h);
		cairo_stroke(cr);
		break;
	case '<':
		cairo_move_to(cr, x + w * 0.9, yTop);
		cairo_line_to(cr, x + w * 0.15, mid);
		cairo_line_to(cr, x + w * 0.9, yTop + h);
		cairo_stroke(cr);
		break;
	case '>':
		cairo_move_to(cr, x + w * 0.1, yTop);
		cairo_line_to(cr, x + w * 0.85, mid);
		cairo_line_to(cr, x + w * 0.1, yTop + h);
		cairo_stroke(cr);
		break;
	case 'L':                        /* lceil */
		cairo_move_to(cr, x + w * 0.8, yTop);
		cairo_line_to(cr, x + w * 0.3, yTop);
		cairo_line_to(cr, x + w * 0.3, yTop + h);
		cairo_stroke(cr);
		break;
	case 'R':
		cairo_move_to(cr, x + w * 0.2, yTop);
		cairo_line_to(cr, x + w * 0.7, yTop);
		cairo_line_to(cr, x + w * 0.7, yTop + h);
		cairo_stroke(cr);
		break;
	case 'F':                        /* lfloor */
		cairo_move_to(cr, x + w * 0.3, yTop);
		cairo_line_to(cr, x + w * 0.3, yTop + h);
		cairo_line_to(cr, x + w * 0.8, yTop + h);
		cairo_stroke(cr);
		break;
	case 'G':
		cairo_move_to(cr, x + w * 0.7, yTop);
		cairo_line_to(cr, x + w * 0.7, yTop + h);
		cairo_line_to(cr, x + w * 0.2, yTop + h);
		cairo_stroke(cr);
		break;
	default:
		break;
	}
}

void GR_MathTypesetter::_drawAccent(const char *id, double x, double w,
                                  double y, double size)
{
	double lw = size * 0.045;
	if (lw < 0.55) lw = 0.55;
	cairo_set_line_width(m_cr, lw);
	cairo_set_line_cap(m_cr, CAIRO_LINE_CAP_ROUND);
	if (!strcmp(id, "overline") || !strcmp(id, "bar") || !strcmp(id, "underline")) {
		cairo_move_to(m_cr, x, y);
		cairo_line_to(m_cr, x + w, y);
		cairo_stroke(m_cr);
	} else if (!strcmp(id, "vec")) {
		double ax = x + w * 0.15, aw = w * 0.7;
		cairo_move_to(m_cr, ax, y);
		cairo_line_to(m_cr, ax + aw, y);
		cairo_line_to(m_cr, ax + aw - size * 0.14, y - size * 0.10);
		cairo_move_to(m_cr, ax + aw, y);
		cairo_line_to(m_cr, ax + aw - size * 0.14, y + size * 0.10);
		cairo_stroke(m_cr);
	} else if (!strcmp(id, "hat")) {
		double hw = w < size ? w : size;
		double hx = x + (w - hw) / 2;
		cairo_move_to(m_cr, hx, y);
		cairo_line_to(m_cr, hx + hw / 2, y - size * 0.22);
		cairo_line_to(m_cr, hx + hw, y);
		cairo_stroke(m_cr);
	} else if (!strcmp(id, "tilde")) {
		double hw = w < size * 1.2 ? w : size * 1.2;
		double hx = x + (w - hw) / 2;
		cairo_move_to(m_cr, hx, y);
		cairo_curve_to(m_cr, hx + hw * 0.3, y - size * 0.25,
		               hx + hw * 0.7, y + size * 0.10, hx + hw, y - size * 0.10);
		cairo_stroke(m_cr);
	} else if (!strcmp(id, "dot")) {
		cairo_arc(m_cr, x + w / 2, y, size * 0.08, 0, 2 * M_PI);
		cairo_fill(m_cr);
	} else if (!strcmp(id, "ddot")) {
		cairo_arc(m_cr, x + w / 2 - size * 0.14, y, size * 0.08, 0, 2 * M_PI);
		cairo_fill(m_cr);
		cairo_arc(m_cr, x + w / 2 + size * 0.14, y, size * 0.08, 0, 2 * M_PI);
		cairo_fill(m_cr);
	} else if (!strcmp(id, "breve") || !strcmp(id, "check")) {
		double hw = w < size ? w : size;
		double hx = x + (w - hw) / 2;
		cairo_move_to(m_cr, hx, y - size * 0.18);
		cairo_line_to(m_cr, hx + hw / 2, y);
		cairo_line_to(m_cr, hx + hw, y - size * 0.18);
		cairo_stroke(m_cr);
	} else if (!strcmp(id, "acute")) {
		cairo_move_to(m_cr, x + w / 2 - size * 0.10, y);
		cairo_line_to(m_cr, x + w / 2 + size * 0.10, y - size * 0.20);
		cairo_stroke(m_cr);
	} else if (!strcmp(id, "grave")) {
		cairo_move_to(m_cr, x + w / 2 + size * 0.10, y);
		cairo_line_to(m_cr, x + w / 2 - size * 0.10, y - size * 0.20);
		cairo_stroke(m_cr);
	}
}

void GR_MathTypesetter::_drawAtom(MNode *n, double x, double y)
{
	/* y = baseline; aux holds the measured font size * 64 */
	double sz = n->aux > 0 ? n->aux / 64.0 : m_baseSize;
	_selectFont(sz, (n->fl & MF_ITALIC) && !(n->fl & MF_UPRIGHT),
	            (n->fl & MF_BOLD) != 0);
	cairo_move_to(m_cr, x, y);
	cairo_show_text(m_cr, n->t.utf8_str());
}

void GR_MathTypesetter::_draw(MNode *n, double x, double y)
{
	/* x,y = node top-left */
	if (!n) return;
	switch (n->kind) {
	case MNode::ATOM:
		_drawAtom(n, x, y + n->a);
		break;
	case MNode::SPACE:
		break;                                    /* phantom: reserve space only */
	case MNode::FRAC: {
		for (size_t i = 0; i < n->k.size(); ++i)
			_draw(n->k[i], x + n->kx[i], y + n->ky[i] - n->k[i]->a);
		if (!(n->fl & MF_NOBAR)) {
			double axis = n->sz * 0.26;
			double ry = y + n->a - axis;
			double lw = n->sz * 0.05;
			if (lw < 0.55) lw = 0.55;
			cairo_set_line_width(m_cr, lw);
			cairo_move_to(m_cr, x + n->sz * 0.10, ry);
			cairo_line_to(m_cr, x + n->w - n->sz * 0.10, ry);
			cairo_stroke(m_cr);
		}
		break;
	}
	case MNode::RADICAL: {
		double esz = n->sz > 0 ? n->sz : m_baseSize;
		double rule = esz * 0.05;
		double pad = esz * 0.12;
		double surdW = esz * 0.55;
		double ry = y + pad * 0.5;                    /* overline y */
		double x0 = x + n->kx[0] - surdW;
		double yb = y + n->a;                         /* radicand baseline */
		double lw = rule;
		if (lw < 0.55) lw = 0.55;
		cairo_set_line_width(m_cr, lw);
		cairo_set_line_join(m_cr, CAIRO_LINE_JOIN_MITER);
		cairo_move_to(m_cr, x0, ry + (yb - ry) * 0.55);
		cairo_line_to(m_cr, x0 + esz * 0.14, yb);
		cairo_line_to(m_cr, x0 + esz * 0.42, ry - esz * 0.06);
		cairo_line_to(m_cr, x + n->w, ry - esz * 0.06);
		cairo_stroke(m_cr);
		if (n->k.size() > 1)
			_draw(n->k[1], x + n->kx[1], y + n->ky[1] - n->k[1]->a);
		MNode *in = n->k[0];
		_draw(in, x + n->kx[0], y + n->ky[0] - in->a);
		break;
	}
	case MNode::FENCE: {
		MNode *in = n->k[0];
		double esz = n->sz > 0 ? n->sz : m_baseSize;
		double pad = esz * 0.10;
		if (n->aux == 1) {
			double t = esz * 0.045;
			cairo_set_line_width(m_cr, t);
			cairo_rectangle(m_cr, x + t / 2, y + t / 2,
			                n->w - t, n->a + n->d - t);
			cairo_stroke(m_cr);
			_draw(in, x + n->kx[0], y + n->ky[0] - in->a);
			break;
		}
		char o = n->t.size() ? n->t.utf8_str()[0] : '.';
		char c = n->t2.size() ? n->t2.utf8_str()[0] : '.';
		double h = n->a + n->d - pad * 0.4;
		double yt = y + pad * 0.2;
		_drawDelim(m_cr, o, x, yt, h, esz);
		_drawDelim(m_cr, c, x + n->w - _delimWidth(c, esz), yt, h, esz);
		_draw(in, x + n->kx[0], y + n->ky[0] - in->a);
		break;
	}
	case MNode::ACCENT: {
		MNode *in = n->k[0];
		_draw(in, x + n->kx[0], y + n->ky[0] - in->a);
		std::string id = n->t.utf8_str();
		double esz = n->sz > 0 ? n->sz : m_baseSize;
		double gap = esz * 0.10;
		if (id == "underline") {
			_drawAccent(id.c_str(), x, n->w, y + n->a + in->d + gap, esz);
		} else {
			double mh = (id == "dot" || id == "ddot") ? esz * 0.08 :
			            (id == "hat" || id == "tilde") ? esz * 0.12 :
			            esz * 0.07;
			_drawAccent(id.c_str(), x, n->w, y + n->a - in->a - gap + mh * 0,
			            esz);
		}
		break;
	}
	case MNode::MATRIX: {
		for (size_t i = 0; i < n->k.size(); ++i)
			_draw(n->k[i], x + n->kx[i], y + n->ky[i] - n->k[i]->a);
		double esz = n->sz > 0 ? n->sz : m_baseSize;
		double pad = esz * 0.10;
		char o = n->t.size() ? n->t.utf8_str()[0] : '.';
		char c = n->t2.size() ? n->t2.utf8_str()[0] : '.';
		double h = n->a + n->d - pad * 0.4;
		double yt = y + pad * 0.2;
		_drawDelim(m_cr, o, x, yt, h, esz);
		_drawDelim(m_cr, c, x + n->w - _delimWidth(c, esz), yt, h, esz);
		break;
	}
	case MNode::ROW:
	case MNode::SCRIPT:
	case MNode::LIMITS:
	default:
		for (size_t i = 0; i < n->k.size(); ++i)
			_draw(n->k[i], x + n->kx[i], y + n->ky[i] - n->k[i]->a);
		break;
	}
}

void GR_MathTypesetter::render(cairo_t *cr)
{
	if (!m_root || !cr) return;
	m_cr = cr;
	cairo_save(cr);
	cairo_set_source_rgb(cr, m_r, m_g, m_b);
	_draw(m_root, 0, 0);
	cairo_new_path(cr);
	cairo_restore(cr);
}

/* ------------------------------------------------------------------ */
/* MathML serializer                                                   */
/* ------------------------------------------------------------------ */

static void s_esc(const UT_UTF8String &in, std::string &out)
{
	const char *p = in.utf8_str();
	while (*p) {
		switch (*p) {
		case '&': out += "&amp;"; break;
		case '<': out += "&lt;"; break;
		case '>': out += "&gt;"; break;
		case '"': out += "&quot;"; break;
		default: out += *p;
		}
		++p;
	}
}

static void s_ser(MNode *n, std::string &o);

static void s_serKids(MNode *n, std::string &o)
{
	for (auto c : n->k) s_ser(c, o);
}

static void s_ser(MNode *n, std::string &o)
{
	if (!n) return;
	switch (n->kind) {
	case MNode::ROW:
		o += "<mrow>"; s_serKids(n, o); o += "</mrow>";
		break;
	case MNode::ATOM: {
		const char *txt = n->t.utf8_str();
		std::string e;
		s_esc(n->t, e);
		bool digit = true, alpha = false;
		for (const char *p = txt; *p; ++p) {
			if (isalpha((unsigned char)*p) || (unsigned char)*p >= 0x80) alpha = true;
			if (!isdigit((unsigned char)*p) && *p != '.' && *p != ',') digit = false;
		}
		if (n->fl & (MF_REL | MF_BIN | MF_OPEN | MF_CLOSE | MF_BIGOP))
			o += "<mo>" + e + "</mo>";
		else if (digit && !alpha)
			o += "<mn>" + e + "</mn>";
		else if (n->fl & MF_UPRIGHT)
			o += "<mtext>" + e + "</mtext>";
		else
			o += "<mi>" + e + "</mi>";
		break;
	}
	case MNode::SCRIPT: {
		bool hasSup = n->aux & 1, hasSub = n->aux & 2;
		if (hasSup && hasSub) o += "<msubsup>";
		else if (hasSup) o += "<msup>";
		else o += "<msub>";
		s_ser(n->k[0], o);
		if (hasSub) s_ser(n->k[(n->aux & 1) ? 2 : 1], o);
		if (hasSup) s_ser(n->k[1], o);
		if (hasSup && hasSub) o += "</msubsup>";
		else if (hasSup) o += "</msup>";
		else o += "</msub>";
		break;
	}
	case MNode::LIMITS: {
		bool hasOver = n->aux & 1, hasUnder = n->aux & 2;
		if (hasOver && hasUnder) o += "<munderover>";
		else if (hasOver) o += "<mover>";
		else o += "<munder>";
		s_ser(n->k[0], o);
		if (hasUnder) s_ser(n->k[(n->aux & 1) ? 2 : 1], o);
		if (hasOver) s_ser(n->k[1], o);
		if (hasOver && hasUnder) o += "</munderover>";
		else if (hasOver) o += "</mover>";
		else o += "</munder>";
		break;
	}
	case MNode::FRAC:
		o += (n->fl & MF_NOBAR) ? "<mfrac linethickness=\"0\">" : "<mfrac>";
		s_ser(n->k[0], o);
		if (n->k.size() > 1) s_ser(n->k[1], o); else o += "<mrow/>";
		o += "</mfrac>";
		break;
	case MNode::RADICAL:
		if (n->k.size() > 1) {
			o += "<mroot>";
			s_ser(n->k[0], o);
			s_ser(n->k[1], o);
			o += "</mroot>";
		} else {
			o += "<msqrt>";
			s_ser(n->k[0], o);
			o += "</msqrt>";
		}
		break;
	case MNode::FENCE:
		if (n->aux == 1) {
			o += "<menclose notation=\"box\">";
			s_serKids(n, o);
			o += "</menclose>";
			break;
		}
		o += "<mrow>";
		if (n->t.size() && n->t.utf8_str()[0] != '.') {
			o += "<mo>";
			std::string e; s_esc(n->t, e); o += e;
			o += "</mo>";
		}
		s_serKids(n, o);
		if (n->t2.size() && n->t2.utf8_str()[0] != '.') {
			o += "<mo>";
			std::string e; s_esc(n->t2, e); o += e;
			o += "</mo>";
		}
		o += "</mrow>";
		break;
	case MNode::ACCENT: {
		std::string id = n->t.utf8_str();
		const char *mark = "\xcc\x85";   /* combining overline */
		const char *tag = "mover";
		if (id == "vec") mark = "\xe2\x86\x92";
		else if (id == "hat") mark = "^";
		else if (id == "tilde") mark = "~";
		else if (id == "dot") mark = "\xcb\x99";
		else if (id == "ddot") mark = "\xc2\xa8";
		else if (id == "underline") { mark = "_"; tag = "munder"; }
		else if (id == "overline" || id == "bar") mark = "\xc2\xaf";
		o += "<"; o += tag; o += ">";
		s_ser(n->k[0], o);
		o += "<mo>"; o += mark; o += "</mo>";
		o += "</"; o += tag; o += ">";
		break;
	}
	case MNode::MATRIX: {
		int ncol = n->aux > 0 ? n->aux : 1;
		o += "<mrow>";
		if (n->t.size() && n->t.utf8_str()[0] != '.') {
			o += "<mo>"; std::string e; s_esc(n->t, e); o += e; o += "</mo>";
		}
		o += "<mtable>";
		for (size_t i = 0; i < n->k.size(); ++i) {
			if (i % ncol == 0) o += "<mtr>";
			o += "<mtd>"; s_ser(n->k[i], o); o += "</mtd>";
			if (i % ncol == (size_t)ncol - 1) o += "</mtr>";
		}
		if (n->k.size() % ncol) o += "</mtr>";
		o += "</mtable>";
		if (n->t2.size() && n->t2.utf8_str()[0] != '.') {
			o += "<mo>"; std::string e; s_esc(n->t2, e); o += e; o += "</mo>";
		}
		o += "</mrow>";
		break;
	}
	case MNode::SPACE:
		if (!n->k.empty()) s_serKids(n, o);
		else {
			char b[64];
			snprintf(b, sizeof b, "<mspace width=\"%.2fem\"/>", n->aux / 100.0);
			o += b;
		}
		break;
	}
}

UT_UTF8String GR_MathTypesetter::toMathML() const
{
	std::string o = "<math xmlns=\"http://www.w3.org/1998/Math/MathML\">";
	s_ser(m_root, o);
	o += "</math>";
	UT_UTF8String r;
	r.assign(o.c_str());
	return r;
}

/* ------------------------------------------------------------------ */
/* LaTeX serializer (for round-tripping imported MathML)               */
/* ------------------------------------------------------------------ */

static void s_lx(MNode *n, std::string &o);

static void s_lxAtom(MNode *n, std::string &o)
{
	std::string t = n->t.utf8_str();
	/* map common unicode back to commands */
	static const struct { const char *u; const char *l; } rev[] = {
		{ "\xce\xb1", "\\alpha" }, { "\xce\xb2", "\\beta" },
		{ "\xce\xb3", "\\gamma" }, { "\xce\xb4", "\\delta" },
		{ "\xcf\xb5", "\\epsilon" }, { "\xce\xb8", "\\theta" },
		{ "\xce\xbb", "\\lambda" }, { "\xce\xbc", "\\mu" },
		{ "\xcf\x80", "\\pi" }, { "\xcf\x83", "\\sigma" },
		{ "\xcf\x86", "\\phi" }, { "\xcf\x89", "\\omega" },
		{ "\xce\x93", "\\Gamma" }, { "\xce\x94", "\\Delta" },
		{ "\xce\xa3", "\\Sigma" }, { "\xce\xa9", "\\Omega" },
		{ "\xe2\x88\x91", "\\sum" }, { "\xe2\x88\x8f", "\\prod" },
		{ "\xe2\x88\xab", "\\int" }, { "\xe2\x88\xae", "\\oint" },
		{ "\xe2\x88\x9e", "\\infty" }, { "\xe2\x88\x82", "\\partial" },
		{ "\xe2\x88\x87", "\\nabla" }, { "\xe2\x89\xa4", "\\leq" },
		{ "\xe2\x89\xa5", "\\geq" }, { "\xe2\x89\xa0", "\\neq" },
		{ "\xe2\x89\x88", "\\approx" }, { "\xc3\x97", "\\times" },
		{ "\xc3\xb7", "\\div" }, { "\xc2\xb1", "\\pm" },
		{ "\xe2\x8b\x85", "\\cdot" }, { "\xe2\x88\x88", "\\in" },
		{ "\xe2\x8a\x82", "\\subset" }, { "\xe2\x8a\x83", "\\supset" },
		{ "\xe2\x8a\x86", "\\subseteq" }, { "\xe2\x8a\x87", "\\supseteq" },
		{ "\xe2\x88\xaa", "\\cup" }, { "\xe2\x88\xa9", "\\cap" },
		{ "\xe2\x86\x92", "\\rightarrow" }, { "\xe2\x86\x90", "\\leftarrow" },
		{ "\xe2\x87\x92", "\\Rightarrow" }, { "\xe2\x87\x90", "\\Leftarrow" },
		{ "\xe2\x88\x80", "\\forall" }, { "\xe2\x88\x83", "\\exists" },
		{ "\xe2\x88\x85", "\\emptyset" }, { "\xc2\xac", "\\neg" },
		{ nullptr, nullptr }
	};
	for (int i = 0; rev[i].u; ++i)
		if (t == rev[i].u) { o += rev[i].l; o += " "; return; }
	if (t.size() > 1 && (n->fl & MF_UPRIGHT)) { o += "\\mathrm{" + t + "}"; return; }
	if (n->fl & MF_UPRIGHT) { o += "\\mathrm{" + t + "}"; return; }
	o += t;
}

static void s_lx(MNode *n, std::string &o)
{
	if (!n) return;
	switch (n->kind) {
	case MNode::ROW:
		for (auto c : n->k) s_lx(c, o);
		break;
	case MNode::ATOM:
		s_lxAtom(n, o);
		break;
	case MNode::SCRIPT: {
		s_lx(n->k[0], o);
		if (n->aux & 1) { o += "^{"; s_lx(n->k[1], o); o += "}"; }
		if (n->aux & 2) { o += "_{"; s_lx(n->k[(n->aux & 1) ? 2 : 1], o); o += "}"; }
		break;
	}
	case MNode::LIMITS: {
		s_lx(n->k[0], o);
		if (n->aux & 2) { o += "_{"; s_lx(n->k[(n->aux & 1) ? 2 : 1], o); o += "}"; }
		if (n->aux & 1) { o += "^{"; s_lx(n->k[1], o); o += "}"; }
		break;
	}
	case MNode::FRAC:
		if (n->fl & MF_NOBAR) o += "\\binom{";
		else o += "\\frac{";
		s_lx(n->k[0], o);
		o += "}{";
		if (n->k.size() > 1) s_lx(n->k[1], o);
		o += "}";
		break;
	case MNode::RADICAL:
		o += "\\sqrt";
		if (n->k.size() > 1) { o += "["; s_lx(n->k[1], o); o += "]"; }
		o += "{"; s_lx(n->k[0], o); o += "}";
		break;
	case MNode::FENCE:
		if (n->aux == 1) { o += "\\boxed{"; s_lx(n->k[0], o); o += "}"; break; }
		o += "\\left";
		o += n->t.size() ? n->t.utf8_str() : ".";
		o += " ";
		s_lx(n->k[0], o);
		o += " \\right";
		o += n->t2.size() ? n->t2.utf8_str() : ".";
		break;
	case MNode::ACCENT: {
		std::string id = n->t.utf8_str();
		o += "\\" + id + "{";
		s_lx(n->k[0], o);
		o += "}";
		break;
	}
	case MNode::MATRIX: {
		const char *env = "matrix";
		std::string o_ = n->t.utf8_str(), c_ = n->t2.utf8_str();
		if (o_ == "(") env = "pmatrix";
		else if (o_ == "[") env = "bmatrix";
		else if (o_ == "|") env = "vmatrix";
		else if (o_ == "{") env = "cases";
		o += "\\begin{"; o += env; o += "}";
		int ncol = n->aux > 0 ? n->aux : 1;
		for (size_t i = 0; i < n->k.size(); ++i) {
			if (i && i % ncol == 0) o += " \\\\ ";
			else if (i % ncol) o += " & ";
			s_lx(n->k[i], o);
		}
		o += "\\end{"; o += env; o += "}";
		break;
	}
	case MNode::SPACE:
		if (n->k.empty()) o += "\\;";
		else s_lx(n->k[0], o);
		break;
	}
}

UT_UTF8String GR_MathTypesetter::toLaTeX() const
{
	std::string o;
	s_lx(m_root, o);
	UT_UTF8String r;
	r.assign(o.c_str());
	return r;
}
