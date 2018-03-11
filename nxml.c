/*
 * NXML is not an XML parser.
 *
 * Project: svg2ass
 *    File: nxml.c
 * Created: 2014-10-27
 *  Author: Urban Wallasch
 *
 *  See LICENSE file for more details.
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>

#include "nxml.h"


#ifdef DEBUG
#include <assert.h>
#define WHOAMI() 	fprintf( stderr, "%d %s\n", __LINE__, __func__ );
#define DPRINT(...) fprintf( stderr, __VA_ARGS__ );
#else
#define assert(...)
#define WHOAMI()
#define DPRINT(...)
#endif

static int is_namechar( int c )
{
	return isalnum( (unsigned char)c ) || strchr( ".-_:", c );
}
static int is_namestart( int c )
{
	return isalpha( (unsigned char)c ) || strchr( "_:", c );
}
static int is_space( int c )
{
	return isspace( (unsigned char)c );
}
static int is_quot( int c )
{
	return ( '\'' == c || '\"' == c ) ? c : 0;  // sic!
}

static char *trim( char *str )
{
	char *d, *s = str;
	int ws = 0;

	while ( is_space( *s ) )
		++s;
	for ( d = str; *s; ++s )
	{
		if ( is_space( *s ) )
		{
			if ( !ws )
			{
				*d++ = ' ';
				ws = 1;
			}
		}
		else
		{
			ws = 0;
			*d++ = *s;
		}
	}
	if ( ws )
		--d;
	*d = '\0';
	return str;
}

static inline char *parseAttrib( char *p, nxmlNode_t *node )
{
	char *m = p;
	char *ns, *ne, *vs, *ve;
	int quot = 0;

	while ( 1 )
	{
		while ( is_space( *m ) )
			++m;
		if ( !is_namestart( *m ) )
			break;
		ns = m;
		while ( is_namechar( *m ) )
			++m;
		ne = m;
		while ( is_space( *m ) )
			++m;
		if ( '=' != *m )
			break;
		++m;
		*ne = '\0';
		while ( is_space( *m ) )
			++m;
		if ( 0 == ( quot = is_quot( *m ) ) )
			break;
		++m;
		vs = m;
		while ( *m && *m != quot )
			++m;
		ve = m;
		if ( *m != quot )
			break;
		++m;
		*ve = '\0';
		if ( node->att_num >= node->att_sz )
		{
			void *p;
			p = realloc( node->att, sizeof *node->att * ( node->att_sz + 50 ) );
			if ( !p )
				break;	// TODO: error handling!
			node->att = p;
			node->att_sz += 50;
		}
		node->att[node->att_num].name = ns;
		node->att[node->att_num].val = vs;
		++node->att_num;
	}
	return m;
}

static inline char *parseMarkup( char *p, nxmlNode_t *node )
{
	int i;
	char *m = p;
	char *e = m;
	static struct {
		const char *s;
		size_t sl;
		const char *e;
		size_t el;
		nxmlTagtype_t t;
	} stag[] = {
		{ "!--",		3, 	"-->", 	3, NXML_TYPE_COMMENT },	// comment
		{ "![CDATA[",	8, 	"]]>", 	3, NXML_TYPE_CDATA },	// cdata section
		{ "?",			1, 	"?>", 	2, NXML_TYPE_PROC },	// prolog / processing instruction
		{ "!DOCTYPE",	8, 	">", 	1, NXML_TYPE_DOCTYPE },	// doctype definition
		{ NULL, 0, NULL, 0, NXML_TYPE_EMPTY },
	};

	node->type = NXML_TYPE_EMPTY;

	if ( is_namestart( *m ) ) // match parent/self tag
	{
		node->type = NXML_TYPE_PARENT;	// tentative!
		node->name = m;
		while ( is_namechar( *m ) )
			++m;
		e = m;
		m = parseAttrib( m, node );
		while ( *m && '>' != *m )
		{	// skip any broken attribute garbage!
			// TODO: match quotes?
			++m;
		}
		if ( '/' == *(m-1) )
			node->type = NXML_TYPE_SELF;
	}
	else if ( '/' == *m )	// match end tag
	{
		++m;
		if ( is_namechar( *m ) )
			node->type = NXML_TYPE_END;
		node->name = m;
		while ( is_namechar( *m ) )
			++m;
		e = m;
		while ( is_space( *m ) )
			++m;
	}
	else 	// match special tags
	{
		for ( i = 0; stag[i].s; ++i )
		{
			if ( 0 == strncasecmp( m, stag[i].s, stag[i].sl ) )
			{
				m += stag[i].sl;
				node->name = m;
				if ( NULL != ( e = strstr( m, stag[i].e ) ) )
				{
					node->type = stag[i].t;
					m = e;
				}
				else
					e = m;
				break;
			}
		}
	}

	while ( *m && '>' != *m++ )
		;
	*e = '\0';
	return m;
}

// internal parser states
enum state {
	ST_BEGIN = 0,
	ST_CONTENT,
	ST_MARKUP,
	ST_END,
	ST_STOP,
};

int nxmlParse( char *buf, nxmlCb_t cb, void *usr )
{
	int res = 0;
	char *p, *m = buf;
	enum state state = ST_BEGIN;
	nxmlNode_t node;

	memset( &node, 0, sizeof node );

	while ( ST_STOP != state )
	{
		p = m;
		node.name = "";
		node.att_num = 0;
		node.error = 0;
		switch ( state )
		{
		case ST_BEGIN:
			node.type = NXML_TYPE_EMPTY;
			res = cb( NXML_EVT_BEGIN, &node, usr );
			state = ST_CONTENT;
			break;
		case ST_END:
			node.type = NXML_TYPE_EMPTY;
			res = cb( NXML_EVT_END, &node, usr );
			state = ST_STOP;
			break;
		case ST_CONTENT:
			m = strchr( p, '<' );
			if ( m )
				*m++ = '\0';
			trim( p );
			if ( *p )
			{
				node.type = NXML_TYPE_CONTENT;
				node.name = p;
				res = cb( NXML_EVT_TEXT, &node, usr );
			}
			state = m ? ST_MARKUP : ST_END;
			break;
		case ST_MARKUP:
			m = parseMarkup( p, &node );
			if ( NXML_TYPE_EMPTY != node.type )
			{
				if ( NXML_TYPE_END != node.type )
					res = cb( NXML_EVT_OPEN, &node, usr );
				node.att_num = 0;
				if ( 0 == res && NXML_TYPE_PARENT != node.type )
					res = cb( NXML_EVT_CLOSE, &node, usr );
			}
			state = ST_CONTENT;
			break;
		case ST_STOP:	/* no break */
		default:
			// never reached!
			assert( 0 == 1 );
			break;
		}
		if ( res )
			state = ST_STOP;
	}
	return res;
}

/* EOF */
