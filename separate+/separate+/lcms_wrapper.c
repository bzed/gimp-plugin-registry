/**********************************************************************
  lcms_wrapper.c
  Copyright(C) 2010 Y.Yamakawa
**********************************************************************/

#include "lcms_wrapper.h"

#include <locale.h>
#include <glib/gi18n.h>

#if defined __APPLE__ && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_3
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFLocale.h>
#include <CoreFoundation/CFPreferences.h>
#endif


__inline__ cmsHPROFILE
lcms_open_profile (gchar *filename)
{
  cmsHPROFILE profile;

#ifdef G_OS_WIN32
  gchar *_filename = g_win32_locale_filename_from_utf8 (filename);

  profile = cmsOpenProfileFromFile (_filename, "r");

  g_free (_filename);
#else
  profile = cmsOpenProfileFromFile (filename, "r");
#endif

  return profile;
}

static guchar *
lcms_read_raw_tag (cmsHPROFILE     profile,
                   icTagSignature  sig,
                   gsize          *size)
{
  guchar *buf = NULL;

#ifndef USE_LCMS2
  LPLCMSICCPROFILE icc = (LPLCMSICCPROFILE)profile;

  gsize offset;
  gint n;

  if (profile && (n = _cmsSearchTag (profile, sig, FALSE)) >= 0)
    {
      offset = icc->TagOffsets[n];
      *size = icc->TagSizes[n];

      if (icc->Seek (icc, offset))
        return NULL;

      buf = g_try_malloc (*size);
      icc->Read (buf, 1, *size, icc);
    }
#else
  if ((*size = cmsReadRawTag (profile, sig, NULL, 0)))
    {
      buf = g_try_malloc (*size);
      cmsReadRawTag (profile, sig, buf, *size);
    }
#endif

  return buf;
}

gboolean
lcms_get_adapted_whitepoint (cmsHPROFILE  profile,
                             LPcmsCIEXYZ  whitepoint,
                             LPcmsCIEXYZ  adapted_whitepoint)
{
  cmsCIEXYZ w1 = { -1 }, w2 = { -1 };
  gdouble mat[3][3], inv_mat[3][3];
  gboolean has_chad = FALSE;

  *whitepoint = w1;
  *adapted_whitepoint = w2;

  g_return_val_if_fail (profile != NULL, FALSE);

  w1 = lcms_get_whitepoint (profile);

  if (w1.X < 0)
    return FALSE;

#ifndef USE_LCMS2
  MAT3 m;

  if (cmsReadChromaticAdaptationMatrix (&m, profile))
    {
      gint i, j;

      for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
          mat[i][j] = m.v[i].n[j];

      has_chad = TRUE;
    }
#else
  cmsCIEXYZ *m;

  if (m = (cmsCIEXYZ *)cmsReadTag (profile, cmsSigChromaticAdaptationTag))
    {
      mat[0][0] = m[0].X;
      mat[1][0] = m[1].X;
      mat[2][0] = m[2].X;

      mat[0][1] = m[0].Y;
      mat[1][1] = m[1].Y;
      mat[2][1] = m[2].Y;

      mat[0][2] = m[0].Z;
      mat[1][2] = m[1].Z;
      mat[2][2] = m[2].Z;

      has_chad = TRUE;
    }
#endif
  if (has_chad)
    {
      inv_mat[0][0] = mat[1][1] * mat[2][2] - mat[1][2] * mat[2][1];
      inv_mat[1][0] = mat[0][2] * mat[2][1] - mat[0][1] * mat[2][2];
      inv_mat[2][0] = mat[0][1] * mat[1][2] - mat[0][2] * mat[1][1];

      inv_mat[0][1] = mat[1][2] * mat[2][0] - mat[1][0] * mat[2][2];
      inv_mat[1][1] = mat[0][0] * mat[2][2] - mat[0][2] * mat[2][0];
      inv_mat[2][1] = mat[0][2] * mat[1][0] - mat[0][0] * mat[1][2];

      inv_mat[0][2] = mat[1][0] * mat[2][1] - mat[1][1] * mat[2][0];
      inv_mat[1][2] = mat[0][1] * mat[2][0] - mat[0][0] * mat[2][1];
      inv_mat[2][2] = mat[0][0] * mat[1][1] - mat[0][1] * mat[1][0];

      w2.X = inv_mat[0][0] * w1.X + inv_mat[1][0] * w1.Y + inv_mat[2][0] * w1.Z;
      w2.Y = inv_mat[0][1] * w1.X + inv_mat[1][1] * w1.Y + inv_mat[2][1] * w1.Z;
      w2.Z = inv_mat[0][2] * w1.X + inv_mat[1][2] * w1.Y + inv_mat[2][2] * w1.Z;
    }

  *whitepoint = w1;
  *adapted_whitepoint = w2;

  return TRUE;
}

gboolean
lcms_has_lut (cmsHPROFILE profile)
{
  icProfileClassSignature class = cmsGetDeviceClass (profile);

  if ((cmsIsTag (profile, icSigAToB0Tag) || cmsIsTag (profile, icSigAToB1Tag) || cmsIsTag (profile, icSigAToB2Tag)
#ifdef USE_LCMS2
       || cmsIsTag (profile, cmsSigDToB0Tag) || cmsIsTag (profile, cmsSigDToB1Tag) || cmsIsTag (profile, cmsSigDToB2Tag) || cmsIsTag (profile, cmsSigDToB3Tag)
#endif
       ) &&
      (class == icSigInputClass || class == icSigAbstractClass || class == icSigLinkClass ||
       cmsIsTag (profile, icSigBToA0Tag) || cmsIsTag (profile, icSigBToA1Tag) || cmsIsTag (profile, icSigBToA2Tag)
#ifdef USE_LCMS2
       || cmsIsTag (profile, cmsSigBToD0Tag) || cmsIsTag (profile, cmsSigBToD1Tag) || cmsIsTag (profile, cmsSigBToD2Tag) || cmsIsTag (profile, cmsSigBToD3Tag)
#endif
       ))
    return TRUE;
  else
    return FALSE;
}

/***************************************************************
  Get a localized text from the 'mluc' type tag
  That text is already converted to UTF-8 from UTF-16BE
****************************************************************/
static gchar *
readLocalizedText (cmsHPROFILE    profile,
                   icTagSignature sig)
{
  guchar *tag_data;
  gsize offet, size;
  gint n;

  if ((tag_data = lcms_read_raw_tag (profile, sig, &size)))
    {
      gchar *buf = NULL;
      icTagTypeSignature tag_type;

      if (size < 8)
        goto failure;

      tag_type = GUINT32_FROM_BE (*((guint32 *)tag_data));
      size -= 8;

      switch (tag_type)
        {
        case icSigTextDescriptionType: {
          guint32 length;

          if (size < 4) /* 4 = length */
            goto failure;

          length = GUINT32_FROM_BE (*((guint32 *)(tag_data + 8)));
          size -= 4;

          if (length <= 0 || size < length)
            goto failure;

          length = MIN (length - 1, ICC_PROFILE_DESC_MAX);

          buf = g_strndup (tag_data + 12, length);

          return buf; }
        case icSigMultiLocalizedUnicodeType: {
          gchar *utf8String = NULL;

          guint32 nNames, nameRecordSize;
          guchar *nameRecord;
          guint16 lang, region, _lang, _region;
          gchar *locale;

          guint i;

          gsize entryLength = 0, entryOffset = 0, _entryLength = 0, _entryOffset = 0;
          gboolean flag = FALSE;

          if (size < 8) /* 8 = nNames + nRecordSize */
            goto failure;

#ifdef G_OS_WIN32
          locale = g_win32_getlocale ();
#elif defined __APPLE__ && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_3
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

          nNames = GUINT32_FROM_BE (*((guint32 *)(tag_data + 8)));
          nameRecordSize = GUINT32_FROM_BE (*((guint32 *)(tag_data + 12)));
          size -= 8;
          nameRecord = tag_data + 16;

          if (size < nNames * nameRecordSize)
            goto failure;

          // scan name records
          for (i = 0; i < nNames; i++)
            {
              lang = GUINT16_FROM_BE (*((guint16 *)nameRecord));
              nameRecord += 2;
              region = GUINT16_FROM_BE (*((guint16 *)nameRecord));
              nameRecord += 2;

              _entryLength = GUINT32_FROM_BE (*((guint32 *)nameRecord));
              nameRecord += 4;
              _entryOffset = GUINT32_FROM_BE (*((guint32 *)nameRecord));
              nameRecord += 4;

              size -= 12 + _entryLength;

              if (strncasecmp ((gchar *)(&lang), (gchar *)(&_lang), 2) == 0)
                {
                  if (!flag || strncasecmp ((gchar *)(&region), (gchar *)(&_region), 2) == 0)
                    {
                      flag = TRUE;
                      entryLength = _entryLength;
                      entryOffset = _entryOffset;
                    }
                }
              else if (i == 0)
                {
                  entryLength = _entryLength;
                  entryOffset = _entryOffset;
                }
            }

          if (entryLength <= 0 || size + entryLength < 0 )
            goto failure;

          entryLength = MIN (entryLength, ICC_PROFILE_DESC_MAX);

          buf = g_strnfill (entryLength, 0);
          memcpy (buf, tag_data + entryOffset, entryLength);
          utf8String = g_convert (buf, entryLength, "UTF-8", "UTF-16BE", NULL, &entryLength, NULL);
          g_free (buf);

          return  utf8String; }
        default:
          return NULL;
        }
    }
  else
    return NULL;

failure:
  g_free (tag_data);
  return NULL;
}

// Get a localized name of the profile.
// The returned value should be freed when no longer needed.

gchar *
lcms_get_profile_desc (cmsHPROFILE profile)
{
  static gchar *utf8String = NULL;

  if (!profile)
    return NULL;

  if (!(utf8String = readLocalizedText (profile, icSigProfileDescriptionMLTag)))
    utf8String = readLocalizedText (profile, icSigProfileDescriptionTag);

  if (utf8String)
    {
      gchar *errorChar, *tmp;

      if (!g_utf8_validate (utf8String, -1, (const gchar **)&errorChar))
        {
          gsize bytes_read, bytes_write;

          if (!(tmp = g_locale_to_utf8 (utf8String, -1, &bytes_read, &bytes_write, NULL)))
            {
              *errorChar = '\0'; /* avoid crashing with sorting profiles by descriptions */
              tmp = g_strdup_printf (_("%s(broken text)..."), utf8String);
              g_free (utf8String);
              utf8String = tmp;
            }
          else
            {
              g_free (utf8String);
              utf8String = tmp;
            }
        }
    }
  else
    utf8String = g_strdup (_("(no name)"));

  return utf8String;
}
