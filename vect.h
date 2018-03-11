/*
 * Naive 2D graphics vector and matrix implementation.
 *
 * Project: svg2ass
 *    File: vect.h
 * Created: 2014-10-27
 *  Author: Urban Wallasch
 *
 * See LICENSE file for more details.
 */

#ifndef H_VECT_INCLUDED
#define H_VECT_INCLUDED

#ifdef __cplusplus
	extern "C" {
#endif


typedef struct {
	double x;
	double y;
} vec_t;

typedef struct {
	// order matches SVG transform matrix!
	double a, c, e;
	double b, d, f;
	//     0  0  1	implied!
} mtx_t;


#define VEC(X,Y)			((vec_t){(X),(Y)})
#define MTX(A,C,E,B,D,F)	((mtx_t){(A),(C),(E),(B),(D),(F)})

#define VEC_ZERO	VEC( 0, 0 )

#define MTX_ZERO	MTX( 0, 0, 0,  \
						 0, 0, 0 )

#define MTX_UNI		MTX( 1, 0, 0,  \
						 0, 1, 0 )

vec_t vec_add( vec_t u, vec_t v );
vec_t vec_sub( vec_t u, vec_t v );
vec_t vec_scal( vec_t u, double f );
vec_t vec_mmul( mtx_t m, vec_t v );
vec_t vec_norm( vec_t v, int dir );
double vec_abs( vec_t v );
double vec_dot( vec_t u, vec_t v );		// vector dot (scalar) product
double vec_ang( vec_t u, vec_t v );
int vec_eq( vec_t u, vec_t v, double e );

mtx_t mtx_mmul( mtx_t m, mtx_t n );


#ifdef __cplusplus
	}
#endif

#endif	// H_VECT_INCLUDED

/* EOF */
