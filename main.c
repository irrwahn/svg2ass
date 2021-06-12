/*
 *	Project: svg2ass
 * 	   File: main.c
 *  Created: 2014-10-23
 *   Author: Urban Wallasch
 *
 * See LICENSE file for more details.
 *
 * TODO:
 *  - Use cubic Bezier curves for elliptical path arcs
 *  - Epsilon optimizations for path
 *  - Improve error handling/propagation
 *  - Implement clippath
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <float.h>
#include <math.h>

#include <strings.h>
#include <unistd.h>

#include "nxml.h"
#include "colors.h"
#include "vect.h"
#include "version.h"


#ifdef DEBUG
#include <assert.h>
#define WHOAMI()		fprintf( stderr, "%s:%d:%s\n", __FILE__, __LINE__, __func__ )
#define DPRINT(...)		do { fprintf( stderr, __VA_ARGS__ ); } while(0)
#define IPRINT(...)		do { fprintf( stderr, "%*s", (int)(stacktop * 4), "" ); \
							 fprintf( stderr, __VA_ARGS__ ); } while(0)
#else
#define assert(...)
#define WHOAMI()
#define DPRINT(...)
#define IPRINT(...)
#endif

/************************************************************
 *	Math constants and helper
 */

#ifndef M_PI
	#define M_PI 3.14159265358979323846
#endif
#define DEG2RAD(D)	((double)(D)*M_PI/180.0)
#define RAD2DEG(R)	((double)(R)*180.0/M_PI)

/*
 * Optimized cubic Bezier curve ellipse approximation factor.
 * Ref: http://spencermortensen.com/articles/bezier-circle/
 */
#define BEZIER_CIRC 	0.551915024494

/*
 * For the elliptical arc approximation we generate one line segment
 * per specified units of estimated arc length.
 */
#define DFLT_ARCLINE	4.0

/*
 * Dimensions smaller than EPSILON are treated as zero by select
 * operations to enable some trivial shortcuts and optimizations.
 * Chose a value considerably smaller than 1 to allow for upscaling
 * without introducing massive errors!
 */
#define DFLT_EPSILON	0.001



/************************************************************
 *	Config and helper
 */

#define MAX_FPREC	5

static struct {
	int ass_mode;
	int ass_fprec;
	int ass_layer;
	int ass_scale_exp;
	int ass_scale;
	const char *ass_style;	// style name for event prefix
	const char *ass_actor;	// actor name for event prefix
	const char *ass_start;	// start time
	const char *ass_end;	// end time
	double epsilon;
	double arcline;
	FILE *of;
	const char *progname;
} config = {
	1,
	1,
	0,
	1,
	1,
	"Default",
	"",
	"0:00:00.00",
	"0:00:01.00",
	DFLT_EPSILON,
	DFLT_ARCLINE,
	NULL,
	"svg2ass",
};

enum {
	ELVL_INFO,
	ELVL_WARNING,
	ELVL_ERROR,
	ELVL_FATAL,
};

static int usage( const char *progname, int version_only );

static int err( int lvl, int usg, const char *fmt, ... )
{
	char *m = "";
	va_list arglist;

	va_start( arglist, fmt );
	switch ( lvl )
	{
	case ELVL_INFO:		break;
	case ELVL_WARNING:	m = "WARNING: "; break;
	case ELVL_ERROR:	/* no break */
	default:			m = "ERROR: "; break;
	}
	fputs( m, stderr );
	vfprintf( stderr, fmt, arglist );
	fputs( "\n", stderr );
	if ( usg )
		usage( config.progname, 0 );
	va_end( arglist );
	if ( ELVL_FATAL <= lvl )
		exit( EXIT_FAILURE );
	return 0;
}


/************************************************************
 *	Context stack
 */

#define CTX_STACKSZ_INC		100

typedef struct {
	int in_svg;
	vec_t org;			// origin
	mtx_t ctm;			// current transformation matrix
	unsigned f_col;		// fill color
	unsigned f_alpha;	// fill aplpha
	unsigned s_col;		// stroke color
	unsigned s_alpha;	// stroke alpha
	double s_width;		// stroke width
} ctx_t;

static ctx_t *stack = NULL;
static size_t stacksz = 0;
static size_t stacktop = 0;

static int ctx_push( ctx_t *ctx )
{
	if ( stacktop + 1 > stacksz )
	{
		ctx_t *p;
		if ( NULL == ( p = realloc( stack, ( stacksz + CTX_STACKSZ_INC ) * sizeof *stack ) ) )
			return -1;
		stack = p;
		stacksz += CTX_STACKSZ_INC;
	}
	stack[stacktop++] = *ctx;
	return 0;
}

static int ctx_pop( ctx_t *ctx )
{
	if ( stacktop < 1 )
		return -1;
	*ctx = stack[--stacktop];
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
 *	Round floating point number to specified precision,
 *	strip trailing zero fractional component.
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

/*
 *	Formatted FP output for scalars and vector components, which are
 * 	transforned using the current transformation matrix.
 */
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
				v = vec_mmul( ctx->ctm, v );
				v = vec_scal( v, config.ass_scale );
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

enum {
	ASS_COMMENT = -1,
	ASS_CLOSE = 0,
	ASS_START = 1,
};

/*
 *  Start / finalize ASS drawing line
 */
static inline int ass_line( ctx_t *ctx, int mode )
{
	static int is_open = 0;
	static int did_comment = 0;

	if ( ASS_COMMENT == mode && !did_comment )
	{
		#if 0
		// TODO: Aegisub is unable to handle pasted comment lines???
		emit( "Comment: 0,%s,%s,%s,,0,0,0,,Generated by svg2ass %s-%s\n",
				config.ass_start, config.ass_end, config.ass_style,
				VERSION, SVNVER );
		#endif
		did_comment = 1;
	}
	else if ( ASS_START == mode && !is_open )	// start a new ASS line
	{
		//Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
		emit( "Dialogue: %d,%s,%s,%s,%s,0,0,0,,", config.ass_layer,
				config.ass_start, config.ass_end,
				config.ass_style, config.ass_actor );
		emit( "{\\an7\\1c&H%06X&\\1a&H%02X&\\3c&H%06X&\\3a&H%02X&",
				ctx->f_col, ctx->f_alpha, ctx->s_col, ctx->s_alpha );
		emitf( ctx, "\\bord%f\\shad0", ctx->s_width );
		emit( "\\p%d}", config.ass_scale_exp );
		is_open = 1;
		++config.ass_layer;
	}
	else if ( ASS_CLOSE == mode && is_open )	// close ASS line
	{
		emit( "{\\p0}\n" );
		is_open = 0;
	}
	return is_open;
}


/************************************************************
 *	Attribute parser
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

static inline const char *skip( const char *str, const char *skip )
{
	while ( *str && strchr( skip, *str ) )
		++str;
	return str;
}

static int parseStyles( ctx_t *ctx, const nxmlNode_t *node )
{
	unsigned nocol = 0;
	const char *s;

	// parse presentation attributes
	IPRINT( "style (presentation attribute)\n" );
	if ( NULL != ( s = getStringAttr( node, "fill" ) ) )
	{
		IPRINT( "    fill=%s\n", s );
		if ( strstr( s, "none" ) )
			nocol |= 1;
		else
		{
			nocol &= ~1;
			ctx->f_col = convColorBGR( s );
		}
	}
	if ( NULL != ( s = getStringAttr( node, "stroke" ) ) )
	{
		IPRINT( "    stroke=%s\n", s );
		if ( strstr( s, "none" ) )
			nocol |= 2;
		else
		{
			nocol &= ~2;
			ctx->s_col = convColorBGR( s );
		}
	}
	if ( NULL != ( s = getStringAttr( node, "fill-opacity" ) ) )
	{
		IPRINT( "    fill-opacity=%s\n", s );
		ctx->f_alpha = 255 - atof( s ) * 255;
	}
	if ( NULL != ( s = getStringAttr( node, "stroke-opacity" ) ) )
	{
		IPRINT( "    stroke-opacity=%s\n", s );
		ctx->s_alpha = 255 - atof( s ) * 255;
	}
	if ( NULL != ( s = getStringAttr( node, "stroke-width" ) ) && *s  )
	{
		IPRINT( "    stroke-width=%s\n", s );
		ctx->s_width = atof( s );
	}

	// parse inline CSS
	IPRINT( "style (inline CSS)\n" );
	if ( NULL != ( s = getStringAttr( node, "style" ) ) && *s )
	{
		int n = 0;
		char name[100];
		char value[100];

		while ( sscanf( s, " %99[^ :] : %99[^ ;] %n", name, value, &n ) == 2 )
		{
			s = skip( s + n, ";" );
			IPRINT( "    %s=%s\n", name, value );
			if ( 0 == strcasecmp( name, "fill" ) )
			{
				if ( 0 == strcasecmp( value, "none" ) )
					nocol |= 1;
				else
				{
					nocol &= ~1;
					ctx->f_col = convColorBGR( value );
				}
			}
			else if ( 0 == strcasecmp( name, "stroke" ) )
			{
				if ( 0 == strcasecmp( value, "none" ) )
					nocol |= 2;
				else
				{
					nocol &= ~2;
					ctx->s_col = convColorBGR( value );
				}
			}
			else if ( 0 == strcasecmp( name, "fill-opacity" ) )
				ctx->f_alpha = 255 - atof( value ) * 255;
			else if ( 0 == strcasecmp( name, "stroke-opacity" ) )
				ctx->s_alpha = 255 - atof( value ) * 255;
			else if ( 0 == strcasecmp( name, "stroke-width" ) )
				ctx->s_width = atof( value );
		}
	}

	// a color set to "none" is emulated by setting full transparency
	if ( nocol & 1 )
		ctx->f_alpha = 255;
	if ( nocol & 2 )
		ctx->s_alpha = 255;
	//IPRINT( "    fill #%06x %u; stroke #%06x %u %g\n", ctx->f_col, ctx->f_alpha, ctx->s_col, ctx->s_alpha, ctx->s_width );
	return 0;
}

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
			a = sscanf( s, " ( %lf %n", &m.e, &n );
			if ( 1 == a )
			{
				s = skip( s + n, "," );
				if ( 1 == sscanf( s, " %lf%n", &m.f, &n ) )
					s += n;
			}
		}
		else if ( 0 == strcasecmp( op, "scale" ) )
		{
			a = sscanf( s, " ( %lf %n", &m.a, &n );
			if ( 1 == a )
			{
				s = skip( s + n, "," );
				m.d = m.a;
				if ( 1 == sscanf( s, " %lf%n", &m.d, &n ) )
					s += n;
			}
		}
		else if ( 0 == strcasecmp( op, "rotate" ) )
		{
			mtx_t r = MTX_UNI;
			a = sscanf( s, " ( %lf%n", &phi, &n );
			if ( 1 == a )
			{
				s += n;
				phi = DEG2RAD(phi);
				r.a = r.d = cos( phi );
				r.b = sin( phi );
				r.c = -r.b;
				if ( 2 == sscanf( s, " , %lf, %lf%n", &m.e, &m.f, &n )
				  || 2 == sscanf( s, " %lf %lf%n", &m.e, &m.f, &n ) )
				{
					s += n;
					ctx->ctm = mtx_mmul( ctx->ctm, m );
					ctx->ctm = mtx_mmul( ctx->ctm, r );
					m.e = -m.e;
					m.f = -m.f;
				}
				else
					m = r;
			}
		}
		else if ( 0 == strcasecmp( op, "skewX" ) )
		{
			a = sscanf( s, " ( %lf%n", &phi, &n );
			if ( 1 == a )
			{
				s += n;
				m.c = tan( DEG2RAD(phi) );
			}
		}
		else if ( 0 == strcasecmp( op, "skewY" ) )
		{
			a = sscanf( s, " ( %lf%n", &phi, &n );
			if ( 1 == a )
			{
				s += n;
				m.b = tan( DEG2RAD(phi) );
			}
		}
		else if ( 0 == strcasecmp( op, "matrix" ) )
		{
			if ( 6 == sscanf( s, " ( %lf , %lf , %lf , %lf , %lf , %lf%n",
								&m.a, &m.b, &m.c, &m.d, &m.e, &m.f, &n )
			  || 6 == sscanf( s, " ( %lf %lf %lf %lf %lf %lf%n",
								&m.a, &m.b, &m.c, &m.d, &m.e, &m.f, &n ) )
			{
				a = 1;
				s += n;
			}
		}
		if ( 0 < a )
		{
			ctx->ctm = mtx_mmul( ctx->ctm, m );
			IPRINT( "    %s(%g,%g,%g,%g,%g,%g) --> CTM(%g,%g,%g,%g,%g,%g)\n",
					op, m.a, m.b, m.c, m.d, m.e, m.f,
					ctx->ctm.a, ctx->ctm.b, ctx->ctm.c,
					ctx->ctm.d, ctx->ctm.e, ctx->ctm.f );
		}
		sscanf( s, "%*[ 0-9.)]%n", &n );
		s += n;
	}
	return 0;
}


/************************************************************
 *	SVG parsing and ASS drawing
 */

static int ass_roundrect( ctx_t *ctx, vec_t o, vec_t d, vec_t r )
{
	int res = 0;
	vec_t c, v0, v1, v2, v3, rq;
	int h_edge, v_edge;

	if ( d.x / 2 < r.x )
		r.x = d.x / 2;
	if ( d.y / 2 < r.y )
		r.y = d.y / 2;

	if ( config.epsilon > r.x && config.epsilon > r.y )	// square corner shortcut
	{
		if ( config.epsilon > d.x && config.epsilon > d.y )	// tiny extent optimization
			res = emitf( ctx, "m %v l %v ", o, VEC( o.x+config.epsilon, o.y ) );
		else
			res = emitf( ctx, "m %v l %v %v %v ", o, (vec_t){o.x+d.x, o.y},
						vec_add( o, d ), (vec_t){o.x, o.y+d.y} );
		return res;
	}

	// prepare parameters, move to start position
	rq = vec_scal( r, BEZIER_CIRC );
	h_edge = config.epsilon < ( d.x - 2 * r.x );
	v_edge = config.epsilon < ( d.y - 2 * r.y );
	v0.x = o.x + r.x;	v0.y = o.y;
	res += emitf( ctx, "m %v ", v0 );

	if ( h_edge )
	{	// upper edge
		v0.x = o.x + d.x - r.x;	v0.y = o.y;
		res += emitf( ctx, "l %v ", v0 );
	}
	c.x = v0.x;			c.y = v0.y + r.y;
	v1.x = c.x + rq.x;	v1.y = c.y - r.y;
	v2.x = c.x + r.x;	v2.y = c.y - rq.y;
	v3.x = c.x + r.x;	v3.y = c.y;
	res += emitf( ctx, "b %v %v %v ", v1, v2, v3 );

	if ( v_edge )
	{	// right edge
		v0.x = o.x + d.x;	v0.y = o.y + d.y - r.y;
		res += emitf( ctx, "l %v ", v0 );
	}
	else
		v0 = v3;
	c.x = v0.x - r.x;	c.y = v0.y;
	v1.x = c.x + r.x;	v1.y = c.y + rq.y;
	v2.x = c.x + rq.x;	v2.y = c.y + r.y;
	v3.x = c.x;			v3.y = c.y + r.y;
	res += emitf( ctx, "b %v %v %v ", v1, v2, v3 );

	if ( h_edge )
	{	// lower edge
		v0.x = o.x + r.x;	v0.y = o.y + d.y;
		res += emitf( ctx, "l %v ", v0 );
	}
	else
		v0 = v3;
	c.x = v0.x;			c.y = v0.y - r.y;
	v1.x = c.x - rq.x;	v1.y = c.y + r.y;
	v2.x = c.x - r.x;	v2.y = c.y + rq.y;
	v3.x = c.x - r.x;	v3.y = c.y;
	res += emitf( ctx, "b %v %v %v ", v1, v2, v3 );

	if ( v_edge )
	{	// left edge
		v0.x = o.x;			v0.y = o.y + r.y;
		res += emitf( ctx, "l %v ", v0 );
	}
	else
		v0 = v3;
	c.x = v0.x + r.x;	c.y = v0.y;
	v1.x = c.x - r.x;	v1.y = c.y - rq.y;
	v2.x = c.x - rq.x;	v2.y = c.y - r.y;
	v3.x = c.x;			v3.y = c.y - r.y;
	res += emitf( ctx, "b %v %v %v ", v1, v2, v3 );

	return res;
}

static int ass_ellipse( ctx_t *ctx, vec_t c, vec_t r )
{
	if ( config.epsilon > r.x && config.epsilon > r.y )
	{	// tiny radius shortcut
		return emitf( ctx, "m %v l %v ", c, vec_add( c, VEC(1,0) ) );
	}
	// Ellipses are basically just degenerate rounded rects.
	return ass_roundrect( ctx, vec_sub( c, r ), vec_scal( r, 2.0 ), r );
}

static int ass_arc( ctx_t *ctx, vec_t v0, vec_t r, double phi, int fa, int fs, vec_t v )
{
	// Draw an elliptical arc, Ref:
	// http://www.w3.org/TR/SVG/implnote.html#ArcSyntax

	// Validate and normalize parameters:
	// F.6.2 drop arc, if endpoints identical
	if ( vec_eq( v0, v, 0 ) )
		return 0;
	// F.6.6 Step 1: Ensure radii are non-zero (otherwise draw straight line)
	if ( config.epsilon > r.x || config.epsilon > r.y || vec_eq( v0, v, config.epsilon ) )
		return emitf( ctx, "l %v ", v );
	// F.6.6 Step 2: Ensure radii are positive
	r.x = fabs( r.x );
	r.y = fabs( r.y );
	// F.6.6 Step 3: Ensure radii are large enough
	//////////////
	// TODO: If rx, ry and φ are such that there is no solution
	// (basically, the ellipse is not big enough to reach from (x1, y1)
	// to (x2, y2)) then the ellipse is scaled up uniformly until
	// there is exactly one solution (until the ellipse is just big enough).
	//////////////
	// F.6.2 rotation angle mod 360
	phi = DEG2RAD( fmod( phi, 360.0 ) );
	// F.6.2 normalize flags
	fa = !!fa;
	fs = !!fs;

	// F.6.5 Conversion from endpoint to center parameterization, Ref:
	// http://www.w3.org/TR/SVG/implnote.html#ArcConversionEndpointToCenter

	double f, t, t1, dt;
	vec_t p, cp, h1, h2;
	vec_t c;
	mtx_t rot = MTX( cos(phi), sin(phi), 0,  -sin(phi), cos(phi), 0 );

	// F.6.5 Step 1: Compute (x1′, y1′)
	p = vec_mmul( rot, vec_scal( vec_sub( v0, v ), 0.5 ) );
	// F.6.5 Step 2: Compute (cx′, cy′)
	f = sqrt( fabs( (r.x*r.x*r.y*r.y - r.x*r.x*p.y*p.y - r.y*r.y*p.x*p.x)
		/ (r.x*r.x*p.y*p.y + r.y*r.y*p.x*p.x) ) );
	cp = vec_scal( VEC( r.x*p.y/r.y, -r.y*p.x/r.x ), fa==fs?-f:f );

	// F.6.5 Step 3: Compute (cx, cy) from (cx′, cy′)
	rot.b = -rot.b;
	rot.c = -rot.c;
	c = vec_add( vec_mmul( rot, cp ), vec_scal( vec_add( v0, v ), 0.5 ) );

	// F.6.5 Step 4: Compute θ1 and Δθ
	h1 = VEC( (p.x-cp.x)/r.x, (p.y-cp.y)/r.y );
	h2 = VEC( (-p.x-cp.x)/r.x, (-p.y-cp.y)/r.y );
	t1 = vec_ang( VEC(1,0), h1 );
	dt = vec_ang( h1, h2 );

	// Perform the sweep in specified direction and draw arc segments
	double step = config.arcline * 2 / ( r.x + r.y );
	// TODO: use bezier curves instead of lines
	emit( "l " );
	if ( fs )
	{
		if ( 0.0 > dt )
			dt += M_PI*2;
		//DPRINT( "fa=%d, fs=%d, t1=%g, dt=%g, step=%g\n", fa, fs, RAD2DEG(t1), RAD2DEG(dt), RAD2DEG(step) );
		for ( t = 0.0; t < dt; t += step )
		{
			p = vec_add( vec_mmul( rot, VEC( r.x*cos(t1+t), r.y*sin(t1+t) ) ), c );
			emitf( ctx, "%v ", p );
		}
	}
	else
	{
		if ( 0.0 < dt )
			dt -= M_PI*2;
		//DPRINT( "fa=%d, fs=%d, t1=%g, dt=%g, step=%g\n", fa, fs, RAD2DEG(t1), RAD2DEG(dt), RAD2DEG(step) );
		for ( t = 0.0; t > dt; t -= step )
		{
			p = vec_add( vec_mmul( rot, VEC( r.x*cos(t1+t), r.y*sin(t1+t) ) ), c );
			emitf( ctx, "%v ", p );
		}
	}
	return emitf( ctx, "%v", v );
}

/*
 * Path parser logic shamelessly stolen from libsvgtiny:
 * http://www.netsurf-browser.org/projects/libsvgtiny/
 */
static int ass_path( ctx_t *ctx, const char *pd )
{
	int res = 0;
	char *s, *d;
	vec_t last = ctx->org;
	vec_t last_cubic = last;
	vec_t last_quad = last;
	vec_t subpath_first = last;

	if ( !pd || !*pd )
		return 0;
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
					v = vec_add( v, last );
				res = ass_arc( ctx, last, r, rot, larc, swp, v );
				last_cubic = last_quad = last = v;
				s += n;
			}
			while ( sscanf(s, "%lf%lf%lf%d%d%lf%lf %n", &r.x, &r.y,
						&rot, &larc, &swp, &v.x, &v.y, &n ) == 7 );
		}
		/* invalid path syntax */
		else {
			err( ELVL_WARNING, 0, "parsePath failed at \"%s\"", s );
			errno = EINVAL;
			res = -1;
			break;
		}
	}
	free( d );
	return res;
}

static int ass_polyline( ctx_t *ctx, const char *pt )
{
	int res = -1;
	int n;
	char *d;
	float dummy;

	if ( pt && *pt
		&& sscanf( pt, "%f ,%f %n", &dummy, &dummy, &n ) == 2
		&& NULL != ( d = malloc( strlen( pt ) + 4 + 1 ) ) )
	{
		strcpy( d, "M " );
		strncat( d, pt, n );
		strcat( d, "L " );
		strcat( d, pt + n );
		res = ass_path( ctx, d );	// Ain't we sneaky?
		free( d );
	}
	return res;
}

/*
 * Callback function for XML parser
 */
static int svg2ass( nxmlEvent_t evt, const nxmlNode_t *node, void *usr )
{
	int res = 0;
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
				err( ELVL_WARNING, 0, "nested <svg> element!" );
			ctx->in_svg++;
		}
		if ( !ctx->in_svg )
			break;
		IPRINT( "<%s \n", node->name );
		if ( 0 != ctx_push( ctx ) )
			err( ELVL_FATAL, 0, "context stack push: %s", strerror( errno ) );

		if ( 0 == strcasecmp( node->name, "svg" ) )
		{
			parseStyles( ctx, node );
			parseTransform( ctx, getStringAttr( node, "transform" ) );
		}
		else if ( 0 == strcasecmp( node->name, "g" ) )
		{
			parseStyles( ctx, node );
			parseTransform( ctx, getStringAttr( node, "transform" ) );
		}
		else if ( 0 == strcasecmp( node->name,  "line" ) )
		{
			parseStyles( ctx, node );
			parseTransform( ctx, getStringAttr( node, "transform" ) );
			ass_line( ctx, ASS_START );
			v1.x = ctx->org.x + getNumericAttr( node, "x1" );
			v1.y = ctx->org.y + getNumericAttr( node, "y1" );
			v2.x = ctx->org.x + getNumericAttr( node, "x2" );
			v2.y = ctx->org.y + getNumericAttr( node, "y2" );
			IPRINT( "x1=%g, y1=%g, x2=%g, y2=%g\n", v1.x, v1.y, v2.x, v2.y );
			res = emitf( ctx, "m %v l %v ", v1, v2 );
		}
		else if ( 0 == strcasecmp( node->name, "rect" ) )
		{
			parseStyles( ctx, node );
			parseTransform( ctx, getStringAttr( node, "transform" ) );
			ass_line( ctx, 	ASS_START );
			v1.x = ctx->org.x + getNumericAttr( node, "x" );
			v1.y = ctx->org.y + getNumericAttr( node, "y" );
			v2.x = getNumericAttr( node, "width" );
			v2.y = getNumericAttr( node, "height" );
			r.x = getNumericAttr( node, "rx" );
			r.y = getNumericAttr( node, "ry" );
			if ( 0 > r.x )	r.x = 0;
			if ( 0 > r.y )	r.y = 0;
			if ( 0 == r.x )	r.x = r.y;
			if ( 0 == r.y )	r.y = r.x;
			IPRINT( "x=%g, y=%g, w=%g, h=%g, rx=%f, ry=%f\n",
						v1.x, v1.y, v2.x, v2.y, r.x, r.y );
			res = ass_roundrect( ctx, v1, v2, r );
		}
		else if ( 0 == strcasecmp( node->name, "circle" ) )
		{
			parseStyles( ctx, node );
			parseTransform( ctx, getStringAttr( node, "transform" ) );
			ass_line( ctx, ASS_START );
			c.x = ctx->org.x + getNumericAttr( node, "cx" );
			c.y = ctx->org.y + getNumericAttr( node, "cy" );
			r.x = r.y = getNumericAttr( node, "r" );
			IPRINT( "x=%g, y=%g, r=%g\n", c.x, c.y, r.x );
			res = ass_ellipse( ctx, c, r );
		}
		else if ( 0 == strcasecmp( node->name, "ellipse" ) )
		{
			parseStyles( ctx, node );
			parseTransform( ctx, getStringAttr( node, "transform" ) );
			ass_line( ctx, ASS_START );
			c.x = ctx->org.x + getNumericAttr( node, "cx" );
			c.y = ctx->org.y + getNumericAttr( node, "cy" );
			r.x = getNumericAttr( node, "rx" );
			r.y = getNumericAttr( node, "ry" );
			IPRINT( "x=%g, y=%g, rx=%g, ry=%g\n", c.x, c.y, r.x, r.y );
			res = ass_ellipse( ctx, c, r );
		}
		else if ( 0 == strcasecmp( node->name, "path" ) )
		{
			parseStyles( ctx, node );
			parseTransform( ctx, getStringAttr( node, "transform" ) );
			ass_line( ctx, ASS_START );
			res = ass_path( ctx, getStringAttr( node, "d" ) );
		}
		else if ( 0 == strcasecmp( node->name, "polyline" )
				|| 0 == strcasecmp( node->name, "polygon" ) )
		{
			parseStyles( ctx, node );
			parseTransform( ctx, getStringAttr( node, "transform" ) );
			ass_line( ctx, ASS_START );
			res = ass_polyline( ctx, getStringAttr( node, "points" ) );
		}
		else
		{
			//IPRINT( "*ignored*\n" );
		}
		break;

	case NXML_EVT_CLOSE:
		if ( 0 == strcasecmp( node->name, "svg" ) )
		{
			if ( !ctx->in_svg )
				err( ELVL_WARNING, 0, "excess </svg> element!" );
			ctx->in_svg--;
		}
		ctx_pop( ctx );
		if ( !ctx->in_svg )
			break;
		IPRINT( "/>\n" );
		break;

	default:
		break;
	}
	if ( 1 == config.ass_mode )
		ass_line( ctx, ASS_CLOSE );
	if ( 0 != res )
	{	// report errors, but keep going!
		err( ELVL_ERROR, 0, "%s: %s", __func__, strerror( errno ) );
		res = 0;
	}
	return res;
}


/************************************************************
 *	Main program stuff
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
	char *svg = NULL;
	size_t sz = 0;
	ctx_t ctx;

	if ( 0 != getFile( &svg, &sz, 4000, fp ) )
	{
		err( ELVL_WARNING, 0, "getFile: %s", strerror( errno ) );
		free( svg );
		return -1;
	}
	// initialize context
	memset( &ctx, 0, sizeof ctx );
	ctx.org = VEC_ZERO;
	ctx.ctm = MTX_UNI;
	ass_line( &ctx, ASS_COMMENT );
	// do some real work
	res = nxmlParse( svg, svg2ass, &ctx );
	// clean up
	ass_line( NULL, ASS_CLOSE );
	while ( 0 == ctx_pop( &ctx ) )
		;	// in case we've read an incomplete document
	free( svg );
	return res;
}

static int usage( const char *progname, int version_only )
{
	char *p;

	if ( NULL != ( p = strrchr( progname, '/' ) ) )
		progname = ++p;
	fprintf( stderr, "%s SVG to ASS converter version %s-%s\n", progname, VERSION, SVNVER );
	if ( version_only )
		return 0;
	fprintf( stderr, "Usage: %s options [file1 [options file2 ...]]\n", progname );
	fprintf( stderr,
		"If no filename is specified, or filename equals '-', input is read from stdin.\n"
		"Options apply to all subsequent input files, unless explicitly altered.\n"
		"General Options:\n"
		"  -h Print this usage message and exit.\n"
		"  -v Print version info and exit.\n"
		"  -o file\n"
		"     Write output to file; default: write to stdout.\n"
		"ASS Options:\n"
		"  -a num\n"
		"     ASS mode, 0 = single draw command per file, 1 = one line per shape; default: 1\n"
		"     Mode 0 will use the same color for all shapes!\n"
		"  -e num\n"
		"     Set the epsilon used for element optimizations; default: %g\n"
		"  -f num\n"
		"     Numerical precision for ASS output in the range 0-%d; default: 1\n"
		"     Zero fractional parts are stripped. Does not affect internal math precision.\n"
		"  -L num\n"
		"     ASS dialog initial layer; default: 0\n"
		"     Layer is incremented for each output line, spanning input files.\n"
		"     If this is undesirable, specify multiple -L options, one per input file.\n"
		"  -S string\n"
		"     ASS dialog start time in H:MM:SS.CC format, not validated; default: 0:00:00.00\n"
		"  -E string\n"
		"     ASS dialog end time in H:MM:SS.CC format, not validated; default: 0:00:01.00\n"
		"  -A string\n"
		"     ASS dialog actor name; default: empty\n"
		"  -T string\n"
		"     ASS dialog style name; default: Default\n"
		"Experimental:\n"
		"  -p num\n"
		"     Set ASS draw mode scaling. This only affects the ASS \\p<num> tags in the\n"
		"     output and expects an appropriately pre-scaled SVG. Default: 1\n"
		"  -s num\n"
		"     Same as -p, but additionally scale up the coordinates by a factor of 2^(num-1),\n"
		"     ultimately resulting in a 1:1 mapping, but with increased ASS internal accuracy.\n"
		"  -z num\n"
		"     For the elliptical arc approximation generate one line segment per num units\n"
		"     of estimated arc length; default: %g\n"
		, DFLT_EPSILON
		, MAX_FPREC
		, DFLT_ARCLINE
	);
	return 0;
}


int main( int argc, char** argv )
{
	int nfiles = 0;
	int opt;
	const char *ostr = "-:a:e:p:s:z:f:ho:vA:E:L:S:T:";
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
				err( ELVL_FATAL, 0, "fopen '%s': %s", optarg, strerror( errno ) );
			if ( 0 != parse( ifp ) )
				err( ELVL_FATAL, 0, "parsing file '%s'", optarg );
			++nfiles;
			break;
		case 'a':
			config.ass_mode = atoi( optarg );
			break;
		case 'e':
			config.epsilon = atof( optarg );
			break;
		case 'p':
			config.ass_scale_exp = atoi( optarg );
			if ( 1 > config.ass_scale_exp )
				err( ELVL_FATAL, 1, "argument for option -p out of range" );
			break;
		case 's':
			config.ass_scale_exp = atoi( optarg );
			if ( 1 > config.ass_scale_exp )
				err( ELVL_FATAL, 1, "argument for option -s out of range" );
			config.ass_scale = 1U << (config.ass_scale_exp - 1);
			break;
		case 'z':
			config.arcline = atof( optarg );
			break;
		case 'f':
			config.ass_fprec = atoi( optarg );
			if ( 0 > config.ass_fprec || MAX_FPREC < config.ass_fprec )
				err( ELVL_FATAL, 1, "argument for option -f out of range" );
			break;
		case 'o':
			DPRINT( "writing to file '%s'\n", optarg );
			if ( NULL == ( config.of = fopen( optarg, "w" ) ) )
				err( ELVL_FATAL, 0, "fopen '%s': %s", optarg, strerror( errno ) );
			break;
		case 'h':
			usage( argv[0], 0 );
			exit( EXIT_SUCCESS );
			break;
		case 'v':
			usage( argv[0], 1 );
			exit( EXIT_SUCCESS );
			break;
		case 'A':
			config.ass_actor = optarg;
			break;
		case 'E':
			config.ass_end = optarg;
			break;
		case 'L':
			config.ass_layer = atoi( optarg );
			break;
		case 'S':
			config.ass_start = optarg;
			break;
		case 'T':
			config.ass_style = optarg;
			break;
		case ':':
			err( ELVL_FATAL, 1, "missing argument for option '%c'", optopt );
			break;
		case '?':
		default:
			err( ELVL_FATAL, 1, "unrecognized option '%c'", optopt );
			break;
		}
	}

	if ( !nfiles )
	{
		DPRINT( "reading from <stdin>\n" );
		if ( 0 != parse( stdin ) )
			err( ELVL_FATAL, 0, "parsing <stdin>" );
		++nfiles;
	}
	DPRINT( "%d file%s processed\n", nfiles, nfiles == 1 ? "" : "s" );
	exit( EXIT_SUCCESS );
}

/* EOF */
