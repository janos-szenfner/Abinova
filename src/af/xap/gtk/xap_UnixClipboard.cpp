/* AbiSource Application Framework
 * Copyright (c) 2002 Dom Lachowicz
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

#include <string.h>
#include "xap_UnixClipboard.h"
#include "xap_Frame.h"
#include "xav_View.h"

//////////////////////////////////////////////////////////////////
// GdkContentProvider subclass that serves clipboard data lazily
// out of XAP_FakeClipboard, like the old GtkClipboard get_func.
//////////////////////////////////////////////////////////////////

#define ABI_TYPE_CONTENT_PROVIDER (abi_content_provider_get_type())
G_DECLARE_FINAL_TYPE(AbiContentProvider, abi_content_provider, ABI,
					 CONTENT_PROVIDER, GdkContentProvider)

struct _AbiContentProvider
{
	GdkContentProvider parent;
	XAP_UnixClipboard *owner;
	bool primary;
	GdkContentFormats *formats;
};

G_DEFINE_TYPE(AbiContentProvider, abi_content_provider, GDK_TYPE_CONTENT_PROVIDER)

static GdkContentFormats *
abi_content_provider_ref_formats(GdkContentProvider *provider)
{
	AbiContentProvider *self = ABI_CONTENT_PROVIDER(provider);
	return gdk_content_formats_ref(self->formats);
}

static void
abi_content_provider_write_mime_type_async(GdkContentProvider *provider,
										   const char *mime_type,
										   GOutputStream *stream,
										   int /*io_priority*/,
										   GCancellable *cancellable,
										   GAsyncReadyCallback callback,
										   gpointer user_data)
{
	AbiContentProvider *self = ABI_CONTENT_PROVIDER(provider);
	GTask *task = g_task_new(provider, cancellable, callback, user_data);
	g_task_set_source_tag(task, reinterpret_cast<gpointer>(abi_content_provider_write_mime_type_async));

	GError *error = nullptr;
	if (self->owner->writeData(mime_type, stream, self->primary, &error))
		g_task_return_boolean(task, TRUE);
	else
		g_task_return_error(task, error ? error :
			g_error_new(G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
						"mime type %s not available", mime_type));
	g_object_unref(task);
}

static gboolean
abi_content_provider_write_mime_type_finish(GdkContentProvider * /*provider*/,
											GAsyncResult *result,
											GError **error)
{
	return g_task_propagate_boolean(G_TASK(result), error);
}

static void
abi_content_provider_finalize(GObject *object)
{
	AbiContentProvider *self = ABI_CONTENT_PROVIDER(object);
	if (self->formats)
		gdk_content_formats_unref(self->formats);
	G_OBJECT_CLASS(abi_content_provider_parent_class)->finalize(object);
}

static void
abi_content_provider_class_init(AbiContentProviderClass *klass)
{
	GdkContentProviderClass *provider_class = GDK_CONTENT_PROVIDER_CLASS(klass);
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->finalize = abi_content_provider_finalize;
	provider_class->ref_formats = abi_content_provider_ref_formats;
	provider_class->write_mime_type_async = abi_content_provider_write_mime_type_async;
	provider_class->write_mime_type_finish = abi_content_provider_write_mime_type_finish;
}

static void
abi_content_provider_init(AbiContentProvider * /*self*/)
{
}

static GdkContentProvider *
abi_content_provider_new(XAP_UnixClipboard *owner, const char **mime_types,
						 guint n_mime_types, bool primary)
{
	AbiContentProvider *self = static_cast<AbiContentProvider *>(
		g_object_new(ABI_TYPE_CONTENT_PROVIDER, nullptr));
	self->owner = owner;
	self->primary = primary;
	self->formats = gdk_content_formats_new(mime_types, n_mime_types);
	return GDK_CONTENT_PROVIDER(self);
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

GdkClipboard * XAP_UnixClipboard::clipboardForTarget(XAP_UnixClipboard::T_AllowGet get) const
{
	if (XAP_UnixClipboard::TAG_ClipboardOnly == get)
		return m_clip;
	else if (XAP_UnixClipboard::TAG_PrimaryOnly == get)
		return m_primary;
	return nullptr;
}

static AV_View * viewFromApp(XAP_App * pApp)
{
	XAP_Frame * pFrame = pApp->getLastFocussedFrame();
	if ( !pFrame )
		return nullptr ;
	return pFrame->getCurrentView () ;
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

XAP_UnixClipboard::XAP_UnixClipboard(XAP_UnixApp * pUnixApp)
	: m_pUnixApp(pUnixApp)
{
	GdkDisplay *display = gdk_display_get_default();
	m_clip = gdk_display_get_clipboard(display);
	m_primary = gdk_display_get_primary_clipboard(display);
}

XAP_UnixClipboard::~XAP_UnixClipboard()
{
	clearData(true,true);
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

void XAP_UnixClipboard::AddFmt(const char * szFormat)
{
	UT_return_if_fail(szFormat && strlen(szFormat));
	m_vecFormat_MimeType.push_back(szFormat);
}

void XAP_UnixClipboard::deleteFmt(const char * szFormat)
{
	UT_return_if_fail(szFormat && strlen(szFormat));
	auto item = std::find(m_vecFormat_MimeType.begin(), m_vecFormat_MimeType.end(), szFormat);
	if (item != m_vecFormat_MimeType.end()) {
		m_vecFormat_MimeType.erase(item);
	}
}

void XAP_UnixClipboard::initialize()
{
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

bool XAP_UnixClipboard::writeData(const char * mime_type, GOutputStream * stream,
								  bool bPrimary, GError ** error)
{
	XAP_FakeClipboard & which_clip = ( bPrimary ? m_fakePrimaryClipboard : m_fakeClipboard );

	// if this is for PRIMARY, we need to copy the current selection
	// else this is for CLIPBOARD and the data is already copied; do nothing
	if (bPrimary)
	{
		// will only get the view from the last focussed frame, and not some offscreen
		// (print, format painter) view. this is fine, since we're operating on PRIMARY
		AV_View * pView = viewFromApp(m_pUnixApp);
		if (!pView)
			return false; // race condition - have request for data but no view. fail harmlessly
		pView->cmdCopy(false);
	}

	guchar * data = nullptr;
	UT_uint32 data_len = 0;
	guchar **pdata = &data;
	if (which_clip.getClipboardData(mime_type, reinterpret_cast<void**>(pdata), &data_len))
	{
		gsize written = 0;
		return g_output_stream_write_all(stream, data, data_len, &written,
										 nullptr, error) == TRUE;
	}
	return false;
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

bool XAP_UnixClipboard::assertSelection()
{
	GdkContentProvider *provider =
		abi_content_provider_new(this, m_vecFormat_MimeType.data(),
								 m_vecFormat_MimeType.size(), true);
	bool bOk = gdk_clipboard_set_content(clipboardForTarget(TAG_PrimaryOnly),
										 provider) == TRUE;
	g_object_unref(provider);
	return bOk;
}

bool XAP_UnixClipboard::addData(T_AllowGet tFrom, const char* format, const void* pData, UT_sint32 iNumBytes)
{
	if(tFrom == TAG_PrimaryOnly)
		return m_fakePrimaryClipboard.addData(format,pData,iNumBytes);
	else
	{
		if(!m_fakeClipboard.addData(format,pData,iNumBytes))
			return false;

		return true;
	}
}

void XAP_UnixClipboard::finishedAddingData(void)
{
	GdkContentProvider *provider =
		abi_content_provider_new(this, m_vecFormat_MimeType.data(),
								 m_vecFormat_MimeType.size(), false);
	gdk_clipboard_set_content(clipboardForTarget(TAG_ClipboardOnly), provider);
	g_object_unref(provider);
}

void XAP_UnixClipboard::clearData(bool bClipboard, bool bPrimary)
{
	if (bClipboard)
	{
		gdk_clipboard_set_content (clipboardForTarget (TAG_ClipboardOnly), nullptr);
		m_fakeClipboard.clearClipboard();
	}

	if (bPrimary)
	{
		gdk_clipboard_set_content(clipboardForTarget (TAG_PrimaryOnly), nullptr);
		m_fakePrimaryClipboard.clearClipboard();
	}
}

bool XAP_UnixClipboard::getData(T_AllowGet tFrom, const char** formatList,
								void ** ppData, UT_uint32 * pLen,
								const char **pszFormatFound)
{
	// Fetch data from the clipboard (using the allowable source(s)) in one of
	// the prioritized list of formats.  Return pointer to clipboard's buffer.
	*pszFormatFound = nullptr;
	*ppData = nullptr;
	*pLen = 0;
	if (TAG_ClipboardOnly == tFrom)
		return _getDataFromServer(tFrom,formatList,ppData,pLen,pszFormatFound);
	else if (TAG_PrimaryOnly == tFrom)
		return _getDataFromServer(tFrom,formatList,ppData,pLen,pszFormatFound);
	else
		return false;
}

struct ReadCtx
{
	GMainLoop *loop;
	GInputStream *stream;
	char *text;
	const char *mime_type;
};

static void read_stream_cb(GObject *src, GAsyncResult *res, gpointer data)
{
	ReadCtx *ctx = static_cast<ReadCtx*>(data);
	ctx->stream = gdk_clipboard_read_finish(GDK_CLIPBOARD(src), res,
											&ctx->mime_type, nullptr);
	g_main_loop_quit(ctx->loop);
}

static void read_text_cb(GObject *src, GAsyncResult *res, gpointer data)
{
	ReadCtx *ctx = static_cast<ReadCtx*>(data);
	ctx->text = gdk_clipboard_read_text_finish(GDK_CLIPBOARD(src), res, nullptr);
	g_main_loop_quit(ctx->loop);
}

bool XAP_UnixClipboard::getTextData(T_AllowGet tFrom, void ** ppData,
									UT_uint32 * pLen)
{
	// start out pessimistic
	*ppData = nullptr;
	*pLen = 0;

	GdkClipboard * clippy = clipboardForTarget (tFrom);

	ReadCtx ctx;
	ctx.loop = g_main_loop_new(nullptr, FALSE);
	ctx.stream = nullptr;
	ctx.text = nullptr;
	ctx.mime_type = nullptr;

	gdk_clipboard_read_text_async(clippy, nullptr, read_text_cb, &ctx);
	g_main_loop_run(ctx.loop);
	g_main_loop_unref(ctx.loop);

	char *txt = ctx.text;
	if (!txt)
		return false;

	size_t len = strlen (txt);
	if (!len)
	{
		g_free(txt);
		return false;
	}

	XAP_FakeClipboard & which_clip = ( tFrom == TAG_ClipboardOnly ? m_fakeClipboard : m_fakePrimaryClipboard );

	which_clip.addData("text/plain",txt,len);

	g_free (txt);

	// ignored
	const char * pszFormatFound = nullptr;

	static const char * txtFormatList [] = {
		"text/plain",
		nullptr
	};

	return _getDataFromFakeClipboard(tFrom, txtFormatList, ppData, pLen, &pszFormatFound);
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

bool XAP_UnixClipboard::_getDataFromFakeClipboard(T_AllowGet tFrom, const char** formatList,
												  void ** ppData, UT_uint32 * pLen,
												  const char **pszFormatFound)
{
	XAP_FakeClipboard & which_clip = ( tFrom == TAG_ClipboardOnly ? m_fakeClipboard : m_fakePrimaryClipboard );

	for (int k=0; (formatList[k]); k++)
		if (which_clip.getClipboardData(formatList[k],ppData,pLen))
		{
			*pszFormatFound = formatList[k];
			return true;
		}

	// should never happen since this is our internal buffer
	return false;
}

bool XAP_UnixClipboard::_getDataFromServer(T_AllowGet tFrom, const char** formatList,
										   void ** ppData, UT_uint32 * pLen,
										   const char **pszFormatFound)
{
	bool rval = false;
	if(formatList == nullptr)
		return false;

	GdkClipboard * clipboard = clipboardForTarget (tFrom);

	for(int i = 0; formatList[i] && !rval; i++)
	{
		const char * mimes[] = { formatList[i], nullptr };

		ReadCtx ctx;
		ctx.loop = g_main_loop_new(nullptr, FALSE);
		ctx.stream = nullptr;
		ctx.text = nullptr;
		ctx.mime_type = nullptr;

		UT_DEBUGMSG(("Looking for %s on clipbaord \n",formatList[i]));
		gdk_clipboard_read_async(clipboard, mimes, G_PRIORITY_DEFAULT,
								 nullptr, read_stream_cb, &ctx);
		g_main_loop_run(ctx.loop);
		g_main_loop_unref(ctx.loop);

		GInputStream *stream = ctx.stream;
		if (stream)
		{
			m_databuf.truncate(0);
			guchar buf[8192];
			gssize n;
			while ((n = g_input_stream_read(stream, buf, sizeof(buf),
											nullptr, nullptr)) > 0)
			{
				m_databuf.append(buf, n);
			}
			g_object_unref(stream);

			if (m_databuf.getLength() > 0)
			{
				*pLen = m_databuf.getLength();
				*ppData = (void *)(m_databuf.getPointer(0));
				*pszFormatFound = formatList[i];
				rval = true;
				UT_DEBUGMSG(("Found format %s on clipbaord \n",formatList[i]));
			}
		}
		/* ctx.mime_type is borrowed from the clipboard — do not free */
	}

	return rval;
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

bool XAP_UnixClipboard::canPaste(T_AllowGet tFrom) const
{
	UT_UNUSED(tFrom);
	return true;
}
