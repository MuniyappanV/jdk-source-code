/*
 * @(#)gtk2_interface.h	1.14 10/03/23
 *
 * Copyright (c) 2006, Oracle and/or its affiliates. All rights reserved.
 * ORACLE PROPRIETARY/CONFIDENTIAL. Use is subject to license terms.
 */
#ifndef _GTK2_INTERFACE_H
#define _GTK2_INTERFACE_H

#include <stdlib.h>
#include <jni.h>

typedef enum _WidgetType
{
    BUTTON,                     /* GtkButton */
    CHECK_BOX,                  /* GtkCheckButton */
    CHECK_BOX_MENU_ITEM,        /* GtkCheckMenuItem */
    COLOR_CHOOSER,              /* GtkColorSelectionDialog */
    COMBO_BOX,                  /* GtkComboBox */
    COMBO_BOX_ARROW_BUTTON,     /* GtkComboBoxEntry */
    COMBO_BOX_TEXT_FIELD,       /* GtkComboBoxEntry */
    DESKTOP_ICON,               /* GtkLabel */
    DESKTOP_PANE,               /* GtkContainer */
    EDITOR_PANE,                /* GtkTextView */
    FORMATTED_TEXT_FIELD,       /* GtkEntry */
    HANDLE_BOX,                 /* GtkHandleBox */
    HSCROLL_BAR,                /* GtkHScrollbar */
    HSCROLL_BAR_BUTTON_LEFT,    /* GtkHScrollbar */
    HSCROLL_BAR_BUTTON_RIGHT,   /* GtkHScrollbar */
    HSCROLL_BAR_TRACK,          /* GtkHScrollbar */
    HSCROLL_BAR_THUMB,          /* GtkHScrollbar */
    HSEPARATOR,                 /* GtkHSeparator */
    HSLIDER,                    /* GtkHScale */
    HSLIDER_TRACK,              /* GtkHScale */
    HSLIDER_THUMB,              /* GtkHScale */
    HSPLIT_PANE_DIVIDER,        /* GtkHPaned */
    INTERNAL_FRAME,             /* GtkWindow */
    INTERNAL_FRAME_TITLE_PANE,  /* GtkLabel */
    IMAGE,                      /* GtkImage */
    LABEL,                      /* GtkLabel */
    LIST,                       /* GtkTreeView */
    MENU,                       /* GtkMenu */
    MENU_BAR,                   /* GtkMenuBar */
    MENU_ITEM,                  /* GtkMenuItem */
    MENU_ITEM_ACCELERATOR,      /* GtkLabel */
    OPTION_PANE,                /* GtkMessageDialog */
    PANEL,                      /* GtkContainer */
    PASSWORD_FIELD,             /* GtkEntry */
    POPUP_MENU,                 /* GtkMenu */
    POPUP_MENU_SEPARATOR,       /* GtkSeparatorMenuItem */
    PROGRESS_BAR,               /* GtkProgressBar */
    RADIO_BUTTON,               /* GtkRadioButton */
    RADIO_BUTTON_MENU_ITEM,     /* GtkRadioMenuItem */
    ROOT_PANE,                  /* GtkContainer */
    SCROLL_PANE,                /* GtkScrolledWindow */
    SPINNER,                    /* GtkSpinButton */
    SPINNER_ARROW_BUTTON,       /* GtkSpinButton */
    SPINNER_TEXT_FIELD,         /* GtkSpinButton */
    SPLIT_PANE,                 /* GtkPaned */
    TABBED_PANE,                /* GtkNotebook */
    TABBED_PANE_TAB_AREA,       /* GtkNotebook */
    TABBED_PANE_CONTENT,        /* GtkNotebook */
    TABBED_PANE_TAB,            /* GtkNotebook */
    TABLE,                      /* GtkTreeView */
    TABLE_HEADER,               /* GtkButton */
    TEXT_AREA,                  /* GtkTextView */
    TEXT_FIELD,                 /* GtkEntry */
    TEXT_PANE,                  /* GtkTextView */
    TOGGLE_BUTTON,              /* GtkToggleButton */
    TOOL_BAR,                   /* GtkToolbar */
    TOOL_BAR_DRAG_WINDOW,       /* GtkToolbar */
    TOOL_BAR_SEPARATOR,         /* GtkSeparatorToolItem */
    TOOL_TIP,                   /* GtkWindow */
    TREE,                       /* GtkTreeView */
    TREE_CELL,                  /* GtkTreeView */
    VIEWPORT,                   /* GtkViewport */
    VSCROLL_BAR,                /* GtkVScrollbar */
    VSCROLL_BAR_BUTTON_UP,      /* GtkVScrollbar */
    VSCROLL_BAR_BUTTON_DOWN,    /* GtkVScrollbar */
    VSCROLL_BAR_TRACK,          /* GtkVScrollbar */
    VSCROLL_BAR_THUMB,          /* GtkVScrollbar */
    VSEPARATOR,                 /* GtkVSeparator */
    VSLIDER,                    /* GtkVScale */
    VSLIDER_TRACK,              /* GtkVScale */
    VSLIDER_THUMB,              /* GtkVScale */
    VSPLIT_PANE_DIVIDER,        /* GtkVPaned */
    WIDGET_TYPE_SIZE
} WidgetType;

typedef enum _ColorType
{
    FOREGROUND,
    BACKGROUND,
    TEXT_FOREGROUND,
    TEXT_BACKGROUND,
    FOCUS,
    LIGHT,
    DARK,
    MID,
    BLACK,
    WHITE
} ColorType;

typedef enum _Setting
{
    GTK_FONT_NAME,
    GTK_ICON_SIZES
} Setting;

/* GTK types, here to eliminate need for GTK headers at compile time */

#ifndef FALSE
#define	FALSE           (0)
#define	TRUE            (!FALSE)
#endif

#define GTK_HAS_FOCUS   (1 << 12)
#define GTK_HAS_DEFAULT (1 << 14)


/* basic types */
typedef char    gchar;
typedef short   gshort;
typedef int     gint;
typedef long    glong;
typedef float   gfloat;
typedef double  gdouble;
typedef void*   gpointer;
typedef gint    gboolean;

typedef signed char  gint8;
typedef signed short gint16;
typedef signed int   gint32;

typedef unsigned char  guchar;
typedef unsigned char  guint8;
typedef unsigned short gushort;
typedef unsigned short guint16;
typedef unsigned int   guint;
typedef unsigned int   guint32;
typedef unsigned int   gsize;
typedef unsigned long  gulong;

typedef signed long long   gint64;
typedef unsigned long long guint64;

/* enumerated constants */
typedef enum
{
  GTK_ARROW_UP,
  GTK_ARROW_DOWN,
  GTK_ARROW_LEFT,
  GTK_ARROW_RIGHT
} GtkArrowType;

typedef enum {
  GDK_COLORSPACE_RGB
} GdkColorspace;

typedef enum
{
  GTK_EXPANDER_COLLAPSED,
  GTK_EXPANDER_SEMI_COLLAPSED,
  GTK_EXPANDER_SEMI_EXPANDED,
  GTK_EXPANDER_EXPANDED
} GtkExpanderStyle;

typedef enum
{
  GTK_ICON_SIZE_INVALID,
  GTK_ICON_SIZE_MENU,
  GTK_ICON_SIZE_SMALL_TOOLBAR,
  GTK_ICON_SIZE_LARGE_TOOLBAR,
  GTK_ICON_SIZE_BUTTON,
  GTK_ICON_SIZE_DND,
  GTK_ICON_SIZE_DIALOG
} GtkIconSize;

typedef enum
{
  GTK_ORIENTATION_HORIZONTAL,
  GTK_ORIENTATION_VERTICAL
} GtkOrientation;

typedef enum
{
  GTK_POS_LEFT,
  GTK_POS_RIGHT,
  GTK_POS_TOP,
  GTK_POS_BOTTOM
} GtkPositionType;

typedef enum
{
  GTK_SHADOW_NONE,
  GTK_SHADOW_IN,
  GTK_SHADOW_OUT,
  GTK_SHADOW_ETCHED_IN,
  GTK_SHADOW_ETCHED_OUT
} GtkShadowType;

typedef enum
{
  GTK_STATE_NORMAL,
  GTK_STATE_ACTIVE,
  GTK_STATE_PRELIGHT,
  GTK_STATE_SELECTED,
  GTK_STATE_INSENSITIVE
} GtkStateType;

typedef enum
{
  GTK_TEXT_DIR_NONE,
  GTK_TEXT_DIR_LTR,
  GTK_TEXT_DIR_RTL
} GtkTextDirection;

typedef enum
{
  GTK_WINDOW_TOPLEVEL,
  GTK_WINDOW_POPUP
} GtkWindowType;

typedef enum
{
  G_PARAM_READABLE            = 1 << 0,
  G_PARAM_WRITABLE            = 1 << 1,
  G_PARAM_CONSTRUCT	      = 1 << 2,
  G_PARAM_CONSTRUCT_ONLY      = 1 << 3,
  G_PARAM_LAX_VALIDATION      = 1 << 4,
  G_PARAM_PRIVATE	      = 1 << 5
} GParamFlags;

/* We define all structure pointers to be void* */
typedef void GError;
typedef void GMainContext;
typedef void GSList;

typedef void GdkColormap;
typedef void GdkDrawable;
typedef void GdkGC;
typedef void GdkPixbuf;
typedef void GdkPixmap;
typedef void GdkWindow;

typedef void GtkAdjustment;
typedef void GtkFixed;
typedef void GtkMenuItem;
typedef void GtkMenuShell;
typedef void GtkWidgetClass;
typedef void PangoFontDescription;
typedef void GtkSettings;

/* Some real structures */
typedef struct
{
  guint32 pixel;
  guint16 red;
  guint16 green;
  guint16 blue;
} GdkColor;

typedef struct {
  gint      fd;
  gushort   events;
  gushort   revents;
} GPollFD;

typedef struct {
  gint x;
  gint y;
  gint width;
  gint height;
} GdkRectangle;

typedef struct {
  gint x;
  gint y;
  gint width;
  gint height;
} GtkAllocation;

typedef struct {
  gint width;
  gint height;
} GtkRequisition;

typedef struct {
  GtkWidgetClass *g_class;
} GTypeInstance;

typedef struct {
  gint left;
  gint right;
  gint top;
  gint bottom;
} GtkBorder;

/******************************************************
 * FIXME: it is more safe to include gtk headers for
 * the precise type definition of GType and other
 * structures. This is a place where getting rid of gtk
 * headers may be dangerous.
 ******************************************************/
typedef gulong         GType;

typedef struct
{
  GType		g_type;

  union {
    gint	v_int;
    guint	v_uint;
    glong	v_long;
    gulong	v_ulong;
    gint64      v_int64;
    guint64     v_uint64;
    gfloat	v_float;
    gdouble	v_double;
    gpointer	v_pointer;
  } data[2];
} GValue;

typedef struct 
{
  GTypeInstance  g_type_instance;

  gchar         *name;
  GParamFlags    flags;
  GType		 value_type;
  GType		 owner_type;
} GParamSpec;

typedef struct {
  GTypeInstance g_type_instance;
  guint         ref_count;
  void         *qdata;
} GObject;

typedef struct {
  GObject parent_instance;
  guint32 flags;
} GtkObject;

typedef struct
{
  GObject parent_instance;

  GdkColor fg[5];
  GdkColor bg[5];
  GdkColor light[5];
  GdkColor dark[5];
  GdkColor mid[5];
  GdkColor text[5];
  GdkColor base[5];
  GdkColor text_aa[5];		/* Halfway between text/base */
  
  GdkColor black;
  GdkColor white;
  PangoFontDescription *font_desc;
  
  gint xthickness;
  gint ythickness;
  
  GdkGC *fg_gc[5];
  GdkGC *bg_gc[5];
  GdkGC *light_gc[5];
  GdkGC *dark_gc[5];
  GdkGC *mid_gc[5];
  GdkGC *text_gc[5];
  GdkGC *base_gc[5];
  GdkGC *text_aa_gc[5];
  GdkGC *black_gc;
  GdkGC *white_gc;
  
  GdkPixmap *bg_pixmap[5];
} GtkStyle;

typedef struct _GtkWidget GtkWidget;
struct _GtkWidget
{
  GtkObject object;
  guint16 private_flags;
  guint8 state;
  guint8 saved_state;
  gchar *name;
  GtkStyle *style;
  GtkRequisition requisition;
  GtkAllocation allocation;
  GdkWindow *window;
  GtkWidget *parent;
};

typedef struct
{
  GtkWidget widget;
  
  gfloat xalign;
  gfloat yalign;
  
  guint16 xpad;
  guint16 ypad;
} GtkMisc;

typedef struct {
  GtkWidget widget;
  GtkWidget *focus_child;
  guint border_width : 16;
  guint need_resize : 1;
  guint resize_mode : 2;
  guint reallocate_redraws : 1;
  guint has_focus_chain : 1;
} GtkContainer;

typedef struct {
  GtkContainer container;
  GtkWidget *child;
} GtkBin;

typedef struct {
  GtkBin bin;
  GdkWindow *event_window;
  gchar *label_text;
  guint activate_timeout;
  guint constructed : 1;
  guint in_button : 1;
  guint button_down : 1;
  guint relief : 2;
  guint use_underline : 1;
  guint use_stock : 1;
  guint depressed : 1;
  guint depress_on_activate : 1;
  guint focus_on_click : 1;
} GtkButton;

typedef struct {
  GtkButton button;
  guint active : 1;
  guint draw_indicator : 1;
  guint inconsistent : 1;
} GtkToggleButton;

/*
 * Converts java.lang.String object to UTF-8 character string.
 */
const char *getStrFor(JNIEnv *env, jstring value);

/*
 * Load the gtk2 library.  If the library is already loaded this method has no
 * effect and returns success.
 * Returns 0 on failure and 1 on success.
 */
int gtk2_load();

/*
 * Unload the gtk2 library.  If the library is already unloaded this method has
 * no effect and returns success.
 * Returns 0 on failure and 1 on success.
 */
int gtk2_unload();

void gtk2_paint_arrow(WidgetType widget_type, GtkStateType state_type,
        GtkShadowType shadow_type, const gchar *detail,
        gint x, gint y, gint width, gint height, 
        GtkArrowType arrow_type, gboolean fill);
void gtk2_paint_box(WidgetType widget_type, GtkStateType state_type,
        GtkShadowType shadow_type, const gchar *detail,
        gint x, gint y, gint width, gint height, 
        gint synth_state, GtkTextDirection dir);
void gtk2_paint_box_gap(WidgetType widget_type, GtkStateType state_type,
        GtkShadowType shadow_type, const gchar *detail,
        gint x, gint y, gint width, gint height, 
        GtkPositionType gap_side, gint gap_x, gint gap_width);
void gtk2_paint_check(WidgetType widget_type, gint synth_state,
        const gchar *detail, gint x, gint y, gint width, gint height);
void gtk2_paint_diamond(WidgetType widget_type, GtkStateType state_type,
        GtkShadowType shadow_type, const gchar *detail,
        gint x, gint y, gint width, gint height);
void gtk2_paint_expander(WidgetType widget_type, GtkStateType state_type,
        const gchar *detail, gint x, gint y, gint width, gint height,
        GtkExpanderStyle expander_style);
void gtk2_paint_extension(WidgetType widget_type, GtkStateType state_type,
        GtkShadowType shadow_type, const gchar *detail,
        gint x, gint y, gint width, gint height, GtkPositionType gap_side);
void gtk2_paint_flat_box(WidgetType widget_type, GtkStateType state_type,
        GtkShadowType shadow_type, const gchar *detail,
        gint x, gint y, gint width, gint height, gboolean has_focus);
void gtk2_paint_focus(WidgetType widget_type, GtkStateType state_type,
        const char *detail, gint x, gint y, gint width, gint height);
void gtk2_paint_handle(WidgetType widget_type, GtkStateType state_type,
        GtkShadowType shadow_type, const gchar *detail,
        gint x, gint y, gint width, gint height, GtkOrientation orientation);
void gtk2_paint_hline(WidgetType widget_type, GtkStateType state_type,
        const gchar *detail, gint x, gint y, gint width, gint height);
void gtk2_paint_option(WidgetType widget_type, gint synth_state,
        const gchar *detail, gint x, gint y, gint width, gint height);
void gtk2_paint_shadow(WidgetType widget_type, GtkStateType state_type,
        GtkShadowType shadow_type, const gchar *detail,
        gint x, gint y, gint width, gint height,
        gint synth_state, GtkTextDirection dir);
void gtk2_paint_slider(WidgetType widget_type, GtkStateType state_type,
        GtkShadowType shadow_type, const gchar *detail,
        gint x, gint y, gint width, gint height, GtkOrientation orientation);
void gtk2_paint_vline(WidgetType widget_type, GtkStateType state_type,
        const gchar *detail, gint x, gint y, gint width, gint height);
void gtk_paint_background(WidgetType widget_type, GtkStateType state_type,
        gint x, gint y, gint width, gint height);

void gtk2_init_painting(JNIEnv *env, gint w, gint h);
gint gtk2_copy_image(gint *dest, gint width, gint height);

gint gtk2_get_xthickness(JNIEnv *env, WidgetType widget_type);
gint gtk2_get_ythickness(JNIEnv *env, WidgetType widget_type);
gint gtk2_get_color_for_state(JNIEnv *env, WidgetType widget_type,
                              GtkStateType state_type, ColorType color_type);
jobject gtk2_get_class_value(JNIEnv *env, WidgetType widget_type, jstring key);

GdkPixbuf *gtk2_get_stock_icon(gint widget_type, const gchar *stock_id,
        GtkIconSize size, GtkTextDirection direction, const char *detail);
GdkPixbuf *gtk2_get_icon(const gchar *filename, gint size);
jstring gtk2_get_pango_font_name(JNIEnv *env, WidgetType widget_type);

void flush_gtk_event_loop();

jobject gtk2_get_image_dimension(JNIEnv *env, WidgetType widget_type,
                                 GtkStateType state_type);
void    gtk2_get_image(gint *dst, gint width, gint height,
                       WidgetType widget_type, GtkStateType state_type);

jobject gtk2_get_setting(JNIEnv *env, Setting property);

void (*fp_g_object_unref)(gpointer object);
int (*fp_gdk_pixbuf_get_bits_per_sample)(const GdkPixbuf *pixbuf);
guchar *(*fp_gdk_pixbuf_get_pixels)(const GdkPixbuf *pixbuf);
gboolean (*fp_gdk_pixbuf_get_has_alpha)(const GdkPixbuf *pixbuf);
int (*fp_gdk_pixbuf_get_height)(const GdkPixbuf *pixbuf);
int (*fp_gdk_pixbuf_get_n_channels)(const GdkPixbuf *pixbuf);
int (*fp_gdk_pixbuf_get_rowstride)(const GdkPixbuf *pixbuf);
int (*fp_gdk_pixbuf_get_width)(const GdkPixbuf *pixbuf);
GdkPixbuf *(*fp_gdk_pixbuf_new_from_file)(const char *filename, GError **error); 

#endif /* !_GTK2_INTERFACE_H */
