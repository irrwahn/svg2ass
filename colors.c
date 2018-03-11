/*
 *	Project: svg2ass
 * 	   File: colors.c
 *  Created: 2014-10-24
 *   Author: Urban Wallasch
 *
 * See LICENSE file for more details.
 */

#include <stdlib.h>
#include <strings.h>

#define RGB2BGR(R,G,B) ((unsigned)(B)<<16 | (G)<<8 | (R))

static struct {
	const char *s;
	unsigned bgr;
} cols[] = {
	/* Who made these up? Your little sister? */
	{ "aliceblue",		RGB2BGR(240, 248, 255) },
	{ "antiquewhite",	RGB2BGR(250, 235, 215) },
	{ "aqua",			RGB2BGR(  0, 255, 255) },
	{ "aquamarine",		RGB2BGR(127, 255, 212) },
	{ "azure",			RGB2BGR(240, 255, 255) },
	{ "beige",			RGB2BGR(245, 245, 220) },
	{ "bisque",			RGB2BGR(255, 228, 196) },
	{ "black",			RGB2BGR(  0,   0,   0) },
	{ "blanchedalmond",	RGB2BGR(255, 235, 205) },
	{ "blue",			RGB2BGR(  0,   0, 255) },
	{ "blueviolet",		RGB2BGR(138,  43, 226) },
	{ "brown",			RGB2BGR(165,  42,  42) },
	{ "burlywood",		RGB2BGR(222, 184, 135) },
	{ "cadetblue",		RGB2BGR( 95, 158, 160) },
	{ "chartreuse",		RGB2BGR(127, 255,   0) },
	{ "chocolate",		RGB2BGR(210, 105,  30) },
	{ "coral",			RGB2BGR(255, 127,  80) },
	{ "cornflowerblue",	RGB2BGR(100, 149, 237) },
	{ "cornsilk",		RGB2BGR(255, 248, 220) },
	{ "crimson",		RGB2BGR(220,  20,  60) },
	{ "cyan",			RGB2BGR(  0, 255, 255) },
	{ "darkblue",		RGB2BGR(  0,   0, 139) },
	{ "darkcyan",		RGB2BGR(  0, 139, 139) },
	{ "darkgoldenrod",	RGB2BGR(184, 134,  11) },
	{ "darkgray",		RGB2BGR(169, 169, 169) },
	{ "darkgreen",		RGB2BGR(  0, 100,   0) },
	{ "darkgrey",		RGB2BGR(169, 169, 169) },
	{ "darkkhaki",		RGB2BGR(189, 183, 107) },
	{ "darkmagenta",	RGB2BGR(139, 0,   139) },
	{ "darkolivegreen",	RGB2BGR( 85, 107,  47) },
	{ "darkorange",		RGB2BGR(255, 140,   0) },
	{ "darkorchid",		RGB2BGR(153,  50, 204) },
	{ "darkred",		RGB2BGR(139,   0,   0) },
	{ "darksalmon",		RGB2BGR(233, 150, 122) },
	{ "darkseagreen",	RGB2BGR(143, 188, 143) },
	{ "darkslateblue",	RGB2BGR( 72,  61, 139) },
	{ "darkslategray",	RGB2BGR( 47,  79,  79) },
	{ "darkslategrey",	RGB2BGR( 47,  79,  79) },
	{ "darkturquoise",	RGB2BGR(  0, 206, 209) },
	{ "darkviolet",		RGB2BGR(148,   0, 211) },
	{ "deeppink",		RGB2BGR(255,  20, 147) },
	{ "deepskyblue",	RGB2BGR(  0, 191, 255) },
	{ "dimgray",		RGB2BGR(105, 105, 105) },
	{ "dimgrey",		RGB2BGR(105, 105, 105) },
	{ "dodgerblue",		RGB2BGR( 30, 144, 255) },
	{ "firebrick",		RGB2BGR(178,  34,  34) },
	{ "floralwhite",	RGB2BGR(255, 250, 240) },
	{ "forestgreen",	RGB2BGR( 34, 139,  34) },
	{ "fuchsia",		RGB2BGR(255,   0, 255) },
	{ "gainsboro",		RGB2BGR(220, 220, 220) },
	{ "ghostwhite",		RGB2BGR(248, 248, 255) },
	{ "gold",			RGB2BGR(255, 215,   0) },
	{ "goldenrod",		RGB2BGR(218, 165,  32) },
	{ "gray",			RGB2BGR(128, 128, 128) },
	{ "grey",			RGB2BGR(128, 128, 128) },
	{ "green",			RGB2BGR(  0, 128,   0) },
	{ "greenyellow",	RGB2BGR(173, 255,  47) },
	{ "honeydew",		RGB2BGR(240, 255, 240) },
	{ "hotpink",		RGB2BGR(255, 105, 180) },
	{ "indianred",		RGB2BGR(205,  92,  92) },
	{ "indigo",			RGB2BGR( 75,   0, 130) },
	{ "ivory",			RGB2BGR(255, 255, 240) },
	{ "khaki",			RGB2BGR(240, 230, 140) },
	{ "lavender",		RGB2BGR(230, 230, 250) },
	{ "lavenderblush",	RGB2BGR(255, 240, 245) },
	{ "lawngreen",		RGB2BGR(124, 252,   0) },
	{ "lemonchiffon",	RGB2BGR(255, 250, 205) },
	{ "lightblue",		RGB2BGR(173, 216, 230) },
	{ "lightcoral",		RGB2BGR(240, 128, 128) },
	{ "lightcyan",		RGB2BGR(224, 255, 255) },
	{ "lightgoldenrodyellow",	RGB2BGR(250, 250, 210) },
	{ "lightgray",		RGB2BGR(211, 211, 211) },
	{ "lightgreen",		RGB2BGR(144, 238, 144) },
	{ "lightgrey",		RGB2BGR(211, 211, 211) },
	{ "lightpink",		RGB2BGR(255, 182, 193) },
	{ "lightsalmon",	RGB2BGR(255, 160, 122) },
	{ "lightseagreen",	RGB2BGR( 32, 178, 170) },
	{ "lightskyblue",	RGB2BGR(135, 206, 250) },
	{ "lightslategray",	RGB2BGR(119, 136, 153) },
	{ "lightslategrey",	RGB2BGR(119, 136, 153) },
	{ "lightsteelblue",	RGB2BGR(176, 196, 222) },
	{ "lightyellow",	RGB2BGR(255, 255, 224) },
	{ "lime",			RGB2BGR(  0, 255,   0) },
	{ "limegreen",		RGB2BGR( 50, 205,  50) },
	{ "linen",			RGB2BGR(250, 240, 230) },
	{ "magenta",		RGB2BGR(255,   0, 255) },
	{ "maroon",			RGB2BGR(128,   0,   0) },
	{ "mediumaquamarine",	RGB2BGR(102, 205, 170) },
	{ "mediumblue",		RGB2BGR(  0,   0, 205) },
	{ "mediumorchid",	RGB2BGR(186,  85, 211) },
	{ "mediumpurple",	RGB2BGR(147, 112, 219) },
	{ "mediumseagreen",	RGB2BGR( 60, 179, 113) },
	{ "mediumslateblue",	RGB2BGR(123, 104, 238) },
	{ "mediumspringgreen",	RGB2BGR(  0, 250, 154) },
	{ "mediumturquoise",	RGB2BGR( 72, 209, 204) },
	{ "mediumvioletred",	RGB2BGR(199,  21, 133) },
	{ "midnightblue",	RGB2BGR( 25,  25, 112) },
	{ "mintcream",		RGB2BGR(245, 255, 250) },
	{ "mistyrose",		RGB2BGR(255, 228, 225) },
	{ "moccasin",		RGB2BGR(255, 228, 181) },
	{ "navajowhite",	RGB2BGR(255, 222, 173) },
	{ "navy",			RGB2BGR(  0,   0, 128) },
	{ "oldlace",		RGB2BGR(253, 245, 230) },
	{ "olive",			RGB2BGR(128, 128,   0) },
	{ "olivedrab",		RGB2BGR(107, 142,  35) },
	{ "orange",			RGB2BGR(255, 165,   0) },
	{ "orangered",		RGB2BGR(255,  69,   0) },
	{ "orchid",			RGB2BGR(218, 112, 214) },
	{ "palegoldenrod",	RGB2BGR(238, 232, 170) },
	{ "palegreen",		RGB2BGR(152, 251, 152) },
	{ "paleturquoise",	RGB2BGR(175, 238, 238) },
	{ "palevioletred",	RGB2BGR(219, 112, 147) },
	{ "papayawhip",		RGB2BGR(255, 239, 213) },
	{ "peachpuff",		RGB2BGR(255, 218, 185) },
	{ "peru",			RGB2BGR(205, 133,  63) },
	{ "pink",			RGB2BGR(255, 192, 203) },
	{ "plum",			RGB2BGR(221, 160, 221) },
	{ "powderblue",		RGB2BGR(176, 224, 230) },
	{ "purple",			RGB2BGR(128,   0, 128) },
	{ "red",			RGB2BGR(255,   0,   0) },
	{ "rosybrown",		RGB2BGR(188, 143, 143) },
	{ "royalblue",		RGB2BGR( 65, 105, 225) },
	{ "saddlebrown",	RGB2BGR(139,  69,  19) },
	{ "salmon",			RGB2BGR(250, 128, 114) },
	{ "sandybrown",		RGB2BGR(244, 164,  96) },
	{ "seagreen",		RGB2BGR( 46, 139,  87) },
	{ "seashell",		RGB2BGR(255, 245, 238) },
	{ "sienna",			RGB2BGR(160,  82,  45) },
	{ "silver",			RGB2BGR(192, 192, 192) },
	{ "skyblue",		RGB2BGR(135, 206, 235) },
	{ "slateblue",		RGB2BGR(106,  90, 205) },
	{ "slategray",		RGB2BGR(112, 128, 144) },
	{ "slategrey",		RGB2BGR(112, 128, 144) },
	{ "snow",			RGB2BGR(255, 250, 250) },
	{ "springgreen",	RGB2BGR(  0, 255, 127) },
	{ "steelblue",		RGB2BGR( 70, 130, 180) },
	{ "tan",			RGB2BGR(210, 180, 140) },
	{ "teal",			RGB2BGR(  0, 128, 128) },
	{ "thistle",		RGB2BGR(216, 191, 216) },
	{ "tomato",			RGB2BGR(255,  99,  71) },
	{ "turquoise",		RGB2BGR( 64, 224, 208) },
	{ "violet",			RGB2BGR(238, 130, 238) },
	{ "wheat",			RGB2BGR(245, 222, 179) },
	{ "white",			RGB2BGR(255, 255, 255) },
	{ "whitesmoke",		RGB2BGR(245, 245, 245) },
	{ "yellow",			RGB2BGR(255, 255,   0) },
	{ "yellowgreen",	RGB2BGR(154, 205,  50) },
	{ NULL, 0 },
};

unsigned convColorBGR( const char *s )
{
	unsigned bgr = 0;
	if ( '#' == *s )
	{
		unsigned rgb = strtoul( s + 1, NULL, 16 );
		bgr = (rgb & 0xFF) << 16 | (rgb & 0xFF00) | (rgb & 0xFF0000 ) >> 16;
	}
	else
	{
		for ( int i = 0; cols[i].s; ++i )
			if ( 0 == strcasecmp( cols[i].s, s ) )
			{
				bgr = cols[i].bgr;
				break;
			}
	}
	return bgr;
}

/* EOF */
