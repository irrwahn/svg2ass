/*
 *    File: vect.h
 * Created: 2014-10-27
 *  Author: uw
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
	double a, c, e;
	double b, d, f;
	   // 0, 0, 1;
} mtx_t;


#define VEC_ZERO	((vec_t){ 0, 0 })
#define MTX_ZERO	((mtx_t){ 0, 0, 0,  0, 0, 0 })
#define MTX_UNI		((mtx_t){ 1, 0, 0,  0, 1, 0 })


vec_t vec_add( vec_t a, vec_t b );
vec_t vec_sub( vec_t a, vec_t b );
vec_t vec_scal( vec_t a, double f );

vec_t mtx_vmul( mtx_t m, vec_t v );
mtx_t mtx_mmul( mtx_t m, mtx_t n );

#ifdef __cplusplus
	}
#endif

#endif	// H_VECT_INCLUDED

/* EOF */
