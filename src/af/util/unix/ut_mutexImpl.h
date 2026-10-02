/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource Program Utilities
 * Copyright (C) 2002 Dom Lachowicz <cinamod@hotmail.com>
 * Copyright (C) 2025 Hubert Figuière
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

#include <glib.h>

/*!
 * Unix GThread impl of a mutex class
 */
class UT_MutexImpl
{
public:

	UT_MutexImpl ()
		{
			g_rec_mutex_init(&mStaticMutex);
		}

	~UT_MutexImpl ()
		{
			g_rec_mutex_clear(&mStaticMutex);
		}

  void lock ()
		{
			g_rec_mutex_lock(&mStaticMutex);
		}

  void unlock ()
		{
			g_rec_mutex_unlock(&mStaticMutex) ;
		}

private:

	UT_MutexImpl(const UT_MutexImpl & other) = delete;
	UT_MutexImpl & operator=(const UT_MutexImpl & other) = delete;

	GRecMutex mStaticMutex;
};
