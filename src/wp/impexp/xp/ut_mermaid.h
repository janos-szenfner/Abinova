/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */
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

#include <string>

class UT_ByteBuf;

/*!
 * Minimal built-in Mermaid renderer.
 *
 * Covers the common subset of the Mermaid diagram language and draws
 * the result with Cairo into PNG bytes, so ```mermaid fenced blocks
 * can be embedded as images on import - no JavaScript engine needed.
 *
 * Supported diagram types:
 *   graph / flowchart (TD/LR)  - nodes [], {}, (), (()), edges -->, ---,
 *                                -.->, ==> with optional |labels|
 *   sequenceDiagram            - participant/actor, ->>, -->>, ->, -->
 *                                messages, note left/right/over
 *   gantt                      - title, dateFormat, section, tasks with
 *                                dates, `after id`, and Nd/Nw durations
 *   classDiagram               - class boxes with members, <|-- and
 *                                related connectors
 *   pie                        - title + "label" : value slices
 */
class ABI_EXPORT UT_Mermaid
{
public:
	/*! Render Mermaid source to PNG.  Returns false when the source
	 *  is not a recognised diagram type (caller should fall back to
	 *  displaying the source verbatim). */
	static bool renderToPNG(const std::string & source, UT_ByteBuf & out);
};
