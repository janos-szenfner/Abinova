/* The AbiWord library
 *
 * Copyright (C) 2006 Robert Staudinger <robert.staudinger@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ap_Args.h"
#include "ap_UnixApp.h"
#include "libabinova.h"

static AP_UnixApp *_abinova_app = nullptr;

/**
 * libabinova_init:
 * @argc: argument count
 * @argv: (array length=argc): Commandline arguments
 *
 * Initializes libabinova
 */
void libabinova_init (int argc, char **argv)
{
	if (!_abinova_app) {
		_abinova_app = new AP_UnixApp(PACKAGE);
		XAP_Args XArgs(argc, argv);
		AP_Args Args = AP_Args(&XArgs, PACKAGE, _abinova_app);
		/* TODO do we need to add the gtk's GOptionGroup here? */
		Args.parseOptions();
		_abinova_app->initialize(TRUE);
		/* TODO set up segfault handlers */
	}
}

/**
 * libabinova_init_noargs:
 *
 * Initializes libabinova
 */
void libabinova_init_noargs ()
{
	if (!_abinova_app) {
		static char *argv[] = {"libabinova", nullptr};
		_abinova_app = new AP_UnixApp(PACKAGE);
		XAP_Args XArgs(1, argv);
		AP_Args Args = AP_Args(&XArgs, PACKAGE, _abinova_app);
		Args.parseOptions();
		_abinova_app->initialize(TRUE);
		/* TODO set up segfault handlers */
	}
}

void libabinova_shutdown ()
{
	if (_abinova_app ) 
	{
		_abinova_app->shutdown();
		delete _abinova_app;
		_abinova_app = nullptr;
	}
}
