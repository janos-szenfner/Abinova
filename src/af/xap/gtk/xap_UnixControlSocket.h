/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* Abinova: a word processor
 *
 * Copyright (C) 2024 Abinova contributors
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
 */

#ifndef XAP_UNIXCONTROLSOCKET_H
#define XAP_UNIXCONTROLSOCKET_H

class XAP_App;

/* Runtime control channel: a JSON-lines TCP listener on loopback
 * that lets tests and automation drive a live session — dispatching
 * edit methods by name and snapshotting windows — instead of
 * injecting synthetic X events.
 *
 * Enabled by --control=<port> on the command line or the
 * ABINOVA_CONTROL_PORT environment variable.  Requests must carry
 * the key written to
 *   <user-private-dir>/control-key.local-<port>
 * (mode 0600), e.g.
 *   {"id":1,"key":"…","method":"engine.execute",
 *    "params":{"command":"insertData","data":"Hello"}}
 *   {"id":2,"key":"…","method":"ui.screenshot",
 *    "params":{"path":"/tmp/shot.png"}}
 * Responses are {"id":N,"result":{…}} or {"id":N,"error":{"message":…}}.
 */
bool abi_control_start(XAP_App * pApp, int port);

/* Automation path confinement: --automation-read-root=DIR and
 * --automation-write-root=DIR (repeatable) limit which filesystem
 * paths control-channel requests may touch.  Roots are resolved
 * through symlinks once at registration; every checked path is
 * likewise resolved (deepest existing ancestor + remaining
 * components) and must equal a root or sit under it at a path
 * boundary.  With no roots registered, nothing is confined. */
void abi_automation_add_root(bool write, const char * path);
bool abi_automation_path_allowed(bool write, const char * path);

#endif /* XAP_UNIXCONTROLSOCKET_H */
