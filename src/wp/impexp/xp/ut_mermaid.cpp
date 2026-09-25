/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
/* Abinova
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

#include <cairo.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "ut_bytebuf.h"
#include "ut_mermaid.h"

namespace {

/* ------------------------------------------------------------------ */
/* small string helpers                                                */

std::string mm_trim(const std::string & s)
{
	size_t b = s.find_first_not_of(" \t\r\n");
	if (b == std::string::npos) return "";
	size_t e = s.find_last_not_of(" \t\r\n");
	return s.substr(b, e - b + 1);
}

bool mm_starts(const std::string & s, const char * prefix)
{
	return s.compare(0, strlen(prefix), prefix) == 0;
}

std::vector<std::string> mm_lines(const std::string & src)
{
	std::vector<std::string> out;
	size_t pos = 0;
	while (pos <= src.size())
	{
		size_t nl = src.find('\n', pos);
		if (nl == std::string::npos) nl = src.size();
		out.push_back(mm_trim(src.substr(pos, nl - pos)));
		pos = nl + 1;
	}
	return out;
}

/* strip a mermaid %% comment from a line */
std::string mm_nocomment(const std::string & s)
{
	size_t p = s.find("%%");
	return mm_trim(p == std::string::npos ? s : s.substr(0, p));
}

/* ------------------------------------------------------------------ */
/* cairo canvas wrapper                                                */

struct MM_Canvas
{
	cairo_surface_t * surf = nullptr;
	cairo_t *        cr   = nullptr;
	int              W = 0, H = 0;

	bool begin(int w, int h)
	{
		W = w; H = h;
		surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
		cr = cairo_create(surf);
		if (cairo_status(cr) != CAIRO_STATUS_SUCCESS)
			return false;
		cairo_set_source_rgb(cr, 1, 1, 1);
		cairo_paint(cr);
		cairo_select_font_face(cr, "Sans",
			CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		return true;
	}

	double textW(const std::string & s, double sz)
	{
		cairo_set_font_size(cr, sz);
		cairo_text_extents_t ext;
		cairo_text_extents(cr, s.c_str(), &ext);
		return ext.x_advance;
	}

	void setColor(double r, double g, double b)
	{
		cairo_set_source_rgb(cr, r, g, b);
	}

	/* draw text; ax in [0,1] = horizontal anchor (0 left, .5 centre),
	 * baseline is y + sz*0.35 centred when bCenterY */
	void text(double x, double y, const std::string & s, double sz,
			  double ax = 0.5, bool bCenterY = true,
			  double r = 0.13, double g = 0.13, double b = 0.13)
	{
		cairo_set_font_size(cr, sz);
		cairo_text_extents_t ext;
		cairo_text_extents(cr, s.c_str(), &ext);
		double tx = x - ext.width * ax - ext.x_bearing * ax;
		double ty = bCenterY ? y + ext.height / 2 : y;
		setColor(r, g, b);
		cairo_move_to(cr, tx, ty);
		cairo_show_text(cr, s.c_str());
	}

	void rect(double x, double y, double w, double h, double rad,
			  double fr, double fg, double fb,
			  double sr = 0.4, double sg = 0.4, double sb = 0.4,
			  double lw = 1.2)
	{
		cairo_new_path(cr);
		if (rad > 0)
		{
			double r2 = std::min(rad, std::min(w, h) / 2);
			cairo_arc(cr, x + w - r2, y + r2, r2, -M_PI / 2, 0);
			cairo_arc(cr, x + w - r2, y + h - r2, r2, 0, M_PI / 2);
			cairo_arc(cr, x + r2, y + h - r2, r2, M_PI / 2, M_PI);
			cairo_arc(cr, x + r2, y + r2, r2, M_PI, 3 * M_PI / 2);
			cairo_close_path(cr);
		}
		else
			cairo_rectangle(cr, x, y, w, h);
		cairo_set_source_rgb(cr, fr, fg, fb);
		cairo_fill_preserve(cr);
		cairo_set_source_rgb(cr, sr, sg, sb);
		cairo_set_line_width(cr, lw);
		cairo_stroke(cr);
	}

	void diamond(double cx, double cy, double w, double h,
				 double fr, double fg, double fb)
	{
		cairo_new_path(cr);
		cairo_move_to(cr, cx, cy - h / 2);
		cairo_line_to(cr, cx + w / 2, cy);
		cairo_line_to(cr, cx, cy + h / 2);
		cairo_line_to(cr, cx - w / 2, cy);
		cairo_close_path(cr);
		cairo_set_source_rgb(cr, fr, fg, fb);
		cairo_fill_preserve(cr);
		cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
		cairo_set_line_width(cr, 1.2);
		cairo_stroke(cr);
	}

	void ellipse(double cx, double cy, double w, double h,
				 double fr, double fg, double fb)
	{
		cairo_save(cr);
		cairo_translate(cr, cx, cy);
		cairo_scale(cr, w / 2, h / 2);
		cairo_new_path(cr);
		cairo_arc(cr, 0, 0, 1, 0, 2 * M_PI);
		cairo_restore(cr);
		cairo_set_source_rgb(cr, fr, fg, fb);
		cairo_fill_preserve(cr);
		cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
		cairo_set_line_width(cr, 1.2);
		cairo_stroke(cr);
	}

	void line(double x1, double y1, double x2, double y2,
			  bool dashed = false, double lw = 1.2,
			  double r = 0.35, double g = 0.35, double b = 0.35)
	{
		cairo_new_path(cr);
		if (dashed)
		{
			double pat[] = { 5, 4 };
			cairo_set_dash(cr, pat, 2, 0);
		}
		else
			cairo_set_dash(cr, nullptr, 0, 0);
		cairo_set_line_width(cr, lw);
		setColor(r, g, b);
		cairo_move_to(cr, x1, y1);
		cairo_line_to(cr, x2, y2);
		cairo_stroke(cr);
		cairo_set_dash(cr, nullptr, 0, 0);
	}

	/* arrowhead triangle pointing along (x1,y1)->(x2,y2) at (x2,y2) */
	void head(double x1, double y1, double x2, double y2,
			  bool open = false, double sz = 9)
	{
		double a = atan2(y2 - y1, x2 - x1);
		double p1x = x2 - sz * cos(a - 0.4);
		double p1y = y2 - sz * sin(a - 0.4);
		double p2x = x2 - sz * cos(a + 0.4);
		double p2y = y2 - sz * sin(a + 0.4);
		cairo_new_path(cr);
		cairo_move_to(cr, x2, y2);
		cairo_line_to(cr, p1x, p1y);
		cairo_line_to(cr, p2x, p2y);
		cairo_close_path(cr);
		if (open)
		{
			cairo_set_source_rgb(cr, 1, 1, 1);
			cairo_fill_preserve(cr);
		}
		setColor(0.35, 0.35, 0.35);
		if (open)
			cairo_stroke(cr);
		else
			cairo_fill(cr);
	}

	bool toPNG(UT_ByteBuf & out)
	{
		struct Ctx { UT_ByteBuf * buf; };
		Ctx ctx { &out };
		cairo_status_t st = cairo_surface_write_to_png_stream(surf,
			[](void * c, const unsigned char * d, unsigned int len)
				-> cairo_status_t
			{
				Ctx * p = static_cast<Ctx *>(c);
				return p->buf->append(
					reinterpret_cast<const UT_Byte *>(d), len)
					? CAIRO_STATUS_SUCCESS : CAIRO_STATUS_WRITE_ERROR;
			}, &ctx);
		cairo_destroy(cr);
		cairo_surface_destroy(surf);
		cr = nullptr; surf = nullptr;
		return st == CAIRO_STATUS_SUCCESS;
	}

	~MM_Canvas()
	{
		if (cr) cairo_destroy(cr);
		if (surf) cairo_surface_destroy(surf);
	}
};

/* palette used by pie/gantt/class fills */
static const double s_palette[][3] = {
	{ 0.72, 0.82, 0.95 }, { 0.99, 0.86, 0.64 }, { 0.75, 0.90, 0.75 },
	{ 0.93, 0.76, 0.84 }, { 0.85, 0.80, 0.95 }, { 0.99, 0.93, 0.70 },
	{ 0.73, 0.89, 0.89 }, { 0.96, 0.78, 0.70 }, { 0.82, 0.87, 0.72 },
	{ 0.80, 0.80, 0.90 }
};
static const int s_paletteN =
	static_cast<int>(sizeof(s_palette) / sizeof(s_palette[0]));

/* ------------------------------------------------------------------ */
/* flowchart: graph / flowchart                                        */

struct MM_Node
{
	std::string id, label;
	int         shape = 0; /* 0 rect, 1 rounded, 2 diamond, 3 circle,
							* 4 subroutine [[x]], 5 flag >x] */
	double      x = 0, y = 0, w = 0, h = 0;
	int         layer = 0;
};

struct MM_Edge
{
	int    from = -1, to = -1;
	std::string label;
	bool   arrow = true;
	bool   dashed = false;
	bool   thick = false;
};

/* parse "ID", "ID[text]", "ID{text}", "ID(text)", "ID((text))",
 * "ID[[text]]", "ID>text]" - returns the node index */
int mm_node(std::map<std::string, int> & ids,
			std::vector<MM_Node> & nodes, const std::string & tok0)
{
	std::string tok = mm_trim(tok0);
	if (tok.empty()) return -1;

	std::string id, label;
	int shape = 0;

	/* find the id: [A-Za-z0-9_]+ possibly quoted */
	size_t i = 0;
	bool quoted = false;
	if (tok[i] == '"') { quoted = true; i++; }
	while (i < tok.size())
	{
		char c = tok[i];
		if (quoted && c == '"') { i++; break; }
		if (!quoted &&
			!isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '.')
			break;
		i++;
	}
	id = tok.substr(0, i);
	label = id;
	if (quoted && !id.empty() && id.front() == '"' && id.back() == '"')
		id = id.substr(1, id.size() - 2);

	bool bShaped = false;
	std::string rest = mm_trim(tok.substr(i));
	if (!rest.empty())
	{
		char open = rest[0];
		char close = 0;
		int osz = 1;
		if (open == '[')
		{
			if (rest.size() > 1 && rest[1] == '[') { shape = 4; osz = 2; close = ']'; }
			else if (rest.size() > 1 && rest[1] == '(') { shape = 4; osz = 2; close = ')'; }
			else { shape = 0; close = ']'; }
		}
		else if (open == '{')
		{
			if (rest.size() > 1 && rest[1] == '{') { osz = 2; }
			shape = 2; close = '}';
		}
		else if (open == '(')
		{
			if (rest.size() > 2 && rest[1] == '(') { shape = 3; osz = 2; }
			else if (rest.size() > 2 && rest[1] == '[') { shape = 1; osz = 2; }
			else shape = 1;
			close = ')';
		}
		else if (open == '>')
		{
			shape = 5; close = ']';
		}
		if (close)
		{
			bShaped = true;
			size_t e = rest.rfind(close);
			std::string inner;
			if (e != std::string::npos && e >= static_cast<size_t>(osz))
				inner = rest.substr(osz, e - osz);
			else
				inner = rest.substr(osz);
			label = inner;
			/* strip surrounding quotes */
			if (label.size() >= 2 && label.front() == '"' && label.back() == '"')
				label = label.substr(1, label.size() - 2);
		}
	}

	auto it = ids.find(id);
	if (it != ids.end())
	{
		/* only an explicit shape/label on re-mention replaces the
		 * stored label; a bare id ("E") or stray punctuation must not
		 * clobber an earlier "E[End]" definition */
		if (bShaped)
		{
			nodes[it->second].label = label;
			nodes[it->second].shape = shape;
		}
		return it->second;
	}
	MM_Node n;
	n.id = id;
	n.label = label;
	n.shape = shape;
	ids[id] = static_cast<int>(nodes.size());
	nodes.push_back(n);
	return static_cast<int>(nodes.size()) - 1;
}

/* split "A -->|lbl| B" / "A -- lbl --> B" / "A --- B" chains into
 * (node,op,label,node) segments */
struct MM_LinkSeg { std::string a, op, label, b; };

std::vector<MM_LinkSeg> mm_parseLinks(const std::string & stmt)
{
	static const char * ops[] = {
		"o--o", "x--x", "<-->", "<--", "<-.-", "<==",
		"-.->", "-.-", "==>", "===", "-->", "---", "--x", "--o", "--",
		"-x", "-o", "-", nullptr
	};
	std::vector<MM_LinkSeg> segs;
	std::string s = stmt;
	size_t pos = 0;
	while (pos < s.size())
	{
		/* find next edge operator */
		size_t best = std::string::npos;
		std::string op;
		for (int k = 0; ops[k]; ++k)
		{
			size_t p = s.find(ops[k], pos);
			if (p != std::string::npos && (best == std::string::npos || p < best))
			{
				/* prefer the longest op at the same position */
				if (p < best || ops[k] == nullptr || strlen(ops[k]) > op.size())
				{
					best = p;
					op = ops[k];
				}
			}
		}
		if (best == std::string::npos) break;
		if (segs.empty())
		{
			MM_LinkSeg seg;
			seg.a = s.substr(pos, best - pos);
			seg.op = op;
			pos = best + op.size();
			/* optional |label| right after the op */
			if (pos < s.size() && s[pos] == '|')
			{
				size_t e = s.find('|', pos + 1);
				if (e != std::string::npos)
				{
					seg.label = s.substr(pos + 1, e - pos - 1);
					pos = e + 1;
				}
			}
			segs.push_back(seg);
		}
		else
		{
			segs.back().b = s.substr(pos, best - pos);
			MM_LinkSeg seg;
			seg.a = segs.back().b;
			seg.op = op;
			pos = best + op.size();
			if (pos < s.size() && s[pos] == '|')
			{
				size_t e = s.find('|', pos + 1);
				if (e != std::string::npos)
				{
					seg.label = s.substr(pos + 1, e - pos - 1);
					pos = e + 1;
				}
			}
			segs.push_back(seg);
		}
	}
	if (!segs.empty())
		segs.back().b = s.substr(pos);
	return segs;
}

/* point where the segment centre->centre exits the node bounds */
void mm_clip(const MM_Node & n, double tx, double ty,
			 double & ox, double & oy)
{
	double cx = n.x + n.w / 2, cy = n.y + n.h / 2;
	double dx = tx - cx, dy = ty - cy;
	if (dx == 0 && dy == 0) { ox = cx; oy = cy; return; }
	double hw = n.w / 2, hh = n.h / 2;
	double t = 1e9;
	if (dx != 0) t = std::min(t, hw / fabs(dx));
	if (dy != 0) t = std::min(t, hh / fabs(dy));
	ox = cx + dx * t;
	oy = cy + dy * t;
}

bool mm_renderFlow(const std::vector<std::string> & stmts,
				   const std::string & dir, UT_ByteBuf & out)
{
	std::map<std::string, int> ids;
	std::vector<MM_Node> nodes;
	std::vector<MM_Edge> edges;
	std::set<int> hasIncoming;
	bool horiz = (dir == "LR" || dir == "RL");

	for (const std::string & st0 : stmts)
	{
		std::string st = mm_nocomment(st0);
		if (st.empty()) continue;
		if (mm_starts(st, "subgraph") || st == "end" ||
			mm_starts(st, "direction") || mm_starts(st, "style ") ||
			mm_starts(st, "class ") || mm_starts(st, "classDef ") ||
			mm_starts(st, "linkStyle") || mm_starts(st, "click "))
			continue;

		auto segs = mm_parseLinks(st);
		if (segs.empty())
		{
			mm_node(ids, nodes, st); /* bare node decl */
			continue;
		}
		for (auto & s : segs)
		{
			int a = mm_node(ids, nodes, s.a);
			int b = mm_node(ids, nodes, s.b);
			if (a < 0 || b < 0) continue;
			MM_Edge e;
			e.from = a; e.to = b; e.label = mm_trim(s.label);
			e.arrow  = s.op.find('>') != std::string::npos;
			e.dashed = s.op.find("-.") != std::string::npos;
			e.thick  = s.op.find('=') != std::string::npos;
			if (s.op == "<--" || s.op == "<-.-" || s.op == "<==")
				std::swap(e.from, e.to);
			edges.push_back(e);
			hasIncoming.insert(e.to);
		}
	}
	if (nodes.empty()) return false;

	const double FS = 13, PAD = 9, LVL_GAP = 46, NODE_GAP = 26;
	MM_Canvas meas;
	meas.begin(8, 8);
	for (auto & n : nodes)
	{
		double tw = meas.textW(n.label, FS);
		n.w = tw + PAD * 2;
		n.h = FS + PAD * 1.7;
		if (n.shape == 2) { n.w = tw * 1.7 + PAD * 2; n.h *= 1.9; }
		else if (n.shape == 3) { n.w = n.h = std::max(tw + PAD * 2, n.h * 1.3); }
	}

	/* layering: longest path from sources */
	std::vector<int> layer(nodes.size(), 0), indeg(nodes.size(), 0);
	for (auto & e : edges) indeg[e.to]++;
	std::vector<int> order;
	for (size_t i = 0; i < nodes.size(); ++i) order.push_back((int)i);
	/* simple longest-path: iterate until stable (N <= small) */
	for (size_t pass = 0; pass < nodes.size() + 1; ++pass)
	{
		bool changed = false;
		for (auto & e : edges)
			if (layer[e.to] < layer[e.from] + 1)
			{ layer[e.to] = layer[e.from] + 1; changed = true; }
		if (!changed) break;
	}
	int maxLayer = 0;
	std::map<int, std::vector<int> > levels;
	for (size_t i = 0; i < nodes.size(); ++i)
	{
		levels[layer[i]].push_back((int)i);
		maxLayer = std::max(maxLayer, layer[i]);
	}

	/* geometry: per-level sizes */
	std::map<int, double> lvlCross, lvlLen;
	double crossTotal = 0;
	for (auto & kv : levels)
	{
		double len = 0, cross = 0;
		for (int i : kv.second)
		{
			double along = horiz ? nodes[i].h : nodes[i].w;
			double acr   = horiz ? nodes[i].w : nodes[i].h;
			len += along;
			cross = std::max(cross, acr);
		}
		len += NODE_GAP * (kv.second.size() - 1);
		lvlCross[kv.first] = cross;
		lvlLen[kv.first] = len;
		crossTotal += cross;
	}
	double crossSpan = crossTotal + LVL_GAP * maxLayer;
	double alongMax = 0;
	for (auto & kv : lvlLen) alongMax = std::max(alongMax, kv.second);

	double margin = 16;
	double W = horiz ? crossSpan : alongMax;
	double H = horiz ? alongMax : crossSpan;
	W += margin * 2; H += margin * 2;

	MM_Canvas c;
	if (!c.begin((int)ceil(W), (int)ceil(H))) return false;

	/* place nodes */
	double crossPos = margin;
	for (int l = 0; l <= maxLayer; ++l)
	{
		double cross = lvlCross[l];
		double along = lvlLen[l];
		double a = margin + (alongMax - along) / 2;
		for (int i : levels[l])
		{
			MM_Node & n = nodes[i];
			if (horiz)
			{
				double nh = n.h;
				n.x = crossPos + (cross - n.w) / 2;
				n.y = a;
				a += nh + NODE_GAP;
			}
			else
			{
				n.x = a;
				n.y = crossPos + (cross - n.h) / 2;
				a += n.w + NODE_GAP;
			}
		}
		crossPos += cross + LVL_GAP;
	}

	/* edges */
	for (auto & e : edges)
	{
		const MM_Node & A = nodes[e.from];
		const MM_Node & B = nodes[e.to];
		double x1, y1, x2, y2;
		mm_clip(A, B.x + B.w / 2, B.y + B.h / 2, x1, y1);
		mm_clip(B, A.x + A.w / 2, A.y + A.h / 2, x2, y2);
		c.line(x1, y1, x2, y2, e.dashed, e.thick ? 2.2 : 1.2);
		if (e.arrow) c.head(x1, y1, x2, y2);
		if (!e.label.empty())
		{
			double mx = (x1 + x2) / 2, my = (y1 + y2) / 2;
			double tw = c.textW(e.label, FS - 2);
			c.rect(mx - tw / 2 - 3, my - (FS - 2) / 2 - 3,
				   tw + 6, FS - 2 + 6, 2, 1, 1, 1, 0.85, 0.85, 0.85, 0.8);
			c.text(mx, my, e.label, FS - 2, 0.5, true);
		}
	}

	/* nodes */
	for (auto & n : nodes)
	{
		const double * col = s_palette[n.layer % s_paletteN];
		switch (n.shape)
		{
		case 1:
			c.rect(n.x, n.y, n.w, n.h, 7, col[0], col[1], col[2]);
			break;
		case 2:
			c.diamond(n.x + n.w / 2, n.y + n.h / 2, n.w, n.h,
					  0.99, 0.90, 0.66);
			break;
		case 3:
			c.ellipse(n.x + n.w / 2, n.y + n.h / 2, n.w, n.h,
					  col[0], col[1], col[2]);
			break;
		case 4:
			c.rect(n.x, n.y, n.w, n.h, 0, col[0], col[1], col[2]);
			c.line(n.x + 4, n.y, n.x + 4, n.y + n.h);
			c.line(n.x + n.w - 4, n.y, n.x + n.w - 4, n.y + n.h);
			break;
		default:
			c.rect(n.x, n.y, n.w, n.h, 2, col[0], col[1], col[2]);
		}
		c.text(n.x + n.w / 2, n.y + n.h / 2, n.label, FS);
	}
	return c.toPNG(out);
}

/* ------------------------------------------------------------------ */
/* sequenceDiagram                                                     */

struct MM_Msg { int a, b; std::string text; bool dashed, arrow = true, cross = false; };

bool mm_renderSequence(const std::vector<std::string> & lines,
					   UT_ByteBuf & out)
{
	std::vector<std::string> parts;
	std::map<std::string, int> pidx;
	std::vector<MM_Msg> msgs;
	struct Note { int which; int side; std::string text; int atMsg; };
	std::vector<Note> notes;

	auto pid = [&](const std::string & s) -> int
	{
		auto it = pidx.find(s);
		if (it != pidx.end()) return it->second;
		pidx[s] = (int)parts.size();
		parts.push_back(s);
		return (int)parts.size() - 1;
	};

	for (auto & l0 : lines)
	{
		std::string l = mm_nocomment(l0);
		if (l.empty()) continue;
		if (mm_starts(l, "participant ") || mm_starts(l, "actor ") ||
			mm_starts(l, "create participant "))
		{
			std::string n = mm_trim(l.substr(l.find(' ') + 1));
			size_t as = n.find(" as ");
			if (as != std::string::npos) n = mm_trim(n.substr(as + 4));
			pid(n);
			continue;
		}
		if (mm_starts(l, "note ") || mm_starts(l, "Note "))
		{
			/* "note right of X: text" / "note over X,Y: text" */
			Note nt; nt.atMsg = (int)msgs.size(); nt.which = -1; nt.side = 1;
			std::string r = mm_trim(l.substr(4));
			size_t colon = r.find(':');
			std::string head = colon == std::string::npos ? r : r.substr(0, colon);
			nt.text = colon == std::string::npos ? "" : mm_trim(r.substr(colon + 1));
			size_t of = head.find(" of "), over = head.find(" over ");
			if (of != std::string::npos)
			{
				nt.side = mm_starts(head, "left") ? -1 : 1;
				nt.which = pid(mm_trim(head.substr(of + 4)));
			}
			else if (over != std::string::npos)
			{
				nt.side = 0;
				nt.which = pid(mm_trim(head.substr(over + 5)));
			}
			if (nt.which >= 0) notes.push_back(nt);
			continue;
		}
		if (mm_starts(l, "autonumber") || mm_starts(l, "loop ") ||
			mm_starts(l, "alt ") || mm_starts(l, "else") ||
			mm_starts(l, "opt ") || l == "end" || mm_starts(l, "par ") ||
			mm_starts(l, "rect ") || mm_starts(l, "activate") ||
			mm_starts(l, "deactivate"))
			continue;

		/* message operators, longest first so "-->>" isn't matched
		 * by "->>" inside it */
		size_t p = std::string::npos;
		bool dashed = false, cross = false, arr = true;
		size_t oplen = 0;
		static const struct { const char * op; bool d, x, a; } s_ops[] = {
			{ "-->>", true,  false, true  },
			{ "--x",  true,  true,  false },
			{ "-.>>", true,  false, true  },
			{ "->>",  false, false, true  },
			{ "-->",  true,  false, true  },
			{ "-x",   false, true,  false },
			{ "->",   false, false, true  },
			{ "--",   true,  false, false },
			{ nullptr, false, false, false }
		};
		for (int k = 0; s_ops[k].op; ++k)
		{
			p = l.find(s_ops[k].op);
			if (p != std::string::npos)
			{
				oplen = strlen(s_ops[k].op);
				dashed = s_ops[k].d; cross = s_ops[k].x; arr = s_ops[k].a;
				break;
			}
		}
		if (p == std::string::npos) continue;

		MM_Msg m;
		m.a = pid(mm_trim(l.substr(0, p)));
		std::string rest = l.substr(p + oplen);
		size_t colon = rest.find(':');
		m.b = pid(mm_trim(colon == std::string::npos ? rest : rest.substr(0, colon)));
		m.text = colon == std::string::npos ? "" : mm_trim(rest.substr(colon + 1));
		m.dashed = dashed;
		m.arrow = arr;
		m.cross = cross;
		msgs.push_back(m);
	}
	if (parts.empty() || msgs.empty()) return false;

	const double FS = 13, PW = 24, PH_PAD = 9, ROW = 34, TOP = 46;
	MM_Canvas meas; meas.begin(8, 8);
	double W = 40;
	std::vector<double> px(parts.size());
	std::vector<double> pw(parts.size());
	for (size_t i = 0; i < parts.size(); ++i)
		pw[i] = meas.textW(parts[i], FS) + PW;
	for (size_t i = 0; i < parts.size(); ++i)
		px[i] = (i == 0) ? 30 : px[i - 1] + pw[i - 1] + 90;
	W = px.back() + pw.back() + 30;
	/* widen for message labels */
	for (auto & m : msgs)
	{
		double cx1 = px[m.a] + pw[m.a] / 2, cx2 = px[m.b] + pw[m.b] / 2;
		double need = meas.textW(m.text, FS) + 30;
		double have = fabs(cx2 - cx1);
		if (need > have) W += need - have;
	}
	double H = TOP + ROW * (msgs.size() + notes.size()) + 40;

	MM_Canvas c;
	if (!c.begin((int)ceil(W), (int)ceil(H))) return false;

	/* participants */
	for (size_t i = 0; i < parts.size(); ++i)
		c.rect(px[i], 14, pw[i], FS + PH_PAD * 2 - 4, 3,
			   s_palette[i % s_paletteN][0], s_palette[i % s_paletteN][1],
			   s_palette[i % s_paletteN][2]);
	for (size_t i = 0; i < parts.size(); ++i)
		c.text(px[i] + pw[i] / 2, 14 + (FS + PH_PAD * 2 - 4) / 2,
			   parts[i], FS);

	double boxBot = 14 + FS + PH_PAD * 2 - 4;
	double y = TOP;
	size_t noteIx = 0;
	for (size_t mi = 0; mi < msgs.size(); ++mi)
	{
		/* notes that precede this message */
		while (noteIx < notes.size() && notes[noteIx].atMsg == (int)mi)
		{
			const Note & nt = notes[noteIx];
			double cx = px[nt.which] + pw[nt.which] / 2;
			double nw = c.textW(nt.text, FS - 1) + 16;
			double nx = nt.side == 0 ? cx - nw / 2
				: nt.side < 0 ? cx - nw - 8 : cx + 8;
			c.rect(nx, y - 6, nw, FS + 10, 2, 0.99, 0.95, 0.75,
				   0.8, 0.75, 0.4, 1);
			c.text(nx + nw / 2, y - 6 + (FS + 10) / 2, nt.text, FS - 1);
			y += FS + 16;
			noteIx++;
		}
		const MM_Msg & m = msgs[mi];
		double x1 = px[m.a] + pw[m.a] / 2;
		double x2 = px[m.b] + pw[m.b] / 2;
		if (m.a == m.b) /* self message: small loop */
		{
			double lx = x1, rx = x1 + 40;
			c.line(lx, y, rx, y, m.dashed);
			c.line(rx, y, rx, y + 14, m.dashed);
			c.line(rx, y + 14, lx, y + 14, m.dashed);
			if (m.arrow) c.head(rx, y + 14, lx, y + 14);
			c.text(lx + 46, y + 7, m.text, FS, 0.0, true);
			y += ROW;
			continue;
		}
		double ty = y + 12;
		if (!m.text.empty())
			c.text((x1 + x2) / 2, y, m.text, FS, 0.5, false);
		c.line(x1, ty, x2, ty, m.dashed);
		if (m.cross)
		{
			c.line(x2 - 5, ty - 5, x2 + 5, ty + 5);
			c.line(x2 - 5, ty + 5, x2 + 5, ty - 5);
		}
		else if (m.arrow)
			c.head(x1, ty, x2, ty);
		y += ROW;
	}
	/* lifelines */
	for (size_t i = 0; i < parts.size(); ++i)
		c.line(px[i] + pw[i] / 2, boxBot,
			   px[i] + pw[i] / 2, y - 8, true, 1);
	return c.toPNG(out);
}

/* ------------------------------------------------------------------ */
/* gantt                                                               */

/* days-from-civil (Howard Hinnant) */
static long mm_days(int y, unsigned m, unsigned d)
{
	y -= m <= 2;
	const long era = (y >= 0 ? y : y - 399) / 400;
	const unsigned yoe = (unsigned)(y - era * 400);
	const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + (long)doe - 719468;
}

static bool mm_parseDate(const std::string & s, long & days)
{
	int y, m, d;
	if (sscanf(s.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return false;
	days = mm_days(y, (unsigned)m, (unsigned)d);
	return true;
}

bool mm_renderGantt(const std::vector<std::string> & lines,
					UT_ByteBuf & out)
{
	struct Task { std::string name, section; long start = 0, dur = 1;
				  bool isSection = false; };
	std::vector<Task> tasks;
	std::string title, section;
	std::map<std::string, const Task *> byId;

	for (auto & l0 : lines)
	{
		std::string l = mm_nocomment(l0);
		if (l.empty()) continue;
		if (mm_starts(l, "gantt")) continue;
		if (mm_starts(l, "title ")) { title = mm_trim(l.substr(6)); continue; }
		if (mm_starts(l, "dateFormat") || mm_starts(l, "axisFormat") ||
			mm_starts(l, "tickInterval") || mm_starts(l, "excludes") ||
			mm_starts(l, "includes") || mm_starts(l, "todayMarker") ||
			mm_starts(l, "inclusiveEndDates") || mm_starts(l, "topAxis") ||
			mm_starts(l, "weekday") || mm_starts(l, "weekend"))
			continue;
		if (mm_starts(l, "section "))
		{
			section = mm_trim(l.substr(7));
			Task t; t.name = section; t.isSection = true;
			tasks.push_back(t);
			continue;
		}
		/* task: "Name :id?, start, dur" */
		size_t colon = l.find(':');
		if (colon == std::string::npos) continue;
		Task t;
		t.name = mm_trim(l.substr(0, colon));
		t.section = section;
		std::string meta = mm_trim(l.substr(colon + 1));

		std::vector<std::string> parts;
		size_t mp = 0;
		while (mp <= meta.size())
		{
			size_t cm = meta.find(',', mp);
			if (cm == std::string::npos) cm = meta.size();
			parts.push_back(mm_trim(meta.substr(mp, cm - mp)));
			mp = cm + 1;
		}
		std::string id, startS, durS;
		for (auto & p : parts)
		{
			if (p.empty()) continue;
			if (mm_starts(p, "after "))
			{
				startS = p;
				continue;
			}
			long dv;
			if (mm_parseDate(p, dv))
			{
				if (startS.empty()) startS = p;
				else durS = p;
				continue;
			}
			char u = p.back();
			if (u == 'd' || u == 'h' || u == 'w' || u == 'm')
			{
				durS = p;
				continue;
			}
			if (p == "done" || p == "active" || p == "crit" ||
				p == "milestone") continue;
			if (id.empty()) id = p;
		}
		if (mm_starts(startS, "after "))
		{
			auto it = byId.find(mm_trim(startS.substr(6)));
			if (it != byId.end()) t.start = it->second->start + it->second->dur;
			else t.start = tasks.empty() ? mm_days(2024,1,1) : tasks.back().start;
		}
		else
		{
			if (!mm_parseDate(startS, t.start)) continue;
		}
		if (!durS.empty())
		{
			char u = durS.back();
			long v = atol(durS.c_str());
			if (u == 'w') v *= 7;
			else if (u == 'h' || u == 'm') v = std::max(1L, v / 24);
			long endD;
			if (mm_parseDate(durS, endD)) t.dur = std::max(1L, endD - t.start);
			else t.dur = std::max(1L, v);
		}
		tasks.push_back(t);
		if (!id.empty()) byId[id] = &tasks.back();
	}
	if (tasks.empty()) return false;

	long d0 = 1L << 60, d1 = 0;
	for (auto & t : tasks)
	{
		if (t.isSection) continue;
		d0 = std::min(d0, t.start);
		d1 = std::max(d1, t.start + t.dur);
	}
	if (d1 <= d0) d1 = d0 + 1;

	const double FS = 12, RH = 24, LBL_W = 150, TOPH = 46;
	double chartW = std::max(300.0, (d1 - d0) * 24.0);
	double W = LBL_W + chartW + 20;
	double H = TOPH + RH * tasks.size() + 24;

	MM_Canvas c;
	if (!c.begin((int)ceil(W), (int)ceil(H))) return false;

	if (!title.empty())
		c.text(W / 2, 10, title, FS + 3, 0.5, false);

	double top = TOPH;
	/* date ticks: start/middle/end */
	for (int k = 0; k <= 2; ++k)
	{
		long dd = d0 + (d1 - d0) * k / 2;
		/* civil from days */
		long z = dd + 719468;
		long era = (z >= 0 ? z : z - 146096) / 146097;
		unsigned doe = (unsigned)(z - era * 146097);
		unsigned yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
		long yy = (long)yoe + era * 400;
		unsigned doy = doe - (365*yoe + yoe/4 - yoe/100);
		unsigned mp = (5*doy + 2)/153;
		unsigned dd2 = doy - (153*mp+2)/5 + 1;
		unsigned mm2 = mp + (mp < 10 ? 3 : -9);
		yy += (mm2 <= 2);
		char buf[40];
		snprintf(buf, sizeof(buf), "%04d-%02u-%02u",
				 static_cast<int>(yy), mm2, dd2);
		double x = LBL_W + (double)(dd - d0) / (d1 - d0) * chartW;
		c.text(std::min(x, W - 50), top - 14, buf, FS - 1);
		c.line(x, top - 4, x, H - 14, true, 0.6, 0.8, 0.8, 0.8);
	}

	int row = 0;
	for (auto & t : tasks)
	{
		double y = top + row * RH;
		if (t.isSection)
		{
			c.rect(LBL_W - 4, y + 4, chartW + 4, RH - 8, 0,
				   0.92, 0.92, 0.92, 0.92, 0.92, 0.92, 0.5);
			c.text(LBL_W + 4, y + RH / 2, t.name, FS, 0.0, true,
				   0.3, 0.3, 0.3);
		}
		else
		{
			c.text(LBL_W - 8, y + RH / 2, t.name, FS - 1, 1.0, true);
			double bx = LBL_W + (double)(t.start - d0) / (d1 - d0) * chartW;
			double bw = std::max(6.0, (double)t.dur / (d1 - d0) * chartW);
			const double * col = s_palette[(row * 3) % s_paletteN];
			c.rect(bx, y + 5, bw, RH - 10, 3, col[0], col[1], col[2]);
		}
		row++;
	}
	/* chart border - stroke only, no fill over the bars */
	cairo_rectangle(c.cr, LBL_W, top - 4, chartW, H - top - 10);
	cairo_set_source_rgb(c.cr, 0.6, 0.6, 0.6);
	cairo_set_line_width(c.cr, 0.8);
	cairo_stroke(c.cr);
	return c.toPNG(out);
}

/* ------------------------------------------------------------------ */
/* classDiagram                                                        */

bool mm_renderClass(const std::vector<std::string> & lines,
					UT_ByteBuf & out)
{
	struct Cls { std::string name; std::vector<std::string> members;
				 double x = 0, y = 0, w = 0, h = 0; };
	std::vector<Cls> classes;
	std::map<std::string, int> cidx;
	struct Rel { int a, b; std::string op, la, lb; };
	std::vector<Rel> rels;

	auto cid = [&](const std::string & n0) -> int
	{
		std::string n = mm_trim(n0);
		auto it = cidx.find(n);
		if (it != cidx.end()) return it->second;
		Cls c; c.name = n;
		cidx[n] = (int)classes.size();
		classes.push_back(c);
		return (int)classes.size() - 1;
	};

	Cls * open = nullptr;
	for (auto & l0 : lines)
	{
		std::string l = mm_nocomment(l0);
		if (l.empty()) continue;
		if (mm_starts(l, "classDiagram")) continue;
		if (open)
		{
			if (l == "}") { open = nullptr; continue; }
			open->members.push_back(l);
			continue;
		}
		if (mm_starts(l, "class "))
		{
			std::string r = mm_trim(l.substr(6));
			size_t b = r.find('{');
			if (b != std::string::npos)
			{
				open = &classes[cid(mm_trim(r.substr(0, b)))];
				continue;
			}
			cid(r);
			continue;
		}
		/* inline member: "Name : +member" */
		size_t cn = l.find(" : ");
		if (cn != std::string::npos && l.find("--") == std::string::npos &&
			l.find("..") == std::string::npos)
		{
			int i = cid(mm_trim(l.substr(0, cn)));
			classes[i].members.push_back(mm_trim(l.substr(cn + 3)));
			continue;
		}
		/* relation: A <|-- B, A --|> B, A --> B, A ..> B, A --o B, ... */
		static const char * rops[] = {
			"<|--", "--|>", "<|..", "..|>", "*--", "o--", "--*", "--o",
			"..>", "-->", "..", "--", nullptr
		};
		size_t best = std::string::npos; std::string op;
		for (int k = 0; rops[k]; ++k)
		{
			size_t p = l.find(rops[k]);
			if (p != std::string::npos && (best == std::string::npos ||
				p < best || (p == best && strlen(rops[k]) > op.size())))
			{ best = p; op = rops[k]; }
		}
		if (best == std::string::npos) continue;
		Rel r;
		r.a = cid(l.substr(0, best));
		std::string right = l.substr(best + op.size());
		size_t colon = right.find(':');
		if (colon != std::string::npos)
		{ r.lb = mm_trim(right.substr(colon + 1)); right = right.substr(0, colon); }
		r.b = cid(right);
		r.op = op;
		rels.push_back(r);
	}
	if (classes.empty()) return false;

	const double FS = 12, PAD = 8;
	MM_Canvas meas; meas.begin(8, 8);
	for (auto & cl : classes)
	{
		double w = meas.textW(cl.name, FS + 1) + PAD * 2;
		for (auto & m : cl.members)
			w = std::max(w, meas.textW(m, FS - 1) + PAD * 2);
		cl.w = w;
		cl.h = (FS + 10) + cl.members.size() * (FS + 4) + 8;
	}
	/* grid layout: up to 3 per row */
	double W = 30, maxRowH = 0, x = 24, y = 24, rowW = 0;
	int inRow = 0;
	for (auto & cl : classes)
	{
		if (inRow == 3) { y += maxRowH + 60; x = 24; maxRowH = 0; inRow = 0; }
		cl.x = x; cl.y = y;
		x += cl.w + 60; inRow++;
		maxRowH = std::max(maxRowH, cl.h);
		rowW = std::max(rowW, x);
	}
	W = rowW + 24;
	double H = y + maxRowH + 24;

	MM_Canvas c;
	if (!c.begin((int)ceil(W), (int)ceil(H))) return false;

	for (auto & r : rels)
	{
		const Cls & A = classes[r.a];
		const Cls & B = classes[r.b];
		double ax = A.x + A.w / 2, ay = A.y + A.h / 2;
		double bx = B.x + B.w / 2, by = B.y + B.h / 2;
		double x1, y1, x2, y2;
		/* clip against class rects */
		double dx = bx - ax, dy = by - ay;
		double t1 = 1e9;
		if (dx) t1 = std::min(t1, (A.w / 2) / fabs(dx));
		if (dy) t1 = std::min(t1, (A.h / 2) / fabs(dy));
		x1 = ax + dx * t1; y1 = ay + dy * t1;
		double t2 = 1e9;
		if (dx) t2 = std::min(t2, (B.w / 2) / fabs(dx));
		if (dy) t2 = std::min(t2, (B.h / 2) / fabs(dy));
		x2 = bx - dx * t2; y2 = by - dy * t2;
		bool dashed = r.op.find("..") != std::string::npos;
		c.line(x1, y1, x2, y2, dashed);
		if (r.op.find("|>") != std::string::npos || r.op.find("<|") != std::string::npos)
		{
			/* open triangle at the parent end */
			if (r.op[0] == '<') c.head(x2, y2, x1, y1, true, 11);
			else c.head(x1, y1, x2, y2, true, 11);
		}
		else if (r.op.find('>') != std::string::npos)
			c.head(x1, y1, x2, y2);
		if (!r.lb.empty())
			c.text((x1 + x2) / 2, (y1 + y2) / 2 - 8, r.lb, FS - 1);
	}

	for (auto & cl : classes)
	{
		double nameH = FS + 10;
		c.rect(cl.x, cl.y, cl.w, cl.h, 2, s_palette[0][0],
			   s_palette[0][1], s_palette[0][2]);
		c.rect(cl.x, cl.y, cl.w, nameH, 2, 0.60, 0.72, 0.88);
		c.text(cl.x + cl.w / 2, cl.y + nameH / 2, cl.name, FS + 1);
		double my = cl.y + nameH + 5;
		for (auto & m : cl.members)
		{
			c.text(cl.x + PAD, my, m, FS - 1, 0.0, true);
			my += FS + 4;
		}
		if (!cl.members.empty())
			c.line(cl.x, cl.y + nameH, cl.x + cl.w, cl.y + nameH);
	}
	return c.toPNG(out);
}

/* ------------------------------------------------------------------ */
/* pie                                                                 */

bool mm_renderPie(const std::vector<std::string> & lines,
				  UT_ByteBuf & out)
{
	std::string title;
	std::vector<std::pair<std::string, double> > slices;

	for (auto & l0 : lines)
	{
		std::string l = mm_nocomment(l0);
		if (l.empty() || l == "pie") continue;
		if (mm_starts(l, "title ")) { title = mm_trim(l.substr(6)); continue; }
		if (mm_starts(l, "showData")) continue;
		size_t colon = l.rfind(':');
		if (colon == std::string::npos) continue;
		std::string name = mm_trim(l.substr(0, colon));
		if (name.size() >= 2 && name.front() == '"' && name.back() == '"')
			name = name.substr(1, name.size() - 2);
		double v = atof(l.substr(colon + 1).c_str());
		if (v <= 0) continue;
		slices.push_back({ name, v });
	}
	if (slices.empty()) return false;

	const double FS = 13, R = 110, LX = 2 * R + 70;
	double total = 0;
	for (auto & s : slices) total += s.second;

	MM_Canvas meas; meas.begin(8, 8);
	double legW = 0;
	for (auto & s : slices)
		legW = std::max(legW, meas.textW(s.first, FS) +
						meas.textW(" (100%)", FS));
	double W = LX + legW + 40;
	double H = std::max(2 * R + 50.0, 60.0 + slices.size() * 22);

	MM_Canvas c;
	if (!c.begin((int)ceil(W), (int)ceil(H))) return false;

	if (!title.empty())
		c.text(W / 2, 14, title, FS + 3, 0.5, false);

	double cx = R + 24, cy = H / 2 + (title.empty() ? 0 : 8);
	double a0 = -M_PI / 2;
	for (size_t i = 0; i < slices.size(); ++i)
	{
		double frac = slices[i].second / total;
		double a1 = a0 + frac * 2 * M_PI;
		const double * col = s_palette[i % s_paletteN];
		cairo_new_path(c.cr);
		cairo_move_to(c.cr, cx, cy);
		cairo_arc(c.cr, cx, cy, R, a0, a1);
		cairo_close_path(c.cr);
		cairo_set_source_rgb(c.cr, col[0], col[1], col[2]);
		cairo_fill_preserve(c.cr);
		cairo_set_source_rgb(c.cr, 1, 1, 1);
		cairo_set_line_width(c.cr, 1.2);
		cairo_stroke(c.cr);
		a0 = a1;
	}
	/* legend */
	double ly = cy - slices.size() * 11;
	for (size_t i = 0; i < slices.size(); ++i)
	{
		const double * col = s_palette[i % s_paletteN];
		c.rect(LX, ly - 7, 12, 12, 2, col[0], col[1], col[2]);
		std::string label = slices[i].first +
			" (" + std::to_string(
				static_cast<int>(slices[i].second / total * 100 + 0.5)) +
			"%)";
		c.text(LX + 20, ly, label, FS, 0.0, true);
		ly += 22;
	}
	return c.toPNG(out);
}

/* ------------------------------------------------------------------ */

} // anonymous namespace

bool UT_Mermaid::renderToPNG(const std::string & source, UT_ByteBuf & out)
{
	/* find the diagram-type keyword */
	std::string first;
	auto lines = mm_lines(source);
	size_t k = 0;
	for (; k < lines.size(); ++k)
	{
		std::string t = mm_nocomment(lines[k]);
		if (!t.empty()) { first = t; break; }
	}
	if (first.empty()) return false;

	std::vector<std::string> body(lines.begin() + (long)k + 1, lines.end());

	if (mm_starts(first, "graph") || mm_starts(first, "flowchart"))
	{
		std::string dir = "TD";
		{
			std::string rest = mm_trim(first.substr(first.find(' ') + 1));
			if (mm_starts(rest, "LR") || mm_starts(rest, "RL") ||
				mm_starts(rest, "TB") || mm_starts(rest, "TD") ||
				mm_starts(rest, "BT"))
				dir = rest.substr(0, 2);
		}
		return mm_renderFlow(body, dir, out);
	}
	if (mm_starts(first, "sequenceDiagram"))
		return mm_renderSequence(body, out);
	if (mm_starts(first, "gantt"))
	{
		std::vector<std::string> all = lines;
		return mm_renderGantt(all, out);
	}
	if (mm_starts(first, "classDiagram"))
	{
		std::vector<std::string> all = lines;
		return mm_renderClass(all, out);
	}
	if (mm_starts(first, "pie"))
	{
		std::vector<std::string> all = lines;
		return mm_renderPie(all, out);
	}
	return false;
}
