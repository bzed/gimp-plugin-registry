/**********************************************************************
  lcms_wrapper.h
  Copyright(C) 2010 Y.Yamakawa
**********************************************************************/

#ifndef LCMS_WRAPPER_H
#define LCMS_WRAPPER_H

#include <glib.h>

#ifndef USE_LCMS2

#include "lcms.h"

// The private tag used in the ICC profiles provided by Apple,Inc.
enum
{
  icSigProfileDescriptionMLTag   = 0x6473636dL  /* 'dscm' */
};

static __inline__ void
lcms_error_setup (void)
{
  cmsErrorAction (LCMS_ERROR_IGNORE);
}

static __inline__ guint32
lcms_get_encoded_icc_version (cmsHPROFILE profile)
{
  return cmsGetProfileICCversion (profile);
}

static __inline__ gboolean
lcms_is_matrix_shaper (cmsHPROFILE profile)
{
  return _cmsIsMatrixShaper (profile);
}

static __inline__ cmsCIEXYZ
lcms_get_whitepoint (cmsHPROFILE profile)
{
  cmsCIEXYZ xyz = { -1 };

  cmsTakeMediaWhitePoint (&xyz, profile);

  return xyz;
}

static __inline__ cmsCIEXYZTRIPLE
lcms_get_colorants (cmsHPROFILE profile)
{
  cmsCIEXYZTRIPLE colorants = { -1 };

  cmsTakeColorants (&colorants, profile);

  return colorants;
}

static __inline__ LPGAMMATABLE
lcms_get_gamma (cmsHPROFILE    profile,
                icTagSignature sig)
{
  return cmsReadICCGamma (profile, sig);
}

static __inline__ double
lcms_estimate_gamma (LPGAMMATABLE gamma)
{
  cmsEstimateGamma (gamma);
}

static __inline__ void
lcms_free_gamma (LPGAMMATABLE gamma)
{
  cmsFreeGamma (gamma);
}

#else

#include "lcms2.h"

#ifdef G_OS_WIN32
#include <windows.h>
#else
#define DWORD guint32
#endif

typedef enum {
    icSigXYZData               = cmsSigXYZData,
    icSigLabData               = cmsSigLabData,
    icSigLuvData               = cmsSigLuvData,
    icSigYCbCrData             = cmsSigYCbCrData,
    icSigYxyData               = cmsSigYxyData,
    icSigRgbData               = cmsSigRgbData,
    icSigGrayData              = cmsSigGrayData,
    icSigHsvData               = cmsSigHsvData,
    icSigHlsData               = cmsSigHlsData,
    icSigCmykData              = cmsSigCmykData,
    icSigCmyData               = cmsSigCmyData,
    icSig2colorData            = cmsSig2colorData,
    icSig3colorData            = cmsSig3colorData,
    icSig4colorData            = cmsSig4colorData,
    icSig5colorData            = cmsSig5colorData,
    icSig6colorData            = cmsSig6colorData,
    icSig7colorData            = cmsSig7colorData,
    icSig8colorData            = cmsSig8colorData,
    icSig9colorData            = cmsSig9colorData,
    icSig10colorData           = cmsSig10colorData,
    icSig11colorData           = cmsSig11colorData,
    icSig12colorData           = cmsSig12colorData,
    icSig13colorData           = cmsSig13colorData,
    icSig14colorData           = cmsSig14colorData,
    icSig15colorData           = cmsSig15colorData,
    icMaxEnumData              = 0xFFFFFFFFL
} icColorSpaceSignature;

typedef enum {
  icSigInputClass              = cmsSigInputClass,
  icSigDisplayClass            = cmsSigDisplayClass,
  icSigOutputClass             = cmsSigOutputClass,
  icSigLinkClass               = cmsSigLinkClass,
  icSigAbstractClass           = cmsSigAbstractClass,
  icSigColorSpaceClass         = cmsSigColorSpaceClass,
  icSigNamedColorClass         = cmsSigNamedColorClass,
  icMaxEnumClass               = 0xFFFFFFFFL
} icProfileClassSignature;

typedef enum {
  icSigAToB0Tag                = cmsSigAToB0Tag,
  icSigAToB1Tag                = cmsSigAToB1Tag,
  icSigAToB2Tag                = cmsSigAToB2Tag,
  icSigBlueColorantTag         = cmsSigBlueColorantTag,
  icSigBlueTRCTag              = cmsSigBlueTRCTag,
  icSigBToA0Tag                = cmsSigBToA0Tag,
  icSigBToA1Tag                = cmsSigBToA1Tag,
  icSigBToA2Tag                = cmsSigBToA2Tag,
  icSigCalibrationDateTimeTag  = cmsSigCalibrationDateTimeTag,
  icSigCharTargetTag           = cmsSigCharTargetTag,
  icSigCopyrightTag            = cmsSigCopyrightTag,
  icSigCrdInfoTag              = cmsSigCrdInfoTag,
  icSigDeviceMfgDescTag        = cmsSigDeviceMfgDescTag,
  icSigDeviceModelDescTag      = cmsSigDeviceModelDescTag,
  icSigGamutTag                = cmsSigGamutTag,
  icSigGrayTRCTag              = cmsSigGrayTRCTag,
  icSigGreenColorantTag        = cmsSigGreenColorantTag,
  icSigGreenTRCTag             = cmsSigGreenTRCTag,
  icSigLuminanceTag            = cmsSigLuminanceTag,
  icSigMeasurementTag          = cmsSigMeasurementTag,
  icSigMediaBlackPointTag      = cmsSigMediaBlackPointTag,
  icSigMediaWhitePointTag      = cmsSigMediaWhitePointTag,
  icSigNamedColorTag           = cmsSigNamedColorTag,
  icSigNamedColor2Tag          = cmsSigNamedColor2Tag,
  icSigPreview0Tag             = cmsSigPreview0Tag,
  icSigPreview1Tag             = cmsSigPreview1Tag,
  icSigPreview2Tag             = cmsSigPreview2Tag,
  icSigProfileDescriptionTag   = cmsSigProfileDescriptionTag,
  icSigProfileSequenceDescTag  = cmsSigProfileSequenceDescTag,
  icSigPs2CRD0Tag              = cmsSigPs2CRD0Tag,
  icSigPs2CRD1Tag              = cmsSigPs2CRD1Tag,
  icSigPs2CRD2Tag              = cmsSigPs2CRD2Tag,
  icSigPs2CRD3Tag              = cmsSigPs2CRD3Tag,
  icSigPs2CSATag               = cmsSigPs2CSATag,
  icSigPs2RenderingIntentTag   = cmsSigPs2RenderingIntentTag,
  icSigRedColorantTag          = cmsSigRedColorantTag,
  icSigRedTRCTag               = cmsSigRedTRCTag,
  icSigScreeningDescTag        = cmsSigScreeningDescTag,
  icSigScreeningTag            = cmsSigScreeningTag,
  icSigTechnologyTag           = cmsSigTechnologyTag,
  icSigUcrBgTag                = cmsSigUcrBgTag,
  icSigViewingCondDescTag      = cmsSigViewingCondDescTag,
  icSigViewingConditionsTag    = cmsSigViewingConditionsTag,
  icSigProfileDescriptionMLTag = 0x6473636dL, /* 'dscm' */
  icMaxEnumTag                 = 0xFFFFFFFFL 
} icTagSignature;

typedef enum {
  icSigCurveType                 = cmsSigCurveType,
  icSigDataType                  = cmsSigDataType,
  icSigDateTimeType              = cmsSigDateTimeType,
  icSigLut16Type                 = cmsSigLut16Type,
  icSigLut8Type                  = cmsSigLut8Type,
  icSigMeasurementType           = cmsSigMeasurementType,
  icSigNamedColorType            = cmsSigNamedColorType,
  icSigProfileSequenceDescType   = cmsSigProfileSequenceDescType,
  icSigS15Fixed16ArrayType       = cmsSigS15Fixed16ArrayType,
  icSigScreeningType             = cmsSigScreeningType,
  icSigSignatureType             = cmsSigSignatureType,
  icSigTextType                  = cmsSigTextType,
  icSigTextDescriptionType       = cmsSigTextDescriptionType,
  icSigU16Fixed16ArrayType       = cmsSigU16Fixed16ArrayType,
  icSigUcrBgType                 = cmsSigUcrBgType,
  icSigUInt16ArrayType           = cmsSigUInt16ArrayType,
  icSigUInt32ArrayType           = cmsSigUInt32ArrayType,
  icSigUInt64ArrayType           = cmsSigUInt64ArrayType,
  icSigUInt8ArrayType            = cmsSigUInt8ArrayType,
  icSigViewingConditionsType     = cmsSigViewingConditionsType,
  icSigXYZType                   = cmsSigXYZType,
  icSigNamedColor2Type           = cmsSigNamedColor2Type,
  icSigCrdInfoType               = cmsSigCrdInfoType,
  icSigMultiLocalizedUnicodeType = cmsSigMultiLocalizedUnicodeType,
  icMaxEnumType                  = 0xFFFFFFFFL 
} icTagTypeSignature;

typedef cmsICCHeader icHeader;
typedef cmsToneCurve *LPGAMMATABLE;
typedef cmsCIExyY *LPcmsCIExyY;
typedef cmsCIEXYZ *LPcmsCIEXYZ;
typedef cmsCIELab *LPcmsCIELab;

static __inline__ void
lcms_error_setup (void)
{
  cmsSetLogErrorHandler (NULL);
}

static __inline__ guint32
lcms_get_encoded_icc_version (cmsHPROFILE profile)
{
  return cmsGetEncodedICCversion (profile);
}

static __inline__ gboolean
lcms_is_matrix_shaper (cmsHPROFILE profile)
{
  return cmsIsMatrixShaper (profile);
}

static __inline__ cmsCIEXYZ
lcms_get_whitepoint (cmsHPROFILE profile)
{
  cmsCIEXYZ *_xyz, xyz = { -1 };

  _xyz = (cmsCIEXYZ *)cmsReadTag (profile, cmsSigMediaWhitePointTag);

  return _xyz ? *_xyz : xyz;
}

static __inline__ cmsCIEXYZTRIPLE
lcms_get_colorants (cmsHPROFILE profile)
{
  cmsCIEXYZTRIPLE colorants = { -1 };

  if (cmsIsTag (profile, cmsSigRedColorantTag))
    colorants.Red = *((cmsCIEXYZ *)cmsReadTag (profile, cmsSigRedColorantTag));
  if (cmsIsTag (profile, cmsSigGreenColorantTag))
    colorants.Green = *((cmsCIEXYZ *)cmsReadTag (profile, cmsSigGreenColorantTag));
  if (cmsIsTag (profile, cmsSigBlueColorantTag))
    colorants.Blue = *((cmsCIEXYZ *)cmsReadTag (profile, cmsSigBlueColorantTag));

  return colorants;
}

static __inline__ LPGAMMATABLE
lcms_get_gamma (cmsHPROFILE    profile,
                icTagSignature sig)
{
  cmsToneCurve *curve;

  curve = cmsReadTag (profile, sig);

  if (curve)
    return cmsDupToneCurve (curve);
  else
    return NULL;
}

static __inline__ double
lcms_estimate_gamma (LPGAMMATABLE gamma)
{
  cmsEstimateGamma (gamma, 0.01);
}

static __inline__ void
lcms_free_gamma (LPGAMMATABLE gamma)
{
  cmsFreeToneCurve (gamma);
}

#endif

#define ICC_PROFILE_DESC_MAX 2048

cmsHPROFILE  lcms_open_profile           (char        *filename);
gboolean     lcms_get_adapted_whitepoint (cmsHPROFILE  profile,
                                          LPcmsCIEXYZ  whitepoint,
                                          LPcmsCIEXYZ  adapted_whitepoint);
gboolean     lcms_has_lut                (cmsHPROFILE  profile);
gchar       *lcms_get_profile_desc       (cmsHPROFILE  profile);

#endif /* LCMS_WRAPPER_H */
