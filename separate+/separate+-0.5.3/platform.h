#ifndef SEPARATE_PLATFORM_H
#define SEPARATE_PLATFORM_H

#if defined G_OS_WIN32

#define DEFAULT_RGB_PROFILE g_build_filename( g_getenv( "SYSTEMROOT" ), "\\system32\\spool\\drivers\\color\\sRGB Color Space Profile.icm", NULL )
#define DEFAULT_CMYK_PROFILE g_build_filename( g_getenv( "SYSTEMROOT" ), "\\system32\\spool\\drivers\\color\\USWebCoatedSWOP.icc", NULL )
/* For Japanese users */
//#define DEFAULT_CMYK_PROFILE g_build_filename( g_getenv( "SYSTEMROOT" ), "\\system32\\spool\\drivers\\color\\JapanColor2001Coated.icc", NULL )

#elif defined MACOSX 

#define DEFAULT_RGB_PROFILE g_strdup( "/System/Library/ColorSync/Profiles/sRGB Profile.icc" )
#define DEFAULT_CMYK_PROFILE g_strdup( "/System/Library/ColorSync/Profiles/Generic CMYK Profile.icc" )

#else

#define DEFAULT_RGB_PROFILE g_strdup( "/usr/share/color/icc/sRGB Color Space Profile.icm" )
#define DEFAULT_CMYK_PROFILE g_strdup( "/usr/share/color/icc/USWebCoatedSWOP.icc" )

#endif


#if ! (GIMP_MAJOR_VERSION > 2 || (GIMP_MAJOR_VERSION == 2 && GIMP_MINOR_VERSION >= 4))
#define USE_ICC_BUTTON
#endif


#define DEBUG(x) fprintf(stdout,x); fflush(stdout)

#endif
