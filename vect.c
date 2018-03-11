/*
 * Naive 2-D graphics vector and matrix implementation.
 *
 * Project: svg2ass
 *    File: vect.c
 * Created: 2014-10-27
 *  Author: Urban Wallasch
 *
 * See LICENSE file for more details.
 */

#include <math.h>

#include "vect.h"


vec_t vec_add( vec_t u, vec_t v )
{
	return VEC( u.x + v.x, u.y + v.y );
}

vec_t vec_sub( vec_t u, vec_t v )
{
	return VEC( u.x - v.x, u.y - v.y );
}

vec_t vec_scal( vec_t v, double f )
{
	return VEC( v.x * f, v.y * f );
}

vec_t vec_mmul( mtx_t m, vec_t v )
{
	return VEC ( m.a * v.x + m.c * v.y + m.e,
				 m.b * v.x + m.d * v.y + m.f );
}

vec_t vec_norm( vec_t v, int dir )
{
	v = dir ? VEC( -v.y, v.x ) : VEC( v.y, -v.x );
	return vec_scal( v, 1.0 / vec_abs( v ) );
}

double vec_abs( vec_t v )
{
	return sqrt( v.x * v.x + v.y * v.y );
}

double vec_dot( vec_t u, vec_t v )
{
	return u.x * v.x + u.y * v.y;
}

double vec_ang( vec_t u, vec_t v )
{
	double sign = ( ( u.x * v.y - u.y * v.x ) < 0.0 ) ? -1.0 : 1.0;
	double t = vec_dot( u, v ) / ( vec_abs( u ) * vec_abs( v ) );
	return acos( t ) * sign;
}

int vec_eq( vec_t u, vec_t v, double e )
{
	return vec_abs( vec_sub( u, v ) ) <= fabs( e );
}

mtx_t mtx_mmul( mtx_t m, mtx_t n )
{
	mtx_t r;
	r.a = m.a * n.a + m.c * n.b;
	r.b = m.b * n.a + m.d * n.b;
	r.c = m.a * n.c + m.c * n.d;
	r.d = m.b * n.c + m.d * n.d;
	r.e = m.a * n.e + m.c * n.f + m.e;
	r.f = m.b * n.e + m.d * n.f + m.f;
	return r;
}

/* EOF */
