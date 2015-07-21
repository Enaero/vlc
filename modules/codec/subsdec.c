/*****************************************************************************
 * subsdec.c : text subtitle decoder
 *****************************************************************************
 * Copyright (C) 2000-2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Samuel Hocevar <sam@zoy.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Bernie Purcell <bitmap@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <limits.h>
#include <errno.h>
#include <ctype.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_charset.h>
#include <vlc_xml.h>

#include "substext.h"

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static const char *const ppsz_encodings[] = {
    "",
    "system",
    "UTF-8",
    "UTF-16",
    "UTF-16BE",
    "UTF-16LE",
    "GB18030",
    "ISO-8859-15",
    "Windows-1252",
    "IBM850",
    "ISO-8859-2",
    "Windows-1250",
    "ISO-8859-3",
    "ISO-8859-10",
    "Windows-1251",
    "KOI8-R",
    "KOI8-U",
    "ISO-8859-6",
    "Windows-1256",
    "ISO-8859-7",
    "Windows-1253",
    "ISO-8859-8",
    "Windows-1255",
    "ISO-8859-9",
    "Windows-1254",
    "ISO-8859-11",
    "Windows-874",
    "ISO-8859-13",
    "Windows-1257",
    "ISO-8859-14",
    "ISO-8859-16",
    "ISO-2022-CN-EXT",
    "EUC-CN",
    "ISO-2022-JP-2",
    "EUC-JP",
    "Shift_JIS",
    "CP949",
    "ISO-2022-KR",
    "Big5",
    "ISO-2022-TW",
    "Big5-HKSCS",
    "VISCII",
    "Windows-1258",
};

static const char *const ppsz_encoding_names[] = {
    /* xgettext:
      The character encoding name in parenthesis corresponds to that used for
      the GetACP translation. "Windows-1252" applies to Western European
      languages using the Latin alphabet. */
    N_("Default (Windows-1252)"),
    N_("System codeset"),
    N_("Universal (UTF-8)"),
    N_("Universal (UTF-16)"),
    N_("Universal (big endian UTF-16)"),
    N_("Universal (little endian UTF-16)"),
    N_("Universal, Chinese (GB18030)"),

  /* ISO 8859 and the likes */
    /* 1 */
    N_("Western European (Latin-9)"), /* mostly superset of Latin-1 */
    N_("Western European (Windows-1252)"),
    N_("Western European (IBM 00850)"),
    /* 2 */
    N_("Eastern European (Latin-2)"),
    N_("Eastern European (Windows-1250)"),
    /* 3 */
    N_("Esperanto (Latin-3)"),
    /* 4 */
    N_("Nordic (Latin-6)"), /* Latin 6 supersedes Latin 4 */
    /* 5 */
    N_("Cyrillic (Windows-1251)"), /* ISO 8859-5 is not practically used */
    N_("Russian (KOI8-R)"),
    N_("Ukrainian (KOI8-U)"),
    /* 6 */
    N_("Arabic (ISO 8859-6)"),
    N_("Arabic (Windows-1256)"),
    /* 7 */
    N_("Greek (ISO 8859-7)"),
    N_("Greek (Windows-1253)"),
    /* 8 */
    N_("Hebrew (ISO 8859-8)"),
    N_("Hebrew (Windows-1255)"),
    /* 9 */
    N_("Turkish (ISO 8859-9)"),
    N_("Turkish (Windows-1254)"),
    /* 10 -> 4 */
    /* 11 */
    N_("Thai (TIS 620-2533/ISO 8859-11)"),
    N_("Thai (Windows-874)"),
    /* 13 */
    N_("Baltic (Latin-7)"),
    N_("Baltic (Windows-1257)"),
    /* 12 -> /dev/null */
    /* 14 */
    N_("Celtic (Latin-8)"),
    /* 15 -> 1 */
    /* 16 */
    N_("South-Eastern European (Latin-10)"),
  /* CJK families */
    N_("Simplified Chinese (ISO-2022-CN-EXT)"),
    N_("Simplified Chinese Unix (EUC-CN)"),
    N_("Japanese (7-bits JIS/ISO-2022-JP-2)"),
    N_("Japanese Unix (EUC-JP)"),
    N_("Japanese (Shift JIS)"),
    N_("Korean (EUC-KR/CP949)"),
    N_("Korean (ISO-2022-KR)"),
    N_("Traditional Chinese (Big5)"),
    N_("Traditional Chinese Unix (EUC-TW)"),
    N_("Hong-Kong Supplementary (HKSCS)"),
  /* Other */
    N_("Vietnamese (VISCII)"),
    N_("Vietnamese (Windows-1258)"),
};

static const struct {
    const char *psz_name;
    uint32_t   i_value;
} p_html_colors[] = {
    /* Official html colors */
    { "Aqua",    0x00FFFF },
    { "Black",   0x000000 },
    { "Blue",    0x0000FF },
    { "Fuchsia", 0xFF00FF },
    { "Gray",    0x808080 },
    { "Green",   0x008000 },
    { "Lime",    0x00FF00 },
    { "Maroon",  0x800000 },
    { "Navy",    0x000080 },
    { "Olive",   0x808000 },
    { "Purple",  0x800080 },
    { "Red",     0xFF0000 },
    { "Silver",  0xC0C0C0 },
    { "Teal",    0x008080 },
    { "White",   0xFFFFFF },
    { "Yellow",  0xFFFF00 },

    /* Common ones */
    { "AliceBlue", 0xF0F8FF },
    { "AntiqueWhite", 0xFAEBD7 },
    { "Aqua", 0x00FFFF },
    { "Aquamarine", 0x7FFFD4 },
    { "Azure", 0xF0FFFF },
    { "Beige", 0xF5F5DC },
    { "Bisque", 0xFFE4C4 },
    { "Black", 0x000000 },
    { "BlanchedAlmond", 0xFFEBCD },
    { "Blue", 0x0000FF },
    { "BlueViolet", 0x8A2BE2 },
    { "Brown", 0xA52A2A },
    { "BurlyWood", 0xDEB887 },
    { "CadetBlue", 0x5F9EA0 },
    { "Chartreuse", 0x7FFF00 },
    { "Chocolate", 0xD2691E },
    { "Coral", 0xFF7F50 },
    { "CornflowerBlue", 0x6495ED },
    { "Cornsilk", 0xFFF8DC },
    { "Crimson", 0xDC143C },
    { "Cyan", 0x00FFFF },
    { "DarkBlue", 0x00008B },
    { "DarkCyan", 0x008B8B },
    { "DarkGoldenRod", 0xB8860B },
    { "DarkGray", 0xA9A9A9 },
    { "DarkGrey", 0xA9A9A9 },
    { "DarkGreen", 0x006400 },
    { "DarkKhaki", 0xBDB76B },
    { "DarkMagenta", 0x8B008B },
    { "DarkOliveGreen", 0x556B2F },
    { "Darkorange", 0xFF8C00 },
    { "DarkOrchid", 0x9932CC },
    { "DarkRed", 0x8B0000 },
    { "DarkSalmon", 0xE9967A },
    { "DarkSeaGreen", 0x8FBC8F },
    { "DarkSlateBlue", 0x483D8B },
    { "DarkSlateGray", 0x2F4F4F },
    { "DarkSlateGrey", 0x2F4F4F },
    { "DarkTurquoise", 0x00CED1 },
    { "DarkViolet", 0x9400D3 },
    { "DeepPink", 0xFF1493 },
    { "DeepSkyBlue", 0x00BFFF },
    { "DimGray", 0x696969 },
    { "DimGrey", 0x696969 },
    { "DodgerBlue", 0x1E90FF },
    { "FireBrick", 0xB22222 },
    { "FloralWhite", 0xFFFAF0 },
    { "ForestGreen", 0x228B22 },
    { "Fuchsia", 0xFF00FF },
    { "Gainsboro", 0xDCDCDC },
    { "GhostWhite", 0xF8F8FF },
    { "Gold", 0xFFD700 },
    { "GoldenRod", 0xDAA520 },
    { "Gray", 0x808080 },
    { "Grey", 0x808080 },
    { "Green", 0x008000 },
    { "GreenYellow", 0xADFF2F },
    { "HoneyDew", 0xF0FFF0 },
    { "HotPink", 0xFF69B4 },
    { "IndianRed", 0xCD5C5C },
    { "Indigo", 0x4B0082 },
    { "Ivory", 0xFFFFF0 },
    { "Khaki", 0xF0E68C },
    { "Lavender", 0xE6E6FA },
    { "LavenderBlush", 0xFFF0F5 },
    { "LawnGreen", 0x7CFC00 },
    { "LemonChiffon", 0xFFFACD },
    { "LightBlue", 0xADD8E6 },
    { "LightCoral", 0xF08080 },
    { "LightCyan", 0xE0FFFF },
    { "LightGoldenRodYellow", 0xFAFAD2 },
    { "LightGray", 0xD3D3D3 },
    { "LightGrey", 0xD3D3D3 },
    { "LightGreen", 0x90EE90 },
    { "LightPink", 0xFFB6C1 },
    { "LightSalmon", 0xFFA07A },
    { "LightSeaGreen", 0x20B2AA },
    { "LightSkyBlue", 0x87CEFA },
    { "LightSlateGray", 0x778899 },
    { "LightSlateGrey", 0x778899 },
    { "LightSteelBlue", 0xB0C4DE },
    { "LightYellow", 0xFFFFE0 },
    { "Lime", 0x00FF00 },
    { "LimeGreen", 0x32CD32 },
    { "Linen", 0xFAF0E6 },
    { "Magenta", 0xFF00FF },
    { "Maroon", 0x800000 },
    { "MediumAquaMarine", 0x66CDAA },
    { "MediumBlue", 0x0000CD },
    { "MediumOrchid", 0xBA55D3 },
    { "MediumPurple", 0x9370D8 },
    { "MediumSeaGreen", 0x3CB371 },
    { "MediumSlateBlue", 0x7B68EE },
    { "MediumSpringGreen", 0x00FA9A },
    { "MediumTurquoise", 0x48D1CC },
    { "MediumVioletRed", 0xC71585 },
    { "MidnightBlue", 0x191970 },
    { "MintCream", 0xF5FFFA },
    { "MistyRose", 0xFFE4E1 },
    { "Moccasin", 0xFFE4B5 },
    { "NavajoWhite", 0xFFDEAD },
    { "Navy", 0x000080 },
    { "OldLace", 0xFDF5E6 },
    { "Olive", 0x808000 },
    { "OliveDrab", 0x6B8E23 },
    { "Orange", 0xFFA500 },
    { "OrangeRed", 0xFF4500 },
    { "Orchid", 0xDA70D6 },
    { "PaleGoldenRod", 0xEEE8AA },
    { "PaleGreen", 0x98FB98 },
    { "PaleTurquoise", 0xAFEEEE },
    { "PaleVioletRed", 0xD87093 },
    { "PapayaWhip", 0xFFEFD5 },
    { "PeachPuff", 0xFFDAB9 },
    { "Peru", 0xCD853F },
    { "Pink", 0xFFC0CB },
    { "Plum", 0xDDA0DD },
    { "PowderBlue", 0xB0E0E6 },
    { "Purple", 0x800080 },
    { "Red", 0xFF0000 },
    { "RosyBrown", 0xBC8F8F },
    { "RoyalBlue", 0x4169E1 },
    { "SaddleBrown", 0x8B4513 },
    { "Salmon", 0xFA8072 },
    { "SandyBrown", 0xF4A460 },
    { "SeaGreen", 0x2E8B57 },
    { "SeaShell", 0xFFF5EE },
    { "Sienna", 0xA0522D },
    { "Silver", 0xC0C0C0 },
    { "SkyBlue", 0x87CEEB },
    { "SlateBlue", 0x6A5ACD },
    { "SlateGray", 0x708090 },
    { "SlateGrey", 0x708090 },
    { "Snow", 0xFFFAFA },
    { "SpringGreen", 0x00FF7F },
    { "SteelBlue", 0x4682B4 },
    { "Tan", 0xD2B48C },
    { "Teal", 0x008080 },
    { "Thistle", 0xD8BFD8 },
    { "Tomato", 0xFF6347 },
    { "Turquoise", 0x40E0D0 },
    { "Violet", 0xEE82EE },
    { "Wheat", 0xF5DEB3 },
    { "White", 0xFFFFFF },
    { "WhiteSmoke", 0xF5F5F5 },
    { "Yellow", 0xFFFF00 },
    { "YellowGreen", 0x9ACD32 },

    { NULL, 0 }
};


static const int  pi_justification[] = { 0, 1, 2 };
static const char *const ppsz_justification_text[] = {
    N_("Center"),N_("Left"),N_("Right")};

#define ENCODING_TEXT N_("Subtitle text encoding")
#define ENCODING_LONGTEXT N_("Set the encoding used in text subtitles")
#define ALIGN_TEXT N_("Subtitle justification")
#define ALIGN_LONGTEXT N_("Set the justification of subtitles")
#define AUTODETECT_UTF8_TEXT N_("UTF-8 subtitle autodetection")
#define AUTODETECT_UTF8_LONGTEXT N_("This enables automatic detection of " \
            "UTF-8 encoding within subtitle files.")
#define FORMAT_TEXT N_("Formatted Subtitles")
#define FORMAT_LONGTEXT N_("Some subtitle formats allow for text formatting. " \
 "VLC partly implements this, but you can choose to disable all formatting.")

static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("Subtitles"))
    set_description( N_("Text subtitle decoder") )
    set_capability( "decoder", 50 )
    set_callbacks( OpenDecoder, CloseDecoder )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )

    add_integer( "subsdec-align", 0, ALIGN_TEXT, ALIGN_LONGTEXT,
                 false )
        change_integer_list( pi_justification, ppsz_justification_text )
    add_string( "subsdec-encoding", "",
                ENCODING_TEXT, ENCODING_LONGTEXT, false )
        change_string_list( ppsz_encodings, ppsz_encoding_names )
    add_bool( "subsdec-autodetect-utf8", true,
              AUTODETECT_UTF8_TEXT, AUTODETECT_UTF8_LONGTEXT, false )
    add_bool( "subsdec-formatted", true, FORMAT_TEXT, FORMAT_LONGTEXT,
                 false )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#define NO_BREAKING_SPACE  "&#160;"

struct decoder_sys_t
{
    int                 i_align;          /* Subtitles alignment on the vout */

    vlc_iconv_t         iconv_handle;            /* handle to iconv instance */
    bool                b_autodetect_utf8;
};


static subpicture_t   *DecodeBlock   ( decoder_t *, block_t ** );
static subpicture_t   *ParseText     ( decoder_t *, block_t * );
static text_segment_t *ParseSubtitles(int *pi_align, const char * );

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to chose.
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    switch( p_dec->fmt_in.i_codec )
    {
        case VLC_CODEC_SUBT:
        case VLC_CODEC_ITU_T140:
            break;
        default:
            return VLC_EGENERIC;
    }

    p_dec->pf_decode_sub = DecodeBlock;
    p_dec->fmt_out.i_cat = SPU_ES;
    p_dec->fmt_out.i_codec = 0;

    /* Allocate the memory needed to store the decoder's structure */
    p_dec->p_sys = p_sys = calloc( 1, sizeof( *p_sys ) );
    if( p_sys == NULL )
        return VLC_ENOMEM;

    /* init of p_sys */
    p_sys->i_align = 0;
    p_sys->iconv_handle = (vlc_iconv_t)-1;
    p_sys->b_autodetect_utf8 = false;

    const char *encoding;
    char *var = NULL;

    /* First try demux-specified encoding */
    if( p_dec->fmt_in.i_codec == VLC_CODEC_ITU_T140 )
        encoding = "UTF-8"; /* IUT T.140 is always using UTF-8 */
    else
    if( p_dec->fmt_in.subs.psz_encoding && *p_dec->fmt_in.subs.psz_encoding )
    {
        encoding = p_dec->fmt_in.subs.psz_encoding;
        msg_Dbg (p_dec, "trying demuxer-specified character encoding: %s",
                 encoding);
    }
    else
    {
        /* Second, try configured encoding */
        if ((var = var_InheritString (p_dec, "subsdec-encoding")) != NULL)
        {
            msg_Dbg (p_dec, "trying configured character encoding: %s", var);
            if (!strcmp (var, "system"))
            {
                free (var);
                var = NULL;
                encoding = "";
                /* ^ iconv() treats "" as nl_langinfo(CODESET) */
            }
            else
                encoding = var;
        }
        else
        /* Third, try "local" encoding */
        {
        /* xgettext:
           The Windows ANSI code page most commonly used for this language.
           VLC uses this as a guess of the subtitle files character set
           (if UTF-8 and UTF-16 autodetection fails).
           Western European languages normally use "CP1252", which is a
           Microsoft-variant of ISO 8859-1. That suits the Latin alphabet.
           Other scripts use other code pages.

           This MUST be a valid iconv character set. If unsure, please refer
           the VideoLAN translators mailing list. */
            encoding = vlc_pgettext("GetACP", "CP1252");
            msg_Dbg (p_dec, "trying default character encoding: %s", encoding);
        }

        /* Check UTF-8 autodetection */
        if (var_InheritBool (p_dec, "subsdec-autodetect-utf8"))
        {
            msg_Dbg (p_dec, "using automatic UTF-8 detection");
            p_sys->b_autodetect_utf8 = true;
        }
    }

    if (strcasecmp (encoding, "UTF-8") && strcasecmp (encoding, "utf8"))
    {
        p_sys->iconv_handle = vlc_iconv_open ("UTF-8", encoding);
        if (p_sys->iconv_handle == (vlc_iconv_t)(-1))
            msg_Err (p_dec, "cannot convert from %s: %s", encoding,
                     vlc_strerror_c(errno));
    }
    free (var);

    p_sys->i_align = var_InheritInteger( p_dec, "subsdec-align" );

    return VLC_SUCCESS;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with complete subtitles units.
 ****************************************************************************/
static subpicture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    subpicture_t *p_spu;
    block_t *p_block;

    if( !pp_block || *pp_block == NULL )
        return NULL;

    p_block = *pp_block;
    if( p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED) )
    {
        block_Release( p_block );
        return NULL;
    }

    p_spu = ParseText( p_dec, p_block );

    block_Release( p_block );
    *pp_block = NULL;

    return p_spu;
}

/*****************************************************************************
 * CloseDecoder: clean up the decoder
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->iconv_handle != (vlc_iconv_t)-1 )
        vlc_iconv_close( p_sys->iconv_handle );

    free( p_sys );
}

/*****************************************************************************
 * ParseText: parse an text subtitle packet and send it to the video output
 *****************************************************************************/
static subpicture_t *ParseText( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    subpicture_t *p_spu = NULL;
    char *psz_subtitle = NULL;

    /* We cannot display a subpicture with no date */
    if( p_block->i_pts <= VLC_TS_INVALID )
    {
        msg_Warn( p_dec, "subtitle without a date" );
        return NULL;
    }

    /* Check validity of packet data */
    /* An "empty" line containing only \0 can be used to force
       and ephemer picture from the screen */
    if( p_block->i_buffer < 1 )
    {
        msg_Warn( p_dec, "no subtitle data" );
        return NULL;
    }

    /* Should be resiliant against bad subtitles */
    psz_subtitle = malloc( p_block->i_buffer + 1 );
    if( psz_subtitle == NULL )
        return NULL;
    memcpy( psz_subtitle, p_block->p_buffer, p_block->i_buffer );
    psz_subtitle[p_block->i_buffer] = '\0';

    if( p_sys->iconv_handle == (vlc_iconv_t)-1 )
    {
        if (EnsureUTF8( psz_subtitle ) == NULL)
        {
            msg_Err( p_dec, "failed to convert subtitle encoding.\n"
                     "Try manually setting a character-encoding "
                     "before you open the file." );
        }
    }
    else
    {

        if( p_sys->b_autodetect_utf8 )
        {
            if( IsUTF8( psz_subtitle ) == NULL )
            {
                msg_Dbg( p_dec, "invalid UTF-8 sequence: "
                         "disabling UTF-8 subtitles autodetection" );
                p_sys->b_autodetect_utf8 = false;
            }
        }

        if( !p_sys->b_autodetect_utf8 )
        {
            size_t inbytes_left = strlen( psz_subtitle );
            size_t outbytes_left = 6 * inbytes_left;
            char *psz_new_subtitle = xmalloc( outbytes_left + 1 );
            char *psz_convert_buffer_out = psz_new_subtitle;
            const char *psz_convert_buffer_in = psz_subtitle;

            size_t ret = vlc_iconv( p_sys->iconv_handle,
                                    &psz_convert_buffer_in, &inbytes_left,
                                    &psz_convert_buffer_out, &outbytes_left );

            *psz_convert_buffer_out++ = '\0';
            free( psz_subtitle );

            if( ( ret == (size_t)(-1) ) || inbytes_left )
            {
                free( psz_new_subtitle );
                msg_Err( p_dec, "failed to convert subtitle encoding.\n"
                        "Try manually setting a character-encoding "
                                "before you open the file." );
                return NULL;
            }

            psz_subtitle = realloc( psz_new_subtitle,
                                    psz_convert_buffer_out - psz_new_subtitle );
            if( !psz_subtitle )
                psz_subtitle = psz_new_subtitle;
        }
    }

    /* Create the subpicture unit */
    p_spu = decoder_NewSubpictureText( p_dec );
    if( !p_spu )
    {
        free( psz_subtitle );
        return NULL;
    }
    p_spu->i_start    = p_block->i_pts;
    p_spu->i_stop     = p_block->i_pts + p_block->i_length;
    p_spu->b_ephemer  = (p_block->i_length == 0);
    p_spu->b_absolute = false;

    subpicture_updater_sys_t *p_spu_sys = p_spu->updater.p_sys;

    p_spu_sys->align = SUBPICTURE_ALIGN_BOTTOM | p_sys->i_align;
    p_spu_sys->p_segments = ParseSubtitles( &p_spu_sys->align, psz_subtitle );

    //FIXME: Remove the variable?
    //if( var_InheritBool( p_dec, "subsdec-formatted" ) )

    return p_spu;
}

static bool AppendCharacter( text_segment_t* p_segment, char c )
{
    char* tmp;
    if ( asprintf( &tmp, "%s%c", p_segment->psz_text, c ) < 0 )
        return false;
    free( p_segment->psz_text );
    p_segment->psz_text = tmp;
    return true;
}

static char* ConsumeAttribute( const char** ppsz_subtitle, char** psz_attribute_value )
{
    const char* psz_subtitle = *ppsz_subtitle;
    char* psz_attribute_name;

    while (*psz_subtitle == ' ')
        psz_subtitle++;

    size_t attr_len = 0;
    char delimiter;

    while ( *psz_subtitle && isalpha( *psz_subtitle ) )
    {
        psz_subtitle++;
        attr_len++;
    }
    if ( !*psz_subtitle )
        return NULL;
    psz_attribute_name = malloc( attr_len + 1 );
    if ( unlikely( !psz_attribute_name ) )
        return NULL;
    strncpy( psz_attribute_name, psz_subtitle - attr_len, attr_len );
    psz_attribute_name[attr_len] = 0;

    // Skip over to the attribute value
    while ( *psz_subtitle && *psz_subtitle != '=' )
        psz_subtitle++;

    // Aknoledge the delimiter if any
    while ( *psz_subtitle && isspace( *psz_subtitle) )
        psz_subtitle++;

    if ( *psz_subtitle == '\'' || *psz_subtitle == '"' )
        delimiter = *psz_subtitle;
    else
        delimiter = 0;

    // Skip spaces, just in case
    while ( *psz_subtitle && isspace( *psz_subtitle ) )
        psz_subtitle++;

    attr_len = 0;
    while ( *psz_subtitle && ( ( delimiter != 0 && *psz_subtitle != delimiter ) ||
                               ( delimiter == 0 && !isalpha( *psz_subtitle ) ) ) )
    {
        psz_subtitle++;
        attr_len++;
    }
    if ( !*psz_subtitle || unlikely( !( *psz_attribute_value = malloc( attr_len + 1 ) ) ) )
    {
        free( psz_attribute_name );
        return NULL;
    }
    strncpy( *psz_attribute_value, psz_subtitle - attr_len, attr_len );
    (*psz_attribute_value)[attr_len] = 0;
    *ppsz_subtitle = psz_subtitle;
    return psz_attribute_name;
}

static int GetColor( const char* psz_color )
{
    for( int i = 0; p_html_colors[i].psz_name != NULL; i++ )
    {
        if( !strcasecmp( psz_color, p_html_colors[i].psz_name ) )
        {
            return p_html_colors[i].i_value;
        }
    }
    return 0;
}

/*
 * mini style stack implementation
 */
typedef struct style_stack style_stack_t;
struct  style_stack
{
    text_style_t* p_style;
    style_stack_t* p_next;
};

static text_style_t* DuplicateAndPushStyle(style_stack_t** pp_stack)
{
    text_style_t* p_dup = *pp_stack ? text_style_Duplicate( (*pp_stack)->p_style ) : text_style_New();
    if ( unlikely( !p_dup ) )
        return NULL;
    style_stack_t* p_entry = malloc( sizeof( *p_entry ) );
    if ( unlikely( !p_entry ) )
    {
        free( p_dup );
        return NULL;
    }
    // Give the style ownership to the segment.
    p_entry->p_style = p_dup;
    p_entry->p_next = *pp_stack;
    *pp_stack = p_entry;
    return p_dup;
}

static void PopStyle(style_stack_t** pp_stack)
{
    style_stack_t* p_old = *pp_stack;
    *pp_stack = p_old->p_next;
    // Don't free the style, it is now owned by the text_segment_t
    free( p_old );
}

static text_segment_t* NewTextSegmentPushStyle( text_segment_t* p_segment, style_stack_t** pp_stack )
{
    text_segment_t* p_new = text_segment_New( NULL );
    if ( unlikely( p_new == NULL ) )
        return NULL;
    text_style_t* p_style = DuplicateAndPushStyle( pp_stack );
    p_new->style = p_style;
    p_segment->p_next = p_new;
    return p_new;
}

static text_segment_t* NewTextSegmentPopStyle( text_segment_t* p_segment, style_stack_t** pp_stack )
{
    text_segment_t* p_new = text_segment_New( NULL );
    if ( unlikely( p_new == NULL ) )
        return NULL;
    PopStyle( pp_stack );
    // We shouldn't have an empty stack since this happens when closing a tag,
    // but better be safe than sorry if (/when) we encounter a broken subtitle file.
    text_style_t* p_dup = *pp_stack ? text_style_Duplicate( (*pp_stack)->p_style ) : text_style_New();
    p_new->style = p_dup;
    p_segment->p_next = p_new;
    return p_new;
}


static text_segment_t* ParseSubtitles( int *pi_align, const char *psz_subtitle )
{
    text_segment_t* p_segment;
    text_segment_t* p_first_segment;
    style_stack_t* p_stack = NULL;

    //FIXME: Remove initial allocation? Might make the below code more complicated
    p_first_segment = p_segment = text_segment_New( "" );

    bool b_has_align = false;

    /* */
    while( *psz_subtitle )
    {
        if( *psz_subtitle == '\n' )
        {
            if ( !AppendCharacter( p_segment, '\n' ) )
                goto fail;
            psz_subtitle++;
        }
        else if( *psz_subtitle == '<' )
        {
            if( !strncasecmp( psz_subtitle, "<br/>", 5 ))
            {
                if ( !AppendCharacter( p_segment, '\n' ) )
                    goto fail;
                psz_subtitle += strlen( "<br/>" );
            }
            else if( !strncasecmp( psz_subtitle, "<b>", 3 ) )
            {
                p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );
                p_segment->style->i_style_flags |= STYLE_BOLD;
                psz_subtitle += strlen( "<b>" );
            }
            else if( !strncasecmp( psz_subtitle, "<i>", 3 ) )
            {
                p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );
                p_segment->style->i_style_flags |= STYLE_ITALIC;
                psz_subtitle += strlen( "<i>" );
            }
            else if( !strncasecmp( psz_subtitle, "<u>", 3 ) )
            {
                p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );
                p_segment->style->i_style_flags |= STYLE_UNDERLINE;
                psz_subtitle += strlen( "<u>" );
            }
            else if( !strncasecmp( psz_subtitle, "<s>", 3 ) )
            {
                p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );
                p_segment->style->i_style_flags |= STYLE_STRIKEOUT;
                psz_subtitle += strlen( "<s>" );
            }
            else if( !strncasecmp( psz_subtitle, "<font ", 6 ))
            {
                p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );
                psz_subtitle += strlen( "<font " );

                char* psz_attribute_name;
                char* psz_attribute_value;

                while( ( psz_attribute_name = ConsumeAttribute( &psz_subtitle, &psz_attribute_value ) ) )
                {
                    if ( !strcasecmp( psz_attribute_name, "face" ) )
                    {
                        p_segment->style->psz_fontname = psz_attribute_value;
                        psz_attribute_value = NULL;
                    }
                    else if ( !strcasecmp( psz_attribute_name, "family" ) )
                    {
                        p_segment->style->psz_monofontname = psz_attribute_value;
                        psz_attribute_value = NULL;
                    }
                    else if ( !strcasecmp( psz_attribute_name, "size" ) )
                    {
                        p_segment->style->i_font_size = atoi( psz_attribute_value );
                    }
                    else if ( !strcasecmp( psz_attribute_name, "color" ) )
                    {
                        p_segment->style->i_font_color = GetColor( psz_attribute_value );
                    }
                    else if ( !strcasecmp( psz_attribute_name, "outline-color" ) )
                    {
                        p_segment->style->i_outline_color = GetColor( psz_attribute_value );
                    }
                    else if ( !strcasecmp( psz_attribute_name, "shadow-color" ) )
                    {
                        p_segment->style->i_shadow_color = GetColor( psz_attribute_value );
                    }
                    else if ( !strcasecmp( psz_attribute_name, "outline-level" ) )
                    {
                        p_segment->style->i_outline_width = atoi( psz_attribute_value );
                    }
                    else if ( !strcasecmp( psz_attribute_name, "shadow-level" ) )
                    {
                        p_segment->style->i_shadow_width = atoi( psz_attribute_value );
                    }
                    else if ( !strcasecmp( psz_attribute_name, "back-color" ) )
                    {
                        p_segment->style->i_background_color = GetColor( psz_attribute_value );
                    }
                    else if ( !strcasecmp( psz_attribute_name, "alpha" ) )
                    {
                        p_segment->style->i_font_alpha = atoi( psz_attribute_value );
                    }

                    free( psz_attribute_name );
                    free( psz_attribute_value );
                }
                // Skip potential spaces & end tag
                while ( *psz_subtitle && *psz_subtitle != '>' )
                    psz_subtitle++;
            }
            else if( !strncmp( psz_subtitle, "</", 2 ))
            {
                size_t tag_length = 0;
                const char* p_old_pos = psz_subtitle;
                while ( *psz_subtitle && *psz_subtitle != '>' )
                {
                    tag_length++;
                    psz_subtitle++;
                }
                if ( !strncasecmp( p_old_pos, "b", tag_length ) ||
                     !strncasecmp( p_old_pos, "i", tag_length ) ||
                     !strncasecmp( p_old_pos, "u", tag_length ) ||
                     !strncasecmp( p_old_pos, "s", tag_length ) ||
                     !strncasecmp( p_old_pos, "font", tag_length ) )
                {
                    // A closing tag for one of the tags we handle, meaning
                    // we pushed a style onto the stack earlier
                    p_segment = NewTextSegmentPopStyle( p_segment, &p_stack );
                }
                else
                {
                    // Unknown closing tag, just append the '<', and go on.
                    // This will make the unknown tag appear as text
                    AppendCharacter( p_segment, '<' );
                    psz_subtitle = p_old_pos + 1;
                }
            }
            else
            {
                /* We have an unknown tag, just append it, and move on.
                 * The rest of the string won't be recognized as a tag, and
                 * we will ignore unknown closing tag
                 */
                AppendCharacter( p_segment, '<' );
                psz_subtitle++;
            }
        }
        else if( psz_subtitle[0] == '{' && psz_subtitle[1] == '\\' &&
                 strchr( psz_subtitle, '}' ) )
        {
            /* Check for forced alignment */
            if( !b_has_align &&
                !strncmp( psz_subtitle, "{\\an", 4 ) && psz_subtitle[4] >= '1' && psz_subtitle[4] <= '9' && psz_subtitle[5] == '}' )
            {
                static const int pi_vertical[3] = { SUBPICTURE_ALIGN_BOTTOM, 0, SUBPICTURE_ALIGN_TOP };
                static const int pi_horizontal[3] = { SUBPICTURE_ALIGN_LEFT, 0, SUBPICTURE_ALIGN_RIGHT };
                const int i_id = psz_subtitle[4] - '1';

                b_has_align = true;
                *pi_align = pi_vertical[i_id/3] | pi_horizontal[i_id%3];
            }
            /* TODO fr -> rotation */

            /* Hide {\stupidity} */
            psz_subtitle = strchr( psz_subtitle, '}' ) + 1;
        }
        else if( psz_subtitle[0] == '{' &&
                ( psz_subtitle[1] == 'Y' || psz_subtitle[1] == 'y' )
                && psz_subtitle[2] == ':' && strchr( psz_subtitle, '}' ) )
        {
            // FIXME: We don't do difference between Y and y, and we should.
            if( psz_subtitle[3] == 'i' )
            {
                p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );
                p_segment->style->i_style_flags |= STYLE_ITALIC;
                psz_subtitle++;
            }
            if( psz_subtitle[3] == 'b' )
            {
                p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );
                p_segment->style->i_style_flags |= STYLE_BOLD;
                psz_subtitle++;
            }
            if( psz_subtitle[3] == 'u' )
            {
                p_segment = NewTextSegmentPushStyle( p_segment, &p_stack );
                p_segment->style->i_style_flags |= STYLE_UNDERLINE;
                psz_subtitle++;
            }
            psz_subtitle = strchr( psz_subtitle, '}' ) + 1;
        }
        else if( psz_subtitle[0] == '{' &&  psz_subtitle[2] == ':' && strchr( psz_subtitle, '}' ) )
        {
            // Hide other {x:y} atrocities, like {c:$bbggrr} or {P:x}
            psz_subtitle = strchr( psz_subtitle, '}' ) + 1;
        }
        else
        {
            //FIXME: Highly inneficient
            AppendCharacter( p_segment, *psz_subtitle );
            psz_subtitle++;
        }
    }

    return p_first_segment;

fail:
    text_segment_ChainDelete( p_first_segment );
    return NULL;
}
