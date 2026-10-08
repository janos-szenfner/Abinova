/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: t -*- */
/* Abinova
 * Copyright (C) 2026 Abinova contributors
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

/*
 * Compatibility shim (TST01): the in-process crash/hang watchdog
 * and main-loop helpers moved to the shared test-framework header
 * src/af/tf/xp/tf_guard.h so every suite and the UI driver can use
 * them.  Keep the historical include + namespace working.
 */

#ifndef EM_GUARD_H
#define EM_GUARD_H

#include "tf_guard.h"

namespace em_guard = tf_guard;

#endif /* EM_GUARD_H */
