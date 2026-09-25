/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiWord
 * Copyright (C) 1998 AbiSource, Inc.
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

#ifndef IE_IMPEXP_ABIWORD_H
#define IE_IMPEXP_ABIWORD_H

#define IE_MIMETYPE_AbiWord			"application/x-abiword"
#define IE_MIMETYPE_ABINOVA			"application/x-abinova"

/* namespaces written into .abwn files - they mirror the doctype's
 * system identifier and replace the historical abisource.com URIs;
 * the XML vocabulary itself is unchanged */
#define ABINOVA_XMLNS				"https://raw.githubusercontent.com/janos-szenfner/Exp-Abi/main/abwn.dtd"
#define ABINOVA_XMLNS_CT			"https://raw.githubusercontent.com/janos-szenfner/Exp-Abi/main/changetracking.dtd"

#endif /* IE_IMPEXP_ABIWORD_H */
