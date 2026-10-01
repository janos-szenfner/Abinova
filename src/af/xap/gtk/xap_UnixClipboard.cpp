/* AbiSource Application Framework
 * Copyright (c) 2002 Dom Lachowicz
 * Copyright (C) 2025 Hubert Figuière
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

#include <string.h>

#include "xap_UnixClipboard.h"
#include "xap_Frame.h"
#include "xav_View.h"

/* Upper bound for a single clipboard read/write.  Clipboard payloads
 * are at most a few MB (large PNGs are the biggest in practice); this
 * stops a hostile or broken clipboard peer from exhausting memory. */
#define ABI_CLIPBOARD_MAX_BYTES  (64u * 1024u * 1024u)

/* Maximum time a synchronous clipboard read may block the UI.  A peer
 * that never answers must not freeze the editor. */
#define ABI_CLIPBOARD_TIMEOUT_MS  5000

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
	XAP_UnixClipboard *owner;	/* nulled when the clipboard dies */
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
	if (self->owner &&
		self->owner->writeData(mime_type, stream, self->primary,
							   cancellable, &error))
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
	owner->_registerProvider(G_OBJECT(self));
	return GDK_CONTENT_PROVIDER(self);
}

/* The clipboard object died while this provider was still registered
 * with GdkClipboard: never serve data again. */
static void
abi_content_provider_disown(AbiContentProvider *self)
{
	self->owner = nullptr;
}

static void
abi_provider_weak_notify(gpointer data, GObject *where_the_object_was)
{
	static_cast<XAP_UnixClipboard *>(data)->_unregisterProvider(
		where_the_object_was);
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
	/* any provider still held by GdkClipboard must not touch 'this' */
	for (GObject * prov : m_vecProviders)
	{
		g_object_weak_unref(prov, abi_provider_weak_notify, this);
		abi_content_provider_disown(ABI_CONTENT_PROVIDER(prov));
	}
	m_vecProviders.clear();

	clearData(true,true);
}

void XAP_UnixClipboard::_registerProvider(GObject * provider)
{
	m_vecProviders.push_back(provider);
	g_object_weak_ref(provider, abi_provider_weak_notify, this);
}

void XAP_UnixClipboard::_unregisterProvider(GObject * provider)
{
	auto it = std::find(m_vecProviders.begin(), m_vecProviders.end(),
						provider);
	if (it != m_vecProviders.end())
		m_vecProviders.erase(it);
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

void XAP_UnixClipboard::AddFmt(const char * szFormat)
{
	UT_return_if_fail(szFormat && strlen(szFormat));
	deleteFmt(szFormat);	/* keep the list unique */
	m_vecFormat_MimeType.emplace_back(szFormat);
}

void XAP_UnixClipboard::deleteFmt(const char * szFormat)
{
	UT_return_if_fail(szFormat && strlen(szFormat));
	auto item = std::find(m_vecFormat_MimeType.begin(),
						  m_vecFormat_MimeType.end(), szFormat);
	if (item != m_vecFormat_MimeType.end()) {
		m_vecFormat_MimeType.erase(item);
	}
}

void XAP_UnixClipboard::initialize()
{
}

/* Build a NULL-terminated mime array for GdkContentProvider. */
static std::vector<const char *> s_mime_ptrs(
	const std::vector<std::string> & formats)
{
	std::vector<const char *> out;
	out.reserve(formats.size() + 1);
	for (const std::string & f : formats)
		out.push_back(f.c_str());
	out.push_back(nullptr);
	return out;
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

bool XAP_UnixClipboard::writeData(const char * mime_type, GOutputStream * stream,
								  bool bPrimary, GCancellable *cancellable,
								  GError ** error)
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

	void * data = nullptr;
	UT_uint32 data_len = 0;
	if (which_clip.getClipboardData(mime_type, &data, &data_len))
	{
		if (data_len > ABI_CLIPBOARD_MAX_BYTES)
		{
			g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
						"clipboard payload too large");
			return false;
		}
		gsize written = 0;
		return g_output_stream_write_all(stream, data, data_len, &written,
										 cancellable, error) == TRUE;
	}
	return false;
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

bool XAP_UnixClipboard::assertSelection()
{
	std::vector<const char *> mimes = s_mime_ptrs(m_vecFormat_MimeType);
	GdkContentProvider *provider =
		abi_content_provider_new(this, mimes.data(),
								 static_cast<guint>(m_vecFormat_MimeType.size()),
								 true);
	bool bOk = gdk_clipboard_set_content(clipboardForTarget(TAG_PrimaryOnly),
										 provider) == TRUE;
	g_object_unref(provider);
	return bOk;
}

bool XAP_UnixClipboard::addData(T_AllowGet tFrom, const char* format, const void* pData, UT_sint32 iNumBytes)
{
	if(!format || !pData || iNumBytes < 0)
		return false;
	if(static_cast<guint64>(iNumBytes) > ABI_CLIPBOARD_MAX_BYTES)
		return false;

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
	std::vector<const char *> mimes = s_mime_ptrs(m_vecFormat_MimeType);
	GdkContentProvider *provider =
		abi_content_provider_new(this, mimes.data(),
								 static_cast<guint>(m_vecFormat_MimeType.size()),
								 false);
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
	if (TAG_ClipboardOnly == tFrom || TAG_PrimaryOnly == tFrom)
	{
		/* when we own the selection ourselves the fake clipboard already
		 * holds every format synchronously - read it directly and skip
		 * the async server round-trip, which can deadlock on a local
		 * content provider */
		GdkClipboard * clippy = clipboardForTarget(tFrom);
		if (clippy && gdk_clipboard_is_local(clippy) &&
			_getDataFromFakeClipboard(tFrom, formatList, ppData, pLen,
									  pszFormatFound))
			return true;
		return _getDataFromServer(tFrom,formatList,ppData,pLen,pszFormatFound);
	}
	return false;
}

//////////////////////////////////////////////////////////////////
// Synchronous-with-timeout async reads.  A nested main loop is
// unavoidable (the XP callers are synchronous), but it is bounded by
// a timeout + cancellable so a dead peer cannot hang the editor.
// If the read is abandoned the ctx is freed by the late callback.
//////////////////////////////////////////////////////////////////

struct ReadCtx
{
	GMainLoop *loop;
	GCancellable *cancellable;
	guint timeout_id;
	bool done;			/* the async callback already ran */
	bool abandoned;		/* caller gave up; callback frees this ctx */
	GInputStream *stream;
	const char *mime_type;	/* borrowed from the clipboard */
	char *text;
};

static void read_ctx_cleanup(ReadCtx *ctx)
{
	g_clear_object(&ctx->stream);
	g_clear_object(&ctx->cancellable);
	g_clear_pointer(&ctx->text, g_free);
	if (ctx->loop)
		g_main_loop_unref(ctx->loop);
	g_free(ctx);
}

static void read_done(ReadCtx *ctx)
{
	if (ctx->timeout_id)
	{
		g_source_remove(ctx->timeout_id);
		ctx->timeout_id = 0;
	}
	if (ctx->abandoned)
	{
		read_ctx_cleanup(ctx);
		return;
	}
	ctx->done = true;
	g_main_loop_quit(ctx->loop);
}

static void read_stream_cb(GObject *src, GAsyncResult *res, gpointer data)
{
	ReadCtx *ctx = static_cast<ReadCtx*>(data);
	ctx->stream = gdk_clipboard_read_finish(GDK_CLIPBOARD(src), res,
											&ctx->mime_type, nullptr);
	read_done(ctx);
}

static void read_text_cb(GObject *src, GAsyncResult *res, gpointer data)
{
	ReadCtx *ctx = static_cast<ReadCtx*>(data);
	ctx->text = gdk_clipboard_read_text_finish(GDK_CLIPBOARD(src), res, nullptr);
	read_done(ctx);
}

static gboolean read_timeout_cb(gpointer data)
{
	ReadCtx *ctx = static_cast<ReadCtx*>(data);
	ctx->timeout_id = 0;
	g_cancellable_cancel(ctx->cancellable);
	if (!ctx->done)
	{
		/* abandon: the late callback frees ctx; caller returns failure */
		ctx->abandoned = true;
		g_main_loop_quit(ctx->loop);
	}
	return G_SOURCE_REMOVE;
}

/* Read up to ABI_CLIPBOARD_MAX_BYTES from a stream. */
static bool s_read_stream_into(GInputStream *stream, UT_ByteBuf & out,
							   GCancellable *cancellable)
{
	out.truncate(0);
	guchar buf[8192];
	gssize n;
	while ((n = g_input_stream_read(stream, buf, sizeof(buf),
									cancellable, nullptr)) > 0)
	{
		if (out.getLength() + static_cast<gsize>(n) > ABI_CLIPBOARD_MAX_BYTES)
		{
			out.truncate(0);
			return false;
		}
		out.append(buf, n);
	}
	return (n >= 0) && (out.getLength() > 0);
}

bool XAP_UnixClipboard::getTextData(T_AllowGet tFrom, void ** ppData,
									UT_uint32 * pLen)
{
	// start out pessimistic
	*ppData = nullptr;
	*pLen = 0;

	GdkClipboard * clippy = clipboardForTarget (tFrom);
	if (!clippy)
		return false;

	/* self-owned clipboard: read the fake clipboard directly, the async
	 * text read can deadlock against our own content provider */
	if (gdk_clipboard_is_local(clippy))
	{
		const char * pszLocal = nullptr;
		static const char * localTxtList [] = {
			"text/plain",
			nullptr
		};
		return _getDataFromFakeClipboard(tFrom, localTxtList, ppData,
										 pLen, &pszLocal);
	}

	ReadCtx *ctx = g_new0(ReadCtx, 1);
	ctx->loop = g_main_loop_new(nullptr, FALSE);
	ctx->cancellable = g_cancellable_new();
	ctx->timeout_id = g_timeout_add(ABI_CLIPBOARD_TIMEOUT_MS,
									read_timeout_cb, ctx);

	gdk_clipboard_read_text_async(clippy, ctx->cancellable,
								  read_text_cb, ctx);
	g_main_loop_run(ctx->loop);

	if (ctx->abandoned)
		return false;	/* ctx freed by the late callback */

	char *txt = ctx->text;
	ctx->text = nullptr;

	g_clear_object(&ctx->cancellable);
	g_main_loop_unref(ctx->loop);
	g_free(ctx);

	if (!txt)
		return false;

	size_t len = strlen (txt);
	if (!len || len > ABI_CLIPBOARD_MAX_BYTES)
	{
		g_free(txt);
		return false;
	}

	/* keep foreign text out of the fake clipboard (it would be served
	 * as if it were our own); hand the caller a buffer we own */
	m_databuf.truncate(0);
	m_databuf.append(reinterpret_cast<const guchar *>(txt), len);
	g_free (txt);

	*pLen = static_cast<UT_uint32>(len);
	*ppData = const_cast<UT_Byte*>(m_databuf.getPointer(0));
	return true;
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
	if (!clipboard)
		return false;

	for(int i = 0; formatList[i] && !rval; i++)
	{
		const char * mimes[] = { formatList[i], nullptr };

		UT_DEBUGMSG(("Looking for %s on clipbaord \n",formatList[i]));

		ReadCtx *ctx = g_new0(ReadCtx, 1);
		ctx->loop = g_main_loop_new(nullptr, FALSE);
		ctx->cancellable = g_cancellable_new();
		ctx->timeout_id = g_timeout_add(ABI_CLIPBOARD_TIMEOUT_MS,
										read_timeout_cb, ctx);

		gdk_clipboard_read_async(clipboard, mimes, G_PRIORITY_DEFAULT,
								 ctx->cancellable, read_stream_cb, ctx);
		g_main_loop_run(ctx->loop);

		if (ctx->abandoned)
			return false;	/* ctx freed by the late callback */

		GInputStream *stream = ctx->stream;
		ctx->stream = nullptr;
		g_clear_object(&ctx->cancellable);
		g_main_loop_unref(ctx->loop);
		g_free(ctx);

		if (stream)
		{
			if (s_read_stream_into(stream, m_databuf, nullptr))
			{
				*pLen = m_databuf.getLength();
				*ppData = (void *)(m_databuf.getPointer(0));
				*pszFormatFound = formatList[i];
				rval = true;
				UT_DEBUGMSG(("Found format %s on clipbaord \n",formatList[i]));
			}
			g_object_unref(stream);
		}
	}

	return rval;
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

bool XAP_UnixClipboard::canPaste(T_AllowGet tFrom) const
{
	GdkClipboard * clippy = clipboardForTarget (tFrom);
	if (!clippy)
		return false;

	if (gdk_clipboard_is_local(clippy))
	{
		const XAP_FakeClipboard & which_clip =
			( tFrom == TAG_ClipboardOnly ? m_fakeClipboard
			  : m_fakePrimaryClipboard );
		for (const std::string & fmt : m_vecFormat_MimeType)
			if (const_cast<XAP_FakeClipboard &>(which_clip)
				.hasFormat(fmt.c_str()))
				return true;
		return false;
	}

	GdkContentFormats *formats = gdk_clipboard_get_formats(clippy);
	if (!formats)
		return false;

	for (const std::string & fmt : m_vecFormat_MimeType)
	{
		if (gdk_content_formats_contain_mime_type(formats, fmt.c_str()))
			return true;
	}
	return false;
}
