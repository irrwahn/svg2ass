/*
 *	Project: svg2ass	
 * 	   File: main.c
 *  Created: 2014-10-23
 *   Author: Urban Wallasch
 * 
 * TODOs:
 *  - Actually test the more esoteric curveto path commands
 *  - Find a way to emulate arcs with Bezier curves? Oh my!
 *  - Clippath? ?? ???
 *  - rounded rects? (c'mon, object-to-path is your friend!)
 * 
 * Notes:
 *  - Unicode support is utterly borked (though UTF-8 should be fine), 
 *    and I couldn't care less.
 *  - The more you "flatten" your SVG (objects to paths, no layering, 
 *    etc. ) the better the chances are to get an acceptable result.
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <float.h>
#include <math.h>

#include <unistd.h>

#include "nxml.h"
#include "colors.h"
#include "vect.h"


#ifdef DEBUG
#include <assert.h>
#define WHOAMI()		fprintf( stderr, "%s:%d:%s\n", __FILE__, __LINE__, __func__ )
#define DPRINT(...)		do { fprintf( stderr, __VA_ARGS__ ); } while(0)
#define IPRINT(...)		do { \
							fprintf( stderr, "%*s", (ctx->indent * 4), "" ); \
							fprintf( stderr, __VA_ARGS__ ); \
						} while(0)
#else
#define assert(...)
#define WHOAMI()
#define DPRINT(...)
#define IPRINT(...)
#endif


/************************************************************
 *	Config stuff
 */
 
#define MAX_FPREC	5

static struct {
	int ass_fup;
	int ass_fprec;
	FILE *of;
} config = {
	0,
	1,
	NULL,
};


/************************************************************
 *	Context stuff
 */

#define CTX_STACK_SIZE	1024	// maximum nesting level supported

typedef struct {
	int in_svg;
	vec_t o;		// origin
	mtx_t m;		// current transformation matrix (CTM)
#ifdef DEBUG
	int indent;
#endif
} ctx_t;

static ctx_t stack[CTX_STACK_SIZE];
static int stackp = 0;

static int ctx_push( ctx_t *ctx )
{
	if ( stackp >= CTX_STACK_SIZE - 1 )
		return -1;
	stack[stackp++] = *ctx;
#ifdef DEBUG
	++ctx->indent;
#endif
	return 0;
}
	
static int ctx_pop( ctx_t *ctx )
{
	if ( stackp < 1 )
		return -1;
	*ctx = stack[--stackp];
	return 0;
}
	

/************************************************************
 *	ASS output generator
 */

#if 0
int emit( const char *fmt, ... )
{
	int r;
	va_list arglist;

	va_start( arglist, fmt );
	r = vfprintf( config.of, fmt, arglist );
	va_end( arglist );
	return r < 0 ? -1 : 0;
}
#else
#define emit(...)	(0>fprintf(config.of,__VA_ARGS__)?-1:0)
#endif

/*
 *	Format floating point number with a given precision,
 *	stripping trailing zero fractional component.
 */
static inline char *ftoa( char *buf, int prec, double d )
{
	char *b;

	sprintf( buf, "%0.*f", prec, d );
	if ( strchr( buf, '.' ) )
	{
		b = buf + strlen( buf ) - 1;
		while ( '0' == *b )
			*b-- = '\0';
		if ( '.' == *b )
			*b = '\0';
	}
	return buf;
}

int emitf( ctx_t *ctx, const char *fmt, ... )
{
	int r = 0;
	const char *p;
	vec_t v;
	va_list arglist;
	static char buf[3 + DBL_MANT_DIG - DBL_MIN_EXP + 1];
	
	va_start( arglist, fmt );
	for ( p = fmt; *p && 0 == r; ++p )
	{
		if ( '%' == *p )
		{
			++p;
			switch( tolower( *p ) )
			{
			case 'f':
				r = emit( "%s", ftoa( buf, config.ass_fprec, va_arg( arglist, double ) ) );
				break;
			case 'v': 
				v = va_arg( arglist, vec_t );
				v = mtx_vmul( ctx->m, v );
				r = emit( "%s ", ftoa( buf, config.ass_fprec, v.x ) );
				r = emit( "%s",  ftoa( buf, config.ass_fprec, v.y ) );
				break;
			case '%':
				r = emit( "%%" );
				break;
			default:
				r = -1; 
				assert( 1 == 0 );
				break;
			}
		}
		else
			r = emit( "%c", *p );
	}
	va_end( arglist );
	return r;
}

// Enable / disable ASS drawing mode
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


/************************************************************
 *	Parse attributes
 */
 
static inline int findAttr( const nxmlNode_t *node, const char *name )
{
	int a;
	for ( a = 0; a < (int)node->att_num; ++a )
		if ( 0 == strcasecmp( node->att[a].name, name ) )
			return a;
	return -1;
}

static inline double getNumericAttr( const nxmlNode_t *node, char *attr )
{
	int a = findAttr( node, attr );
	return ( 0 <= a ) ? strtod( node->att[a].val, NULL ) : 0.0;
}

static inline const char *getStringAttr( const nxmlNode_t *node, char *attr )
{
	int a = findAttr( node, attr );
	return ( 0 <= a ) ? node->att[a].val : NULL;
}

static int parseStyles( ctx_t *ctx, const char *style )
{
	int n = 0;
	int nocol = 3;
	unsigned fill_col = 0, fill_a = 0, stroke_col = 0, stroke_a = 0;
	double stroke_wid = 1;
	const char *s = style;
	char name[100];
	char value[100];
	
	if ( !s || !*s )
		return 0;
	IPRINT( "style\n" );
	while ( sscanf( s, " %99[^ :] : %99[^ ;] ;%n", name, value, &n ) == 2 )
	{   
		IPRINT( "    %s=%s\n", name, value );
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
	emitf( ctx, "\\bord%f", stroke_wid );
	emit( "}" );
	return 0;
}


#ifndef M_PI 
	#define M_PI 3.14159265358979323846
#endif
#define DEG2RAD(D)	((double)(D)/180.0*M_PI)

static int parseTransform( ctx_t *ctx, const char *trf )
{
	int a = 0, n;
	const char *s = trf;
	char op[100];
	double phi;
	mtx_t m;
	
	if ( !s || !*s )
		return 0;
	IPRINT( "transform\n" );	
	while ( *s )
	{
		if ( sscanf( s, " %99[^ (]%n", op, &n ) != 1 || !*op )
			break;
		s += n;
		m = MTX_UNI;
		if ( 0 == strcasecmp( op, "translate" ) )
		{
			a = sscanf( s, " (%lf%n", &m.e, &n );
			if ( 1 == a )
			{
				s += n;
				if ( 1 == sscanf( s, " ,%lf%n", &m.f, &n ) )
					s += n;
			}
		}
		else if ( 0 == strcasecmp( op, "scale" ) )
		{
			a = sscanf( s, " (%lf%n", &m.a, &n );
			if ( 1 == a )
			{
				s += n;
				m.d = m.a;
				if ( 1 == sscanf( s, " ,%lf%n", &m.d, &n ) )
					s += n;
			}
		}
		else if ( 0 == strcasecmp( op, "rotate" ) )
		{
			mtx_t r = MTX_UNI;
			a = sscanf( s, " (%lf%n", &phi, &n );
			if ( 1 == a )
			{
				s += n;
				phi = DEG2RAD(phi);
				r.a = r.d = cos( phi );
				r.b = sin( phi );
				r.c = -r.b;
				if ( 2 == sscanf( s, " ,%lf ,%lf%n", &m.e, &m.f, &n ) )
				{
					s += n;
					ctx->m = mtx_mmul( ctx->m, m );
					ctx->m = mtx_mmul( ctx->m, r );
					m.e = -m.e;
					m.f = -m.f;
				}
				else
					m = r;
			}
		}
		else if ( 0 == strcasecmp( op, "skewX" ) )
		{
			a = sscanf( s, " (%lf%n", &phi, &n );
			if ( 1 == a )
			{
				s += n;
				m.c = tan( DEG2RAD(phi) );
			}
		}
		else if ( 0 == strcasecmp( op, "skewY" ) )
		{
			a = sscanf( s, " (%lf%n", &phi, &n );
			if ( 1 == a )
			{
				s += n;
				m.b = tan( DEG2RAD(phi) );
			}
		}
		else if ( 0 == strcasecmp( op, "matrix" ) )
		{
			a = sscanf( s, " (%lf ,%lf ,%lf ,%lf ,%lf ,%lf%n", 
								&m.a, &m.b, &m.c, &m.d, &m.e, &m.f, &n );
			if ( 6 == a )
				s += n;
		}
		if ( 0 < a )
		{
			ctx->m = mtx_mmul( ctx->m, m );
			IPRINT( "    %s(%g,%g,%g,%g,%g,%g) --> CTM(%g,%g,%g,%g,%g,%g)\n", 
					op, m.a, m.b, m.c, m.d, m.e, m.f, 
					ctx->m.a, ctx->m.b, ctx->m.c, ctx->m.d, ctx->m.e, ctx->m.f );
		}
		sscanf( s, "%*[ 0-9.)]%n", &n );
		s += n;
	}
	return 0;
}


/************************************************************
 *	ASS drawing stuff and SVG parsing
 */

#define BEZIER_CIRC 	0.551915024494

static int ass_ellipse( ctx_t *ctx, vec_t c, vec_t r )
{
	/*
	Approximate a circle using four Bézier curves:
	P_0 = (0,1),  P_1 = (c,1),   P_2 = (1,c),   P_3 = (1,0)
	P_0 = (1,0),  P_1 = (1,-c),  P_2 = (c,-1),  P_3 = (0,-1)
	P_0 = (0,-1), P_1 = (-c,-1), P_2 = (-1,-c), P_3 = (-1,0)
	P_0 = (-1,0), P_1 = (-1,c),  P_2 = (-c,1),  P_3 = (0,1)
	with c = 0.551915024494
	[ http://spencermortensen.com/articles/bezier-circle/ ]
	*/
	vec_t v0, v1, v2, v3;
	vec_t rq = vec_scal( r, BEZIER_CIRC );
	
	v0.x = c.x;			v0.y = c.y + r.y;
	emitf( ctx, "m %v ", v0 );
	v1.x = c.x + rq.x;	v1.y = c.y + r.y;
	v2.x = c.x + r.x;	v2.y = c.y + rq.y;
	v3.x = c.x + r.x;	v3.y = c.y;
	emitf( ctx, "b %v %v %v ", v1, v2, v3 );
	v1.x = c.x + r.x;	v1.y = c.y - rq.y;
	v2.x = c.x + rq.x;	v2.y = c.y - r.y;
	v3.x = c.x;			v3.y = c.y - r.y;
	emitf( ctx, "b %v %v %v ", v1, v2, v3 );
	v1.x = c.x - rq.x;	v1.y = c.y - r.y;
	v2.x = c.x - r.x;	v2.y = c.y - rq.y;
	v3.x = c.x - r.x;	v3.y = c.y;
	emitf( ctx, "b %v %v %v ", v1, v2, v3 );
	v1.x = c.x - r.x;	v1.y = c.y + rq.y;
	v2.x = c.x - rq.x;	v2.y = c.y + r.y;
	v3.x = c.x;			v3.y = c.y + r.y;
	emitf( ctx, "b %v %v %v ", v1, v2, v3 );
	return 0;
}

static int ass_arc( ctx_t *ctx, vec_t v0, vec_t r, double rot, int larc, int swp, vec_t v )
{
#if 0
	// TODO: draw elliptical arc using cubic Bézier curves. Ugh!
#else
	IPRINT( "path arc not implemented!\n" );
	emitf( ctx, "l %v ", v );
	(void)v0; (void)r; (void)rot; (void)larc; (void)swp; 
#endif
	return 0;
}

/*
 * Path parser logic shamelessly stolen from libsvgtiny:
 * http://www.netsurf-browser.org/projects/libsvgtiny/
 */
static int parsePath( ctx_t *ctx, const char *pd )
{
	int res = 0;
	char *s, *d;
	vec_t last = ctx->o;
	vec_t last_cubic = last;
	vec_t last_quad = last;
	vec_t subpath_first = last;

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
		vec_t v, v1, v2, r;
		double rot;
		int larc, swp;

		/* moveto (M, m), lineto (L, l) (2 arguments) */
		if ( sscanf( s, " %1[MmLl]%lf%lf %n", svg_cmd, &v.x, &v.y, &n ) == 3 ) 
		{
			char ass_cmd;
			if ( *svg_cmd == 'M' || *svg_cmd == 'm' )
			{
				IPRINT( "moveto\n" );
				ass_cmd = 'm';
			}
			else
			{
				IPRINT( "lineto\n" );
				ass_cmd = 'l';
			}
			do 
			{
				emit( "%c ", ass_cmd );
				if ( *svg_cmd == 'l' || *svg_cmd == 'm' )
					v = vec_add( v, last );
				if ( ass_cmd == 'm' ) 
					subpath_first = v;
				emitf( ctx, "%v ", v );
				last_cubic = last_quad = last = v;
				s += n;
				ass_cmd = 'l';
			} 
			while ( sscanf( s, "%lf%lf %n", &v.x, &v.y, &n ) == 2 );
		}
		/* closepath (Z, z) (no arguments) */
		else if ( sscanf(s, " %1[Zz] %n", svg_cmd, &n) == 1 ) 
		{
			IPRINT( "closepath\n" );
			// in ASS paths are automatically closed
			// IOW: there are no "open" paths, only closed shapes!
			s += n;
			last_cubic = last_quad = last = subpath_first;
		}
		/* horizontal lineto (H, h) (1 argument) */
		else if ( sscanf( s, " %1[Hh]%lf %n", svg_cmd, &v.x, &n ) == 2 ) 
		{
			IPRINT( "h-lineto\n" );
			emit( "l " );
			v.y = last.y;
			do 
			{
				if ( *svg_cmd == 'h' )
					v.x += last.x;
				emitf( ctx, "%v ", v );
				last_cubic = last_quad = last = v;
				s += n;
			} 
			while ( sscanf( s, "%lf %n", &v.x, &n ) == 1 );
		}
		/* vertical lineto (V, v) (1 argument) */
		else if ( sscanf( s, " %1[Vv]%lf %n", svg_cmd, &v.y, &n ) == 2 ) 
		{
			IPRINT( "v-lineto\n" );
			emit( "l " );
			v.x = last.x;
			do 
			{
				if ( *svg_cmd == 'v' )
					v.y += last.y;
				emitf( ctx, "%v ", v );
				last_cubic = last_quad = last = v;
				s += n;
			} 
			while ( sscanf( s, "%lf %n", &v.x, &n ) == 1 );
		}
		/* cubic Bézier curveto (C, c) (6 arguments) */
		else if ( sscanf( s, " %1[Cc]%lf%lf%lf%lf%lf%lf %n", svg_cmd, 
					&v1.x, &v1.y, &v2.x, &v2.y, &v.x, &v.y, &n ) == 7 ) 
		{
			IPRINT( "c-bezier\n" );
			do 
			{
				if ( *svg_cmd == 'c' ) 
				{
					v1 = vec_add( v1, last );
					v2 = vec_add( v2, last );
					v  = vec_add( v , last );
				}
				emitf( ctx, "b %v %v %v ", v1, v2, v );
				last_cubic = v2;
				last_quad = last = v;
				s += n;
			} 
			while ( sscanf( s, "%lf%lf%lf%lf%lf%lf %n", 
						&v1.x, &v1.y, &v2.x, &v2.y, &v.x, &v.y, &n ) == 6 );
		}
		/* shorthand/smooth cubic curveto (S, s) (4 arguments) */
		else if ( sscanf(s, " %1[Ss]%lf%lf%lf%lf %n", svg_cmd,
					&v2.x, &v2.y, &v.x, &v.y, &n) == 5 ) 
		{
			IPRINT( "s-bezier\n" );
			do 
			{
				v1 = vec_add( last, vec_sub( last, last_cubic ) );
				if ( *svg_cmd == 's' ) 
				{
					v2 = vec_add( v2, last );
					v = vec_add( v, last );
				}
				emitf( ctx, "b %v %v %v ", v1, v2, v );
				last_cubic = v2;
				last_quad = last = v;
				s += n;
			} 
			while (sscanf(s, "%lf%lf%lf%lf %n",
						&v2.x, &v2.y, &v.x, &v.y, &n) == 4);
		}
		/* quadratic Bezier curveto (Q, q) (4 arguments) */
		else if ( sscanf( s, " %1[Qq]%lf%lf%lf%lf %n", svg_cmd,
					&v1.x, &v1.y, &v.x, &v.y, &n ) == 5 ) 
		{
			IPRINT( "q-bezier\n" );
			do 
			{
				last_quad = v;
				if ( *svg_cmd == 'q' ) 
				{
					v1 = vec_add( v1, last );
					v = vec_add( v, last );
				}
				emitf( ctx, "b %v %v %v ",
					vec_add( vec_scal( last, 1./3 ), vec_scal( v1, 2./3 ) ),
					vec_add( vec_scal( v1, 2./3 ), vec_scal( v, 1./3 ) ),
					v );
				last_cubic = last = v;
				s += n;
			} 
			while ( sscanf( s, "%lf%lf%lf%lf %n", &v1.x, &v1.y, &v.x, &v.y, &n) == 4 );
		}
		/* shorthand/smooth quadratic curveto (T, t) (2 arguments) */
		else if ( sscanf( s, " %1[Tt]%lf%lf %n", svg_cmd,	&v.x, &v.y, &n ) == 3 ) 
		{
			IPRINT( "t-bezier\n" );
			do 
			{
				v1 = vec_add( last, vec_sub( last, last_quad ) );
				last_quad = v1;
				if ( *svg_cmd == 't' ) 
				{
					v1 = vec_add( v1, last );
					v = vec_add( v, last );
				}
				emitf( ctx, "b %v %v %v ",
					vec_add( vec_scal( last, 1./3 ), vec_scal( v1, 2./3 ) ),
					vec_add( vec_scal( v1, 2./3 ), vec_scal( v, 1./3 ) ),
					v ),
				last_cubic = last = v;
				s += n;
			} 
			while ( sscanf(s, "%lf%lf %n", &v.x, &v.y, &n ) == 2 );
		}
		/* elliptical arc (A, a) (7 arguments) */
		else if ( sscanf( s, " %1[Aa]%lf%lf%lf%d%d%lf%lf %n", svg_cmd,
				&r.x, &r.y, &rot, &larc, &swp, &v.x, &v.y, &n ) == 8 ) 
		{
			IPRINT( "arc\n" );
			do 
			{
				if ( *svg_cmd == 'a' ) 
					vec_add( v, last );
				ass_arc( ctx, last, r, rot, larc, swp, v );
				last_cubic = last_quad = last = v;
				s += n;
			} 
			while ( sscanf(s, "%lf%lf%lf%d%d%lf%lf %n", &r.x, &r.y, 
						&rot, &larc, &swp, &v.x, &v.y, &n ) == 7 );
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

/*
 * Callback function for XML parser
 */
static int svg2ass( nxmlEvent_t evt, const nxmlNode_t *node, void *usr )
{
	ctx_t *ctx = usr;
	vec_t v1, v2, c, r;

	if ( NXML_TYPE_PARENT != node->type 
		&& NXML_TYPE_SELF != node->type 
		&& NXML_TYPE_END != node->type )				
		return 0;
		
	switch ( evt ) 
	{
	case NXML_EVT_OPEN:
		if ( 0 == strcasecmp( node->name, "svg" ) )
		{
			if ( ctx->in_svg )
			{
				IPRINT( "Warning: nested <svg>!\n" );
			}
			ctx->in_svg = 1;
		}
		if ( !ctx->in_svg ) 
			break;
		IPRINT( "%s {\n", node->name );
		ctx_push( ctx );
		parseStyles( ctx, getStringAttr( node, "style" ) );
		parseTransform( ctx, getStringAttr( node, "transform" ) );

		if ( 0 == strcasecmp( node->name, "g" ) )
		{
			// no special action for <g>roups yet
		}
		else if ( 0 == strcasecmp( node->name,  "line" ) )
		{
			pmode( 1 );
			v1.x = ctx->o.x + getNumericAttr( node, "x1" );
			v1.y = ctx->o.y + getNumericAttr( node, "y1" );
			v2.x = ctx->o.x + getNumericAttr( node, "x2" );
			v2.y = ctx->o.y + getNumericAttr( node, "y2" );
			IPRINT( "x1=%g, y1=%g, x2=%g, y2=%g\n", v1.x, v1.y, v2.x, v2.y );
			emitf( ctx, "m %v l %v ", v1, v2 );
		}
		else if ( 0 == strcasecmp( node->name, "rect" ) )
		{
			pmode( 1 );
			v1.x = ctx->o.x + getNumericAttr( node, "x" );
			v1.y = ctx->o.y + getNumericAttr( node, "y" );
			v2.x = v1.x + getNumericAttr( node, "width" );
			v2.y = v1.y + getNumericAttr( node, "height" );
			// TODO: evaluate rx,ry and emulate rounded rect using Bézier curves
			IPRINT( "x=%g, y=%g, w=%g, h=%g\n", v1.x, v1.y, v2.x-v1.x, v2.y-v1.y );
			emitf( ctx, "m %v l %v %v %v", v1, (vec_t){v2.x,v1.y}, v2, (vec_t){v1.x,v2.y} );
		}
		else if ( 0 == strcasecmp( node->name, "circle" ) )
		{
			pmode( 1 );
			c.x = ctx->o.x + getNumericAttr( node, "cx" );
			c.y = ctx->o.y + getNumericAttr( node, "cy" );
			r.x = r.y = getNumericAttr( node, "r" );
			IPRINT( "x=%g, y=%g, r=%g\n", c.x, c.y, r.x );
			ass_ellipse( ctx, c, r );
		}
		else if ( 0 == strcasecmp( node->name, "ellipse" ) )
		{
			pmode( 1 );
			c.x = ctx->o.x + getNumericAttr( node, "cx" );
			c.y = ctx->o.y + getNumericAttr( node, "cy" );
			r.x = getNumericAttr( node, "rx" );
			r.y = getNumericAttr( node, "ry" );
			IPRINT( "x=%g, y=%g, rx=%g, ry=%g\n", c.x, c.y, r.x, r.y );
			ass_ellipse( ctx, c, r );
		}
		else if ( 0 == strcasecmp( node->name, "path" ) )
		{
			pmode( 1 );
			parsePath( ctx, getStringAttr( node, "d" ) );
		}
		else 
		{
			/* (yet) unhandled relevant tags:
			 * polyline, polygon: easily converted to SVG paths!
			 * text: Forget it, why would you want to export SVG 
			 *       text to ASS?!? (or simply convert to path)
			 * clippath: maybe later, maybe never
			 */
		}
		break;

	case NXML_EVT_CLOSE:
		if ( 0 == strcasecmp( node->name, "svg" ) )
		{
			if ( !ctx->in_svg )
			{
				IPRINT( "Warning: excess </svg>!\n" );
			}
			ctx->in_svg = 0;
		}
		ctx_pop( ctx );
		if ( !ctx->in_svg ) 
			break;
		IPRINT( "}\n" );
		break;

	default:
		break;
	}
	if ( 1 == config.ass_fup )
		pmode( 0 );
	return 0;
}


/************************************************************
 *	XML parser bindung stuff
 */
static int getFile( char **pbuf, size_t *psz, size_t inc, FILE *pf )
{
	size_t x = 0, n;
	char *buf = *pbuf;
	
	do
	{
		if ( x + inc + 1 >= *psz )
		{
			if ( NULL == ( buf = realloc( buf, *psz + inc + 1 ) ) )
				return -1;
			*pbuf = buf;
			*psz += inc + 1;
		}
		n = fread( buf + x, 1, inc, pf );
		x += n;
	}
	while ( 0 < n );
	buf[x++] = '\0';
	if ( x < *psz && NULL != ( buf = realloc( buf, x ) ) )
	{
		*pbuf = buf;
		*psz = x;
	}
	return ferror( pf );
}

static int parse( FILE *fp ) 
{
	int res;
	char *buf = NULL;
	size_t sz = 0;
	ctx_t ctx;
	
	if ( 0 != getFile( &buf, &sz, 4000, fp ) )
	{
		fprintf( stderr, "getLine: %s\n", strerror( errno ) );
		free( buf );
		return -1;
	}
	memset( &ctx, 0, sizeof ctx );
	// initialize origin and matrix:
	ctx.o = VEC_ZERO;
	ctx.m = MTX_UNI;
	res = nxmlParse( buf, svg2ass, &ctx );
	pmode( 0 );
	emit( "\n" );
	while ( 0 == ctx_pop( &ctx ) )
		;	// in case we read an incomplete document
	free( buf );
	return res;
}


/************************************************************
 *	Main program stuff
 */
static int usage( char* progname )
{
	char *p;
	if ( NULL != ( p = strrchr( progname, '/' ) ) )
		progname = ++p;
	fprintf( stderr, "Usage: %s [options] [svg_file ...]\n", progname );
	fprintf( stderr,
		" SVG to ASS converter\n"
		" If no file is specified, or filename equals '-', input is taken from stdin.\n"
		" Options:\n"
		"  -a ASS-fuckup mode: 0 = preserve layout (default), 1 = preserve colors.\n"
		"  -f ASS floating point number prescision, 0-%d; default: 1\n", MAX_FPREC );
	fprintf( stderr,
		"  -h Print usage message and exit.\n"
		"  -o Write output to file; default: stdout.\n"
	);
	return 0;
}

int main( int argc, char** argv )
{
	int nfiles = 0;
	int opt;
	const char *ostr = "-:a:f:ho:";
	FILE *ifp;
	
	config.of = stdout;

	while ( -1 != ( opt = getopt( argc, argv, ostr ) ) )
	{
		switch ( opt )
		{
		case 1:
			DPRINT( "reading from file '%s'\n", optarg );
			if ( 0 == strcmp( "-", optarg ) )
				ifp = stdin;
			else if ( NULL == ( ifp = fopen( optarg, "r" ) ) )
			{
				fprintf( stderr, "fopen '%s': %s\n", optarg, strerror( errno ) );		
				goto ERR0;
			}
			if ( 0 != parse( ifp ) )
			{
				fprintf( stderr, "Error processing file '%s'\n", optarg );
				goto ERR0;
			}
			++nfiles;
			break;
		case 'a':
			config.ass_fup = atoi( optarg );
			break;
		case 'f':
			config.ass_fprec = atoi( optarg );
			if ( 0 > config.ass_fprec || MAX_FPREC < config.ass_fprec )
			{
				fprintf( stderr, "Floating point precision %d out of range\n", config.ass_fprec );		
				goto ERR1;
			}
			break;
		case 'o':
			DPRINT( "writing to file '%s'\n", optarg );
			if ( NULL == ( config.of = fopen( optarg, "w" ) ) )
			{
				fprintf( stderr, "fopen '%s': %s\n", optarg, strerror( errno ) );		
				goto ERR0;
			}
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
		DPRINT( "reading from <stdin>\n" );
		if ( 0 != parse( stdin ) )
		{
			fprintf( stderr, "Error processing <stdin>\n" );
			goto ERR0;
		}
		++nfiles;
	}
	DPRINT( "%d file%s processed\n", nfiles, nfiles == 1 ? "" : "s" );
	exit( EXIT_SUCCESS );
}

/* EOF */
