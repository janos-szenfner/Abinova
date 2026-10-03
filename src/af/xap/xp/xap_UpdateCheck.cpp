/* AbiSource Application Framework
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

#include "config.h"

#include <climits>
#include <cstdlib>
#include <string>

#include <gio/gio.h>

#include "xap_UpdateCheck.h"

/* blocking HTTPS GET; true when the server gave a 2xx response (body
 * may still be empty, e.g. an empty JSON list), false on any
 * transport or HTTP error. On false errOut names the cause */
bool XAP_httpsGet(const char * host, const char * path,
				  std::string & bodyOut, std::string & errOut)
{
	bodyOut.clear();
	errOut.clear();

	/* GSocketClient's TLS path is a GIO extension point - it needs a
	 * runtime module (glib-networking / openssl backend) plus a CA
	 * bundle. Bundled builds must ship both (see PACK06) or there is
	 * simply no TLS at all; name the cause instead of a generic
	 * failure */
	GTlsBackend * tls = g_tls_backend_get_default();
	if (!tls || !g_tls_backend_supports_tls(tls))
	{
		errOut = "TLS support is unavailable - the GLib TLS module "
				 "(glib-networking) is missing from this installation.";
		return false;
	}

	GError * err = nullptr;
	GSocketClient * client = g_socket_client_new();
	g_socket_client_set_tls(client, TRUE);
	g_socket_client_set_timeout(client, 15);

	GSocketConnection * conn = g_socket_client_connect_to_host(
		client, host, 443, nullptr, &err);
	g_object_unref(client);
	if (!conn)
	{
		errOut = "cannot reach ";
		errOut += host;
		if (err && err->message)
		{
			errOut += " (";
			errOut += err->message;
			errOut += ")";
		}
		g_clear_error(&err);
		return false;
	}

	/* HTTP/1.0 has no chunked transfer coding - the response body is
	 * close-delimited. A 1.1 request could legally get a chunked body
	 * a chunk boundary could split across ("tag_name" cut in two),
	 * and this client does not decode chunks */
	std::string req = "GET ";
	req += path;
	req += " HTTP/1.0\r\nHost: ";
	req += host;
	req += "\r\nUser-Agent: abinova-update-check\r\n"
		   "Accept: application/vnd.github+json\r\n"
		   "Connection: close\r\n\r\n";

	GOutputStream * out = g_io_stream_get_output_stream(G_IO_STREAM(conn));
	GInputStream * in = g_io_stream_get_input_stream(G_IO_STREAM(conn));
	bool ok = g_output_stream_write_all(out, req.data(), req.size(),
										nullptr, nullptr, &err) == TRUE;
	if (!ok)
	{
		errOut = "sending the request to ";
		errOut += host;
		errOut += " failed";
		g_clear_error(&err);
	}
	if (ok)
	{
		std::string resp;
		char buf[4096];
		gssize n;
		while ((n = g_input_stream_read(in, buf, sizeof(buf),
										nullptr, &err)) > 0)
			resp.append(buf, n);
		if (n < 0)
		{
			errOut = "the connection to ";
			errOut += host;
			errOut += " broke mid-response";
			g_clear_error(&err);
			ok = false;
		}

		if (ok)
		{
			size_t sp = resp.find(' ');
			long status = (sp == std::string::npos)
				? 0 : strtol(resp.c_str() + sp + 1, nullptr, 10);
			size_t bodyPos = resp.find("\r\n\r\n");
			ok = status >= 200 && status < 300 &&
				bodyPos != std::string::npos;
			if (!ok)
			{
				if (bodyPos == std::string::npos)
				{
					errOut = host;
					errOut += " sent a malformed HTTP response";
				}
				else
				{
					errOut = host;
					errOut += " answered with HTTP status ";
					errOut += std::to_string(status);
				}
			}
			else
				bodyOut = resp.substr(bodyPos + 4);
		}
	}
	g_io_stream_close(G_IO_STREAM(conn), nullptr, nullptr);
	g_object_unref(conn);
	return ok;
}

/* minimal "key":"value" extraction - good enough for tag names/URLs */
std::string XAP_jsonStringValue(const std::string & json, const char * key)
{
	std::string k = "\"";
	k += key;
	k += "\"";
	size_t p = json.find(k);
	if (p == std::string::npos)
		return std::string();
	p = json.find(':', p + k.size());
	if (p == std::string::npos)
		return std::string();
	p = json.find('"', p + 1);
	if (p == std::string::npos)
		return std::string();
	size_t e = json.find('"', p + 1);
	if (e == std::string::npos)
		return std::string();
	return json.substr(p + 1, e - p - 1);
}

/* parse "v4.0.0"-style tags into numeric components; digit runs
 * saturate at INT_MAX so a pathological server tag cannot overflow */
bool XAP_parseVersionTag(const std::string & tag, int out[3])
{
	const char * p = tag.c_str();
	while (*p && !g_ascii_isdigit(*p))
		++p;
	out[0] = out[1] = out[2] = 0;
	int n = 0;
	while (*p && n < 3)
	{
		if (!g_ascii_isdigit(*p))
			break;
		while (g_ascii_isdigit(*p))
		{
			int digit = *p - '0';
			if (out[n] > (INT_MAX - digit) / 10)
				out[n] = INT_MAX;
			else
				out[n] = out[n] * 10 + digit;
			++p;
		}
		++n;
		if (*p == '.')
			++p;
	}
	return n > 0;
}

void XAP_updateCheckQuery(XAP_UpdateInfo & info, const char * currentVersion)
{
	/* newest published release first... */
	std::string body, err;
	bool answered = XAP_httpsGet(
		"api.github.com",
		"/repos/janos-szenfner/Abinova/releases/latest", body, err);
	if (answered)
	{
		info.version = XAP_jsonStringValue(body, "tag_name");
		info.url = XAP_jsonStringValue(body, "html_url");
	}

	/* ...otherwise fall back to the newest git tag (a repo with no
	 * releases at all answers 404 on releases/latest) */
	if (info.version.empty())
	{
		body.clear();
		err.clear();
		if (XAP_httpsGet("api.github.com",
						 "/repos/janos-szenfner/Abinova/tags",
						 body, err))
		{
			answered = true;
			info.version = XAP_jsonStringValue(body, "name");
		}
	}

	/* the check itself succeeded if the server answered at all, even
	 * when the repo simply has no releases/tags yet */
	info.fetched = answered;
	if (!info.fetched)
	{
		info.errorDetail = err;
		return;
	}
	int cur[3], lat[3];
	if (XAP_parseVersionTag(currentVersion ? currentVersion : "", cur) &&
		XAP_parseVersionTag(info.version, lat))
	{
		info.newer = lat[0] > cur[0] ||
			(lat[0] == cur[0] && lat[1] > cur[1]) ||
			(lat[0] == cur[0] && lat[1] == cur[1] && lat[2] > cur[2]);
	}
	if (info.url.empty() && !info.version.empty())
	{
		info.url =
			"https://github.com/janos-szenfner/Abinova/releases/tag/" +
			info.version;
	}
}
