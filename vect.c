/*
 *	Project: svg2ass	
 *    File: vect.c
 * Created: 2014-10-27
 *  Author: uw
*/

#include "vect.h"


vec_t vec_add( vec_t a, vec_t b )
{
	vec_t r;
	r.x = a.x + b.x;
	r.y = a.y + b.y;
	return r;
}

vec_t vec_sub( vec_t a, vec_t b )
{
	vec_t r;
	r.x = a.x - b.x;
	r.y = a.y - b.y;
	return r;
}

vec_t vec_scal( vec_t v, double f )
{
	vec_t r;
	r.x = v.x * f;
	r.y = v.y * f;
	return r;
}

vec_t mtx_vmul( mtx_t m, vec_t v )
{
	vec_t r;
	r.x = m.a * v.x + m.c * v.y + m.e;
	r.y = m.b * v.x + m.d * v.y + m.f;
	return r;
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
