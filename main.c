/*
 *	Project: svg2ass	
 * 	   File: main.c
 *  Created: 2014-10-23
 *   Author: Urban Wallasch
 * 
 * TODOs:
 *  - Actually test the more esoteric curveto path commands
 *  - Parse transformation, apply to context (at least translate;
 *      ... matrices? Bruahahaha!
 *  - Find a way to emulate arcs with Bezier curves? Oh my!
 *  - Clippath? ?? ???
 *  - rounded rects? (c'mon, object-to-path is your friend!)
 * 
 * Notes:
 *  - Unicode support is utterly borked, and I couldn't care less.
 *  - The more you "flatten" your SVG (objects to paths, no layering, 
 *    etc. ) the better the chances are to get an acceptable result.
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>

#include <unistd.h>

#include "nxml.h"
#include "colors.h"


#if (defined(WIN32) || defined(WIN64)) && !defined(strcasecmp)
#ifdef SXMLC_UNICODE
#define strcasecmp _wcsicmp
#else
#define strcasecmp _strcmpi
#endif
#endif

//#define  DEBUG		// belongs in Makefile

#ifdef DEBUG
#define WHOAMI()		fprintf( stderr, "%s:%d:%s\n", __FILE__, __LINE__, __func__ )
#define DPRINT(...)		do { \
							fprintf( stderr, "%*s", (ctx->indent * 4), "" ); \
							fprintf( stderr, __VA_ARGS__ ); \
						} while(0)
#else
#define WHOAMI()
#define DPRINT(...)
#endif


#define emit(...)		fprintf( stdout, __VA_ARGS__ )


/*
 *  Config
 */
 
 static struct {
	 int assfup;
 } config;

/*
 * Context stack crap
 */

#define CTX_STACK_SIZE	1024

typedef 
	struct _SVGContext 
	SVGContext;

struct _SVGContext {
	int in_svg;
	int x, y;
#ifdef DEBUG
	int indent;
#endif
};

static SVGContext stack[CTX_STACK_SIZE];
static int stackp = 0;

static int ctx_push( SVGContext *ctx )
{
	if ( stackp >= CTX_STACK_SIZE - 1 )
		return -1;
	stack[stackp++] = *ctx;
#ifdef DEBUG
	++ctx->indent;
#endif
	return 0;
}
	
static int ctx_pop( SVGContext *ctx )
{
	if ( stackp < 1 )
		return -1;
	*ctx = stack[--stackp];
	return 0;
}

/*
 * Parse generic attributes
 */
static int parseStyles( SVGContext *ctx, const char *style )
{
	int n = 0;
	int nocol = 3;
	unsigned fill_col = 0, fill_a = 0, stroke_col = 0, stroke_a = 0;
	double stroke_wid = 1;
	const char *s;
	char name[100];
	char value[100];

	s = style;
	while ( sscanf( s, "%99[^:]:%99[^;];%n", name, value, &n ) == 2 )
	{   
		//DPRINT( "%s=%s\n", name, value );
		if ( 0 == strcasecmp( name, "fill" ) && 0 != strcasecmp( value, "none" ) )
		{
			nocol &= ~1;
			fill_col = convColorBGR( value );
		}
		else if ( 0 == strcasecmp( name, "stroke" )  && 0 != strcasecmp( value, "none" ) )
		{
			nocol &= ~2;
			stroke_col = convColorBGR( value );
		}
		else if ( 0 == strcasecmp( name, "fill-opacity" ) )
			fill_a = 255 - atof( value ) * 255;
		else if ( 0 == strcasecmp( name, "stroke-opacity" ) )
			stroke_a = 255 - atof( value ) * 255;
		else if ( 0 == strcasecmp( name, "stroke-width" ) )
			stroke_wid = atof( value );
		s += n;
	}

	if ( nocol & 1 )
		fill_a = 255;
	if ( nocol & 2 )
		stroke_a = 255;

	emit( "{" );
	emit( "\\1c&H%06X&", fill_col );
	emit( "\\1a&H%02X&", fill_a );
	emit( "\\3c&H%06X&", stroke_col );
	emit( "\\3a&H%02X&", stroke_a );
	emit( "\\bord%g", stroke_wid );
	emit( "}" );
	(void)ctx;
	return 0;
}

static inline int findAttr( const nxmlNode_t *node, const char *name )
{
	int a;
	for ( a = 0; a < (int)node->att_num; ++a )
		if ( 0 == strcasecmp( node->att[a].name, name ) )
			return a;
	return -1;
}

static int parseGenAttr( SVGContext *ctx, const nxmlNode_t *node )
{
	int a;
	
	a = findAttr( node, "style" );
	if ( 0 <= a )
		parseStyles( ctx, node->att[a].val );
	
	// TODO: transform --> ctx->x,y
	
	return 0;
}

static inline double getNumericAttr( const nxmlNode_t *node, char *attr )
{
	int a = findAttr( node, attr );
	return ( 0 <= a ) ? strtod( node->att[a].val, NULL ) : 0.0;
}

static inline const char *getStringAttr( const nxmlNode_t *node, char *attr )
{
	int a = findAttr( node, attr );
	return ( 0 <= a ) ? node->att[a].val : "";
}

/*
 * Enable / disable ASS drawing mode
 */
static inline int pmode( int x )
{
	static int en = 0;
	if ( en && !x ) 
	{
		emit( "{\\p0}" );
		en = 0; 
	}
	else if ( !en && x )
	{
		emit( "{\\shad0\\an7\\p1}" ); 
		en = 1; 
	}
	return en;
}

/*
 * Path parser logic shamelessly stolen from libsvgtiny:
 * http://www.netsurf-browser.org/projects/libsvgtiny/
 */
static int parsePath( SVGContext *ctx, const char *pd )
{
	int res = 0;
	char *s, *d;
	double last_x = ctx->x, last_y = ctx->y;
	double last_cubic_x = 0, last_cubic_y = 0;
	double last_quad_x = 0, last_quad_y = 0;
	double subpath_first_x = 0, subpath_first_y = 0;

	/* obtain a clean copy of path */
	if ( NULL == ( d = strdup( pd ) ) )
		return -1;
	for ( s = d; *s; ++s )
		if ( ',' == *s )
			*s = ' ';
	
	for ( s = d; *s; ) 
	{
		int n;
		char svg_cmd[2] = "";
		float x, y, x1, y1, x2, y2;
		float rx, ry, rot, larc, swp;

		/* moveto (M, m), lineto (L, l) (2 arguments) */
		if ( sscanf( s, " %1[MmLl]%f%f %n", svg_cmd, &x, &y, &n) == 3 ) 
		{
			char ass_cmd;
			if ( *svg_cmd == 'M' || *svg_cmd == 'm' )
			{
				DPRINT( "moveto\n" );
				ass_cmd = 'm';
			}
			else
			{
				DPRINT( "lineto\n" );
				ass_cmd = 'l';
			}
			do 
			{
				emit( "%c ", ass_cmd );
				if ( *svg_cmd == 'l' || *svg_cmd == 'm' )
				{
					x += last_x;
					y += last_y;
				}
				if ( ass_cmd == 'm' ) 
				{
					subpath_first_x = x;
					subpath_first_y = y;
				}
				emit( "%g %g ", x, y );
				last_cubic_x = last_quad_x = last_x = x;
				last_cubic_y = last_quad_y = last_y = y;
				s += n;
				ass_cmd = 'l';
			} 
			while ( sscanf( s, "%f%f %n", &x, &y, &n ) == 2 );
		}
		/* closepath (Z, z) (no arguments) */
		else if ( sscanf(s, " %1[Zz] %n", svg_cmd, &n) == 1 ) 
		{
			DPRINT( "closepath\n" );
			// in ASS paths are automatically closed
			// IOW: there are no "open" paths, only closed shapes!
			s += n;
			last_cubic_x = last_quad_x = last_x = subpath_first_x;
			last_cubic_y = last_quad_y = last_y = subpath_first_y;
		}
		/* horizontal lineto (H, h) (1 argument) */
		else if ( sscanf( s, " %1[Hh]%f %n", svg_cmd, &x, &n ) == 2 ) 
		{
			DPRINT( "h-lineto\n" );
			emit( "l " );
			y = last_y;
			do 
			{
				if ( *svg_cmd == 'h' )
					x += last_x;
				emit( "%g %g ", x, y );
				last_cubic_x = last_quad_x = last_x	= x;
				s += n;
			} 
			while ( sscanf( s, "%f %n", &x, &n ) == 1 );
		}
		/* vertical lineto (V, v) (1 argument) */
		else if ( sscanf( s, " %1[Vv]%f %n", svg_cmd, &y, &n ) == 2 ) 
		{
			DPRINT( "v-lineto\n" );
			emit( "l " );
			x = last_x;
			do 
			{
				if ( *svg_cmd == 'v' )
					y += last_y;
				emit( "%g %g ", x, y );
				last_cubic_y = last_quad_y = last_y	= y;
				s += n;
			} 
			while ( sscanf( s, "%f %n", &x, &n ) == 1 );
		}
		/* cubic Bézier curveto (C, c) (6 arguments) */
		else if ( sscanf( s, " %1[Cc]%f%f%f%f%f%f %n", svg_cmd, 
					&x1, &y1, &x2, &y2, &x, &y, &n ) == 7 ) 
		{
			DPRINT( "c-bezier\n" );
			do 
			{
				emit( "b " );
				if ( *svg_cmd == 'c' ) 
				{
					x1 += last_x;
					y1 += last_y;
					x2 += last_x;
					y2 += last_y;
					x += last_x;
					y += last_y;
				}
				emit( "%g %g %g %g %g %g ", x1, y1, x2, y2, x, y );
				last_cubic_x = x2;
				last_cubic_y = y2;
				last_quad_x = last_x = x;
				last_quad_y = last_y = y;
				s += n;
			} 
			while ( sscanf( s, "%f%f%f%f%f%f %n", 
						&x1, &y1, &x2, &y2, &x, &y, &n ) == 6 );
		}
		/* shorthand/smooth cubic curveto (S, s) (4 arguments) */
		else if ( sscanf(s, " %1[Ss]%f%f%f%f %n", svg_cmd,
					&x2, &y2, &x, &y, &n) == 5 ) 
		{
			DPRINT( "s-bezier\n" );
			do 
			{
				emit( "b " );
				x1 = last_x + ( last_x - last_cubic_x );
				y1 = last_y + ( last_y - last_cubic_y );
				if ( *svg_cmd == 's' ) 
				{
					x2 += last_x;
					y2 += last_y;
					x += last_x;
					y += last_y;
				}
				emit( "%g %g %g %g %g %g ", x1, y1, x2, y2, x, y );
				last_cubic_x = x2;
				last_cubic_y = y2;
				last_quad_x = last_x = x;
				last_quad_y = last_y = y;
				s += n;
			} 
			while (sscanf(s, "%f%f%f%f %n",
						&x2, &y2, &x, &y, &n) == 4);
		}
		/* quadratic Bezier curveto (Q, q) (4 arguments) */
		else if ( sscanf( s, " %1[Qq]%f%f%f%f %n", svg_cmd,
					&x1, &y1, &x, &y, &n ) == 5 ) 
		{
			DPRINT( "q-bezier\n" );
			do 
			{
				emit( "b " );
				last_quad_x = x1;
				last_quad_y = y1;
				if ( *svg_cmd == 'q' ) 
				{
					x1 += last_x;
					y1 += last_y;
					x += last_x;
					y += last_y;
				}
				emit( "%g %g %g %g %g %g ",
					1./3 * last_x + 2./3 * x1,
					1./3 * last_y + 2./3 * y1,
					2./3 * x1 + 1./3 * x,
					2./3 * y1 + 1./3 * y,
					x, y );
				last_cubic_x = last_x = x;
				last_cubic_y = last_y = y;
				s += n;
			} 
			while ( sscanf( s, "%f%f%f%f %n", &x1, &y1, &x, &y, &n) == 4 );
		}
		/* shorthand/smooth quadratic curveto (T, t) (2 arguments) */
		else if ( sscanf( s, " %1[Tt]%f%f %n", svg_cmd,	&x, &y, &n ) == 3 ) 
		{
			DPRINT( "t-bezier\n" );
			do 
			{
				emit( "b " );
				x1 = last_x + ( last_x - last_quad_x );
				y1 = last_y + ( last_y - last_quad_y );
				last_quad_x = x1;
				last_quad_y = y1;
				if ( *svg_cmd == 't' ) 
				{
					x1 += last_x;
					y1 += last_y;
					x += last_x;
					y += last_y;
				}
				emit( "%g %g %g %g %g %g ",
					1./3 * last_x + 2./3 * x1,
					1./3 * last_y + 2./3 * y1,
					2./3 * x1 + 1./3 * x,
					2./3 * y1 + 1./3 * y,
					x, y ),
				last_cubic_x = last_x = x;
				last_cubic_y = last_y = y;
				s += n;
			} 
			while ( sscanf(s, "%f%f %n", &x, &y, &n ) == 2 );
		}
		/* elliptical arc (A, a) (7 arguments) */
		else if ( sscanf( s, " %1[Aa]%f%f%f%f%f%f%f %n", svg_cmd,
				&rx, &ry, &rot, &larc, &swp, &x, &y, &n ) == 8 ) 
		{
			DPRINT( "path arc not implemented!\n" );
			do 
			{
				// TODO: is there a sane way to do arcs with B-curves?
				emit( "l " );
				if ( *svg_cmd == 'a' ) 
				{
					x += last_x;
					y += last_y;
				}
				emit( "%g %g ", x, y );
				last_cubic_x = last_quad_x = last_x = x;
				last_cubic_y = last_quad_y = last_y = y;
				s += n;
			} 
			while ( sscanf(s, "%f%f%f%f%f%f%f %n", &rx, &ry, 
						&rot, &larc, &swp,	&x, &y, &n ) == 7 );
		}
		/* invalid path syntax */
		else {
			fprintf( stderr, "parsePath failed at \"%s\"\n", s );
			res = -1;
			break;
		}
	}
	free( d );
	return res;
}


int svg2ass( nxmlEvent_t evt, const nxmlNode_t *node, void *usr )
{
	SVGContext *ctx = usr;
	double x0, y0, x1, y1, x2, y2, x3, y3;
	double cx, cy, rx, ry;

	if ( NXML_TYPE_PARENT != node->type 
		&& NXML_TYPE_SELF != node->type 
		&& NXML_TYPE_END != node->type )				
		return 0;
		
	switch ( evt ) 
	{
	case NXML_EVT_OPEN:
		if ( 0 == strcasecmp( node->name, "svg" ) ) 
			ctx->in_svg = 1;
		if ( !ctx->in_svg ) 
			break;
		DPRINT( "%s {\n", node->name );
		ctx_push( ctx );
		// handle generic attributes (style, transform, ...)
		parseGenAttr( ctx, node );

		if ( 0 == strcasecmp( node->name, "g" ) )
		{
			// no special action for <g>roups yet
		}
		else if ( 0 == strcasecmp( node->name,  "line" ) )
		{
			pmode( 1 );
			x1 = ctx->x + getNumericAttr( node, "x1" );
			y1 = ctx->y + getNumericAttr( node, "y1" );
			x2 = ctx->x + getNumericAttr( node, "x2" );
			y2 = ctx->y + getNumericAttr( node, "y2" );
			emit( "m %g %g l %g %g ", x1, y1, x2, y2 );
		}
		else if ( 0 == strcasecmp( node->name, "rect" ) )
		{
			pmode( 1 );
			x1 = ctx->x + getNumericAttr( node, "x" );
			y1 = ctx->y + getNumericAttr( node, "y" );
			x2 = x1 + getNumericAttr( node, "width" );
			y2 = y1 + getNumericAttr( node, "height" );
			// TODO: evaluate rx,ry and emulate rounded rect using Bézier curves
			emit( "m %g %g l %g %g %g %g %g %g ", x1, y1, x2, y1, x2, y2, x1, y2 );
		}
		else if ( 0 == strcasecmp( node->name, "ellipse" )
			   || 0 == strcasecmp( node->name, "circle" ) )
		{
			pmode( 1 );
			cx = ctx->x + getNumericAttr( node, "cx" );
			cy = ctx->y + getNumericAttr( node, "cy" );
			if ( 0 == strcasecmp( node->name, "ellipse" ) )
			{
				rx = getNumericAttr( node, "rx" );
				ry = getNumericAttr( node, "ry" );
			}
			else
				rx = ry = getNumericAttr( node, "r" );
			/*
			Approximate a circle using four Bézier curves:
			P_0 = (0,1),  P_1 = (c,1),   P_2 = (1,c),   P_3 = (1,0)
			P_0 = (1,0),  P_1 = (1,-c),  P_2 = (c,-1),  P_3 = (0,-1)
			P_0 = (0,-1), P_1 = (-c,-1), P_2 = (-1,-c), P_3 = (-1,0)
			P_0 = (-1,0), P_1 = (-1,c),  P_2 = (-c,1),  P_3 = (0,1)
			with c = 0.551915024494
			[ http://spencermortensen.com/articles/bezier-circle/ ]
			*/
			#define BEZIER_CIRC 	0.551915024494
			double crx = rx * BEZIER_CIRC;
			double cry = ry * BEZIER_CIRC;
			x0 = cx;		y0 = cy + ry;
			emit( "m %g %g ", x0, y0 );
			x1 = cx + crx;	y1 = cy + ry;
			x2 = cx + rx;	y2 = cy + cry;
			x3 = cx + rx;	y3 = cy;
			emit( "b %g %g %g %g %g %g ", x1, y1, x2, y2, x3, y3 );
			x1 = cx + rx;	y1 = cy - cry;
			x2 = cx + crx;	y2 = cy - ry;
			x3 = cx;		y3 = cy - ry;
			emit( "b %g %g %g %g %g %g ", x1, y1, x2, y2, x3, y3 );
			x1 = cx - crx;	y1 = cy - ry;
			x2 = cx - rx;	y2 = cy - cry;
			x3 = cx - rx;	y3 = cy;
			emit( "b %g %g %g %g %g %g ", x1, y1, x2, y2, x3, y3 );
			x1 = cx - rx;	y1 = cy + cry;
			x2 = cx - crx;	y2 = cy + ry;
			x3 = cx;		y3 = cy + ry;
			emit( "b %g %g %g %g %g %g ", x1, y1, x2, y2, x3, y3 );
		}
		else if ( 0 == strcasecmp( node->name, "path" ) )
		{
			pmode( 1 );
			parsePath( ctx, getStringAttr( node, "d" ) );
		}
		else 
		{
			/* (yet) unhandled tags:
			 * polyline, polygon: easily converted to SVG paths!
			 * text: Forget it, why would you want to export SVG 
			 *       text to ASS?!? (or simply convert to path)
			 * clippath: maybe later, maybe never
			 */
		}
		break;

	case NXML_EVT_CLOSE:
		if ( !strcasecmp( node->name, "svg" ) ) 
			ctx->in_svg = 0;
		ctx_pop( ctx );
		if ( !ctx->in_svg ) 
			break;
		DPRINT( "}\n" );
		break;

	default:
		break;
	}
	if ( 1 == config.assfup )
		pmode( 0 );
	return 0;
}


#define GET_LINE_INC	4000
static size_t getLine( char **pbuf, size_t *psz, size_t off, const int dlm, FILE *pf )
{
	int c;
	size_t x = 0;
	char *buf = *pbuf;
	size_t sz = *psz;
	
	while ( EOF != ( c = fgetc( pf ) ) )
	{
		if ( off + x + 1 >= sz )
		{
			char *p;
			if ( NULL == ( p = realloc( buf, sz + GET_LINE_INC ) ) )
				break;
			*pbuf = buf = p;
			*psz = sz += GET_LINE_INC;
		}
		buf[off + x++] = c;
		if ( dlm == c )
			break;
	}
	buf[off + x] = '\0';
	return x;
}

static int parse( FILE *fp, SVGContext *ctx ) 
{
	int res;
	char *buf = NULL;
	size_t sz = 0;
	size_t len = 0;
	size_t n = 0;
	
	errno = 0;
	while ( 0 < ( n = getLine( &buf, &sz, len, EOF, fp ) ) )
		len += n;
	if ( errno )
	{
		fprintf( stderr, "getLine: %s\n", strerror( errno ) );		
		free( buf );
		return -1;
	}
	memset( ctx, 0, sizeof *ctx );
	res = nxmlParse( buf, svg2ass, ctx );
	pmode( 0 );
	emit( "\n" );
	while ( 0 == ctx_pop( ctx ) )
		;
	free( buf );
	return res;
}

static int usage( char* progname )
{
	char *p;
	if ( NULL != ( p = strrchr( progname, '/' ) ) )
		progname = ++p;
	fprintf( stderr, "Usage: %s [options] [svg_file ...]\n", progname );
	fprintf( stderr,
		" If no file is specified input is taken from stdin.\n"
		" Options:\n"
		"  -a ASS-fuckup mode: 0 = preserve layout (default), 1 = preserve colors\n"
	);
	return 0;
}

int main( int argc, char** argv )
{
	SVGContext ctx_;
	SVGContext *ctx = &ctx_;
	int nfiles = 0;
	int opt;
	const char *ostr = "-:a:h";
	FILE *fp;

	while ( -1 != ( opt = getopt( argc, argv, ostr ) ) )
	{
		switch ( opt )
		{
		case 1:
			DPRINT( "parsing file '%s' ...\n", optarg );
			if ( NULL == ( fp = fopen( optarg, "r" ) ) )
			{
				fprintf( stderr, "fopen '%s': %s\n", optarg, strerror( errno ) );		
				goto ERR0;
			}
			if ( 0 != parse( fp, ctx ) )
			{
				fprintf( stderr, "Error processing file '%s'\n", optarg );
				goto ERR0;
			}
			++nfiles;
			break;
		case 'a':
			config.assfup = atoi( optarg );
			break;
		case 'h':
			usage( argv[0] );
			exit( EXIT_SUCCESS );
			break;
		case ':':
			fprintf( stderr, "Missing argument for option '%c'\n", optopt );
			goto ERR1;
			break;
		case '?':
		default:
			fprintf( stderr, "Unrecognized option '%c'\n", optopt );
		ERR1:
			usage( argv[0] );
		ERR0:
			exit( EXIT_FAILURE );
			break;
		}
	}
	
	if ( !nfiles )
	{
		DPRINT( "parsing stdin ...\n" );
		if ( 0 != parse( stdin, ctx ) )
		{
			fprintf( stderr, "Error processing stdin'\n" );
			goto ERR0;
		}
	}
	exit( EXIT_SUCCESS );
}

/* EOF */
