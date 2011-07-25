/**********************************************************************
  iccbutton.c
  Copyright(C) 2007-2010 Y.Yamakawa
**********************************************************************/

#include "iccbutton.h"
#include "iccclassicons.h"
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#if GLIB_MAJOR_VERSION > 2 || (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 16)
#define HAVE_G_CHECKSUM
#endif

#ifdef G_OS_WIN32
#define WINVER 0x0500
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <icm.h>
#endif

static cmsHPROFILE checkProfile   (const gchar    *path,
                                   guint16         class,
                                   guint16         pcs,
                                   guint16         colorSpace);

static void icc_button_class_init (IccButtonClass *klass);
static void icc_button_init       (IccButton      *button);
static void icc_button_finalize   (IccButton      *button);
static void icc_button_clicked    (IccButton      *button,
                                   gpointer        data);
static void icc_button_run_dialog (GtkWidget      *widget,
                                   gpointer        data);
static void setupMenu             (IccButton      *button);


static gint nInstances = 0;


////////// Signals

enum {
  CHANGED,
  LAST_SIGNAL
};

static guint iccButtonSignals[LAST_SIGNAL] = { 0 };

////////// Profile information

typedef struct _profileData {
  gchar *name;
  gchar *path;
  gchar *digest;
  icProfileClassSignature class;
  gchar *colorspace;
  gchar *pcs;
  gboolean is_history;
} profileData;

static GArray *profileDataArray = NULL;

static glong last_changed = 0;

static gboolean
profile_data_new_from_file (const gchar  *path,
                            profileData **data,
                            guint16       class,
                            guint16       pcs,
                            guint16       colorspace)
{
  cmsHPROFILE profile;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  if ((profile = checkProfile (path, class, pcs, colorspace)))
    {
      profileData *_data;
      guint32 sig;

      _data = g_new (profileData, 1);

      _data->path       = g_strdup (path);
      _data->name       = lcms_get_profile_desc (profile);
      _data->class      = cmsGetDeviceClass (profile);
      _data->colorspace = g_strndup ((sig = GUINT32_TO_BE (cmsGetColorSpace (profile)), (gchar *)&sig), 4);
      _data->pcs        = g_strndup ((sig = GUINT32_TO_BE (cmsGetPCS (profile)), (gchar *)&sig), 4);

      cmsCloseProfile (profile);

#ifdef HAVE_G_CHECKSUM
      {
        gchar *buf;
        gsize len;

        if (g_file_get_contents (path, &buf, &len, NULL))
          {
            _data->digest = g_compute_checksum_for_data (G_CHECKSUM_MD5, buf, len);
            g_free (buf);
          }
      }
#else
      _data->digest = NULL;
#endif

      *data = _data;

      return TRUE;
    }
  else
    {
      *data = NULL;
      return FALSE;
    }
}

static void
profile_data_destroy (profileData *data)
{
  g_return_if_fail (data != NULL);

  g_free (data->path);
  g_free (data->name);
  g_free (data->digest);
  g_free (data->colorspace);
  g_free (data->pcs);

  g_free (data);
}

////////// Display names of the profile classes

typedef struct _profileClassEntry {
  icProfileClassSignature sig;
  gchar *displayName;
} profileClassEntry;

#define N_PROFILE_CLASSES 7

profileClassEntry *profileClassTable = NULL;

const icProfileClassSignature profileClassSigEntry[N_PROFILE_CLASSES] =
{
  icSigInputClass,
  icSigDisplayClass,
  icSigOutputClass,
  icSigLinkClass,
  icSigAbstractClass,
  icSigColorSpaceClass,
  icSigNamedColorClass
};

const gchar profileClassDisplayNameEntry[N_PROFILE_CLASSES][32] =
{
  N_("Input"),
  N_("Display"),
  N_("Output"),
  N_("Devicelink"),
  N_("Abstract"),
  N_("Colorspace conversion"),
  N_("Named color")
};

static const gchar *
getProfileClassDisplayName (icProfileClassSignature sig)
{
  gint i;

  if (profileClassTable == NULL)
    return "";

  for (i = 0; i < N_PROFILE_CLASSES; i++)
    {
      if (profileClassTable[i].sig == sig)
        return profileClassTable[i].displayName;
    }

  return "";
}

static const gchar *
get_stock_id_from_profile_data (profileData *data)
{
  switch (data->class)
    {
    case icSigInputClass:
      return ICC_BUTTON_STOCK_INPUT_CLASS;
    case icSigDisplayClass:
      return ICC_BUTTON_STOCK_DISPLAY_CLASS;
    case icSigOutputClass:
      {
        guint32 colorspace = *((icColorSpaceSignature *)data->colorspace);

        colorspace = GUINT32_FROM_BE (colorspace);

        switch (colorspace)
          {
          case icSigCmykData:
          case icSigCmyData:
            return ICC_BUTTON_STOCK_CMYK_OUTPUT_CLASS;
          case icSigRgbData:
          default:
            return ICC_BUTTON_STOCK_RGB_OUTPUT_CLASS;
          }
      }
    case icSigLinkClass:
      return ICC_BUTTON_STOCK_LINK_CLASS;
    case icSigAbstractClass:
      return ICC_BUTTON_STOCK_ABSTRACT_CLASS;
    case icSigColorSpaceClass:
      return ICC_BUTTON_STOCK_COLORSPACE_CLASS;
    case icSigNamedColorClass:
      return ICC_BUTTON_STOCK_NAMEDCOLOR_CLASS;
    default:
      return GTK_STOCK_FILE;
    }
}

/***************************
  Drag & drop
****************************/

static GtkTargetEntry targets[] = {
  { "text/uri-list", 0, 0 }
};

static void
icc_button_drag_data_received (GtkWidget        *widget,
                               GdkDragContext   *context,
                               gint              x,
                               gint              y,
                               GtkSelectionData *selection_data,
                               guint             info,
                               guint             time,
                               gpointer          data)
{
  gchar **uris;
  gboolean success = FALSE;

  uris = g_uri_list_extract_uris (selection_data->data);

  /*iccbutton only accepts the single file */
  if (uris[0] && !uris[1])
    {
      gchar *path = g_filename_from_uri (uris[0], NULL, NULL);
      if (IS_ICC_BUTTON (widget))
        {
          if ((success = icc_button_set_filename (ICC_BUTTON (widget), path, TRUE)))
            g_signal_emit (ICC_BUTTON (widget), iccButtonSignals[CHANGED], 0);
        }
      else if (GTK_IS_ENTRY (widget))
        {
          gtk_entry_set_text (GTK_ENTRY (widget), path);
          success = TRUE;
        }
      g_free (path);
    }

  g_strfreev (uris);

  gtk_drag_finish (context, success, FALSE, time);
}


/* search the profileData by filename, and remove it */
static void
profile_data_remove_filename (const gchar *filename)
{
  gint i;
  profileData *data;

  g_return_if_fail (filename != NULL);
  g_return_if_fail (profileDataArray != NULL);

  for (i = 0; i < profileDataArray->len; i++)
    {
      data = g_array_index (profileDataArray, profileData *, i);

      if (strcmp (filename, data->path) == 0)
        {
          profileDataArray = g_array_remove_index (profileDataArray, i);
          profile_data_destroy (data);
          i--;
        }
    }
}

/* check the device class */
static inline gboolean
checkProfileClass (guint16                  class,
                   guint16                  pcs,
                   guint16                  colorSpace,
                   icProfileClassSignature  classSig,
                   icColorSpaceSignature    pcsSig,
                   icColorSpaceSignature    colorSpaceSig)
{
  return ( ( ( class & ICC_BUTTON_CLASS_INPUT && classSig == icSigInputClass ) ||
             ( class & ICC_BUTTON_CLASS_OUTPUT && classSig == icSigOutputClass ) ||
             ( class & ICC_BUTTON_CLASS_DISPLAY && classSig == icSigDisplayClass ) ||
             ( class & ICC_BUTTON_CLASS_LINK && classSig == icSigLinkClass ) ||
             ( class & ICC_BUTTON_CLASS_ABSTRACT && classSig == icSigAbstractClass ) ) &&
           ( ( pcs & ICC_BUTTON_COLORSPACE_GRAY && pcsSig == icSigGrayData ) ||
             ( pcs & ICC_BUTTON_COLORSPACE_RGB && pcsSig == icSigRgbData ) ||
             ( pcs & ICC_BUTTON_COLORSPACE_CMY && pcsSig == icSigCmyData ) ||
             ( pcs & ICC_BUTTON_COLORSPACE_CMYK && pcsSig == icSigCmykData ) ||
             ( pcs & ICC_BUTTON_COLORSPACE_XYZ && pcsSig == icSigXYZData ) ||
             ( pcs & ICC_BUTTON_COLORSPACE_LAB && pcsSig == icSigLabData ) ) &&
           ( ( colorSpace & ICC_BUTTON_COLORSPACE_GRAY && colorSpaceSig == icSigGrayData ) ||
             ( colorSpace & ICC_BUTTON_COLORSPACE_RGB && colorSpaceSig == icSigRgbData ) ||
             ( colorSpace & ICC_BUTTON_COLORSPACE_CMY && colorSpaceSig == icSigCmyData ) ||
             ( colorSpace & ICC_BUTTON_COLORSPACE_CMYK && colorSpaceSig == icSigCmykData ) ||
             ( colorSpace & ICC_BUTTON_COLORSPACE_XYZ && colorSpaceSig == icSigXYZData ) ||
             ( colorSpace & ICC_BUTTON_COLORSPACE_LAB && colorSpaceSig == icSigLabData ) ) ? TRUE : FALSE );
}

static cmsHPROFILE
checkProfile (const gchar *path,
              guint16      class,
              guint16      pcs,
              guint16      colorSpace)
{
  cmsHPROFILE profile;

  if ((profile = lcms_open_profile ((gchar *)path)) != NULL)
    {
      if (checkProfileClass (class, pcs, colorSpace,
                             cmsGetDeviceClass (profile),
                             cmsGetPCS (profile),
                             cmsGetColorSpace (profile)))
        return profile;
      else
        cmsCloseProfile (profile);
    }

  return NULL;
}


#define SEARCH_PROFILE_MAXLEVEL 3

static void
_searchProfile (gchar *searchPath)
{
  static gint level = 0;
  GDir *dir;

  if ((dir = g_dir_open (searchPath, 0, NULL)) != NULL)
    {
      gchar *path;

      level++;

      while ((path = (gchar *)g_dir_read_name (dir)) != NULL)
        {
          profileData *data;
          gchar *tmp;

          tmp = g_build_filename (searchPath, path, NULL);

          if (g_file_test (tmp, G_FILE_TEST_IS_DIR) && level < SEARCH_PROFILE_MAXLEVEL)
            _searchProfile (tmp);
          else if (g_str_has_suffix (path, ".icc") || g_str_has_suffix (path, ".icm"))
            {
              gint i;

#ifdef HAVE_G_CHECKSUM
              if (profile_data_new_from_file (tmp, &data, ICC_BUTTON_CLASS_ALL, ICC_BUTTON_COLORSPACE_ALL, ICC_BUTTON_COLORSPACE_ALL))
                {
                  /* If the profile was found in the history, skip the registration */
                  for (i = 0; i < profileDataArray->len; i++)
                    if (strcmp (data->digest, g_array_index (profileDataArray, profileData *, i)->digest) == 0)
                      break;

                  if (i >= profileDataArray->len)
                    {
                      data->is_history = FALSE;
                      g_array_append_val (profileDataArray, data);
                    }
                  else
                    profile_data_destroy (data);
                }
#else
              /* If the profile was found in the history, skip the registration */
              for (i = 0; i < profileDataArray->len; i++)
                if (strcmp (tmp, g_array_index (profileDataArray, profileData *, i)->path) == 0)
                  break;

              if (i >= profileDataArray->len &&
                  profile_data_new_from_file (tmp, &data, ICC_BUTTON_CLASS_ALL, ICC_BUTTON_COLORSPACE_ALL, ICC_BUTTON_COLORSPACE_ALL))
                {
                  data->is_history = FALSE;
                  g_array_append_val (profileDataArray, data);
                }
#endif
            }
          g_free (tmp);
        }

      level--;
      g_dir_close (dir);
    }
}

static void
searchProfile (void)
{
  gint i, count;
  profileData *data;

  if (profileDataArray)
    {
      /* remove the non-history data */
      for (i = 0; i < profileDataArray->len; i++)
        {
          data = g_array_index (profileDataArray, profileData *, i);

          if (!data->is_history || !g_file_test (data->path, G_FILE_TEST_EXISTS))
            {
              profile_data_destroy (data);
              g_array_remove_index (profileDataArray, i);
              i--;
            }
        }
    }
  else
    {
      /* read the history file */
      gchar *path;
      GKeyFile *key_file;

      profileDataArray = g_array_new (FALSE, TRUE, sizeof (profileData *));

      if (!(path = (gchar *)g_getenv ("HOME")))
        path = (gchar *)g_get_home_dir ();
      path = g_build_filename (path, ".iccbutton_history", NULL);

      key_file = g_key_file_new ();

      if (g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, NULL))
        {
          gchar **group_names = g_key_file_get_groups (key_file, NULL);
          profileData *_data;

          for (i = 0; group_names[i] != NULL; i++)
            {
              gchar *tmp;

              tmp = g_filename_from_uri (group_names[i], NULL, NULL);

              if (!tmp)
                continue;

              if (profile_data_new_from_file (tmp, &_data, ICC_BUTTON_CLASS_ALL, ICC_BUTTON_COLORSPACE_ALL, ICC_BUTTON_COLORSPACE_ALL))
                {
                  _data->is_history = TRUE;
                  profileDataArray = g_array_append_val (profileDataArray, _data);
                }

              g_free (tmp);
            }

          g_strfreev (group_names);
        }

      g_key_file_free (key_file);
      g_free (path);
    }

#if defined G_OS_WIN32
  gchar *searchPath[5];
  DWORD DirNameSize;
  searchPath[3] = g_build_filename (g_getenv ("COMMONPROGRAMFILES"), "\\adobe\\color\\profiles", NULL);
  GetColorDirectory (NULL, NULL, &DirNameSize);
  searchPath[4] = g_try_malloc0 (DirNameSize);
  GetColorDirectory (NULL, searchPath[4], &DirNameSize);
#elif defined __APPLE__
  gchar *searchPath[6];
  searchPath[3] = g_build_filename (g_get_home_dir (), "Library/ColorSync/Profiles", NULL);
  searchPath[4] = g_strdup ("/Library/Application Support/Adobe/Color/Profiles");
  searchPath[5] = g_strdup ("/Library/ColorSync/Profiles");
#else
  gchar *searchPath[3];
#endif
  searchPath[0] = g_build_filename (g_get_home_dir (), ".color/icc", NULL);
  searchPath[1] = g_strdup ("/usr/color/icc");
  searchPath[2] = g_strdup ("/usr/share/color/icc");

  count = sizeof (searchPath) / sizeof (searchPath[0]);

  for (i = 0; i < count; i++)
    {
      _searchProfile (searchPath[i]);
      g_free (searchPath[i]);
    }
}


GType
icc_button_get_type (void)
{
  static GType iccButtonType = 0;

  if (!iccButtonType)
    {
      static const GTypeInfo iccButtonInfo = {
        sizeof (IccButtonClass),
        NULL, // base_init
        NULL, // base_finalize
        (GClassInitFunc)icc_button_class_init,
        NULL, // class_finalize
        NULL, // class_data
        sizeof (IccButton),
        0, // n_preallocs
        (GInstanceInitFunc)icc_button_init };

      iccButtonType = g_type_register_static (GTK_TYPE_BUTTON, "IccButton", &iccButtonInfo, 0);
  }

  return iccButtonType;
}


// Init the class of IccButton

static void
icc_button_class_init (IccButtonClass *klass)
{
  /*klass->dispose = icc_button_dispose;*/
  G_OBJECT_CLASS (klass)->finalize = (GObjectFinalizeFunc)icc_button_finalize;

  iccButtonSignals[CHANGED] = g_signal_new ("changed",
                                            G_TYPE_FROM_CLASS (klass),
                                            G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                            G_STRUCT_OFFSET (IccButtonClass, changed),
                                            NULL,
                                            NULL,
                                            g_cclosure_marshal_VOID__VOID,
                                            G_TYPE_NONE, 0);

  if (!profileClassTable)
    {
      gint i;
      profileClassTable = g_new (profileClassEntry, N_PROFILE_CLASSES);
      GtkIconFactory *factory;
      GtkIconSet *icon;
      GdkPixbuf *pixbuf;

      for (i = 0; i < N_PROFILE_CLASSES; i++)
        {
          profileClassTable[i].sig = profileClassSigEntry[i];
          profileClassTable[i].displayName = _(profileClassDisplayNameEntry[i]);
        }

      factory = gtk_icon_factory_new ();

      pixbuf = gdk_pixbuf_new_from_inline (-1, icc_input_class_icon, FALSE, NULL);
      icon = gtk_icon_set_new_from_pixbuf (pixbuf);
      gtk_icon_factory_add (factory, ICC_BUTTON_STOCK_INPUT_CLASS, icon);
      g_object_unref (pixbuf);
      gtk_icon_set_unref (icon);

      pixbuf = gdk_pixbuf_new_from_inline (-1, icc_display_class_icon, FALSE, NULL);
      icon = gtk_icon_set_new_from_pixbuf (pixbuf);
      gtk_icon_factory_add (factory, ICC_BUTTON_STOCK_DISPLAY_CLASS, icon);
      g_object_unref (pixbuf);
      gtk_icon_set_unref (icon);

      pixbuf = gdk_pixbuf_new_from_inline (-1, icc_rgb_output_class_icon, FALSE, NULL);
      icon = gtk_icon_set_new_from_pixbuf (pixbuf);
      gtk_icon_factory_add (factory, ICC_BUTTON_STOCK_RGB_OUTPUT_CLASS, icon);
      g_object_unref (pixbuf);
      gtk_icon_set_unref (icon);

      pixbuf = gdk_pixbuf_new_from_inline (-1, icc_cmyk_output_class_icon, FALSE, NULL);
      icon = gtk_icon_set_new_from_pixbuf (pixbuf);
      gtk_icon_factory_add (factory, ICC_BUTTON_STOCK_CMYK_OUTPUT_CLASS, icon);
      g_object_unref (pixbuf);
      gtk_icon_set_unref (icon);

      pixbuf = gdk_pixbuf_new_from_inline (-1, icc_link_class_icon, FALSE, NULL);
      icon = gtk_icon_set_new_from_pixbuf (pixbuf);
      gtk_icon_factory_add (factory, ICC_BUTTON_STOCK_LINK_CLASS, icon);
      g_object_unref (pixbuf);
      gtk_icon_set_unref (icon);

      pixbuf = gdk_pixbuf_new_from_inline (-1, icc_abstract_class_icon, FALSE, NULL);
      icon = gtk_icon_set_new_from_pixbuf (pixbuf);
      gtk_icon_factory_add (factory, ICC_BUTTON_STOCK_ABSTRACT_CLASS, icon);
      g_object_unref (pixbuf);
      gtk_icon_set_unref (icon);

      pixbuf = gdk_pixbuf_new_from_inline (-1, icc_link_class_icon, FALSE, NULL); // replace the icon with correct icon later
      icon = gtk_icon_set_new_from_pixbuf (pixbuf);
      gtk_icon_factory_add (factory, ICC_BUTTON_STOCK_COLORSPACE_CLASS, icon);
      g_object_unref (pixbuf);
      gtk_icon_set_unref (icon);

      pixbuf = gdk_pixbuf_new_from_inline (-1, icc_link_class_icon, FALSE, NULL); // replace the icon with correct icon later
      icon = gtk_icon_set_new_from_pixbuf (pixbuf);
      gtk_icon_factory_add (factory, ICC_BUTTON_STOCK_NAMEDCOLOR_CLASS, icon);
      g_object_unref (pixbuf);
      gtk_icon_set_unref (icon);

      gtk_icon_factory_add_default (factory);
    }
}


// Init an instance of IccButton

static void
icc_button_init (IccButton *button)
{
  GtkWidget *separator, *arrow;

  button->dialog = NULL;
  button->title = NULL;
  button->path = NULL;
  button->classMask = ICC_BUTTON_CLASS_ALL;
  button->pcsMask = ICC_BUTTON_COLORSPACE_ALL;
  button->colorspaceMask = ICC_BUTTON_COLORSPACE_ALL;
  button->enable_empty = TRUE;
  button->dialog_show_detail = TRUE;
  button->dialog_list_columns = ICC_BUTTON_COLUMN_ALL;

  button->popupMenu = NULL;
  button->menuItems = NULL;
  button->nEntries = 0;
  button->maxEntries = 15;
  button->last_updated = 0;

  button->hbox = gtk_hbox_new (FALSE, 0);
  button->icon = gtk_image_new ();
  button->label = gtk_label_new (_("(not selected)"));
  separator = gtk_vseparator_new ();
  arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
  gtk_label_set_ellipsize (GTK_LABEL (button->label), PANGO_ELLIPSIZE_END);
  gtk_misc_set_alignment (GTK_MISC (button->label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (button->hbox), button->icon, FALSE, FALSE, 4);
  gtk_box_pack_start (GTK_BOX (button->hbox), button->label, TRUE, TRUE, 4);
  gtk_box_pack_start (GTK_BOX (button->hbox), separator, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (button->hbox), arrow, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (button), button->hbox);
  gtk_widget_show_all (button->hbox);

  gtk_button_set_alignment (GTK_BUTTON (button), 0, 0.5);

  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (icc_button_clicked), NULL);

  gtk_drag_dest_set (GTK_WIDGET (button), GTK_DEST_DEFAULT_ALL,
                     targets, 1, GDK_ACTION_COPY);
  g_signal_connect (G_OBJECT (button), "drag_data_received",
                    G_CALLBACK (icc_button_drag_data_received), NULL);

  if (!profileDataArray)
    searchProfile ();

  nInstances++;
};

static void
icc_button_finalize (IccButton *button)
{
  if (button->popupMenu)
    gtk_widget_destroy (button->popupMenu);
  if (button->path)
    g_free (button->path);
  if (button->title)
    g_free (button->title);

  if (--nInstances <= 0)
    {
      /* store the history */
      GKeyFile *key_file;
      gchar *path;
      guchar *buf;
      gssize len;
      gint i, count;

      key_file = g_key_file_new ();

      for (i = count = 0; i < profileDataArray->len; i++)
        {
          profileData *data;
          gchar *tmp;

          data = g_array_index (profileDataArray, profileData *, i);

          if (data->is_history)
            {
              tmp = g_filename_to_uri (data->path, NULL, NULL);

              if (tmp)
                {
                  g_key_file_set_value (key_file, tmp, "name", data->name);
                  g_free (tmp);

                  count++;
                }
            }
          profile_data_destroy (data);
        }
      g_array_free (profileDataArray, TRUE);
      profileDataArray = NULL;

      if (count)
        {
          if (!(path = (gchar *)g_getenv ("HOME")))
            path = (gchar *)g_get_home_dir ();
          path = g_build_filename (path, ".iccbutton_history", NULL);
          buf = g_key_file_to_data (key_file, &len, NULL);
          g_file_set_contents (path, buf, len, NULL);
          g_free (path);
          g_free (buf);
        }
      g_key_file_free (key_file);
    }
}

GtkWidget *
icc_button_new (void)
{
  return GTK_WIDGET (g_object_new (ICC_BUTTON_TYPE, NULL));
}


// 値の設定・取得
static void
icc_button_select_first (IccButton *button)
{
  gint i, len;
  gboolean result = FALSE;
  profileData *data;

  g_return_if_fail (IS_ICC_BUTTON (button));

  for (i = 0; i < profileDataArray->len; i++)
    {
      len = profileDataArray->len;
      data = g_array_index (profileDataArray, profileData *, i);

      if ((result = icc_button_set_filename (button, data->path, FALSE)))
        break;

      /* icc_button_set_filename ()を実行すると
         profileDataArrayの要素が減る場合がある */
      i -= len - profileDataArray->len;
    }

  if (!result)
    {
      g_free (button->path);
      button->path = NULL;
      gtk_label_set_text (GTK_LABEL (button->label), _("(not selected)"));
      gtk_image_clear (GTK_IMAGE (button->icon));
    }
}

void
icc_button_set_title (IccButton   *button,
                      const gchar *newTitle)
{
  g_return_if_fail (IS_ICC_BUTTON (button));

  g_free (button->title);
  button->title = g_strdup (newTitle);
}

gchar *
icc_button_get_title (IccButton *button)
{
  g_return_val_if_fail (IS_ICC_BUTTON (button), NULL);

  return g_strdup (button->title);
}

void
icc_button_set_mask (IccButton *button,
                     guint16    classMask,
                     guint16    pcsMask,
                     guint16    colorspaceMask)
{
  g_return_if_fail (IS_ICC_BUTTON (button));

  button->classMask = classMask & ICC_BUTTON_CLASS_ALL;
  button->pcsMask = pcsMask & ICC_BUTTON_COLORSPACE_ALL;
  button->colorspaceMask = colorspaceMask & ICC_BUTTON_COLORSPACE_ALL;

  if (button->path &&
      !checkProfileClass (button->classMask, button->pcsMask, button->colorspaceMask,
                          button->class, button->pcs, button->colorspace))
    {
      icc_button_select_first (button);
    }

  setupMenu (button);
}

void
icc_button_get_mask (IccButton *button,
                     guint16   *classMask,
                     guint16   *pcsMask,
                     guint16   *colorspaceMask)
{
  g_return_if_fail (button != NULL);

  if (classMask)
    *classMask = button->classMask;
  if (pcsMask)
    *pcsMask = button->pcsMask;
  if (colorspaceMask)
    *colorspaceMask = button->colorspaceMask;
}

gboolean
icc_button_set_filename (IccButton   *button,
                         const gchar *filename,
                         gboolean     add_history)
{
  profileData *data;

  g_return_val_if_fail (IS_ICC_BUTTON (button), FALSE);

  if (!filename)
    {
      if (button->enable_empty)
        {
          g_free (button->path);
          button->path = NULL;

          gtk_label_set_text (GTK_LABEL (button->label), _("None"));
          gtk_image_clear (GTK_IMAGE (button->icon));

          return TRUE;
        }
      else
        return FALSE;
    }
  else if (profile_data_new_from_file (filename, &data,
                                       button->classMask,
                                       button->pcsMask,
                                       button->colorspaceMask))
    {
      if (button->path)
        g_free (button->path);

      button->path = g_strdup (data->path);
      button->class = data->class;
      button->pcs = GUINT32_FROM_BE (*((icColorSpaceSignature *)data->pcs));
      button->colorspace = GUINT32_FROM_BE (*((icColorSpaceSignature *)data->colorspace));

      gtk_label_set_text (GTK_LABEL (button->label), data->name);
      gtk_image_set_from_stock (GTK_IMAGE (button->icon),
                                get_stock_id_from_profile_data (data),
                                GTK_ICON_SIZE_MENU);

      if (add_history)
        {
          GTimeVal time_val;

          profile_data_remove_filename (data->path);

          g_get_current_time (&time_val);
          data->is_history = TRUE;
          last_changed = time_val.tv_sec;

          profileDataArray = g_array_prepend_val (profileDataArray, data);
        }
      else
        profile_data_destroy (data);
    }
  else
    {
      /* remove profile info if the file isn't found */
      if (!g_file_test (filename, G_FILE_TEST_EXISTS))
        profile_data_remove_filename (filename);

      return FALSE;
    }
  return TRUE;
}

gchar *
icc_button_get_filename (IccButton *button)
{
  g_return_val_if_fail (IS_ICC_BUTTON (button), NULL);

  return g_strdup (button->path);
}

gboolean
icc_button_set_max_entries (IccButton *button, gint n)
{
  g_return_val_if_fail (IS_ICC_BUTTON (button), FALSE);
  button->maxEntries = n;
  return TRUE;
}

gint
icc_button_get_max_entries (IccButton *button)
{
  g_return_val_if_fail (IS_ICC_BUTTON (button), -1);
  return button->maxEntries;
}

void
icc_button_set_enable_empty (IccButton *button,
                             gboolean   enabled)
{
  gboolean old_value;

  g_return_if_fail (IS_ICC_BUTTON (button));

  old_value = button->enable_empty;

  if (old_value != enabled)
    {
      button->enable_empty = enabled;

      if (!enabled && !button->path)
        {
          /* 指定なしが許容されないので履歴上位のファイル名をセットする */
          icc_button_select_first (button);
        }

      setupMenu (button);
    }
}

gboolean
icc_button_get_enable_empty (IccButton *button)
{
  g_return_val_if_fail (IS_ICC_BUTTON (button), FALSE);

  return button->enable_empty;
}

gboolean
icc_button_is_empty (IccButton *button)
{
  g_return_val_if_fail (IS_ICC_BUTTON (button), FALSE);

  return (button->path == NULL);
}

void
icc_button_dialog_set_show_detail (IccButton *button,
                                   gboolean   show_detail)
{
  g_return_if_fail (IS_ICC_BUTTON (button));

  button->dialog_show_detail = show_detail;
}

gboolean
icc_button_dialog_get_show_detail (IccButton *button)
{
  g_return_val_if_fail (IS_ICC_BUTTON (button), FALSE);

  return button->dialog_show_detail;
}

void
icc_button_dialog_set_list_columns (IccButton *button,
                                    guint16    list_columns)
{
  g_return_if_fail (IS_ICC_BUTTON (button));

  button->dialog_list_columns = list_columns;
}

guint16
icc_button_dialog_get_list_columns (IccButton *button)
{
  g_return_val_if_fail (IS_ICC_BUTTON (button), 0);

  return button->dialog_list_columns;
}

/* Profile info of current selection */
const gchar *
icc_button_get_profile_desc (IccButton *button)
{
  g_return_val_if_fail (button != NULL, NULL);

  return button->path ? gtk_label_get_text (GTK_LABEL (button->label)) : NULL;
}

icProfileClassSignature
icc_button_get_class (IccButton *button)
{
  g_return_val_if_fail (button != NULL, 0);

  return button->path ? button->class : 0;
}

icColorSpaceSignature
icc_button_get_pcs (IccButton *button)
{
  g_return_val_if_fail (button != NULL, 0);

  return button->path ? button->pcs : 0;
}

icColorSpaceSignature
icc_button_get_colorspace (IccButton *button)
{
  g_return_val_if_fail (button != NULL, 0);

  return button->path ? button->colorspace : 0;
}


/* If ICCButton is clicked: */

enum {
  COLUMN_STOCK_ID,
  COLUMN_NAME,
  COLUMN_CLASS,
  COLUMN_COLORSPACE,
  COLUMN_PCS,
  COLUMN_PATH,
  N_COLUMNS
};

static void
setupData (IccButton *button)
{
  gint i;

  GtkListStore *store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW(button->treeView)));
  GtkTreeIter iter;
  GtkTreePath *treePath;

  if (button->enable_empty)
    {
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter,
                          COLUMN_NAME, _("None"),
                          COLUMN_CLASS, "",
                          COLUMN_COLORSPACE, "",
                          COLUMN_PCS, "",
                          COLUMN_PATH, "",
                          -1);

      if(!button->path &&
         (treePath = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter)) != NULL)
        {
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (button->treeView), treePath, NULL, FALSE);
          gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (button->treeView), treePath, NULL, FALSE, 0, 0);
        }
    }

  for (i = 0; i < profileDataArray->len; i++)
    {
      profileData *data = g_array_index (profileDataArray, profileData *, i);

      if (checkProfileClass (button->classMask, button->pcsMask, button->colorspaceMask,
                             data->class,
                             GUINT32_FROM_BE (*((icColorSpaceSignature *)data->pcs)),
                             GUINT32_FROM_BE (*((icColorSpaceSignature *)data->colorspace))))
        {
          gsize nWritten;
          gchar *tmp;

          tmp = g_filename_to_utf8 (data->path, -1, NULL, &nWritten, NULL);

          gtk_list_store_append (store, &iter);
          gtk_list_store_set (store, &iter,
                              COLUMN_STOCK_ID, get_stock_id_from_profile_data (data),
                              COLUMN_NAME, data->name,
                              COLUMN_CLASS, getProfileClassDisplayName (data->class),
                              COLUMN_COLORSPACE, data->colorspace,
                              COLUMN_PCS, data->pcs,
                              COLUMN_PATH, tmp,
                              -1);
          g_free (tmp);

          if (button->path != NULL &&
              strcmp (button->path, data->path) == 0 &&
              (treePath = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter)) != NULL)
            {
              gtk_tree_view_set_cursor (GTK_TREE_VIEW (button->treeView), treePath, NULL, FALSE);
              gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (button->treeView), treePath, NULL, FALSE, 0, 0);
            }
        }
    }
}

void
icc_button_choose_profile (GtkMenuItem *item,
                           IccButton   *button)
{
  guchar *path;

  path = g_object_get_data (G_OBJECT (item), "profile_path");
  icc_button_set_filename (button, path, TRUE);

  g_signal_emit (button, iccButtonSignals[CHANGED], 0);
}

static void
setupMenu (IccButton *button)
{
  gint i;
  GtkWidget *item, *image;
  GTimeVal time_val;

  profileData **matched_profiles;
  gint n_profiles_in_history = 0;
  gint n_menu_items;

  g_return_if_fail (IS_ICC_BUTTON (button));

  if (button->popupMenu)
    {
      gtk_widget_destroy (button->popupMenu);
      g_ptr_array_free (button->menuItems, TRUE);
    }

  /* list of matched profiles */
  matched_profiles = g_new (profileData *, button->maxEntries);
  for (i = button->nEntries = 0; i < profileDataArray->len; i++)
    {
      profileData *data = g_array_index (profileDataArray, profileData *, i);

      if (checkProfileClass (button->classMask, button->pcsMask, button->colorspaceMask,
                             data->class,
                             GUINT32_FROM_BE (*((icColorSpaceSignature *)data->pcs)),
                             GUINT32_FROM_BE (*((icColorSpaceSignature *)data->colorspace))))
        {
          /* 履歴にあるプロファイルは履歴にないプロファイルより先に出てくる */
          if (data->is_history)
            n_profiles_in_history++;

          if (button->nEntries < button->maxEntries)
            matched_profiles[button->nEntries] = data;

          button->nEntries++;
        }
    }

  n_menu_items = button->nEntries <= button->maxEntries ? button->nEntries : MIN (n_profiles_in_history, button->maxEntries);

  if (n_menu_items > 1)
    {
      button->popupMenu = gtk_menu_new ();
      button->menuItems = g_ptr_array_new ();

      for (i = 0; i < n_menu_items; i++)
        {
          item = gtk_image_menu_item_new_with_label (matched_profiles[i]->name);
          image = gtk_image_new_from_stock (get_stock_id_from_profile_data (matched_profiles[i]),
                                            GTK_ICON_SIZE_MENU);
          gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

          g_object_set_data (G_OBJECT (item), "profile_path", matched_profiles[i]->path);
          g_object_set_data (G_OBJECT (item), "index", GINT_TO_POINTER (i));
          gtk_menu_shell_append (GTK_MENU_SHELL (button->popupMenu), item);

          g_signal_connect (G_OBJECT (item), "activate",
                            G_CALLBACK (icc_button_choose_profile),
                            button);

          g_ptr_array_add (button->menuItems, item);
        }

      if (button->enable_empty)
        {
          item = gtk_separator_menu_item_new ();
          gtk_menu_shell_prepend (GTK_MENU_SHELL (button->popupMenu), item);
          item = gtk_menu_item_new_with_label (_("None"));
          gtk_menu_shell_prepend (GTK_MENU_SHELL (button->popupMenu), item);

          g_object_set_data (G_OBJECT (item), "profile_path", NULL);
          g_object_set_data (G_OBJECT (item), "index", GINT_TO_POINTER (-1));

          /*if (!button->path)
            gtk_menu_set_active (GTK_MENU (button->popupMenu), 0);*/

          g_signal_connect (G_OBJECT (item), "activate",
                            G_CALLBACK (icc_button_choose_profile),
                            button);
        }

      item = gtk_separator_menu_item_new();
      gtk_menu_shell_append( GTK_MENU_SHELL (button->popupMenu), item);
      item = gtk_menu_item_new_with_label (button->menuItems->len < button->nEntries ?
                                           _("More...") : _("Show dialog..."));
      gtk_menu_shell_append (GTK_MENU_SHELL (button->popupMenu), item);
      g_signal_connect (G_OBJECT (item), "activate",
                        G_CALLBACK (icc_button_run_dialog),
                        button);

      gtk_widget_show_all (button->popupMenu);

      button->menuWidth = -1;
      button->menuHeight = -1;
    }
  else
    {
      button->popupMenu = NULL;
      button->menuItems = NULL;
    }

  g_get_current_time (&time_val);
  button->last_updated = time_val.tv_sec;

  g_free (matched_profiles);
}

static void
icc_dialog_selected (GtkTreeView *treeView,
                     IccButton   *button)
{
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  gchar *path;

  if ((selection = gtk_tree_view_get_selection (treeView)) != NULL)
    {
      if (gtk_tree_selection_get_selected (selection, &model, &iter))
        {
          cmsHPROFILE profile = NULL;
          GString *str = g_string_new (NULL);

          gtk_tree_model_get(model, &iter, COLUMN_PATH, &path, -1);
          gtk_entry_set_text(GTK_ENTRY (button->entry), path);

          /* update the detail text */
          path = g_filename_from_utf8 (path, -1, NULL, NULL, NULL);
          if (path && (profile = lcms_open_profile (path)))
            {
              gchar c;
              guint32 version, class, colorspace, pcs;

              class      = cmsGetDeviceClass (profile);
              colorspace = cmsGetColorSpace (profile);
              pcs        = cmsGetPCS (profile);

              gtk_label_set_text (GTK_LABEL (button->class_label), getProfileClassDisplayName (class));

              version = lcms_get_encoded_icc_version (profile);
              g_string_printf (str, "%d.%d.%d.%d",
                               version >> 24, version >> 20 & 0xf,        /* major and minor version */
                               version >> 16 & 0xf, version >> 12 & 0xf); /* revision number */
              gtk_label_set_text (GTK_LABEL (button->version_label), str->str);

              /* whitepoint */
              if (class == icSigLinkClass)
                gtk_label_set_text (GTK_LABEL (button->whitepoint_label), "n/a");
              else
                {
                  cmsCIEXYZ wtpt, awtpt;
                  cmsCIExyY xyy;
                  gdouble temperature;

                  if (lcms_get_adapted_whitepoint (profile, &wtpt, &awtpt))
                    {
                      cmsXYZ2xyY (&xyy, &wtpt);

#ifdef USE_LCMS2
                      if (class == icSigDisplayClass && colorspace == icSigRgbData)
                        {
                          cmsTempFromWhitePoint (&temperature, &xyy);
                          g_string_printf (str, "%.1f", temperature);

                          if (awtpt.X >= 0)
                            {
                              cmsXYZ2xyY (&xyy, &awtpt);
                              cmsTempFromWhitePoint (&temperature, &xyy);
                              g_string_append_printf (str, " / %.1f", temperature);
                            }

                          g_string_append_printf (str, "K", temperature);
                        }
                      else
#endif
                        g_string_printf (str, "x%.3f, y%.3f", xyy.x, xyy.y);

                      gtk_label_set_text (GTK_LABEL (button->whitepoint_label), str->str);
                    }
                  else
                    gtk_label_set_text (GTK_LABEL (button->whitepoint_label), "n/a");
                }

#ifdef USE_LCMS2
              /* TAC */
              if (class == icSigOutputClass && colorspace == icSigCmykData)
                {
                  gtk_label_set_text (GTK_LABEL (button->matrix_label1), _("Estimated TAC:"));
                  g_string_printf (str, "%.0f%%", cmsDetectTAC (profile));
                  gtk_label_set_text (GTK_LABEL (button->matrix_label2), str->str);
                }
              else
#endif
              /* matrix-based profile? */
              if (class == icSigDisplayClass)
                {
                  gtk_label_set_text (GTK_LABEL (button->matrix_label1), _("Matrix / LUT:"));
                  g_string_printf (str, "%s / %s",
                                   lcms_is_matrix_shaper (profile) ? Q_("matrix-lut|Yes") : Q_("matrix-lut|No"),
                                   lcms_has_lut (profile) ? Q_("matrix-lut|Yes") : Q_("matrix-lut|No"));
                  gtk_label_set_text (GTK_LABEL (button->matrix_label2), str->str);
                }
              else
                {
                  gtk_label_set_text (GTK_LABEL (button->matrix_label1), "");
                  gtk_label_set_text (GTK_LABEL (button->matrix_label2), "");
                }

              /* colorspace / PCS */
              g_string_printf (str, "%c%c%c%c",
                               colorspace >> 24, colorspace >> 16 & 0xff,
                               colorspace >> 8 & 0xff, (c = colorspace & 0xff, c == 0x20 ? 0 : c));
              g_string_set_size (str, strlen (str->str));

              if (class == icSigLinkClass || class == icSigAbstractClass)
                {
                  gtk_label_set_text (GTK_LABEL (button->colorspace_label1), _("Conversion:"));

                  g_string_append_printf (str, _(" to %c%c%c%c"),
                                          pcs >> 24, pcs >> 16 & 0xff,
                                          pcs >> 8 & 0xff, (c = pcs & 0xff, c == 0x20 ? 0 : c));
                  gtk_label_set_text (GTK_LABEL (button->colorspace_label2), str->str);
                }
              else
                {
                  gtk_label_set_text (GTK_LABEL (button->colorspace_label1), _("Colorspace / PCS:"));

                  g_string_append_printf (str, " / %c%c%c%c",
                                          pcs >> 24, pcs >> 16 & 0xff,
                                          pcs >> 8 & 0xff, (c = pcs & 0xff, c == 0x20 ? 0 : c));
                }

              gtk_label_set_text (GTK_LABEL (button->colorspace_label2), str->str);

              cmsCloseProfile (profile);
              g_free (path);
            }
          else
            {
              gtk_label_set_text (GTK_LABEL (button->class_label), "n/a");
              gtk_label_set_text (GTK_LABEL (button->version_label), "n/a");
              gtk_label_set_text (GTK_LABEL (button->colorspace_label2), "n/a");
              gtk_label_set_text (GTK_LABEL (button->whitepoint_label), "n/a");
              gtk_label_set_text (GTK_LABEL (button->matrix_label1), "");
              gtk_label_set_text (GTK_LABEL (button->matrix_label2), "");
            }

          g_string_free (str, TRUE);
        }
    }
}

static void
icc_dialog_double_clicked (GtkTreeView       *treeView,
                           GtkTreePath       *path,
                           GtkTreeViewColumn *column,
                           IccButton         *button)
{
  gtk_dialog_response (GTK_DIALOG (button->dialog), GTK_RESPONSE_ACCEPT);
}

static void
icc_button_menu_position (GtkMenu  *menu,
                          gint     *x,
                          gint     *y,
                          gboolean *push_in,
                          gpointer  user_data)
{
  GtkWidget *widget = GTK_WIDGET (user_data);
  IccButton *button = ICC_BUTTON (user_data);
  GtkWidget *menuItem;
  GtkWidget *child;
  GList *children;
  GtkRequisition requisition;
  gint _x, _y;

  if (button->menuWidth == -1)
    {
      gtk_widget_get_child_requisition (GTK_WIDGET (menu), &requisition);
      button->menuWidth = requisition.width;
      button->menuHeight = requisition.height;
    }

  gdk_window_get_origin (widget->window, &_x, &_y);

  if (widget->allocation.width > button->menuWidth)
    {
      gtk_widget_set_size_request (GTK_WIDGET (menu), widget->allocation.width, -1);
      _x += widget->allocation.x;
    }
  else
    {
      gtk_widget_set_size_request (GTK_WIDGET (menu), button->menuWidth, -1);
      _x += widget->allocation.x - (button->menuWidth - widget->allocation.width) / 2;
    }

  menuItem = gtk_menu_get_active (menu );
  children = GTK_MENU_SHELL (menu)->children;

  if (menuItem == NULL)
    {
      gtk_menu_shell_select_first (GTK_MENU_SHELL (menu), FALSE);
      menuItem = gtk_menu_get_active (menu );
    }
  else
    gtk_menu_shell_select_item (GTK_MENU_SHELL (menu), menuItem);

  if (menuItem != NULL)
    {
      gtk_widget_get_child_requisition (menuItem, &requisition);
      _y += requisition.height / 2;
    }

  while (children)
    {
      child = children->data;

      if (menuItem == child)
        break;

      if (GTK_WIDGET_VISIBLE (child))
        {
          gtk_widget_get_child_requisition (child, &requisition);
          _y -= requisition.height;
        }

      children = children->next;
    }

  _y += widget->allocation.y - widget->allocation.height / 2 + 2;

  *x = _x;
  *y = _y;
  *push_in = TRUE;
}

static void
icc_button_clicked (IccButton *button,
                    gpointer   data)
{
  if (!button->popupMenu || last_changed > button->last_updated)
    setupMenu (button);

  if (button->popupMenu && button->menuItems->len > 0)
    {
      /* 現在のファイルと同じ項目があれば選択 */
      if (!button->path)
        gtk_menu_set_active (GTK_MENU (button->popupMenu), 0);
      else
        {
          gint i;

          for (i = 0; i < button->menuItems->len; i++)
            {
              GObject *obj = g_ptr_array_index (button->menuItems, i);
              gchar *path = g_object_get_data (obj, "profile_path");

              if (path && strcmp (path, button->path) == 0)
                {
                  gtk_menu_set_active (GTK_MENU (button->popupMenu), i + (button->enable_empty ? 2 : 0));
                  break;
                }
            }
        }

      gtk_menu_popup (GTK_MENU (button->popupMenu), NULL, NULL,
                      icc_button_menu_position, button,
                      0, gtk_get_current_event_time());
    }
  else
    icc_button_run_dialog (GTK_WIDGET (button), button);
}

static void
icc_button_run_dialog (GtkWidget *widget,
                       gpointer   data)
{
  IccButton         *button = ICC_BUTTON (data);
  GtkWidget         *label;
  GtkWidget         *vbox, *hbox;
  GtkWidget         *frame;
  GtkListStore      *listStore;
  GtkCellRenderer   *renderer;
  GtkTreeViewColumn *column;

  gint               response;
  gboolean           result = FALSE;

  button->dialog =
    gtk_dialog_new_with_buttons (button->title ? button->title : _("Choose an ICC Profile"),
                                 NULL,
                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
                                 GTK_STOCK_CANCEL,
                                 GTK_RESPONSE_REJECT,
                                 GTK_STOCK_OK,
                                 GTK_RESPONSE_ACCEPT,
                                 NULL);

  gtk_dialog_set_alternative_button_order (GTK_DIALOG (button->dialog),
                                           GTK_RESPONSE_ACCEPT,
                                           GTK_RESPONSE_REJECT,
                                           -1);
  gtk_widget_set_size_request (button->dialog, 540, -1);

  hbox = gtk_hbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (button->dialog)->vbox), hbox, TRUE, TRUE, 0);

  button->scrolledWindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (button->scrolledWindow), GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (button->scrolledWindow),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

  listStore = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  button->treeView = gtk_tree_view_new_with_model (GTK_TREE_MODEL (listStore));
  g_signal_connect (G_OBJECT (button->treeView), "cursor-changed", G_CALLBACK (icc_dialog_selected), button);
  g_signal_connect (G_OBJECT (button->treeView), "row-activated", G_CALLBACK (icc_dialog_double_clicked), button);
  g_object_unref (listStore);

  renderer = gtk_cell_renderer_pixbuf_new ();
  column = gtk_tree_view_column_new_with_attributes ("", renderer, "stock-id", COLUMN_STOCK_ID, NULL);
  gtk_tree_view_column_set_resizable (column, FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (button->treeView), column);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_STOCK_ID);
  gtk_tree_view_column_set_visible (column, button->dialog_list_columns & ICC_BUTTON_COLUMN_ICON);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Name"), renderer, "text", COLUMN_NAME, NULL);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (button->treeView), column);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_NAME);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Class"), renderer, "text", COLUMN_CLASS, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (button->treeView), column);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_CLASS);
  gtk_tree_view_column_set_visible (column, button->dialog_list_columns & ICC_BUTTON_COLUMN_CLASS);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Colorspace"), renderer, "text", COLUMN_COLORSPACE, NULL);
  gtk_tree_view_column_set_resizable (column, TRUE );
  gtk_tree_view_append_column (GTK_TREE_VIEW (button->treeView), column);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_COLORSPACE);
  gtk_tree_view_column_set_visible (column, button->dialog_list_columns & ICC_BUTTON_COLUMN_COLORSPACE);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("PCS"), renderer, "text", COLUMN_PCS, NULL);
  gtk_tree_view_column_set_resizable( column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (button->treeView), column);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_PCS);
  gtk_tree_view_column_set_visible (column, button->dialog_list_columns & ICC_BUTTON_COLUMN_PCS);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Path"), renderer, "text", COLUMN_PATH, NULL);
  gtk_tree_view_column_set_resizable (column, TRUE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (button->treeView), column);
  gtk_tree_view_column_set_sort_column_id (column, COLUMN_PATH);
  gtk_tree_view_column_set_visible (column, button->dialog_list_columns & ICC_BUTTON_COLUMN_PATH);

  gtk_container_add (GTK_CONTAINER (button->scrolledWindow), button->treeView);
  gtk_box_pack_start (GTK_BOX (hbox), button->scrolledWindow, TRUE, TRUE, 0);

  /* Detail */
  frame = gtk_frame_new (_("Detail"));
  gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, FALSE, 0);
  vbox = gtk_vbox_new (FALSE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
  gtk_container_add (GTK_CONTAINER (frame), vbox);

  label = gtk_label_new (_("Profile class:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  button->class_label = gtk_label_new ("n/a");
  gtk_misc_set_alignment (GTK_MISC (button->class_label), 1.0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox), button->class_label, FALSE, FALSE, 0);

  label = gtk_label_new (_("ICC version:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  button->version_label = gtk_label_new ("n/a");
  gtk_misc_set_alignment (GTK_MISC (button->version_label), 1.0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox), button->version_label, FALSE, FALSE, 0);

  button->colorspace_label1 = gtk_label_new (_("Colorspace / PCS:"));
  gtk_misc_set_alignment (GTK_MISC (button->colorspace_label1), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox), button->colorspace_label1, FALSE, FALSE, 0);
  button->colorspace_label2 = gtk_label_new ("n/a");
  gtk_misc_set_alignment (GTK_MISC (button->colorspace_label2), 1.0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox), button->colorspace_label2, FALSE, FALSE, 0);

  label = gtk_label_new (_("Whitepoint:            "));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  button->whitepoint_label = gtk_label_new ("n/a");
  gtk_misc_set_alignment (GTK_MISC (button->whitepoint_label), 1.0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox), button->whitepoint_label, FALSE, FALSE, 0);

  button->matrix_label1 = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (button->matrix_label1), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox), button->matrix_label1, FALSE, FALSE, 0);
  button->matrix_label2 = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (button->matrix_label2), 1.0, 0.5);
  gtk_box_pack_start (GTK_BOX (vbox), button->matrix_label2, FALSE, FALSE, 0);

  // Path entry
  hbox = gtk_hbox_new (FALSE, 6);
  gtk_container_border_width (GTK_CONTAINER (hbox), 8);
  label = gtk_label_new_with_mnemonic (_("_Path:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, TRUE, 0);
  button->entry = gtk_entry_new ();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), button->entry);
  gtk_drag_dest_set (button->entry, GTK_DEST_DEFAULT_ALL,
                     targets, 1, GDK_ACTION_COPY);
  g_signal_connect (G_OBJECT (button->entry), "drag_data_received",
                    G_CALLBACK (icc_button_drag_data_received), NULL);
  gtk_box_pack_start (GTK_BOX (hbox), button->entry, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (button->dialog)->vbox), hbox, FALSE, FALSE, 0);

  setupData (button);

  if (button->path != NULL && strlen (gtk_entry_get_text (GTK_ENTRY (button->entry))) == 0)
    {
      gchar *path;
      gsize nWritten;

      path = g_filename_to_utf8 (button->path, -1, NULL, &nWritten, NULL);
      if (path)
        {
          gtk_entry_set_text (GTK_ENTRY (button->entry), path);
          g_free (path);
        }
    }

  gtk_widget_show_all (button->dialog);

  if (!button->dialog_show_detail)
    gtk_widget_hide (frame);

  do
    {
      gchar *path;
      gsize nWritten;

      response = gtk_dialog_run (GTK_DIALOG (button->dialog));

      if (response == GTK_RESPONSE_ACCEPT)
        {
          path = (gchar *)gtk_entry_get_text (GTK_ENTRY (button->entry));
          path = path[0] ? g_filename_from_utf8 (path, -1, NULL, &nWritten, NULL) : NULL;

          if (!(result = icc_button_set_filename (button, path, TRUE)))
            {
              GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW( button->dialog),
                                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                                          GTK_MESSAGE_WARNING,
                                                          GTK_BUTTONS_CLOSE,
                                                          _("Invalid profile"));
              gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog),
                                                          _("Please check the filename, device class, and colorspace."));
              gtk_dialog_run (GTK_DIALOG (dialog));
              gtk_widget_destroy (dialog);
            }
          else
            g_signal_emit (button, iccButtonSignals[CHANGED], 0);

          g_free (path);
        }
    }
  while (response == GTK_RESPONSE_ACCEPT && result == FALSE);

  gtk_widget_destroy (button->dialog);

  button->dialog = NULL;
  button->scrolledWindow = NULL;
  button->treeView = NULL;
  button->entry = NULL;
}
