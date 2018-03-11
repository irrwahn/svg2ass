/*
 * NXML is not an XML parser.
 *
 * Project: svg2ass
 *    File: nxml.h
 * Created: 2014-10-27
 *  Author: Urban Wallasch
 *
 * See LICENSE file for more details.
 */

#ifndef H_NXML_INCLUDED
#define H_NXML_INCLUDED

#ifdef __cplusplus
	extern "C" {
#endif

#include <stdlib.h>

typedef enum nxmlTagtype {
	NXML_TYPE_EMPTY = 0,
	NXML_TYPE_CONTENT,
	NXML_TYPE_PARENT,
	NXML_TYPE_SELF,
	NXML_TYPE_END,
	NXML_TYPE_COMMENT,
	NXML_TYPE_CDATA,
	NXML_TYPE_PROC,
	NXML_TYPE_DOCTYPE,
} nxmlTagtype_t;

typedef enum nxmlEvent {
	NXML_EVT_BEGIN = 0,
	NXML_EVT_END,
	NXML_EVT_TEXT,
	NXML_EVT_OPEN,
	NXML_EVT_CLOSE,
} nxmlEvent_t;

typedef struct {
	const char *name;
	const char *val;
} nxmlAttrib_t;

typedef struct {
	nxmlTagtype_t type;
	const char *name;
	nxmlAttrib_t *att;
	size_t att_num;
	size_t att_sz;
	int error;
} nxmlNode_t;

typedef int (*nxmlCb_t)( nxmlEvent_t evt, const nxmlNode_t *node, void *usr );

int nxmlParse( char *buf, nxmlCb_t cb, void *usr );

#ifdef __cplusplus
	}
#endif

#endif	// H_NXML_INCLUDED

/* EOF */
