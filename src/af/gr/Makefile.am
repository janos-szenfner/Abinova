AM_CPPFLAGS = \
	$(AF_CPPFLAGS)

noinst_LTLIBRARIES = libgr.la

libgr_la_SOURCES = \
	xp/gr_Caret.cpp \
	xp/gr_Caret.h \
	xp/gr_CharWidthsCache.cpp \
	xp/gr_CharWidthsCache.h \
	xp/gr_CharWidths.cpp \
	xp/gr_CharWidths.h \
	xp/gr_DrawArgs.cpp \
	xp/gr_DrawArgs.h \
	xp/gr_EmbedManager.cpp \
	xp/gr_EmbedManager.h \
	xp/gr_Graphics.cpp \
	xp/gr_Graphics.h \
	xp/gr_Image.cpp \
	xp/gr_Image.h \
	xp/gr_Painter.cpp \
	xp/gr_Painter.h \
	xp/gr_RenderInfo.cpp \
	xp/gr_RenderInfo.h \
	xp/gr_Transform.cpp \
	xp/gr_Transform.h \
	xp/gr_VectorImage.cpp \
	xp/gr_VectorImage.h


if WITH_CAIRO
libgr_la_SOURCES += \
	xp/gr_CairoGraphics.h \
	xp/gr_CairoGraphics.cpp \
	xp/gr_CairoNullGraphics.h \
	xp/gr_CairoNullGraphics.cpp \
	xp/gr_MathTypesetter.h \
	xp/gr_MathTypesetter.cpp
endif

libgr_la_SOURCES += \
	gtk/gr_CairoPrintGraphics.cpp \
	gtk/gr_CairoPrintGraphics.h \
	gtk/gr_GtkMathManager.h \
	gtk/gr_GtkMathManager.cpp \
	gtk/gr_UnixCairoGraphics.h \
	gtk/gr_UnixCairoGraphics.cpp \
	gtk/gr_UnixImage.cpp \
	gtk/gr_UnixImage.h \
	gtk/gr_CairoImage.h \
	gtk/gr_CairoImage.cpp



