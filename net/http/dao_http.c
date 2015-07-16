/*
// Dao Standard Modules
// http://www.daovm.net
//
// Copyright (c) 2015, Limin Fu
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// 2015-07: Danilov Aleksey, initial implementation.

#include <ctype.h>
#include <string.h>
#include <time.h>

#include"dao_http.h"

static DaoType *daox_type_header = NULL;
static DaoType *daox_type_request = NULL;
static DaoType *daox_type_response = NULL;
static DaoType *daox_type_chunkDecoder = NULL;

DaoHttpRequest* DaoHttpRequest_New()
{
	DaoHttpRequest *res = (DaoHttpRequest*)dao_malloc( sizeof(DaoHttpRequest) );
	res->method = DString_New();
	res->uri = DString_New();
	res->version = DString_New();
	res->headers = DMap_New( DAO_DATA_STRING, DAO_DATA_STRING );
	res->cookies = DList_New( DAO_DATA_STRING );
	res->size = 0;
}

void DaoHttpRequest_Delete( DaoHttpRequest *self )
{
	DString_Delete( self->method );
	DString_Delete( self->uri );
	DString_Delete( self->version );
	DMap_Delete( self->headers );
	DList_Delete( self->cookies );
	dao_free( self );
}

DaoHttpResponse* DaoHttpResponse_New()
{
	DaoHttpResponse *res = (DaoHttpResponse*)dao_malloc( sizeof(DaoHttpResponse) );
	res->version = DString_New();
	res->reason = DString_New();
	res->headers = DMap_New( DAO_DATA_STRING, DAO_DATA_STRING );
	res->cookies = DList_New( DAO_DATA_STRING );
	res->code = 0;
	res->size = 0;
}

void DaoHttpResponse_Delete( DaoHttpResponse *self )
{
	DString_Delete( self->version );
	DString_Delete( self->reason );
	DList_Delete( self->cookies );
	DMap_Delete( self->headers );
	dao_free( self );
}

const char* ParseHttpVersion( const char *context, DString *version )
{
	const char *cp;
	if ( memcmp( context, "HTTP/", 5 ) != 0 )
		return NULL;
	cp = context + 5;
	if ( !isdigit( cp[0] ) || cp[1] != '.' || !isdigit( cp[2] ) )
		return NULL;
	DString_SetBytes( version, cp, 3 );
	return cp + 3;
}

const char* ParseURI( const char *context, DString *url )
{
	const char *schars = ":/?#[]@!$&'()*+,;=%-._~";
	const char *cp = context;
	for ( ; *cp && !isspace( *cp ); ++cp )
		if ( !isalnum( *cp ) && !strchr( schars, *cp ) )
			return NULL;
	if ( url )
		DString_SetBytes( url, context, cp - context );
	return cp;
}

const char* ParseToken( const char *context, DString *token )
{
	const char *schars = "!#$%&'*+-.^_`|~";
	const char *cp = context;
	for ( ; *cp; ++cp )
		if ( !isalpha( *cp ) && !isdigit( *cp ) && !strchr( schars, *cp ) )
			break;
	if ( token )
		DString_SetBytes( token, context, cp - context );
	return cp;
}

http_err_t ParseRequestLine( DaoHttpRequest *self, const char *context, daoint *offset )
{
	const char *cp = ParseToken( context, self->method );
	if ( !self->method->size )
		return Http_InvalidMethod;
	if ( *( cp++ ) != ' ' )
		return Http_InvalidStartLine;
	cp = ParseURI( cp, self->uri );
	if ( !cp || !self->uri->size )
		return Http_InvalidUrl;
	if ( *( cp++ ) != ' ' )
		return Http_InvalidStartLine;
	cp = ParseHttpVersion( cp, self->version );
	if ( !cp )
		return Http_InvalidHttpVersion;
	if ( *( cp++ ) != '\r' || *( cp++ ) != '\n' )
		return Http_InvalidStartLine;
	if ( offset )
		*offset = cp - context;
	return 0;
}

#define IS_OWS( cp ) ( *( cp ) == ' ' || *( cp ) == '\t' )

const char* ParseReason( const char *context, DString *reason )
{
	const char *cp = context;
	for ( ; *cp; ++cp )
		if ( *cp == '\r' ){
			if ( cp[1] == '\n' ){
				DString_SetBytes( reason, context, cp - context );
				return cp + 2;
			}
			return NULL;
		}
		else if ( !IS_OWS( cp ) && !isprint( *cp ) )
			return NULL;
	return NULL;
}

http_err_t ParseStatusLine( DaoHttpResponse *self, const char *context, daoint *offset )
{
	const char *cp = ParseHttpVersion( context, self->version );
	if ( !cp )
		return Http_InvalidHttpVersion;
	if ( *( cp++ ) != ' ' )
		return Http_InvalidStartLine;
	if ( !isdigit( cp[0] ) || !isdigit( cp[1] ) || !isdigit( cp[2] ) )
		return Http_InvalidStatusCode;
	self->code = ( cp[0] - '0' )*100 + ( cp[1] - '0' )*10 + ( cp[2] - '0' );
	cp += 3;
	if ( *( cp++ ) != ' ' )
		return Http_InvalidStartLine;
	cp = ParseReason( cp, self->reason );
	if ( !cp )
		return Http_InvalidStartLine;
	if ( offset )
		*offset = cp - context;
	return 0;
}

const char* ParseFieldValue( const char *context, DString *value )
{
	const char *cp = context;
	for ( ; *cp; ++cp )
		if ( *cp == '\r' ){
			if ( cp[1] == '\n' ){
				const char *end = cp;
				for ( ; end > context && IS_OWS( end - 1 ); --end );
				DString_SetBytes( value, context, end - context );
				return cp + 2;
			}
			return NULL;
		}
		else if ( !IS_OWS( cp ) && !isprint( *cp ) )
			return NULL;
	return NULL;
}

http_err_t ParseHeader( const char *context, DMap *headers, DList *cookies, int request, daoint *offset )
{
	DString *name = DString_New();
	DString *value;
	const char *cp = ParseToken( context, name );
	if ( !name->size ){
		DString_Delete( name );
		return Http_InvalidFieldName;
	}
	if ( *( cp++ ) != ':' ){
		DString_Delete( name );
		return Http_InvalidHeader;
	}
	for ( ; IS_OWS( cp ); ++cp );
	if ( !*cp ){
		DString_Delete( name );
		return Http_InvalidHeader;
	}
	value = DString_New();
	cp = ParseFieldValue( cp, value );
	if ( !value->size ){
		DString_Delete( name );
		DString_Delete( value );
		return Http_InvalidFieldValue;
	}
	if ( !cp ){
		DString_Delete( name );
		DString_Delete( value );
		return Http_InvalidHeader;
	}
	DString_ToLower( name );
	if ( cookies && strcmp( name->chars, request? "cookie" : "set-cookie" ) == 0 )
		DList_PushBack( cookies, value ); // put cookies separately
	else {
		DNode *node = DMap_Find( headers, name );
		if ( node ){
			DString *list = DString_Copy( node->value.pString );
			DString_AppendChars( list, ", " );
			DString_Append( list, value );
			DMap_EraseNode( headers, node );
			DMap_Insert( headers, name, list );
		}
		else
			DMap_Insert( headers, name, value );
	}
	if ( offset )
		*offset = cp - context;
	DString_Delete( name );
	DString_Delete( value );
	return 0;
}

http_err_t DaoHttpRequest_Parse( DaoHttpRequest *self, DString *text, daoint end )
{
	http_err_t err;
	daoint offset;
	const char *cp = text->chars;
	if ( end <= 0 )
		end = DString_FindChars( text, "\r\n\r\n", 0 );
	if ( end < 0 )
		return Http_IncompleteMessage;
	end += 2;
	err = ParseRequestLine( self, cp, &offset );
	if ( err )
		return err;
	for ( cp += offset; cp < text->chars + end; cp += offset ){
		err = ParseHeader( cp, self->headers, self->cookies, 1, &offset );
		if ( err )
			return err;
	}
	self->size = end + 2;
	return 0;
}

http_err_t DaoHttpResponse_Parse( DaoHttpResponse *self, DString *text, daoint end )
{
	daoint offset;
	const char *cp = text->chars;
	http_err_t err;
	if ( end <= 0 )
		end = DString_FindChars( text, "\r\n\r\n", 0 );
	if ( end < 0 )
		return Http_IncompleteMessage;
	end += 2;
	err = ParseStatusLine( self, cp, &offset );
	if ( err )
		return err;
	for ( cp += offset; cp < text->chars + end; cp += offset ){
		err = ParseHeader( cp, self->headers, self->cookies, 0, &offset );
		if ( err )
			return err;
	}
	self->size = end + 2;
	return 0;
}

void SplitValue( DString *value, DaoList *list )
{
	const char *start = value->chars;
	const char *cp = start;
	int quoted = 0;
	while ( 1 ){
		if ( !*cp ){
			DaoString *str = DaoString_NewBytes( start, cp - start );
			DaoList_Append( list, (DaoValue*)str );
			break;
		}
		if ( quoted ){
			if ( *cp == '\\' ){
				++cp;
				if ( !*cp )
					continue;
			}
			else if ( *cp == '"' )
				quoted = 0;
		}
		else if ( *cp == ',' ){
			const char *end = cp;
			for ( ; end > value->chars && IS_OWS( end - 1 ); --end );
			if ( 1 ){
				DaoString *str = DaoString_NewBytes( start, end - start );
				DaoList_Append( list, (DaoValue*)str );
			}
			for ( ++cp; IS_OWS( cp ); ++cp );
			start = cp;
			continue;
		}
		if ( *cp == '"' )
			quoted = 1;
		++cp;
	}
}

time_t ParseDate(DString *date)
{
	const time_t inv_date = (time_t)-1;
	const char *dnames[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
	const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	const char *cp = date->chars;
	struct tm time = {0};
	int i, found = 0;
	time.tm_isdst = -1;
	for ( i = 0; i < 7; ++i )
		if ( strncmp( dnames[i], cp, 3 ) == 0 ){
			found = 1;
			break;
		}
	if ( !found )
		return inv_date;
	cp += 3;
	if ( *( cp++ ) != ',' || *( cp++ ) != ' ' )
		return inv_date;
	if ( !isdigit( cp[0] ) || !isdigit( cp[1] ) || cp[2] != ' ' )
		return inv_date;
	time.tm_mday = ( cp[0] - '0' )*10 + ( cp[1] - '0' );
	cp += 3;
	found = 0;
	for ( i = 0; i < 12; ++i )
		if ( strncmp( months[i], cp, 3 ) == 0 ){
			time.tm_mon = i;
			found = 1;
			break;
		}
	if ( !found )
		return inv_date;
	cp += 3;
	if ( *( cp++ ) != ' ' )
		return inv_date;
	if ( !isdigit( cp[0] ) || !isdigit( cp[1] ) || !isdigit( cp[2] ) || !isdigit( cp[3] ) || cp[4] != ' ' )
		return inv_date;
	time.tm_year = ( cp[0] - '0' )*1000 + ( cp[1] - '0' )*100 + ( cp[2] - '0' )*10 + ( cp[3] - '0' ) - 1900;
	cp += 5;
	if ( !isdigit( cp[0] ) || !isdigit( cp[1] ) || cp[2] != ':' )
		return inv_date;
	time.tm_hour = ( cp[0] - '0' )*10 + ( cp[1] - '0' );
	cp += 3;
	if ( !isdigit( cp[0] ) || !isdigit( cp[1] ) || cp[2] != ':' )
		return inv_date;
	time.tm_min = ( cp[0] - '0' )*10 + ( cp[1] - '0' );
	cp += 3;
	if ( !isdigit( cp[0] ) || !isdigit( cp[1] ) || cp[2] != ' ' )
		return inv_date;
	time.tm_sec = ( cp[0] - '0' )*10 + ( cp[1] - '0' );
	cp += 3;
	if ( strcmp( cp, "GMT" ) != 0 )
		return inv_date;
	return mktime( &time );
}

static void DaoHttpHeader_Version( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpHeader *self = (DaoHttpHeader*)DaoValue_TryGetCdata( p[0] );
	DaoProcess_PutString( proc, self->version );
}

static void DaoHttpHeader_Size( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpHeader *self = (DaoHttpHeader*)DaoValue_TryGetCdata( p[0] );
	DaoProcess_PutInteger( proc, self->size );
}

static void DaoHttpHeader_Field( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpHeader *self = (DaoHttpHeader*)DaoValue_TryGetCdata( p[0] );
	DString *field = p[1]->xString.value;
	DNode *node;
	DString_ToLower( field );
	node = DMap_Find( self->headers, field );
	if ( node )
		DaoProcess_PutString( proc, node->value.pString );
	else
		DaoProcess_PutChars( proc, "" );
}

DaoList* PutListValue( DaoProcess *proc, DMap *headers, const char *field )
{
	DString key = DString_WrapChars( field );
	DNode *node = DMap_Find( headers, &key );
	if ( node ){
		DaoList *res = DaoProcess_PutList( proc );
		SplitValue( node->value.pString, res );
		return res;
	}
	DaoProcess_PutNone( proc );
	return NULL;
}

static void DaoHttpHeader_TransEnc( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpHeader *self = (DaoHttpHeader*)DaoValue_TryGetCdata( p[0] );
	PutListValue( proc, self->headers, "transfer-encoding" );
}

static void DaoHttpHeader_ContEnc( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpHeader *self = (DaoHttpHeader*)DaoValue_TryGetCdata( p[0] );
	PutListValue( proc, self->headers, "content-encoding" );
}

static void DaoHttpHeader_ContLang( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpHeader *self = (DaoHttpHeader*)DaoValue_TryGetCdata( p[0] );
	PutListValue( proc, self->headers, "content-language" );
}

static void DaoHttpHeader_CacheCtrl( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpHeader *self = (DaoHttpHeader*)DaoValue_TryGetCdata( p[0] );
	PutListValue( proc, self->headers, "cache-control" );
}

void PutIntValue( DaoProcess *proc, DMap *headers, const char *field )
{
	DString name = DString_WrapChars( field );
	DNode *node = DMap_Find( headers, &name );
	if ( node ){
		DString *value = node->value.pString;
		char *end;
		long long len = -1;
		if ( !strchr( "+-", *value->chars ) ){
			len = strtoull( value->chars, &end, 10 );
			if ( *end || len < 0 )
				len = -1;
		}
		DaoProcess_PutInteger( proc, len );
	}
	else
		DaoProcess_PutNone( proc );
}

static void DaoHttpHeader_ContLen( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpHeader *self = (DaoHttpHeader*)DaoValue_TryGetCdata( p[0] );
	PutIntValue( proc, self->headers, "content-length" );
}

static void DaoHttpHeader_Via( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpHeader *self = (DaoHttpHeader*)DaoValue_TryGetCdata( p[0] );
	PutListValue( proc, self->headers, "via" );
}

static void DaoHttpHeader_Connection( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpHeader *self = (DaoHttpHeader*)DaoValue_TryGetCdata( p[0] );
	daoint i;
	DaoList *res = PutListValue( proc, self->headers, "connection" );
	// connection options are case-insensitive
	if ( res )
		for ( i = 0; i < res->value->size; ++i )
			DString_ToLower( DaoList_GetItem( res, i )->xString.value );
}

static void DaoHttpHeader_Upgrade( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpHeader *self = (DaoHttpHeader*)DaoValue_TryGetCdata( p[0] );
	PutListValue( proc, self->headers, "upgrade" );
}

static void DaoHttpHeader_Trailer( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpHeader *self = (DaoHttpHeader*)DaoValue_TryGetCdata( p[0] );
	PutListValue( proc, self->headers, "trailer" );
}

static void DaoHttpHeader_TE( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpHeader *self = (DaoHttpHeader*)DaoValue_TryGetCdata( p[0] );
	PutListValue( proc, self->headers, "te" );
}

void SplitMediaType( DString *value, DaoTuple *tuple )
{
	const char *cp = value->chars;
	for ( ; *cp && *cp != ';'; ++cp );
	if ( *cp ){
		const char *end = cp;
		for ( ; end > value->chars && IS_OWS( end - 1 ); --end );
		DString_SetBytes( tuple->values[0]->xString.value, value->chars, end - value->chars );
		for ( ++cp; IS_OWS( cp ); ++cp );
		DString_SetChars( tuple->values[1]->xString.value, cp );
	}
	else
		DString_Assign( tuple->values[0]->xString.value, value );
}

static void DaoHttpHeader_ContType( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpHeader *self = (DaoHttpHeader*)DaoValue_TryGetCdata( p[0] );
	DString field = DString_WrapChars( "content-type" );
	DNode *node = DMap_Find( self->headers, &field );
	if ( node ){
		DaoTuple *res = DaoProcess_PutTuple( proc, 2 );
		SplitMediaType( node->value.pString, res );
	}
	else
		DaoProcess_PutNone( proc );
}

void PutStringValue( DaoProcess *proc, DMap *headers, const char *field )
{
	DString name = DString_WrapChars( field );
	DNode *node = DMap_Find( headers, &name );
	if ( node )
		DaoProcess_PutString( proc, node->value.pString );
	else
		DaoProcess_PutNone( proc );
}

void PutDateValue( DaoProcess *proc, DMap *headers, const char *field )
{
	DString name = DString_WrapChars( field );
	DNode *node = DMap_Find( headers, &name );
	if ( sizeof(dao_integer) < sizeof(time_t) )
		DaoProcess_RaiseWarning( proc, "Value", "The time value might overflow the int type" );
	if ( node ){
		time_t date = ParseDate( node->value.pString );
		if ( date != (time_t)-1 )
			date -= timezone;
		DaoProcess_PutInteger( proc, (dao_integer)date );
	}
	else
		DaoProcess_PutNone( proc );
}

static void DaoHttpHeader_ContLoc( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpHeader *self = (DaoHttpHeader*)DaoValue_TryGetCdata( p[0] );
	PutStringValue( proc, self->headers, "content-location" );
}

static void DaoHttpHeader_Date( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpHeader *self = (DaoHttpHeader*)DaoValue_TryGetCdata( p[0] );
	PutDateValue( proc, self->headers, "date" );
}

static void DaoHttpHeader_Cookies( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpHeader *self = (DaoHttpHeader*)DaoValue_TryGetCdata( p[0] );
	DaoList *res = DaoProcess_PutList( proc );
	daoint i;
	for ( i = 0; i < self->cookies->size; ++i ){
		DaoString *str = DaoString_New();
		DaoString_Set( str, self->cookies->items.pString[i] );
		DaoList_Append( res, (DaoValue*)str );
	}
}

static DaoFuncItem headerMeths[] =
{
	//! HTTP version number
	{ DaoHttpHeader_Version,	".version(self: Header) => string" },

	//! Header size including the terminating '\r\n'
	{ DaoHttpHeader_Size,		".size(self: Header) => int" },

	//! Cookies set by 'Cookie' or 'Set-Cookie' fields (for requests and responses accordingly)
	{ DaoHttpHeader_Cookies,	".cookies(self: Header) => list<string>" },

	//! Specific field (header) value
	//!
	//! \note Does not include 'Set-Cookie', which are available via \c ResponseHeader::.cookies()
	{ DaoHttpHeader_Field,		"[](self: Header, field: string) => string" },

	//! Some of the standard fields. \c none indicate field absence; empty values (or \c contentLength
	//! value equal to -1) indicate a parsing error
	//!
	//! \note \c .date() returns \c time_t value (-1 or parsing error); use \c time module to interpret it
	{ DaoHttpHeader_TransEnc,	".transferEncoding(self: Header) => list<string>|none" },
	{ DaoHttpHeader_ContLen,	".contentLength(self: Header) => int|none" },
	{ DaoHttpHeader_Via,		".via(self: Header) => list<string>|none" },
	{ DaoHttpHeader_Connection,	".connection(self: Header) => list<string>|none" },
	{ DaoHttpHeader_Upgrade,	".upgrade(self: Header) => list<string>|none" },
	{ DaoHttpHeader_Trailer,	".trailer(self: Header) => list<string>|none" },
	{ DaoHttpHeader_TE,			".te(self: Header) => list<string>|none" },
	{ DaoHttpHeader_ContType,	".contentType(self: Header) => tuple<name: string, params: string>|none" },
	{ DaoHttpHeader_ContEnc,	".contentEncoding(self: Header) => list<string>|none" },
	{ DaoHttpHeader_ContLang,	".contentLanguage(self: Header) => list<string>|none" },
	{ DaoHttpHeader_ContLoc,	".contentLocation(self: Header) => string|none" },
	{ DaoHttpHeader_Date,		".date(self: Header) => int|none" },
	{ DaoHttpHeader_CacheCtrl,	".cacheControl(self: Header) => list<string>|none" },
	{ NULL, NULL }
};

//! Encapsulates shared properties of HTTP request and response
DaoTypeBase headerTyper = {
	"Header", NULL, NULL, headerMeths, {NULL}, {0},
	(FuncPtrDel)NULL, NULL
};

static void DaoHttpRequest_Uri( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpRequest *self = (DaoHttpRequest*)DaoValue_TryGetCdata( p[0] );
	DaoProcess_PutString( proc, self->uri );
}

static void DaoHttpRequest_Method( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpRequest *self = (DaoHttpRequest*)DaoValue_TryGetCdata( p[0] );
	DaoProcess_PutString( proc, self->method );
}

static void DaoHttpRequest_Host( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpRequest *self = (DaoHttpRequest*)DaoValue_TryGetCdata( p[0] );
	PutStringValue( proc, self->headers, "host" );
}

static void DaoHttpRequest_Expect( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpRequest *self = (DaoHttpRequest*)DaoValue_TryGetCdata( p[0] );
	PutStringValue( proc, self->headers, "expect" );
}

static void DaoHttpRequest_From( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpRequest *self = (DaoHttpRequest*)DaoValue_TryGetCdata( p[0] );
	PutStringValue( proc, self->headers, "from" );
}

static void DaoHttpRequest_Referer( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpRequest *self = (DaoHttpRequest*)DaoValue_TryGetCdata( p[0] );
	PutStringValue( proc, self->headers, "referer" );
}

static void DaoHttpRequest_UserAgent( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpRequest *self = (DaoHttpRequest*)DaoValue_TryGetCdata( p[0] );
	PutStringValue( proc, self->headers, "user-agent" );
}

static void DaoHttpRequest_Auth( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpRequest *self = (DaoHttpRequest*)DaoValue_TryGetCdata( p[0] );
	PutStringValue( proc, self->headers, "authorization" );
}

static void DaoHttpRequest_ProxyAuth( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpRequest *self = (DaoHttpRequest*)DaoValue_TryGetCdata( p[0] );
	PutStringValue( proc, self->headers, "proxy-authorization" );
}

static void DaoHttpRequest_Accept( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpRequest *self = (DaoHttpRequest*)DaoValue_TryGetCdata( p[0] );
	PutListValue( proc, self->headers, "accept" );
}

static void DaoHttpRequest_AcceptChar( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpRequest *self = (DaoHttpRequest*)DaoValue_TryGetCdata( p[0] );
	PutListValue( proc, self->headers, "accept-charset" );
}

static void DaoHttpRequest_AcceptEnc( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpRequest *self = (DaoHttpRequest*)DaoValue_TryGetCdata( p[0] );
	PutListValue( proc, self->headers, "accept-encoding" );
}

static void DaoHttpRequest_AcceptLang( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpRequest *self = (DaoHttpRequest*)DaoValue_TryGetCdata( p[0] );
	PutListValue( proc, self->headers, "accept-language" );
}

static void DaoHttpRequest_MaxForw( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpRequest *self = (DaoHttpRequest*)DaoValue_TryGetCdata( p[0] );
	PutIntValue( proc, self->headers, "max-forwards" );
}

static DaoFuncItem requestMeths[] =
{
	//! Requested URI
	{ DaoHttpRequest_Uri,		".uri(self: RequestHeader) => string" },

	//! Request method
	{ DaoHttpRequest_Method,	".method(self: RequestHeader) => string" },

	//! Some of the standard fields (also see \c Header)
	{ DaoHttpRequest_Host,		".host(self: RequestHeader) => string|none" },
	{ DaoHttpRequest_Expect,	".expect(self: RequestHeader) => string|none" },
	{ DaoHttpRequest_From,		".from(self: RequestHeader) => string|none" },
	{ DaoHttpRequest_Referer,	".referer(self: RequestHeader) => string|none" },
	{ DaoHttpRequest_UserAgent,	".userAgent(self: RequestHeader) => string|none" },
	{ DaoHttpRequest_Auth,		".authorization(self: RequestHeader) => string|none" },
	{ DaoHttpRequest_ProxyAuth,	".proxyAuthorization(self: RequestHeader) => string|none" },
	{ DaoHttpRequest_MaxForw,	".maxForwards(self: RequestHeader) => int|none" },
	{ DaoHttpRequest_Accept,	".accept(self: RequestHeader) => list<string>|none" },
	{ DaoHttpRequest_AcceptChar,".acceptCharset(self: RequestHeader) => list<string>|none" },
	{ DaoHttpRequest_AcceptEnc,	".acceptEncoding(self: RequestHeader) => list<string>|none" },
	{ DaoHttpRequest_AcceptLang,".acceptLanguage(self: RequestHeader) => list<string>|none" },
	{ NULL, NULL }
};

//! HTTP request header
DaoTypeBase requestTyper = {
	"RequestHeader", NULL, NULL, requestMeths, {&headerTyper}, {0},
	(FuncPtrDel)DaoHttpRequest_Delete, NULL
};

static void DaoHttpResponse_Code( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpResponse *self = (DaoHttpResponse*)DaoValue_TryGetCdata( p[0] );
	DaoProcess_PutInteger( proc, self->code );
}

static void DaoHttpResponse_Reason( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpResponse *self = (DaoHttpResponse*)DaoValue_TryGetCdata( p[0] );
	DaoProcess_PutString( proc, self->reason );
}

static void DaoHttpResponse_Location( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpResponse *self = (DaoHttpResponse*)DaoValue_TryGetCdata( p[0] );
	PutStringValue( proc, self->headers, "location" );
}

static void DaoHttpResponse_RetryAfter( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpResponse *self = (DaoHttpResponse*)DaoValue_TryGetCdata( p[0] );
	PutStringValue( proc, self->headers, "retry-after" );
}

static void DaoHttpResponse_Server( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpResponse *self = (DaoHttpResponse*)DaoValue_TryGetCdata( p[0] );
	PutStringValue( proc, self->headers, "server" );
}

static void DaoHttpResponse_Expires( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpResponse *self = (DaoHttpResponse*)DaoValue_TryGetCdata( p[0] );
	PutDateValue( proc, self->headers, "expires" );
}

static void DaoHttpResponse_Vary( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpResponse *self = (DaoHttpResponse*)DaoValue_TryGetCdata( p[0] );
	DString field = DString_WrapChars( "vary" );
	DNode *node = DMap_Find( self->headers, &field );
	if ( node ){
		DString *value = node->value.pString;
		if ( strcmp( value->chars, "*" ) == 0 )
			DaoProcess_PutString( proc, value );
		else
			SplitValue( value, DaoProcess_PutList( proc ) );
	}
	else
		DaoProcess_PutNone( proc );
}

static void DaoHttpResponse_Allow( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpResponse *self = (DaoHttpResponse*)DaoValue_TryGetCdata( p[0] );
	PutListValue( proc, self->headers, "allow" );
}

static void DaoHttpResponse_WwwAuth( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpResponse *self = (DaoHttpResponse*)DaoValue_TryGetCdata( p[0] );
	PutListValue( proc, self->headers, "www-authenticate" );
}

static void DaoHttpResponse_ProxyAuth( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpResponse *self = (DaoHttpResponse*)DaoValue_TryGetCdata( p[0] );
	PutListValue( proc, self->headers, "proxy-authenticate" );
}

static void DaoHttpResponse_Age( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoHttpResponse *self = (DaoHttpResponse*)DaoValue_TryGetCdata( p[0] );
	PutIntValue( proc, self->headers, "age" );
}

static DaoFuncItem responseMeths[] =
{
	//! Status code
	{ DaoHttpResponse_Code,			".code(self: ResponseHeader) => int" },

	//! Textual description of response status
	{ DaoHttpResponse_Reason,		".reason(self: ResponseHeader) => string" },

	//! Some of the standard fields (also see \c Header)
	//!
	//! \note \c .expires() returns \c time_t value (-1 on parsing error); use \c time module to interpret it
	{ DaoHttpResponse_Location,		".location(self: Header) => string|none" },
	{ DaoHttpResponse_RetryAfter,	".retryAfter(self: Header) => string|none" },
	{ DaoHttpResponse_Server,		".server(self: Header) => string|none" },
	{ DaoHttpResponse_Expires,		".expires(self: Header) => int|none" },
	{ DaoHttpResponse_Vary,			".vary(self: Header) => list<string>|string|none" },
	{ DaoHttpResponse_Allow,		".allow(self: Header) => list<string>|none" },
	{ DaoHttpResponse_WwwAuth,		".wwwAuthenticate(self: Header) => list<string>|none" },
	{ DaoHttpResponse_ProxyAuth,	".proxyAuthenticate(self: Header) => list<string>|none" },
	{ DaoHttpResponse_Age,			".age(self: Header) => int|none" },
	{ NULL, NULL }
};

//! HTTP response header
DaoTypeBase responseTyper = {
	"ResponseHeader", NULL, NULL, responseMeths, {&headerTyper}, {0},
	(FuncPtrDel)DaoHttpResponse_Delete, NULL
};

void GetParsingErrorMsg( http_err_t error, char *buffer, size_t size )
{
	switch ( error ){
	case Http_InvalidStartLine:		snprintf( buffer, size, "malformed start line" ); break;
	case Http_InvalidMethod:		snprintf( buffer, size, "invalid method name" ); break;
	case Http_InvalidHttpVersion:	snprintf( buffer, size, "invalid HTTP version string" ); break;
	case Http_InvalidUrl:			snprintf( buffer, size, "invalid URL" ); break;
	case Http_InvalidStatusCode:	snprintf( buffer, size, "invalid status code" ); break;
	case Http_InvalidHeader:		snprintf( buffer, size, "malformed header line (field-value)" ); break;
	case Http_InvalidFieldName:		snprintf( buffer, size, "invalid header (field) name" ); break;
	case Http_InvalidFieldValue:	snprintf( buffer, size, "invalid header (field) value" ); break;
	default:						snprintf( buffer, size, "unexpected error" ); break;
	}
}

static void HTTP_AcceptRequest( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *msg = p[0]->xString.value;
	daoint end = DString_FindChars( msg, "\r\n\r\n", 0 );
	if ( end >= 0 ){
		DaoHttpRequest *res = DaoHttpRequest_New();
		http_err_t err = DaoHttpRequest_Parse( res, msg, end );
		if ( !err )
			DaoProcess_PutCdata( proc, res, daox_type_request );
		else {
			char errbuf[512];
			int len = snprintf( errbuf, sizeof(errbuf), "Failed to parse request: " );
			GetParsingErrorMsg( err, errbuf + len, sizeof(errbuf) - len );
			DaoProcess_RaiseError( proc, "Http", errbuf );
			DaoHttpRequest_Delete( res );
		}
	}
	else
		DaoProcess_PutNone( proc );
}

static void HTTP_AcceptResponse( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *msg = p[0]->xString.value;
	daoint end = DString_FindChars( msg, "\r\n\r\n", 0 );
	if ( end >= 0 ){
		DaoHttpResponse *res = DaoHttpResponse_New();
		http_err_t err = DaoHttpResponse_Parse( res, msg, end );
		if ( !err )
			DaoProcess_PutCdata( proc, res, daox_type_response );
		else {
			char errbuf[512];
			int len = snprintf( errbuf, sizeof(errbuf), "Failed to parse response: " );
			GetParsingErrorMsg( err, errbuf + len, sizeof(errbuf) - len );
			DaoProcess_RaiseError( proc, "Http", errbuf );
			DaoHttpResponse_Delete( res );
		}
	}
	else
		DaoProcess_PutNone( proc );
}

// converts 'fieldName' to 'Field-Name'
void AppendFieldName( DString *name, DString *dest )
{
	const char *cp = name->chars, *start;
	DString_AppendChar( dest, toupper( *cp ) );
	while ( 1 ){
		start = cp + 1;
		for ( ++cp; *cp && islower( *cp ); ++cp );
		DString_AppendBytes( dest, start, cp - start );
		if ( *cp ){
			DString_AppendChar( dest, '-' );
			DString_AppendChar( dest, *cp );
		}
		else
			break;
	}
}

void AppendFieldValue( DaoValue *value, DString *dest )
{
	switch ( value->type ){
	case DAO_INTEGER:
		if ( 1 ){
			char buf[20];
			snprintf( buf, sizeof(buf), "%"DAO_I64, value->xInteger.value );
			DString_AppendChars( dest, buf );
		}
		break;
	case DAO_STRING:
		DString_Append( dest, value->xString.value );
		break;
	case DAO_LIST:
		if ( 1 ){
			DaoList *list = &value->xList;
			daoint i;
			for ( i = 0; i < list->value->size; ++i ){
				DString_Append( dest, DaoList_GetItem( list, i )->xString.value );
				if ( i != list->value->size - 1 )
					DString_AppendChars( dest, ", " );
			}
		}
		break;
	}
}

int AppendFieldHeader( DaoProcess *proc, DaoEnum *token, DaoValue *value, DString *dest )
{
	DString *name = DString_New();
	DaoEnum_MakeName( token, name );
	DString_Erase( name, 0, 1 );
	if ( *ParseToken( name->chars, NULL ) ){
		char errbuf[255];
		snprintf( errbuf, sizeof(errbuf), "Invalid field name: %s", name->chars );
		DaoProcess_RaiseError( proc, "Http", errbuf );
		DString_Delete( name );
		return 0;
	}
	AppendFieldName( name, dest );
	DString_AppendChars( dest, ": " );
	AppendFieldValue( value, dest );
	DString_AppendChars( dest, "\r\n" );
	DString_Delete( name );
	return 1;
}

static void HTTP_InitRequest( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *method = p[0]->xString.value;
	DString *url = p[1]->xString.value;
	DString *req = DaoProcess_PutChars( proc, "" );
	DString *host = DString_Copy( url );
	DString *uri = DString_NewChars( "/" );
	const char *cp = ParseToken( method->chars, NULL );
	daoint i;
	if ( *cp ){
		DaoProcess_RaiseError( proc, "Http", "Invalid request method" );
		goto End;
	}
	cp = ParseURI( url->chars, NULL );
	if ( !cp || *cp ){
		DaoProcess_RaiseError( proc, "Http", "Invalid URL" );
		goto End;
	}

	if ( memcmp( host->chars, "http://", 7 ) == 0 )
		DString_Erase( host, 0, 7 );
	else if( memcmp( host->chars, "https://", 8 ) == 0 )
		DString_Erase( host, 0, 8 );
	i = DString_FindChar( host, '/', 0 );
	if ( i != DAO_NULLPOS ){
		DString_SubString( host, uri, i, -1 );
		DString_Erase( host, i, -1 );
	}

	DString_Append( req, method );
	DString_AppendChar( req, ' ' );
	DString_Append( req, uri );
	DString_AppendChar( req, ' ' );
	DString_AppendChars( req, "HTTP/1.1\r\n" );
	DString_AppendChars( req, "Host: " );
	DString_Append( req, host );
	DString_AppendChars( req, "\r\n" );

	for ( i = 2; i < N; ++i ){
		DaoEnum *name = &p[i]->xTuple.values[0]->xEnum;
		DaoValue *value = p[i]->xTuple.values[1];
		if ( !AppendFieldHeader( proc, name, value, req ) )
			goto End;
	}
	DString_AppendChars( req, "\r\n" );
End:
	DString_Delete( host );
	DString_Delete( uri );
}

static void HTTP_InitResponse( DaoProcess *proc, DaoValue *p[], int N )
{
	DString *resp = DaoProcess_PutChars( proc, "HTTP/1.1 " );
	daoint i;
	AppendFieldValue( p[0], resp );
	DString_AppendChar( resp, ' ' );
	switch ( p[0]->xInteger.value ){
	case 100:	DString_AppendChars( resp, "Continue" ); break;
	case 101:	DString_AppendChars( resp, "Switching Protocols" ); break;
	case 200:	DString_AppendChars( resp, "OK" ); break;
	case 201:	DString_AppendChars( resp, "Created" ); break;
	case 202:	DString_AppendChars( resp, "Accepted" ); break;
	case 203:	DString_AppendChars( resp, "Non-Authoritative Information" ); break;
	case 204:	DString_AppendChars( resp, "No Content" ); break;
	case 205:	DString_AppendChars( resp, "Reset Content" ); break;
	case 206:	DString_AppendChars( resp, "Partial Content" ); break;
	case 300:	DString_AppendChars( resp, "Multiple Choices" ); break;
	case 301:	DString_AppendChars( resp, "Moved Permanently" ); break;
	case 302:	DString_AppendChars( resp, "Found" ); break;
	case 303:	DString_AppendChars( resp, "See Other" ); break;
	case 304:	DString_AppendChars( resp, "Not Modified" ); break;
	case 305:	DString_AppendChars( resp, "Use Proxy" ); break;
	case 307:	DString_AppendChars( resp, "Temporary Redirect" ); break;
	case 400:	DString_AppendChars( resp, "Bad Request" ); break;
	case 401:	DString_AppendChars( resp, "Unauthorized" ); break;
	case 402:	DString_AppendChars( resp, "Payment Required" ); break;
	case 403:	DString_AppendChars( resp, "Forbidden" ); break;
	case 404:	DString_AppendChars( resp, "Not Found" ); break;
	case 405:	DString_AppendChars( resp, "Method Not Allowed" ); break;
	case 406:	DString_AppendChars( resp, "Not Acceptable" ); break;
	case 407:	DString_AppendChars( resp, "Proxy Authentication Required" ); break;
	case 408:	DString_AppendChars( resp, "Request Timeout" ); break;
	case 409:	DString_AppendChars( resp, "Conflict" ); break;
	case 410:	DString_AppendChars( resp, "Gone" ); break;
	case 411:	DString_AppendChars( resp, "Length Required" ); break;
	case 412:	DString_AppendChars( resp, "Precondition Failed" ); break;
	case 413:	DString_AppendChars( resp, "Payload Too Large" ); break;
	case 414:	DString_AppendChars( resp, "URI Too Long" ); break;
	case 415:	DString_AppendChars( resp, "Unsupported Media Type" ); break;
	case 416:	DString_AppendChars( resp, "Range Not Satisfiable" ); break;
	case 417:	DString_AppendChars( resp, "Expectation Failed" ); break;
	case 426:	DString_AppendChars( resp, "Upgrade Required" ); break;
	case 500:	DString_AppendChars( resp, "Internal Server Error" ); break;
	case 501:	DString_AppendChars( resp, "Not Implemented" ); break;
	case 502:	DString_AppendChars( resp, "Bad Gateway" ); break;
	case 503:	DString_AppendChars( resp, "Service Unavailable" ); break;
	case 504:	DString_AppendChars( resp, "Gateway Timeout" ); break;
	case 505:	DString_AppendChars( resp, "HTTP Version Not Supported" ); break;
	default:
		DaoProcess_RaiseError( proc, "Http", "Non-standard status code" );
		return;
	}
	DString_AppendChars( resp, "\r\n" );
	for ( i = 1; i < N; ++i ){
		DaoEnum *name = &p[i]->xTuple.values[0]->xEnum;
		DaoValue *value = p[i]->xTuple.values[1];
		if ( !AppendFieldHeader( proc, name, value, resp ) )
			return;
	}
	DString_AppendChars( resp, "\r\n" );
}

static void DaoChunkDecoder_Create( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoChunkDecoder *res = (DaoChunkDecoder*)dao_malloc(sizeof(DaoChunkDecoder));
	res->status = Status_Idle;
	res->pending = 0;
	res->part = DString_New();
	res->last = 0;
	DaoProcess_PutCdata( proc, res, daox_type_chunkDecoder );
}

void DaoChunkDecoder_Delete( DaoChunkDecoder *self )
{
	DString_Delete( self->part );
	dao_free( self );
}

static void DaoChunkDecoder_Status( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoChunkDecoder *self = (DaoChunkDecoder*)DaoValue_TryGetCdata( p[0] );
	const char *symbol = "";
	switch ( self->status ){
	case Status_Idle:				symbol = "idle"; break;
	case Status_IncompleteBody:		symbol = "body"; break;
	case Status_TrailExpected:		symbol = "trail"; break;
	case Status_IncompleteHeader:	symbol = "header"; break;
	case Status_Finished:			symbol = "finished"; break;
	}
	DaoProcess_PutEnum( proc, symbol );
}

static void DaoChunkDecoder_Count( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoChunkDecoder *self = (DaoChunkDecoder*)DaoValue_TryGetCdata( p[0] );
	DaoProcess_PutInteger( proc, self->pending );
}

int CheckTrail( const char *str, int expected )
{
	if ( !*str )
		return expected;
	if ( expected == 1 )
		return *str == '\n'? 0 : -1;
	if ( *str != '\r' )
		return -1;
	return CheckTrail( str + 1, 1 );
}

static void DaoChunkDecoder_Decode( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoChunkDecoder *self = (DaoChunkDecoder*)DaoValue_TryGetCdata( p[0] );
	DString *msg = p[1]->xString.value;
	daoint start = p[2]->xInteger.value;
	DString *data = DaoProcess_PutChars( proc, "" );
	daoint pos;
	char *end;
	const char *cp;
	dao_integer size;
	int count;
	if ( start >= msg->size )
		return;
	// continue from the previous iteration
	switch ( self->status ){
	case Status_Idle: // clean state
		break;
	case Status_IncompleteBody: // continue reading chunk body
		size = self->pending;
		goto ReadBody;
	case Status_TrailExpected: // pass trailing '\r\n'
		count = self->pending;
		self->pending = CheckTrail( msg->chars + start, count );
		if ( self->pending < 0 ){
			DaoProcess_RaiseError( proc, "Http", "No trailing '\\r\\n' after chunk body" );
			return;
		}
		if ( self->pending )
			return;
		start += count;
		self->pending = 0;
		self->status = self->last? Status_Finished : Status_Idle;
		if ( self->last || start == msg->size )
			return;
		break;
	case Status_IncompleteHeader: // prepend the earlier header part to the data
		DString_Append( self->part, msg );
		DString_Assign( msg, self->part );
		DString_Clear( self->part );
		break;
	case Status_Finished: // nothing to do
		return;
	}
	// fresh start
	while ( 1 ){
		pos = DString_FindChars( msg, "\r\n", start );
		if ( pos == DAO_NULLPOS ){ // incomplete chunk header
			self->status = Status_IncompleteHeader;
			if ( msg->size - start > 50 ){
				DaoProcess_RaiseError( proc, "Http", "Chunk header not found" );
				return;
			}
			DString_SetBytes( self->part, msg->chars + start, msg->size - start );
			self->pending = -1;
			break;
		}
		cp = msg->chars + start;
		size = strtoull( cp, &end, 16 ); // chunk size
		if ( cp == end || ( end - cp != pos - start && *end != ';' ) || size < 0 ){
			DaoProcess_RaiseError( proc, "Http", "Invalid chunk size" );
			return;
		}
		if ( size == 0 )
			self->last = 1;
		start = pos + 2;
	ReadBody:
		if ( start + size > msg->size ){ // incomplete chunk body
			DString_AppendBytes( data, msg->chars + start, msg->size - start );
			self->status = Status_IncompleteBody;
			self->pending = size - ( msg->size - start );
			break;
		}
		DString_AppendBytes( data, msg->chars + start, size ); // full body is available
		start += size;
		count = CheckTrail( msg->chars + start, 2 ); // should end with '\r\n'
		if ( count < 0 ){
			DaoProcess_RaiseError( proc, "Http", "No trailing '\\r\\n' after chunk body" );
			return;
		}
		if ( count ){ // trailing bytes are not available
			self->status = Status_TrailExpected;
			self->pending = count;
			break;
		}
		start += 2;
		if ( self->last ){ // it was the last chunk
			self->status = Status_Finished;
			self->pending = 0;
			break;
		}
		if ( start == msg->size ){ // data ends on chunk end
			self->status = Status_Idle;
			self->pending = 0;
			break;
		}
	}
}

static void DaoChunkDecoder_Reset( DaoProcess *proc, DaoValue *p[], int N )
{
	DaoChunkDecoder *self = (DaoChunkDecoder*)DaoValue_TryGetCdata( p[0] );
	DString_Clear( self->part );
	self->pending = 0;
	self->status = Status_Idle;
	self->last = 0;
}

static DaoFuncItem chunkDecoderMeths[] =
{
	//! Creates idle-state decoder
	{ DaoChunkDecoder_Create,	"ChunkDecoder()" },

	/*! Decoding status:
	 *  - idle -- after creation or upon reading a complete chunk
	 *  - body -- stopped on reading chunk body
	 *  - trail -- stopped on reading trailing '\r\n' after chunk body
	 *  - header -- stopped on reading chunk header
	 *  - finished -- the last chunk was read */
	{ DaoChunkDecoder_Status,	".status(invar self: ChunkDecoder) => enum<idle,body,trail,header,finished>" },

	//! Bytes left to read in case of incomplete chunk (will be -1 for an incomplete header)
	{ DaoChunkDecoder_Count,	".bytesPending(invar self: ChunkDecoder) => int" },

	//! Feeds \a data to the decoder starting with the given \a offset. Returns message body or its part
	//! extracted from \a data
	{ DaoChunkDecoder_Decode,	"decode(self: ChunkDecoder, data: string, offset = 0) => string" },

	//! Sets the decoder into the idle state
	{ DaoChunkDecoder_Reset,	"reset(self: ChunkDecoder)" },
	{ NULL, NULL }
};

//! Statefule decoder for chunked encoding
DaoTypeBase chunkDecoderTyper = {
	"ChunkDecoder", NULL, NULL, chunkDecoderMeths, {NULL}, {0},
	(FuncPtrDel)DaoChunkDecoder_Delete, NULL
};

static DaoFuncItem httpMeths[] =
{
	//! Returns HTTP request or response header read from \a message. If \a message does not
	//! contain a complete header (terminated by '\r\n'), returns \c none
	{ HTTP_AcceptRequest,	"acceptRequest(message: string) => RequestHeader|none" },
	{ HTTP_AcceptResponse,	"acceptResponse(message: string) => ResponseHeader|none" },

	//! Constructs HTTP/1.1 request header string with the given request \a method and \a url, from which
	//! origin-form URI and 'Host' field are formed. Other field name-values (headers) may be provided as additional
	//! variadic parameters, the names of which are converted from camel case to hyphen-delimited notation:
	//! 'fieldName' to 'Field-Name'. Returns the resulting header along with the terminating '\r\n'
	{ HTTP_InitRequest,		"initRequest(method: string, url: string, ...: tuple<enum,int|string|list<string>>) => string" },

	//! Constructs HTTP/1.1 response header string with the given status \a code, from which the reason string
	//! is deduced. Field name-values (headers) may be provided the same way as with \c initRequest. Returns
	//! the resulting header along with the terminating '\r\n'
	{ HTTP_InitResponse,	"initResponse(code: int, ...: tuple<enum,int|string|list<string>>) => string" },
	{ NULL, NULL }
};

DAO_DLL int DaoHttp_OnLoad( DaoVmSpace *vmSpace, DaoNamespace *ns )
{
	DaoNamespace *httptns = DaoVmSpace_GetNamespace( vmSpace, "http" );
	DaoNamespace_AddConstValue( ns, "http", (DaoValue*)httptns );
	daox_type_header = DaoNamespace_WrapType( httptns, &headerTyper, DAO_CTYPE_OPAQUE | DAO_CTYPE_INVAR );
	daox_type_request = DaoNamespace_WrapType( httptns, &requestTyper, DAO_CTYPE_OPAQUE | DAO_CTYPE_INVAR );
	daox_type_response = DaoNamespace_WrapType( httptns, &responseTyper, DAO_CTYPE_OPAQUE | DAO_CTYPE_INVAR );
	daox_type_chunkDecoder = DaoNamespace_WrapType( httptns, &chunkDecoderTyper, DAO_CTYPE_OPAQUE );
	DaoNamespace_WrapFunctions( httptns, httpMeths );
	return 0;
}