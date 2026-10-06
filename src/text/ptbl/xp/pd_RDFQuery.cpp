/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiWord
 * Copyright (c) 2011 Ben Martin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "pd_RDFQuery.h"
#include "pd_Document.h"
#include "pt_PieceTable.h"
#include "ut_debugmsg.h"
#include "pf_Frag_Object.h"
#include "pf_Frag_Strux.h"
#include "ut_std_string.h"
#include "ut_string.h"

#include <cstring>
#include <map>
#include <vector>
#include <sstream>
#include <set>
#include <iostream>
using std::make_pair;

#ifdef WITH_REDLAND

// RDF support
#include <redland.h>
#include <rasqal.h>
#include "pd_RDFSupportRed.h"

librdf_world* getWorld()
{
    static librdf_world* world = nullptr;
    if( !world )
    {
        world = librdf_new_world();
        librdf_world_open( world );
    }
    return world;
}

std::string tostr( librdf_node* n )
{
    if( !n )
        return "NULL";

    if( librdf_uri* u = librdf_node_get_uri( n ))
    {
        std::string s = static_cast<const char*>(librdf_uri_as_string( u ));
        return s;
    }
    
    std::string s = static_cast<const char*>(librdf_node_to_string( n ));
    return s;
}

librdf_statement* toRedland( const PD_RDFStatement& st )
{
    librdf_world* w = getWorld();
    librdf_statement* ret = librdf_new_statement_from_nodes(
        w,
        librdf_new_node_from_uri_string( w, static_cast<const unsigned char*>(st.getSubject().toString().c_str() )),
        librdf_new_node_from_uri_string( w, static_cast<const unsigned char*>(st.getPredicate().toString().c_str() )),
        librdf_new_node_from_uri_string( w, static_cast<const unsigned char*>(st.getObject().toString().c_str() ))
        );
    return ret;
}




/********************************************************************************/
/********************************************************************************/
/*** Redland internal storage class *********************************************/
/********************************************************************************/
/********************************************************************************/

struct abiwordContext
{
    librdf_storage*   m_storage;
    PD_RDFModelHandle m_model;
    int               m_x;
    
    abiwordContext( librdf_storage* storage,
                    const char * /*name*/,
                    librdf_hash* options )
        : m_storage(storage)
        , m_x(0)
    {
        librdf_storage_set_instance( storage, this );

        if(librdf_hash_get_as_boolean(options, "x")>0)
            m_x = 1;
    }

    static abiwordContext* get( librdf_storage* storage )
    {
        if( !storage || !librdf_storage_get_instance(storage) )
        {
            UT_DEBUGMSG(("problem getting abiwordContext from RDF storage!\n"));
            return nullptr;
        }
        
        abiwordContext* ret = static_cast<abiwordContext*>(librdf_storage_get_instance(storage));
        return ret;
    }
    
    void setModel( PD_RDFModelHandle m )
    {
        m_model = m;
    }

#ifdef DEBUG
    void dump( const std::string& s )
    {
        m_model->dumpModel( s );
    }
#endif
};


struct abiwordFindStreamContext
{
    librdf_storage*     m_storage;
    abiwordContext*     m_context;
    librdf_statement*   m_query;
    librdf_statement*   m_statement;
    librdf_node*        m_context_node;
    PD_RDFModelIterator m_iter;
    bool                m_done;
    bool                m_queryIsSubjectOnly;
    
    abiwordFindStreamContext( librdf_storage* storage,
                              abiwordContext* c,
                              librdf_statement* statement,
                              librdf_node* context_node )
        : m_storage( storage )
        , m_context(c)
        , m_query(nullptr)
        , m_statement(nullptr)
        , m_context_node(nullptr)
        , m_done(false)
        , m_queryIsSubjectOnly(false)
    {
        librdf_storage_add_reference(m_storage);
        if( statement )
            m_query = librdf_new_statement_from_statement( statement );
        if( context_node )
            m_context_node = librdf_new_node_from_node( context_node );

        xxx_UT_DEBUGMSG(("abiwordFindStreamContext() query...\n"));
        xxx_UT_DEBUGMSG(("  subj: %s\n", tostr(librdf_statement_get_subject( statement )).c_str()));
        xxx_UT_DEBUGMSG(("  pred: %s\n", tostr(librdf_statement_get_predicate( statement )).c_str()));
        xxx_UT_DEBUGMSG(("   obj: %s\n", tostr(librdf_statement_get_object( statement )).c_str()));

        if( m_query
            &&  librdf_statement_get_subject( m_query )
            && !librdf_statement_get_predicate( m_query )
            && !librdf_statement_get_object( m_query ) )
        {
            m_queryIsSubjectOnly = true;
        }
        
    }

    ~abiwordFindStreamContext()
    {
        if(m_storage)
            librdf_storage_remove_reference(m_storage);

        if(m_query)
            librdf_free_statement(m_query);

        if(m_statement)
            librdf_free_statement(m_statement);

        if(m_context_node)
            librdf_free_node(m_context_node);
    }
    

    static abiwordFindStreamContext* get( void* context )
    {
        return static_cast<abiwordFindStreamContext*>(context);
    }
    

    int getNext()
    {
        xxx_UT_DEBUGMSG(("getNext() top...\n"));
        if(m_statement)
        {
            librdf_free_statement(m_statement);
            m_statement = nullptr;
        }

        PD_RDFModelIterator e = m_context->m_model->end();
        if( m_iter == e )
        {
            xxx_UT_DEBUGMSG(("getNext() hit end()\n"));
            m_done = 1;
            return -1;
        }

        for( ; m_iter != e; )
        {
            PD_RDFStatement& st = *m_iter;
            xxx_UT_DEBUGMSG(("getNext() testing statement...st: %s\n", st.toString().c_str()));

            //
            // Short cut evaluation for subj only queries
            //
            if( m_queryIsSubjectOnly )
            {
                xxx_UT_DEBUGMSG(("getNext(qso) testing statement...st: %s\n", st.toString().c_str()));
                if( st.getSubject().toString() != tostr(librdf_statement_get_subject( m_query )) )
                {
//                    m_iter = e;
                    m_done = 1;
                    xxx_UT_DEBUGMSG(("getNext(qso) DIFFERENT SUBJECT...\n" ));
                    return -1;
                }
            }
            
            // if( m_queryIsSubjectOnly && !m_iter.moveToNextSubjectHavePOCol() )
            // {
            //     UT_DEBUGMSG(("getNext(qso) testing statement...st: %s\n", st.toString().c_str()));
            //     if( st.getSubject().toString() == tostr(librdf_statement_get_subject( m_query )) )
            //     {
            //         if( m_iter.moveToNextSubjectHavePOCol() )
            //             m_iter.moveToNextSubjectReadPO();
            //         UT_DEBUGMSG(("getNext() matched statement...st: %s\n", st.toString().c_str()));
            //         librdf_statement* stred = toRedland( *m_iter );
            //         m_statement = stred;
            //         break;
            //     }
                
            //     UT_DEBUGMSG(("getNext() skipping statement...st: %s\n", st.toString().c_str()));
            //     m_iter.moveToNextSubject();
            //     continue;
            // }

            ++m_iter;
            
            librdf_statement* stred = toRedland( st );
            if( !m_query || librdf_statement_match( stred, m_query ) )
            {
                xxx_UT_DEBUGMSG(("getNext() statement matches...\n"));
                xxx_UT_DEBUGMSG((" st: %s\n", st.toString().c_str()));
                m_statement = stred;
                break;
            }
            librdf_free_statement( stred );
        }
        
        xxx_UT_DEBUGMSG(("getNext() ret 0...\n"));
        return 0;
    }
    

    void setup( librdf_world* /*world*/ )
    {
//        static int counter = 0;
        xxx_UT_DEBUGMSG(("setup() top c:%d\n", counter++ ));

        // {
        //     PD_RDFModelIterator iter = context->m_model->begin();
        //     PD_RDFModelIterator  end = context->m_model->end();
        //     for( ; iter != end; ++iter )
        //     {
        //         PD_RDFStatement st = *iter;
        //         cerr << "setup(a)...st loop:" << st.toString() << endl;
        //     }
        // }
        
        // cerr << "setup() B............" << endl;

        // {
        //     PD_RDFModelIterator iter = context->m_model->begin();
        //     PD_RDFModelIterator  end = context->m_model->end();
        //     for( ; iter != end; ++iter )
        //     {
        //         PD_RDFStatement st = *iter;
        //         cerr << "setup(b)...st loop:" << st.toString() << endl;
        //     }
        // }
        
        // cerr << "setup() C............" << endl;

        // {
        //     m_iter = context->m_model->begin();
        //     PD_RDFModelIterator  end = context->m_model->end();
        //     for( ; m_iter != end; ++m_iter )
        //     {
        //         PD_RDFStatement st = *m_iter;
        //         cerr << "setup(c)...st loop:" << st.toString() << endl;
        //     }
        // }
        
        // cerr << "setup() D............" << endl;
        

        m_iter = m_context->m_model->begin();
        PD_RDFModelIterator e = m_context->m_model->end();
//        UT_DEBUGMSG(("setup()...model.sz: %d\n", m_context->m_model->size() ));
        xxx_UT_DEBUGMSG(("setup()...iter!=end: %d\n", (m_iter != e)));

        if( m_queryIsSubjectOnly )
        {
            xxx_UT_DEBUGMSG(("setup(qso)...\n"));
            for( ; m_iter != e ; )
            {
                PD_RDFStatement& st = *m_iter;
                if( st.getSubject().toString() == tostr(librdf_statement_get_subject( m_query )) )
                {
                    m_iter.moveToNextSubjectReadPO();
                    break;
                }
                m_iter.moveToNextSubject();
            }
        }
        
        
        PD_RDFStatement st = *m_iter;
        xxx_UT_DEBUGMSG(("setup()...st1: %s\n", st.toString().c_str()));
    }
};


static int
abiword_storage_init( librdf_storage* storage,
                      const char *name,
                      librdf_hash* options )
{
    bool failed = true;
    
    if( name )
    {
        failed = false;
        abiwordContext* context = new abiwordContext( storage, name, options );
        librdf_storage_set_instance( storage, context );
    }
    
    if(options)
        librdf_free_hash(options);

    return failed;
}

static void
abiword_storage_terminate(librdf_storage* storage)
{
    abiwordContext* c = abiwordContext::get( storage );
    delete c;
}

static int
abiword_storage_open(librdf_storage* storage, librdf_model* /*model*/)
{
    /*abiwordContext* c =*/ abiwordContext::get( storage );
    return 0;
}

static int
abiword_storage_close(librdf_storage* storage)
{
    /*abiwordContext* c =*/ abiwordContext::get( storage );
    return 0;

}

static int
abiword_storage_size(librdf_storage* storage)
{
    abiwordContext* c = abiwordContext::get( storage );
    int statementCount = c->m_model->getTripleCount();
    return statementCount;
}


static int
abiword_storage_find_statements_end_of_stream( void* context )
{
    abiwordFindStreamContext* sc = abiwordFindStreamContext::get( context );
    xxx_UT_DEBUGMSG(("abiword_storage_find_statements_end_of_stream() ctx: %p done: %d\n", sc, sc->m_done));
    
    if( sc->m_done )
        return 1;
  
    if( !sc->m_statement )
        sc->getNext();
    
    xxx_UT_DEBUGMSG(("abiword_storage_find_statements_end_of_stream(2) done: %d\n", sc->m_done));
    return sc->m_done;
}

static int
abiword_storage_find_statements_next_statement( void* context )
{
    abiwordFindStreamContext* sc = abiwordFindStreamContext::get( context );
    xxx_UT_DEBUGMSG(("abiword_storage_find_statements_next_statement() done: %d\n", sc->m_done));
    if( sc->m_done )
        return 1;
    return sc->getNext();
}

static void*
abiword_storage_find_statements_get_statement(void* context, int flags)
{
    abiwordFindStreamContext* sc = abiwordFindStreamContext::get( context );
    xxx_UT_DEBUGMSG(("abiword_storage_find_statements_get_statement() done: %d flags: %d\n", sc->m_done, flags));

    switch(flags)
    {
        case LIBRDF_ITERATOR_GET_METHOD_GET_OBJECT:
            xxx_UT_DEBUGMSG(("get_statement() result.... %s\n", tostr(sc->m_statement).c_str()));
            return sc->m_statement;
            
        default:
            UT_DEBUGMSG(("ERROR: Unknown iterator method flag: %d\n", flags));
            return nullptr;
    }
}


static void
abiword_storage_find_statements_finished(void* context)
{
    abiwordFindStreamContext* sc = abiwordFindStreamContext::get( context );
    delete sc;
    xxx_UT_DEBUGMSG(("=== abiword_storage_find_statements_finished()\n"));
}


static librdf_stream*
abiword_storage_find_statements_with_context( librdf_storage* storage,
                                              librdf_statement* statement,
                                              librdf_node* context_node )
{
    xxx_UT_DEBUGMSG(("=== abiword_storage_find_statements()\n"));
	xxx_UT_DEBUGMSG(("statement: %p\n", statement));

    if( statement )
    {
        xxx_UT_DEBUGMSG(("subj: %s\n", tostr(librdf_statement_get_subject( statement )).c_str()));
        xxx_UT_DEBUGMSG(("pred: %s\n", tostr(librdf_statement_get_predicate( statement )).c_str()));
        xxx_UT_DEBUGMSG((" obj: %s\n", tostr(librdf_statement_get_object( statement )).c_str()));
    }
    
    abiwordContext* c = abiwordContext::get( storage );
    abiwordFindStreamContext* sc = new abiwordFindStreamContext( storage, c, statement, context_node );
    sc->setup( librdf_storage_get_world(storage) );
    
    librdf_stream* stream = librdf_new_stream( librdf_storage_get_world(storage),
                                               static_cast<void*>(sc),
                                               &abiword_storage_find_statements_end_of_stream,
                                               &abiword_storage_find_statements_next_statement,
                                               &abiword_storage_find_statements_get_statement,
                                               &abiword_storage_find_statements_finished);
    if(!stream)
    {
        abiword_storage_find_statements_finished(static_cast<void*>(sc));
        return nullptr;
    }
    
    xxx_UT_DEBUGMSG(("abiword_storage_find_statements(done)\n"));
    return stream;  
}


static librdf_stream*
abiword_storage_find_statements( librdf_storage* storage,
                                 librdf_statement* statement )
{
    return abiword_storage_find_statements_with_context(storage, statement, nullptr);
}

static int
abiword_storage_contains_statement( librdf_storage* storage, 
                                    librdf_statement* query )
{
    xxx_UT_DEBUGMSG(("abiword_storage_contains_statement()\n"));
    abiwordContext* c = abiwordContext::get( storage );
    PD_RDFModelIterator iter = c->m_model->begin();
    PD_RDFModelIterator    e = c->m_model->end();
    
    for( ; iter != e; ++iter )
    {
        PD_RDFStatement st = *iter;
        librdf_statement* stred = toRedland( st );
        RedStatementHolder h(stred);
        
        if( librdf_statement_match( stred, query ) )
            return 1;
    }
    
    return 0;
}


static int
abiword_storage_context_add_statement( librdf_storage* storage,
                                       librdf_node* /*context_node*/,
                                       librdf_statement* /*statement*/ )
{
    // storage models are read-only and used for queries only.
    /*abiwordContext* c =*/ abiwordContext::get( storage );
    return 0;
}

static int
abiword_storage_add_statement( librdf_storage* storage,
                               librdf_statement* statement )
{
  if( abiword_storage_contains_statement( storage, statement ))
    return 0;

  return abiword_storage_context_add_statement(storage, nullptr, statement);
}

static int
abiword_storage_add_statements( librdf_storage* storage,
                                librdf_stream* statement_stream )
{
    int rc = 1;
    
    for( ; !librdf_stream_end(statement_stream);
         librdf_stream_next(statement_stream))
    {
        librdf_statement* statement    = librdf_stream_get_object(statement_stream);
        librdf_node*      context_node = librdf_stream_get_context2(statement_stream);

        if(abiword_storage_contains_statement(storage, statement))
            continue;

        rc &= abiword_storage_context_add_statement(storage, context_node, statement);
    }

    return rc;
}


static librdf_stream*
abiword_storage_context_serialise( librdf_storage* storage,
                                   librdf_node* context_node ) 
{
    return abiword_storage_find_statements_with_context(storage, nullptr, context_node);
}

static librdf_stream*
abiword_storage_serialise(librdf_storage* storage)
{
    return abiword_storage_find_statements_with_context(storage, nullptr, nullptr);
}


void abiword_storage_factory( librdf_storage_factory* f )
{
    xxx_UT_DEBUGMSG(("abiword_storage_factory()\n"));
	xxx_UT_DEBUGMSG(("factory->name: %s\n", f->name));

    f->version               = LIBRDF_STORAGE_INTERFACE_VERSION;
    f->init                  = abiword_storage_init;
    f->terminate             = abiword_storage_terminate;
    f->open                  = abiword_storage_open;
    f->close                 = abiword_storage_close;
    f->size                  = abiword_storage_size;
    f->find_statements       = abiword_storage_find_statements;
    f->serialise             = abiword_storage_serialise;
    f->context_add_statement = abiword_storage_context_add_statement;
    f->context_serialise     = abiword_storage_context_serialise;
    f->add_statement         = abiword_storage_add_statement;
    f->add_statements        = abiword_storage_add_statements;
    f->contains_statement    = abiword_storage_contains_statement;
}





/********************************************************************************/
/********************************************************************************/
/*** Public class interface *****************************************************/
/********************************************************************************/
/********************************************************************************/

static void
ensureStorageIsRegistered()
{
    static bool v = true;
    if( v )
    {
        v = false;
    
        /*int rc = */librdf_storage_register_factory( getWorld(),
                                                  "abiword", "abiword",
                                                  abiword_storage_factory );
    }
}


static librdf_model* getRedlandModel( PD_RDFModelHandle abimodel )
{
    const char *storage_name   = "abiword";
    const char *name           = "abiword";
    const char *options_string = "";

    ensureStorageIsRegistered();
    
    librdf_storage* storage = librdf_new_storage( getWorld(),
                                                  storage_name, name,
                                                  options_string );
    xxx_UT_DEBUGMSG(("getRedlandModel() storage: %p\n", storage));
    if( !storage )
    {
        return nullptr;
    }
    abiwordContext* ac = abiwordContext::get( storage );
    ac->setModel( abimodel );
    xxx_UT_DEBUGMSG(("getRedlandModel(2) storage: %p abimodel: %p\n", storage, abimodel.get()));
    
    librdf_model* model = nullptr;
    /*int rc = */librdf_storage_open( storage, model );
    model = librdf_new_model( getWorld(), storage, nullptr );
    
    xxx_UT_DEBUGMSG(("getRedlandModel(3) storage: %p model: %p\n", storage, model));
    return model;
}

#endif // ifdef WITH_REDLAND


#ifndef WITH_REDLAND

/********************************************************************************/
/*** Built-in SPARQL-subset evaluator (no libredland) *****************************/
/********************************************************************************/
//
// rasqal is not linked in this build, so the SELECT queries the RDF
// subsystem issues internally (contact/event/location discovery,
// xml:id scoping) are answered by this evaluator instead.  It covers
// the subset of SPARQL the code generates: PREFIX declarations, SELECT
// [DISTINCT] variables or *, a WHERE group of triple patterns,
// OPTIONAL { ... } sub-groups, and FILTER( expr ) where expr is a
// boolean combination of str(?var) =|"!=" "literal" comparisons, ||
// and &&.  Queries outside the subset (CONSTRUCT, UNION, ORDER BY,
// nested property paths, ...) parse-error and return no bindings.
//
namespace {

enum SparqlTokType
{
    TOK_EOF, TOK_LBRACE, TOK_RBRACE, TOK_LPAREN, TOK_RPAREN,
    TOK_DOT, TOK_COMMA, TOK_STAR, TOK_EQ, TOK_NEQ, TOK_OR, TOK_AND,
    TOK_IRI, TOK_STRING, TOK_VAR, TOK_WORD
};

struct SparqlTok
{
    SparqlTokType type;
    std::string   text;
};

class SparqlLexer
{
    const std::string& m_s;
    std::string::size_type m_pos;
    SparqlTok m_peeked;
    bool m_havePeeked;

    static bool isWordChar( char c )
    {
        return c > ' ' && strchr("{}(),*=\"'<>|&!?", c) == nullptr;
    }

  public:
    explicit SparqlLexer( const std::string& s )
        : m_s(s), m_pos(0), m_havePeeked(false)
    {
    }

    const SparqlTok& peek()
    {
        if( !m_havePeeked )
        {
            nextInto( m_peeked );
            m_havePeeked = true;
        }
        return m_peeked;
    }

    SparqlTok next()
    {
        SparqlTok t = peek();
        m_havePeeked = false;
        return t;
    }

  private:
    void nextInto( SparqlTok& t )
    {
        t.text.clear();
        while( m_pos < m_s.size() )
        {
            char c = m_s[m_pos];
            if( isspace(static_cast<unsigned char>(c)) )
            {
                ++m_pos;
                continue;
            }
            if( c == '#' )
            {
                while( m_pos < m_s.size() && m_s[m_pos] != '\n' )
                    ++m_pos;
                continue;
            }
            break;
        }
        if( m_pos >= m_s.size() )
        {
            t.type = TOK_EOF;
            return;
        }

        const char c = m_s[m_pos];
        switch( c )
        {
            case '{': ++m_pos; t.type = TOK_LBRACE; return;
            case '}': ++m_pos; t.type = TOK_RBRACE; return;
            case '(': ++m_pos; t.type = TOK_LPAREN; return;
            case ')': ++m_pos; t.type = TOK_RPAREN; return;
            case '.': ++m_pos; t.type = TOK_DOT;    return;
            case ',': ++m_pos; t.type = TOK_COMMA;  return;
            case '*': ++m_pos; t.type = TOK_STAR;   return;
            case '|':
                if( m_pos + 1 < m_s.size() && m_s[m_pos+1] == '|' )
                {
                    m_pos += 2; t.type = TOK_OR; return;
                }
                break;
            case '&':
                if( m_pos + 1 < m_s.size() && m_s[m_pos+1] == '&' )
                {
                    m_pos += 2; t.type = TOK_AND; return;
                }
                break;
            case '=':
                ++m_pos; t.type = TOK_EQ; return;
            case '!':
                if( m_pos + 1 < m_s.size() && m_s[m_pos+1] == '=' )
                {
                    m_pos += 2; t.type = TOK_NEQ; return;
                }
                break;
            case '<':
            {
                std::string::size_type e = m_s.find( '>', m_pos + 1 );
                if( e == std::string::npos )
                {
                    t.type = TOK_EOF;
                    return;
                }
                t.type = TOK_IRI;
                t.text = m_s.substr( m_pos + 1, e - m_pos - 1 );
                m_pos = e + 1;
                return;
            }
            case '"':
            case '\'':
            {
                const char quote = c;
                std::string::size_type p = m_pos + 1;
                std::string v;
                bool closed = false;
                for( ; p < m_s.size(); ++p )
                {
                    char d = m_s[p];
                    if( d == quote )
                    {
                        closed = true;
                        ++p;
                        break;
                    }
                    if( d == '\\' && p + 1 < m_s.size() )
                    {
                        ++p;
                        switch( m_s[p] )
                        {
                            case 'n': v += '\n'; break;
                            case 't': v += '\t'; break;
                            case 'r': v += '\r'; break;
                            default:  v += m_s[p]; break;
                        }
                        continue;
                    }
                    v += d;
                }
                if( !closed )
                {
                    t.type = TOK_EOF;
                    return;
                }
                t.type = TOK_STRING;
                t.text = v;
                m_pos = p;
                return;
            }
            case '?':
            case '$':
            {
                std::string::size_type p = m_pos + 1;
                const std::string::size_type b = p;
                // SPARQL var names cannot contain ':' or '.'
                while( p < m_s.size() && isWordChar( m_s[p] ) &&
                       m_s[p] != ':' && m_s[p] != '.' )
                    ++p;
                if( p == b )
                {
                    t.type = TOK_EOF;
                    return;
                }
                t.type = TOK_VAR;
                t.text = m_s.substr( b, p - b );
                m_pos = p;
                return;
            }
            default:
            {
                if( !isWordChar( c ) )
                {
                    ++m_pos;
                    t.type = TOK_EOF;
                    return;
                }
                std::string::size_type p = m_pos;
                while( p < m_s.size() && isWordChar( m_s[p] ) )
                    ++p;
                // a '.' can legitimately appear inside a prefixed
                // name (geo84:x.y) but a trailing '.' is the pattern
                // terminator -- hand it back as TOK_DOT
                while( p > m_pos && m_s[p-1] == '.' )
                    --p;
                t.type = TOK_WORD;
                t.text = m_s.substr( m_pos, p - m_pos );
                m_pos = p;
                return;
            }
        }
        // unrecognised character
        ++m_pos;
        t.type = TOK_EOF;
    }
};

static bool tokIsKeyword( const SparqlTok& t, const char* kw )
{
    if( t.type != TOK_WORD )
        return false;
    return g_ascii_strcasecmp( t.text.c_str(), kw ) == 0;
}

struct SparqlTerm
{
    enum Kind { VAR, IRI, LITERAL, BNODE } kind;
    std::string text;   // IRI: expanded uri; LITERAL/BNODE: lexical form; VAR: name
};

struct SparqlPattern
{
    SparqlTerm s, p, o;
};

struct SparqlFilterAtom
{
    bool negated = false;      // '=' vs '!='
    std::string var;           // str(?var) or bare ?var operand
    std::string literal;       // comparison literal
};

struct SparqlFilter
{
    // disjunction of conjunctions of atoms
    std::vector< std::vector< SparqlFilterAtom > > disj;
};

struct SparqlGroup
{
    std::vector< SparqlPattern > patterns;
    std::vector< SparqlGroup >   optionals;
    std::vector< SparqlFilter >  filters;
};

struct SparqlQuery
{
    bool distinct = false;
    bool selectAll = false;
    std::vector< std::string > selectVars;
    SparqlGroup where;
};

class SparqlParser
{
    SparqlLexer m_lex;
    std::map< std::string, std::string > m_prefixes;
    bool m_ok;

  public:
    explicit SparqlParser( const std::string& q )
        : m_lex( q )
        , m_ok( true )
    {
        // well-known defaults, same table PD_RDFModel::getUriToPrefix uses
        m_prefixes["rdf"]   = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
        m_prefixes["rdfs"]  = "http://www.w3.org/2000/01/rdf-schema#";
        m_prefixes["foaf"]  = "http://xmlns.com/foaf/0.1/";
        m_prefixes["pkg"]   = "http://docs.oasis-open.org/opendocument/meta/package/common#";
        m_prefixes["odf"]   = "http://docs.oasis-open.org/opendocument/meta/package/odf#";
        m_prefixes["dc"]    = "http://purl.org/dc/elements/1.1/";
        m_prefixes["dcterms"] = "http://dublincore.org/documents/dcmi-terms/#";
        m_prefixes["cal"]   = "http://www.w3.org/2002/12/cal/icaltzd#";
        m_prefixes["geo84"] = "http://www.w3.org/2003/01/geo/wgs84_pos#";
    }

    bool parse( SparqlQuery& q )
    {
        // prologue: [prefix] pfx: <iri> / base <iri>
        while( tokIsKeyword( m_lex.peek(), "prefix" ) ||
               tokIsKeyword( m_lex.peek(), "base" ) )
        {
            bool isPrefix = tokIsKeyword( m_lex.next(), "prefix" );
            if( isPrefix )
            {
                SparqlTok pfx = m_lex.next();            // e.g. "foaf:"
                SparqlTok iri = m_lex.next();
                if( pfx.type != TOK_WORD || iri.type != TOK_IRI )
                    return false;
                std::string name = pfx.text;
                if( !name.empty() && name[name.size()-1] == ':' )
                    name.erase( name.size()-1 );
                m_prefixes[name] = iri.text;
            }
            else
            {
                if( m_lex.next().type != TOK_IRI )
                    return false;
            }
        }

        if( !tokIsKeyword( m_lex.next(), "select" ) )
            return false;

        if( tokIsKeyword( m_lex.peek(), "distinct" ) ||
            tokIsKeyword( m_lex.peek(), "reduced" ) )
        {
            m_lex.next();
            q.distinct = true;
        }

        if( m_lex.peek().type == TOK_STAR )
        {
            m_lex.next();
            q.selectAll = true;
        }
        else
        {
            while( m_lex.peek().type == TOK_VAR )
                q.selectVars.push_back( m_lex.next().text );
            if( q.selectVars.empty() )
                return false;
        }

        if( tokIsKeyword( m_lex.peek(), "where" ) )
            m_lex.next();
        if( m_lex.next().type != TOK_LBRACE )
            return false;
        if( !parseGroup( q.where ) )
            return false;

        // reject trailing content: only EOF acceptable
        return m_ok && m_lex.next().type == TOK_EOF;
    }

  private:
    bool parseGroup( SparqlGroup& g )
    {
        for(;;)
        {
            const SparqlTok& t = m_lex.peek();
            if( t.type == TOK_EOF )
                return false;
            if( t.type == TOK_RBRACE )
            {
                m_lex.next();
                return true;
            }
            if( t.type == TOK_DOT )
            {
                m_lex.next();
                continue;
            }
            if( tokIsKeyword( t, "optional" ) )
            {
                m_lex.next();
                if( m_lex.next().type != TOK_LBRACE )
                    return false;
                SparqlGroup sub;
                if( !parseGroup( sub ) )
                    return false;
                g.optionals.push_back( sub );
                continue;
            }
            if( tokIsKeyword( t, "filter" ) )
            {
                m_lex.next();
                if( m_lex.next().type != TOK_LPAREN )
                    return false;
                SparqlFilter f;
                if( !parseFilterOr( f.disj ) )
                    return false;
                if( m_lex.next().type != TOK_RPAREN )
                    return false;
                g.filters.push_back( f );
                continue;
            }
            // unsupported group elements: fail the whole query
            if( tokIsKeyword( t, "union" ) || tokIsKeyword( t, "graph" ) ||
                tokIsKeyword( t, "minus" )  || tokIsKeyword( t, "service" ) ||
                tokIsKeyword( t, "group" )  || tokIsKeyword( t, "order" ) ||
                tokIsKeyword( t, "limit" )  || tokIsKeyword( t, "values" ) )
            {
                return false;
            }

            SparqlPattern pat;
            if( !parseTerm( pat.s ) || !parseTerm( pat.p ) || !parseTerm( pat.o ) )
                return false;
            g.patterns.push_back( pat );
            if( m_lex.peek().type == TOK_DOT )
                m_lex.next();
        }
    }

    bool parseTerm( SparqlTerm& term )
    {
        SparqlTok t = m_lex.next();
        switch( t.type )
        {
            case TOK_VAR:
                term.kind = SparqlTerm::VAR;
                term.text = t.text;
                return true;
            case TOK_IRI:
                term.kind = SparqlTerm::IRI;
                term.text = t.text;
                return true;
            case TOK_STRING:
                term.kind = SparqlTerm::LITERAL;
                term.text = t.text;
                return true;
            case TOK_WORD:
                if( t.text.size() > 2 && t.text.compare(0,2,"_:") == 0 )
                {
                    term.kind = SparqlTerm::BNODE;
                    term.text = t.text;
                    return true;
                }
                if( t.text == "a" )
                {
                    term.kind = SparqlTerm::IRI;
                    term.text = "http://www.w3.org/1999/02/22-rdf-syntax-ns#type";
                    return true;
                }
                {
                    std::string::size_type colon = t.text.find(':');
                    if( colon != std::string::npos )
                    {
                        std::map< std::string, std::string >::const_iterator
                            mi = m_prefixes.find( t.text.substr(0, colon) );
                        if( mi != m_prefixes.end() )
                        {
                            term.kind = SparqlTerm::IRI;
                            term.text = mi->second + t.text.substr(colon+1);
                            return true;
                        }
                    }
                    // undeclared prefix or bare word: match as a literal
                    // string so queries degrade gracefully
                    term.kind = SparqlTerm::LITERAL;
                    term.text = t.text;
                    return true;
                }
            default:
                return false;
        }
    }

    bool parseFilterOr( std::vector< std::vector< SparqlFilterAtom > >& disj )
    {
        std::vector< SparqlFilterAtom > conj;
        if( !parseFilterAnd( conj ) )
            return false;
        disj.push_back( conj );
        while( m_lex.peek().type == TOK_OR )
        {
            m_lex.next();
            conj.clear();
            if( !parseFilterAnd( conj ) )
                return false;
            disj.push_back( conj );
        }
        return true;
    }

    bool parseFilterAnd( std::vector< SparqlFilterAtom >& conj )
    {
        if( !parseFilterAtom( conj ) )
            return false;
        while( m_lex.peek().type == TOK_AND )
        {
            m_lex.next();
            if( !parseFilterAtom( conj ) )
                return false;
        }
        return true;
    }

    bool parseFilterAtom( std::vector< SparqlFilterAtom >& conj )
    {
        if( m_lex.peek().type == TOK_LPAREN )
        {
            // parenthesised sub-expression: merge its single-branch
            // conjunction; a multi-branch OR nested inside an AND
            // can't be folded into this flat representation -- fail
            // the query rather than mis-evaluate it
            m_lex.next();
            std::vector< std::vector< SparqlFilterAtom > > inner;
            if( !parseFilterOr( inner ) )
                return false;
            if( m_lex.next().type != TOK_RPAREN )
                return false;
            if( inner.size() != 1 )
                return false;
            conj.insert( conj.end(), inner[0].begin(), inner[0].end() );
            return true;
        }

        SparqlFilterAtom a;
        // left operand: str(?v) or bare ?v
        if( tokIsKeyword( m_lex.peek(), "str" ) )
        {
            m_lex.next();
            if( m_lex.next().type != TOK_LPAREN )
                return false;
            SparqlTok v = m_lex.next();
            if( v.type != TOK_VAR )
                return false;
            if( m_lex.next().type != TOK_RPAREN )
                return false;
            a.var = v.text;
        }
        else if( m_lex.peek().type == TOK_VAR )
        {
            a.var = m_lex.next().text;
        }
        else
        {
            return false;
        }

        const SparqlTok op = m_lex.next();
        if( op.type == TOK_EQ )
            a.negated = false;
        else if( op.type == TOK_NEQ )
            a.negated = true;
        else
            return false;

        SparqlTok rhs = m_lex.next();
        if( rhs.type != TOK_STRING && rhs.type != TOK_IRI && rhs.type != TOK_WORD )
            return false;
        a.literal = rhs.text;

        conj.push_back( a );
        return true;
    }
};

typedef std::map< std::string, std::string > SparqlBinding;

static bool termMatches( const SparqlTerm& t,
                         const std::string& nodeValue,
                         bool nodeIsLiteral )
{
    switch( t.kind )
    {
        case SparqlTerm::IRI:
            return nodeValue == t.text && !nodeIsLiteral;
        case SparqlTerm::BNODE:
            return nodeValue == t.text;
        case SparqlTerm::LITERAL:
            return nodeValue == t.text;
        default:
            return true;    // VAR binds anything
    }
}

static bool bindTerm( SparqlBinding& b,
                      const SparqlTerm& t,
                      const std::string& nodeValue,
                      bool nodeIsLiteral )
{
    if( t.kind == SparqlTerm::VAR )
    {
        SparqlBinding::iterator it = b.find( t.text );
        if( it == b.end() )
        {
            b[t.text] = nodeValue;
            return true;
        }
        return it->second == nodeValue;
    }
    return termMatches( t, nodeValue, nodeIsLiteral );
}

static void joinPattern( std::vector< SparqlBinding >& bindings,
                         const SparqlPattern& pat,
                         PD_RDFModelHandle model,
                         std::size_t maxRows )
{
    std::vector< SparqlBinding > out;
    for( std::vector< SparqlBinding >::iterator bi = bindings.begin();
         bi != bindings.end(); ++bi )
    {
        for( PD_RDFModelIterator it = model->begin();
             it != model->end(); ++it )
        {
            const PD_RDFStatement& st = *it;
            if( !st.isValid() )
                continue;

            SparqlBinding cand = *bi;
            if( !bindTerm( cand, pat.s, st.getSubject().toString(), false ) )
                continue;
            if( !bindTerm( cand, pat.p, st.getPredicate().toString(), false ) )
                continue;
            if( !bindTerm( cand, pat.o, st.getObject().toString(),
                           st.getObject().isLiteral() ) )
                continue;
            out.push_back( cand );
            if( out.size() >= maxRows )
            {
                bindings = out;
                return;
            }
        }
    }
    bindings.swap( out );
}

static bool filterPasses( const SparqlFilter& f, const SparqlBinding& b )
{
    for( std::vector< std::vector< SparqlFilterAtom > >::const_iterator
             di = f.disj.begin(); di != f.disj.end(); ++di )
    {
        bool conjOk = true;
        for( std::vector< SparqlFilterAtom >::const_iterator
                 ai = di->begin(); ai != di->end(); ++ai )
        {
            SparqlBinding::const_iterator v = b.find( ai->var );
            // comparing an unbound variable is an error in SPARQL and
            // drops the row for both = and !=
            bool eq = (v != b.end() && v->second == ai->literal);
            bool pass = (v != b.end()) && (ai->negated ? !eq : eq);
            if( !pass )
            {
                conjOk = false;
                break;
            }
        }
        if( conjOk )
            return true;
    }
    return false;
}

static void evalGroup( const SparqlGroup& g,
                       std::vector< SparqlBinding >& bindings,
                       PD_RDFModelHandle model,
                       std::size_t maxRows )
{
    for( std::vector< SparqlPattern >::const_iterator
             pi = g.patterns.begin(); pi != g.patterns.end(); ++pi )
    {
        joinPattern( bindings, *pi, model, maxRows );
        if( bindings.empty() )
            break;
    }

    for( std::vector< SparqlGroup >::const_iterator
             oi = g.optionals.begin(); oi != g.optionals.end(); ++oi )
    {
        std::vector< SparqlBinding > out;
        for( std::vector< SparqlBinding >::iterator
                 bi = bindings.begin(); bi != bindings.end(); ++bi )
        {
            std::vector< SparqlBinding > ext( 1, *bi );
            evalGroup( *oi, ext, model, maxRows );
            if( ext.empty() )
            {
                out.push_back( *bi );   // left-join: keep unextended
            }
            else
            {
                for( std::vector< SparqlBinding >::iterator
                         xi = ext.begin(); xi != ext.end(); ++xi )
                {
                    out.push_back( *xi );
                    if( out.size() >= maxRows )
                        break;
                }
            }
            if( out.size() >= maxRows )
                break;
        }
        bindings.swap( out );
    }

    for( std::vector< SparqlFilter >::const_iterator
             fi = g.filters.begin(); fi != g.filters.end(); ++fi )
    {
        std::vector< SparqlBinding > out;
        for( std::vector< SparqlBinding >::iterator
                 bi = bindings.begin(); bi != bindings.end(); ++bi )
        {
            if( filterPasses( *fi, *bi ) )
                out.push_back( *bi );
        }
        bindings.swap( out );
    }
}

} // anonymous namespace

#endif // !WITH_REDLAND



/**
 * Perpare to execute a SPARQL query on the submodel 'm' of the whole
 * document RDF 'rdf'. If you want to execute the query against all
 * the RDF for a document omit the submodel PD_RDFModelHandle
 * parameter 'm'.
 */
PD_RDFQuery::PD_RDFQuery( PD_DocumentRDFHandle rdf, PD_RDFModelHandle m )
    : m_rdf(rdf)
    , m_model(m)
{
    if( !m_model )
    {
        m_model = m_rdf;
    }
}

PD_RDFQuery::~PD_RDFQuery()
{
}

PD_ResultBindings_t
PD_RDFQuery::executeQuery( const std::string& sparql_query_string )
{
    PD_ResultBindings_t ret;

#ifndef WITH_REDLAND
    // Built-in SPARQL-subset evaluator; covers the query shapes the
    // RDF subsystem generates internally.  Queries outside the subset
    // return no bindings rather than mis-evaluating.
    if( !m_model || m_model->empty() )
        return ret;

    SparqlQuery q;
    SparqlParser parser( sparql_query_string );
    if( !parser.parse( q ) )
    {
        UT_DEBUGMSG(("PD_RDFQuery::executeQuery() unsupported SPARQL, no bindings\n"));
        return ret;
    }

    const std::size_t maxRows = 100000;
    std::vector< SparqlBinding > bindings( 1 );
    evalGroup( q.where, bindings, m_model, maxRows );

    std::set< std::string > seen;
    for( std::vector< SparqlBinding >::iterator
             bi = bindings.begin(); bi != bindings.end(); ++bi )
    {
        std::map< std::string, std::string > row;
        if( q.selectAll )
            row = *bi;
        else
        {
            for( std::vector< std::string >::const_iterator
                     vi = q.selectVars.begin(); vi != q.selectVars.end(); ++vi )
            {
                SparqlBinding::const_iterator v = bi->find( *vi );
                if( v != bi->end() )
                    row[*vi] = v->second;
            }
        }

        if( q.distinct )
        {
            std::string key;
            for( std::map< std::string, std::string >::const_iterator
                     ri = row.begin(); ri != row.end(); ++ri )
            {
                key += ri->first;
                key += '\x01';
                key += ri->second;
                key += '\x02';
            }
            if( seen.count( key ) )
                continue;
            seen.insert( key );
        }
        ret.push_back( row );
    }
    return ret;
#else
        
    if( m_model->empty() )
    {
        // The redland backend assumes there are 1+ triples
        // to avoid the edge case where there is nothing to query
        return ret;
    }
    
    librdf_model* rdfmodel = getRedlandModel( m_model );
    librdf_uri*   base_uri = nullptr;
    librdf_query* query = librdf_new_query( getWorld(),
                                            "sparql", nullptr,
                                            static_cast<unsigned char*>(sparql_query_string.c_str()),
                                            base_uri );
    librdf_query_results* results = librdf_query_execute( query, rdfmodel );
    if( !results )
    {
        return ret;
    }
    
    

    // convert redland results into our model format.
    for( ; !librdf_query_results_finished( results );
         librdf_query_results_next( results ))
    {
        xxx_UT_DEBUGMSG(("have query result, loop...\n"));
        
        std::map< std::string, std::string > x;
        const char ** names = nullptr;
        librdf_node** values = nullptr;
        int bc = librdf_query_results_get_bindings_count( results );
        if( !bc )
            continue;
        xxx_UT_DEBUGMSG(("have query result, bc: %d\n", bc));
        
        values = static_cast<librdf_node**>(calloc( bc+1, sizeof(librdf_node*)));
        if( !librdf_query_results_get_bindings( results, &names, values ) )
        {
            const char * name  = names[0];
            librdf_node* value = values[0];
            xxx_UT_DEBUGMSG(("initial  name: %s\n", name));

            for( int i = 0; name; ++i, name = names[i], value = values[i] )
            {
                xxx_UT_DEBUGMSG(("i: %d name: %s\n"));
                x.insert( make_pair( name, tostr( value )));
                librdf_free_node( value );
            }
        }
        free(values);

        ret.push_back(x);
    }
    
    return ret;

#endif
}



