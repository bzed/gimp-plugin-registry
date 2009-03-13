/**********************************************************************
  iccbutton.c
  Copyright(C) 2007 Y.Yamakawa
**********************************************************************/

#include "iccbutton.h"
#include "iccclassicons.h"
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#if defined MACOSX && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_3
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFLocale.h>
#include <CoreFoundation/CFPreferences.h>
#endif

static cmsHPROFILE checkProfile   (gchar *path,
                                   guint16 class,
                                   guint16 colorSpace);

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
  icProfileClassSignature class;
  gchar *colorspace;
  gchar *pcs;
  gboolean is_history;
} profileData;

static GArray *profileDataArray = NULL;

static glong last_changed = 0;

static gboolean
profile_data_new_from_file (gchar        *path,
                            profileData **data,
                            guint16       class,
                            guint16       colorspace)
{
  cmsHPROFILE profile;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  if ((profile = checkProfile (path, class, colorspace)))
    {
      profileData *_data;
      guint32 sig;

      _data = g_new (profileData, 1);

      _data->path       = g_strdup (path);
      _data->name       = _icc_button_get_profile_desc (profile);
      _data->class      = cmsGetDeviceClass (profile);
      _data->colorspace = g_strndup ((sig = GUINT32_TO_BE (cmsGetColorSpace (profile)), (gchar *)&sig), 4);
      _data->pcs        = g_strndup ((sig = GUINT32_TO_BE (cmsGetPCS (profile)), (gchar *)&sig), 4);

      cmsCloseProfile (profile);

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

  if( profileClassTable == NULL )
    return "";

  for( i = 0; i < N_PROFILE_CLASSES; i++ ) {
    if( profileClassTable[i].sig == sig )
      return profileClassTable[i].displayName;
  }

  return "";
}

static const gchar *
get_stock_id_from_profile_data (profileData *data)
{
  gint i;

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


/***************************************************************
  Get a localized text from the 'mluc' type tag
  That text is already converted to UTF-8 from UTF-16BE

  Note : In the future, this function will be moved to external
         library.
****************************************************************/

static gchar *readLocalizedText (cmsHPROFILE profile, icTagSignature sig)
{
  LPLCMSICCPROFILE icc = (LPLCMSICCPROFILE)profile;

  size_t offset, size;
  gint n;

  if (profile != NULL && (n = _cmsSearchTag (profile, sig, FALSE)) >= 0)
    {
      gchar *buf = NULL;

      icTagBase base;

      offset = icc->TagOffsets[n];
      size = icc->TagSizes[n];

      if (icc->Seek (icc, offset))
        return NULL;

      icc->Read (&base, sizeof (icTagBase), 1, icc);
      base.sig = g_ntohl (base.sig);

      size -= sizeof (icTagBase);

      switch (base.sig)
        {
        case icSigTextDescriptionType: {
          icUInt32Number length;

          icc->Read (&length, sizeof (icUInt32Number), 1, icc);
          length = g_ntohl (length);
          if (length <= 0)
            return NULL;
          length = length > ICC_PROFILE_DESC_MAX ? ICC_PROFILE_DESC_MAX : length;

          if ((buf = g_new0 (gchar, length + 1)) != NULL)
            icc->Read (buf, sizeof (gchar), length, icc);

          return buf; }
        case icSigMultiLocalizedUnicodeType: {
          gchar *utf8String = NULL;

          icUInt32Number nNames, nameRecordSize;
          icUInt16Number lang, region, _lang, _region;
          gchar *locale;

          guint i;

          size_t entryLength = 0, entryOffset = 0, _entryLength = 0, _entryOffset = 0;
          gboolean flag = FALSE;

#ifdef G_OS_WIN32
          locale = g_win32_getlocale ();
#elif defined MACOSX && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_3
          {
            static gchar *localeIDString = NULL;

            if (localeIDString == NULL)
              {
                CFTypeRef array = CFPreferencesCopyAppValue (CFSTR ("AppleLanguages"), kCFPreferencesCurrentApplication);
                CFTypeRef localeID = CFPreferencesCopyAppValue (CFSTR ("AppleLocale"), kCFPreferencesCurrentApplication);
                CFTypeRef languageID = NULL;

                gchar *langStr = NULL, *localeStr = NULL;

                if (array != NULL && CFGetTypeID (array) == CFArrayGetTypeID())
                  languageID = CFArrayGetValueAtIndex (array, 0);
                if (languageID != NULL && CFGetTypeID (languageID) == CFStringGetTypeID())
                  langStr = CFStringGetCStringPtr (languageID, kCFStringEncodingASCII);

                if (localeID != NULL && CFGetTypeID (localeID) == CFStringGetTypeID())
                  localeStr = CFStringGetCStringPtr (localeID, kCFStringEncodingASCII);

                if (langStr == NULL ||
                    (localeStr != NULL && strncmp (langStr, localeStr, 2) == 0))
                  localeIDString = localeStr;
                else
                  localeIDString = langStr;

                localeIDString = g_strdup (localeIDString == NULL ? "C" : localeIDString);

                if (array) CFRelease (array);
                if (localeID) CFRelease (localeID);
                if (languageID) CFRelease (languageID);
              }
            locale = localeIDString;
          }
#else
          locale = setlocale (LC_ALL, NULL);
#endif
          _lang = locale[0] << 8 | locale[1];
          _region = strlen (locale) >= 5 ? locale[3] << 8 | locale[4] : 0;

#ifdef G_OS_WIN32
          g_free (locale);
#endif

          icc->Read (&nNames, sizeof (icUInt32Number), 1, icc);
          nNames = g_ntohl (nNames);
          icc->Read (&nameRecordSize, sizeof (icUInt32Number), 1, icc);

          // scan name records
          for (i = 0; i < nNames; i++)
            {
              icc->Read (&lang, sizeof (icUInt16Number), 1, icc);
              lang = g_ntohs (lang );
              icc->Read (&region, sizeof (icUInt16Number), 1, icc);
              region = g_ntohs (region);

              icc->Read (&_entryLength, sizeof (icUInt32Number), 1, icc);
              icc->Read (&_entryOffset, sizeof (icUInt32Number), 1, icc);

              if (strncasecmp ((gchar *)(&lang), (gchar *)(&_lang), 2) == 0)
                {
                  if (!flag || strncasecmp ((gchar *)(&region), (gchar *)(&_region), 2) == 0)
                    {
                      flag = TRUE;
                      entryLength = g_ntohl (_entryLength);
                      entryOffset = g_ntohl (_entryOffset);
                    }
                }
              else if (i == 0)
                {
                  entryLength = g_ntohl (_entryLength);
                  entryOffset = g_ntohl (_entryOffset);
                }
            }

          if( entryLength <= 0 )
            return NULL;
          entryLength = entryLength > ICC_PROFILE_DESC_MAX ? ICC_PROFILE_DESC_MAX : entryLength;

          if ((buf = g_new0 (gchar, entryLength + 1)) != NULL)
            {
              icc->Seek (icc, offset + entryOffset);
              icc->Read (buf, sizeof (gchar), entryLength, icc);
              utf8String = g_convert (buf, entryLength, "UTF-8", "UTF-16BE", NULL, &i, NULL);
            }
          g_free (buf);

          return  utf8String; }
        default:
          return NULL;
        }
    } else
      return NULL;
}


// The private tag used in the ICC profiles provided by Apple,Inc.

enum
{
  icSigProfileDescriptionMLTag = 0x6473636dL    /* 'dscm' */
};


// Get a localized name of the profile.
// The returned value should be freed when no longer needed.
// Note : In the future, this function will be moved to external library.

gchar *_icc_button_get_profile_desc (cmsHPROFILE profile)
{
  static gchar *utf8String = NULL;

  if (!profile)
    return NULL;

  if (!(utf8String = readLocalizedText (profile, icSigProfileDescriptionMLTag)))
    utf8String = readLocalizedText (profile, icSigProfileDescriptionTag);

  if (utf8String)
    {
      gchar *errorChar, *str;

      if (!g_utf8_validate (utf8String, -1, &errorChar))
        {
          *errorChar = '\0'; /* avoid crashing with sorting profiles by descriptions */
          str = g_strdup_printf (_("%s(broken text)..."), utf8String);
          g_free (utf8String);
          utf8String = str;
        }
    }
  else
    utf8String = g_strdup (_("(no name)"));

  return utf8String;
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
checkProfileClass( guint16 class, guint16 colorSpace,
                   icProfileClassSignature classSig, icColorSpaceSignature colorSpaceSig )
{
  return ( ( ( class & CLASS_INPUT && classSig == icSigInputClass ) ||
             ( class & CLASS_OUTPUT && classSig == icSigOutputClass ) ||
             ( class & CLASS_DISPLAY && classSig == icSigDisplayClass ) ||
             ( class & CLASS_LINK && classSig == icSigLinkClass ) ||
             ( class & CLASS_ABSTRACT && classSig == icSigAbstractClass ) ) &&
           ( ( colorSpace & COLORSPACE_GRAY && colorSpaceSig == icSigGrayData ) ||
             ( colorSpace & COLORSPACE_RGB && colorSpaceSig == icSigRgbData ) ||
             ( colorSpace & COLORSPACE_CMY && colorSpaceSig == icSigCmyData ) ||
             ( colorSpace & COLORSPACE_CMYK && colorSpaceSig == icSigCmykData ) ||
             ( colorSpace & COLORSPACE_XYZ && colorSpaceSig == icSigXYZData ) ||
             ( colorSpace & COLORSPACE_LAB && colorSpaceSig == icSigLabData ) ) ? TRUE : FALSE );
}

static cmsHPROFILE checkProfile( gchar *path, guint16 class, guint16 colorSpace ) {
  cmsHPROFILE profile;

  if( ( profile = cmsOpenProfileFromFile( path, "r" ) ) != NULL ) {
    if( checkProfileClass( class, colorSpace, cmsGetDeviceClass( profile ), cmsGetColorSpace( profile ) ) ) {
      return profile;
    } else
      cmsCloseProfile( profile );
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

              /* If the profile was found in the history, skip the registration */
              for (i = 0; i < profileDataArray->len; i++)
                if (strcmp (tmp, g_array_index (profileDataArray, profileData *, i)->path) == 0)
                  break;

              if (i >= profileDataArray->len &&
                  profile_data_new_from_file (tmp, &data, CLASS_ALL, COLORSPACE_ALL))
                {
                  data->is_history = FALSE;
                  g_array_append_val (profileDataArray, data);
                }
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

      if (!(path = g_getenv ("HOME")))
        path = g_get_home_dir ();
      path = g_build_filename (path, ".iccbutton_history", NULL);

      key_file = g_key_file_new ();

      if (g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, NULL))
        {
          gchar **group_names = g_key_file_get_groups (key_file, NULL);
          profileData *_data;

          for (i = 0; group_names[i] != NULL; i++)
            {
              gchar *tmp;
              gsize nWritten;

              tmp = g_filename_from_utf8 (group_names[i], -1, NULL, &nWritten, NULL);

              if (profile_data_new_from_file (tmp, &_data, CLASS_ALL, COLORSPACE_ALL))
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
  searchPath[3] = g_build_filename (g_getenv ("SYSTEMROOT"), "system32\\spool\\drivers\\color", NULL);
  searchPath[4] = g_build_filename (g_getenv ("PROGRAMFILES"), "common files\\adobe\\color\\profiles", NULL);
#elif defined MACOSX
  gchar *searchPath[6];
  searchPath[3] = g_build_filename (g_get_home_dir (), "Library/ColorSync/Profiles", NULL);
  searchPath[4] = g_strdup ("/Library/ColorSync/Profiles");
  searchPath[5] = g_strdup ("/Library/Application Support/Adobe/Color/Profiles");
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
icc_button_get_type( void )
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
  G_OBJECT_CLASS (klass)->finalize = icc_button_finalize;

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

      pixbuf = gdk_pixbuf_new_from_inline (-1, icc_link_class_icon, FALSE, NULL);
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
  button->dialog = NULL;
  button->title = NULL;
  button->path = NULL;
  button->classMask = CLASS_ALL;
  button->colorspaceMask = COLORSPACE_ALL;

  button->popupMenu = NULL;
  button->menuItems = NULL;
  button->nEntries = 0;
  button->maxEntries = 15;
  button->last_updated = 0;

  button->hbox = gtk_hbox_new (FALSE, 0);
  button->icon = gtk_image_new ();
  button->label = gtk_label_new (_("(not selected)"));
  gtk_label_set_ellipsize (GTK_LABEL (button->label), PANGO_ELLIPSIZE_END);
  gtk_misc_set_alignment (GTK_MISC (button->label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (button->hbox), button->icon, FALSE, FALSE, 4);
  gtk_box_pack_start (GTK_BOX (button->hbox), button->label, TRUE, TRUE, 4);
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
          gsize nWritten;

          data = g_array_index (profileDataArray, profileData *, i);

          if (data->is_history)
            {
              tmp = g_filename_to_utf8 (data->path, -1, NULL, &nWritten, NULL);

              g_key_file_set_value (key_file, tmp, "name", data->name);

              g_free (tmp);

              count++;
            }
          profile_data_destroy (data);
        }
      g_array_free (profileDataArray, TRUE);
      profileDataArray = NULL;

      if (count)
        {
          if (!(path = g_getenv ("HOME")))
            path = g_get_home_dir ();
          path = g_build_filename (path, ".iccbutton_history", NULL);
          buf = g_key_file_to_data (key_file, &len, NULL);
          g_file_set_contents (path, buf, len, NULL);
          g_free (path);
          g_free (buf);
        }
      g_key_file_free (key_file);
    }
}

GtkWidget
*icc_button_new( void )
{
  return GTK_WIDGET (g_object_new (ICC_BUTTON_TYPE, NULL));
}


// 値の設定・取得

void
icc_button_set_title (IccButton *button, const gchar *newTitle)
{
  g_return_if_fail (IS_ICC_BUTTON (button));

  g_free (button->title);
  button->title = g_strdup (newTitle);
}

gchar
*icc_button_get_title (IccButton *button)
{
  g_return_val_if_fail (IS_ICC_BUTTON (button), NULL);

  return g_strdup (button->title);
}

void icc_button_set_mask( IccButton *button, guint16 classMask, guint16 colorspaceMask )
{
  g_return_if_fail( IS_ICC_BUTTON( button ) );

  button->classMask = classMask & CLASS_ALL;
  button->colorspaceMask = colorspaceMask & COLORSPACE_ALL;

  setupMenu( button );
}

gboolean
icc_button_set_filename (IccButton   *button,
                         const gchar *filename,
                         gboolean     add_history)
{
  profileData *data;

  g_return_val_if_fail (IS_ICC_BUTTON (button), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  if (profile_data_new_from_file (filename, &data,
                                  button->classMask,
                                  button->colorspaceMask))
    {
      if (button->path)
        g_free (button->path);

      button->path = g_strdup (data->path);

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

gchar *icc_button_get_filename( IccButton *button )
{
  g_return_val_if_fail( IS_ICC_BUTTON( button ), NULL );

  return g_strdup( button->path );
}

gboolean icc_button_set_max_entries( IccButton *button, gint n )
{
  g_return_val_if_fail( IS_ICC_BUTTON( button ), FALSE );
  button->maxEntries = n;
  return TRUE;
}

gint icc_button_get_max_entries( IccButton *button )
{
  g_return_val_if_fail( IS_ICC_BUTTON( button ), -1 );
  return button->maxEntries;
}


/* If ICCButton is clicked: */

enum {
  COLUMN_ICON,
  COLUMN_NAME,
  COLUMN_CLASS,
  COLUMN_COLORSPACE,
  COLUMN_PCS,
  COLUMN_PATH,
  N_COLUMNS
};

static void setupData( IccButton *button )
{
  gint i;

  GtkListStore *store = GTK_LIST_STORE( gtk_tree_view_get_model( GTK_TREE_VIEW( button->treeView ) ) );
  GtkTreeIter iter;

  for( i = 0; i < profileDataArray->len; i++ ) {
    profileData *data = g_array_index( profileDataArray, profileData *, i );

    if( checkProfileClass( button->classMask, button->colorspaceMask,
                           data->class,
                           GUINT32_FROM_BE( *( (icColorSpaceSignature *)data->colorspace ) ) ) ) {
      gsize nWritten;
      gchar *tmp;
      GtkTreePath *treePath;
      GdkPixbuf *icon;
      
      tmp = g_filename_to_utf8( data->path, -1, NULL, &nWritten, NULL );
      icon = gtk_widget_render_icon (GTK_WIDGET (button), get_stock_id_from_profile_data (data),
                                     GTK_ICON_SIZE_MENU, NULL);

      gtk_list_store_append( store, &iter );
      gtk_list_store_set( store, &iter,
                          COLUMN_ICON, icon,
                          COLUMN_NAME, data->name,
                          COLUMN_CLASS, getProfileClassDisplayName( data->class ),
                          COLUMN_COLORSPACE, data->colorspace,
                          COLUMN_PCS, data->pcs,
                          COLUMN_PATH, tmp,
                          -1 );
      g_free( tmp );
      g_object_unref (icon);

      if( button->path != NULL &&
          strcmp( button->path, data->path ) == 0 &&
          ( treePath = gtk_tree_model_get_path( GTK_TREE_MODEL( store ), &iter ) ) != NULL ) {
        gtk_tree_view_set_cursor( GTK_TREE_VIEW( button->treeView ), treePath, NULL, FALSE );
        gtk_tree_view_scroll_to_cell( GTK_TREE_VIEW( button->treeView ), treePath, NULL, FALSE, 0, 0 );
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
  GtkRequisition requisition;

  g_return_if_fail (IS_ICC_BUTTON (button));

  if (button->popupMenu)
    {
      gtk_widget_destroy (button->popupMenu);
      g_array_free (button->menuItems, TRUE);
    }

  button->popupMenu = gtk_menu_new ();
  button->menuItems = g_array_new (FALSE, FALSE, sizeof (gpointer));

  for (i = button->nEntries = 0; i < profileDataArray->len; i++)
    {
      profileData *data = g_array_index (profileDataArray, profileData *, i);

      if (checkProfileClass (button->classMask, button->colorspaceMask,
                             data->class,
                             GUINT32_FROM_BE (*((icColorSpaceSignature *)data->colorspace))))
        {
          if (button->nEntries < button->maxEntries && data->is_history)
            {
              item = gtk_image_menu_item_new_with_label (data->name);
              image = gtk_image_new_from_stock (get_stock_id_from_profile_data (data),
                                                GTK_ICON_SIZE_MENU);
              gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);

              g_object_set_data (G_OBJECT (item), "profile_path", data->path);
              gtk_menu_shell_append (GTK_MENU_SHELL (button->popupMenu), item);

              if (data->path && button->path && strcmp (data->path, button->path) == 0)
                gtk_menu_set_active (GTK_MENU (button->popupMenu), button->menuItems->len);

              g_signal_connect (G_OBJECT (item), "activate",
                                G_CALLBACK (icc_button_choose_profile),
                                button);

              button->menuItems = g_array_append_val (button->menuItems, item);
          }
          button->nEntries++;
    }
  }

  if (button->menuItems->len > 1)
    {
      if (button->menuItems->len < button->nEntries)
        {
          item = gtk_separator_menu_item_new();
          gtk_menu_shell_append( GTK_MENU_SHELL (button->popupMenu), item);
          item = gtk_menu_item_new_with_label (_("More..."));
          gtk_menu_shell_append (GTK_MENU_SHELL (button->popupMenu), item);
          g_signal_connect (G_OBJECT (item), "activate",
                            G_CALLBACK (icc_button_run_dialog),
                            button);
        }

      gtk_widget_show_all (button->popupMenu);

      button->menuWidth = -1;
      button->menuHeight = -1;
    }
  else
    {
      gtk_widget_destroy (button->popupMenu);
      g_array_free (button->menuItems, TRUE);
      button->popupMenu = NULL;
      button->menuItems = NULL;
    }
}

static void icc_dialog_selected( GtkTreeView *treeView, IccButton *button )
{
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  gchar *path;

  if( ( selection = gtk_tree_view_get_selection( treeView ) ) != NULL) {
    if( gtk_tree_selection_get_selected( selection, &model, &iter ) ) {
      gtk_tree_model_get( model, &iter, COLUMN_PATH, &path, -1 );
      gtk_entry_set_text( GTK_ENTRY( button->entry ), path );
    }
  }
}

static void icc_dialog_double_clicked( GtkTreeView *treeView,
                                       GtkTreePath *path,
                                       GtkTreeViewColumn *column,
                                       IccButton *button )
{
  gtk_dialog_response( GTK_DIALOG( button->dialog ), GTK_RESPONSE_ACCEPT );
}

static void icc_button_menu_position( GtkMenu *menu, gint *x, gint *y,
                                      gboolean *push_in, gpointer user_data )
{
  GtkWidget *widget = GTK_WIDGET( user_data );
  IccButton *button = ICC_BUTTON( user_data );
  GtkWidget *menuItem;
  GtkWidget *child;
  GList *children;
  GtkRequisition requisition;
  gint _x, _y;

  if( button->menuWidth == -1 ) {
    gtk_widget_get_child_requisition( menu, &requisition );
    button->menuWidth = requisition.width;
    button->menuHeight = requisition.height;
  }

  gdk_window_get_origin( widget->window, &_x, &_y );

  if( widget->allocation.width > button->menuWidth ) {
    gtk_widget_set_size_request( GTK_WIDGET( menu ), widget->allocation.width, -1 );
    _x += widget->allocation.x;
  } else {
    gtk_widget_set_size_request( GTK_WIDGET( menu ), button->menuWidth, -1 );
    _x += widget->allocation.x - ( button->menuWidth - widget->allocation.width ) / 2;
  }

  menuItem = gtk_menu_get_active( menu );
  children = GTK_MENU_SHELL( menu )->children;

  if( menuItem == NULL ) {
    gtk_menu_shell_select_first( GTK_MENU_SHELL( menu ), FALSE );
    menuItem = gtk_menu_get_active( menu );
  } else
    gtk_menu_shell_select_item( GTK_MENU_SHELL( menu ), menuItem );

  if( menuItem != NULL ) {
    gtk_widget_get_child_requisition( menuItem, &requisition );
    _y += requisition.height / 2;
  }

  while( children ) {
    child = children->data;

    if( menuItem == child )
      break;

    if( GTK_WIDGET_VISIBLE( child ) ) {
      gtk_widget_get_child_requisition( child, &requisition );
      _y -= requisition.height;
    }

    children = children->next;
  }

  _y += widget->allocation.y - widget->allocation.height / 2 + 2;

  *x = _x;
  *y = _y;
  *push_in = TRUE;
}

static void icc_button_clicked( IccButton *button, gpointer data )
{
  if( !button->popupMenu || last_changed > button->last_updated) {
    GTimeVal time_val;

    setupMenu( button );

    g_get_current_time( &time_val );
    button->last_updated = time_val.tv_sec;
  }

  if( button->popupMenu && button->maxEntries > 0 ) {
    gtk_menu_popup( GTK_MENU (button->popupMenu), NULL, NULL,
                    icc_button_menu_position, button,
                    0, gtk_get_current_event_time() );
  } else
    icc_button_run_dialog (GTK_WIDGET (button), button);
}

static void
icc_button_run_dialog (GtkWidget *widget,
                       gpointer   data)
{
  IccButton         *button = ICC_BUTTON (data);
  GtkWidget         *label;
  GtkWidget         *hbox;
  GtkListStore      *listStore;
  GtkCellRenderer   *renderer;
  GtkTreeViewColumn *column;

  gint               response;
  gboolean           result = FALSE;

  button->dialog =
    gtk_dialog_new_with_buttons (button->title ? button->title : _("Choose an ICC Profile"),
                                 NULL,
                                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
                                 GTK_STOCK_OK,
                                 GTK_RESPONSE_ACCEPT,
                                 GTK_STOCK_CANCEL,
                                 GTK_RESPONSE_REJECT,
                                 NULL);

  gtk_widget_set_size_request (button->dialog, 480, 320);
  gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (button->dialog)->vbox), 6);

  button->scrolledWindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_container_set_border_width (GTK_CONTAINER (button->scrolledWindow), 6);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (button->scrolledWindow), GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (button->scrolledWindow),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);

  listStore = gtk_list_store_new( N_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING );
  button->treeView = gtk_tree_view_new_with_model( GTK_TREE_MODEL( listStore ) );
  g_signal_connect( G_OBJECT( button->treeView ), "cursor-changed", G_CALLBACK( icc_dialog_selected ), button );
  g_signal_connect( G_OBJECT( button->treeView ), "row-activated", G_CALLBACK( icc_dialog_double_clicked ), button );
  g_object_unref( listStore );

  renderer = gtk_cell_renderer_pixbuf_new();
  column = gtk_tree_view_column_new_with_attributes( "    ", renderer, "pixbuf", COLUMN_ICON, NULL );
  gtk_tree_view_column_set_resizable( column, FALSE );
  gtk_tree_view_append_column( GTK_TREE_VIEW( button->treeView ), column );
  //gtk_tree_view_column_set_sort_column_id( column, COLUMN_ICON );

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes( _("Name"), renderer, "text", COLUMN_NAME, NULL );
  gtk_tree_view_column_set_resizable( column, TRUE );
  gtk_tree_view_append_column( GTK_TREE_VIEW( button->treeView ), column );
  gtk_tree_view_column_set_sort_column_id( column, COLUMN_NAME );

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes( _("Class"), renderer, "text", COLUMN_CLASS, NULL );
  gtk_tree_view_append_column( GTK_TREE_VIEW( button->treeView ), column );
  gtk_tree_view_column_set_resizable( column, TRUE );
  gtk_tree_view_column_set_sort_column_id( column, COLUMN_CLASS );

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes( _("Colorspace"), renderer, "text", COLUMN_COLORSPACE, NULL );
  gtk_tree_view_column_set_resizable( column, TRUE );
  gtk_tree_view_append_column( GTK_TREE_VIEW( button->treeView ), column );
  gtk_tree_view_column_set_sort_column_id( column, COLUMN_COLORSPACE );

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes( _("PCS"), renderer, "text", COLUMN_PCS, NULL );
  gtk_tree_view_column_set_resizable( column, TRUE );
  gtk_tree_view_append_column( GTK_TREE_VIEW( button->treeView ), column );
  gtk_tree_view_column_set_sort_column_id( column, COLUMN_PCS );

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes( _("Path"), renderer, "text", COLUMN_PATH, NULL );
  gtk_tree_view_column_set_resizable( column, TRUE );
  gtk_tree_view_append_column( GTK_TREE_VIEW( button->treeView ), column );
  gtk_tree_view_column_set_sort_column_id( column, COLUMN_PATH );

  gtk_container_add( GTK_CONTAINER( button->scrolledWindow ), button->treeView );
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG( button->dialog )->vbox ), button->scrolledWindow, TRUE, TRUE, 0 );

  // Path entry
  hbox = gtk_hbox_new( FALSE, 6 );
  gtk_container_border_width( GTK_CONTAINER( hbox ), 6 );
  label = gtk_label_new( _("Path:") );
  gtk_box_pack_start( GTK_BOX( hbox ), label, FALSE, FALSE, 0 );
  button->entry = gtk_entry_new();
  gtk_drag_dest_set (button->entry, GTK_DEST_DEFAULT_ALL,
                     targets, 1, GDK_ACTION_COPY);
  g_signal_connect (G_OBJECT (button->entry), "drag_data_received",
                    G_CALLBACK (icc_button_drag_data_received), NULL);
  gtk_box_pack_start( GTK_BOX( hbox ), button->entry, TRUE, TRUE, 0 );
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG( button->dialog )->vbox ), hbox, FALSE, FALSE, 0 );

  setupData( button );

  if( button->path != NULL && strlen( gtk_entry_get_text( GTK_ENTRY( button->entry ) ) ) == 0 ) {
    gchar *path;
    gsize nWritten;
    
    path = g_filename_to_utf8( button->path, -1, NULL, &nWritten, NULL );
    if( path ) {
      gtk_entry_set_text( GTK_ENTRY( button->entry ), path );
      g_free( path );
    }
  }

  gtk_widget_show_all( button->dialog );

  do {
    gchar *path;
    gsize nWritten;

    response = gtk_dialog_run( GTK_DIALOG( button->dialog ) );

    if( response == GTK_RESPONSE_ACCEPT ) {
      path = g_filename_from_utf8( gtk_entry_get_text( GTK_ENTRY( button->entry ) ),
                                   -1, NULL, &nWritten, NULL );
      if (!(result = icc_button_set_filename (button, path, TRUE))) {
        GtkWidget *dialog = gtk_message_dialog_new( GTK_WINDOW( button->dialog ),
                                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    GTK_MESSAGE_WARNING,
                                                    GTK_BUTTONS_CLOSE,
                                                    _("Invalid profile") );
        gtk_message_dialog_format_secondary_markup( GTK_MESSAGE_DIALOG( dialog ),
                                                    _("Please check the filename, device class, and colorspace.") );
        gtk_dialog_run( GTK_DIALOG( dialog ) );
        gtk_widget_destroy( dialog );
      } else
        g_signal_emit( button, iccButtonSignals[CHANGED], 0 );
    }
  } while( response == GTK_RESPONSE_ACCEPT && result == FALSE );

  gtk_widget_destroy( button->dialog );

  button->dialog = NULL;
  button->scrolledWindow = NULL;
  button->treeView = NULL;
  button->entry = NULL;
}
