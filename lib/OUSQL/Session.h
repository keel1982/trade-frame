/************************************************************************
 * Copyright(c) 2010, One Unified. All rights reserved.                 *
 *                                                                      *
 * This file is provided as is WITHOUT ANY WARRANTY                     *
 *  without even the implied warranty of                                *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                *
 *                                                                      *
 * This software may not be used nor distributed without proper license *
 * agreement.                                                           *
 *                                                                      *
 * See the file LICENSE.txt for redistribution information.             *
 ************************************************************************/

#pragma once

// 2011-01-30
// need to work on:
//   getting key back when inserting new record with auto-increment
//   adding transactions with rollback and commit

#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include <typeinfo>

#include <boost/intrusive_ptr.hpp>
#include <boost/noncopyable.hpp>

#include "Database.h"

namespace ou {
namespace db {

// dummy place holder for when no fields to bind
// will need to figure out a better way to handle this
struct NoBind {
};

// CQuery contains:
// * SS: statement state structures dedicated to the specific database
// * F: struct containing Fields function with ou::db::Field calls

// 2011-01-30  need to modify the structures so they can handle instance when there is no F.

class QueryBase {  // used as base representation for stowage in vectors and such
protected:
  enum enumClause { EClauseNone, EClauseQuery, EClauseWhere, EClauseOrderBy, EClauseGroupBy, EClauseBind, EClauseNoExecute };
public:

  typedef boost::intrusive_ptr<QueryBase> pQueryBase_t;

  QueryBase( void ): m_bHasFields( false ), m_cntRef( 0 ), m_bPrepared( false ), m_clause( EClauseNone ) {};
  virtual ~QueryBase( void ) {};

  void SetHasFields( void ) { m_bHasFields = true; };
  inline bool HasFields( void ) { return m_bHasFields; };

  void SetPrepared( void ) { m_bPrepared = true; };
  inline bool IsPrepared( void ) { return m_bPrepared; };

  // instrusive reference counting
  size_t RefCnt( void ) { return m_cntRef; };
  void Ref( void ) { ++m_cntRef; };
  size_t UnRef( void ) { --m_cntRef; return m_cntRef; };

  std::string& UpdateQueryText( void ) { 
//    assert( EClauseQuery >= m_clause );
//    m_clause = EClauseQuery;
    return m_sQueryText; 
  };

  const std::string& QueryText( void ) { return m_sQueryText; };

protected:
  enumClause m_clause;
  std::string m_sQueryText;  // 'compose' results end up here
private:
  bool m_bPrepared;  // used by session.execute
  bool m_bHasFields;
  size_t m_cntRef;
};

// =====

template<class F>  // used for returning structures to queriers
class QueryFields: 
  public QueryBase
{
public:
  typedef boost::intrusive_ptr<QueryFields<F> > pQueryFields_t;
  QueryFields( F& f ): QueryBase(), var( f ) {};
  ~QueryFields( void ) {};
  F& var;  // 2011/03/07  I want to make this a reference to a constant var at some point
   // will require mods to Bind, and will need Fields( A& a ) const, hopefully will work with the Actions passed in
protected:
private:
};

// =====

template<class SS, class F, class S>  // SS statement state, F fields, S session
class Query: 
  public QueryFields<F>,
  public SS // statement state
{
  friend S;
public:

  typedef boost::intrusive_ptr<Query<SS, F, S> > pQuery_t;

  Query( S& session, F& f ): QueryFields<F>( f ), m_session( session ), m_bExecuteOneTime( false ) {};
  ~Query( void ) {};

  Query& Where( const std::string& sWhere ) { // todo: ensure sub clause ordering
    assert( EClauseWhere > m_clause );
    m_sQueryText += " WHERE " + sWhere;
    m_clause = EClauseWhere;
    return *this; 
  };

  Query& OrderBy( const std::string& sOrderBy ) { // todo: ensure sub clause ordering
    assert( EClauseOrderBy > m_clause );
    m_sQueryText += " ORDERBY " + sOrderBy;
    m_clause = EClauseOrderBy;
    return *this;
  }

  Query& GroupBy( const std::string& sGroupBy ) {
    assert( EClauseGroupBy > m_clause );
    m_sQueryText += " GROUPBY " + sGroupBy;
    m_clause = EClauseGroupBy;
    return *this;
  }

  Query& NoExecute( void ) {
    assert( EClauseNoExecute > m_clause );
    m_bExecuteOneTime = false; // don't execute on the conversion
    m_clause = EClauseNoExecute;
    return *this;
  }

  template<class F>
  operator QueryFields<F>() { 
    return dynamic_cast<QueryFields<F> >( *this ); }

  // conversion operator:  upon converstion from Query to QueryFields (upon assignment), execute the bind and execute
  // may need to add an auto-reset before the bind:  therefore need m_bBound member variable
  template<class F>
  operator QueryFields<F>*() { 
    if ( m_bExecuteOneTime ) {
      if ( HasFields() ) {
        m_session.Bind( *this );
      }
      m_session.Execute( *this );
      m_bExecuteOneTime = false;
    }
    return dynamic_cast<QueryFields<F>* >( this ); }

protected:
private:

  bool m_bExecuteOneTime;
  void SetExecuteOneTime( void ) { m_bExecuteOneTime = true; };

  S& m_session;

};

// functions for intrusive ptr of the query structures

template<class Q>  // Q = Query
void intrusive_ptr_add_ref( Q* pq ) {
  pq->Ref();
}

template<class Q>
void intrusive_ptr_release( Q* pq ) {
  if ( 0 == pq->UnRef() ) {
    delete pq;
  }
}

//
// CSession
//   IDatabase needs typedef for structStatementState
//

template<class IDatabase> // IDatabase is a the specific handler type:  sqlite3 or pg or ...
class CSession: boost::noncopyable {
public:

  typedef CSession<IDatabase> session_t;

   CSession( void );
  ~CSession( void );

  void Open( const std::string& sDbFileName, enumOpenFlags flags = EOpenFlagsZero );
  void Close( void );

  template<class F>
  void Bind( QueryFields<F>& qf ) {
    IDatabase::structStatementState& StatementState 
      = dynamic_cast<IDatabase::structStatementState&>( qf );
    if ( !qf.IsPrepared() ) {
      m_db.PrepareStatement( StatementState, qf.UpdateQueryText() );
      qf.SetPrepared();
    }
    IDatabase::Action_Bind_Values action( StatementState );
    qf.var.Fields( action );
  }

  template<>
  void Bind<NoBind>( QueryFields<NoBind>& qf ) {
  }

  template<class F>
  void Bind( typename QueryFields<F>::pQueryFields_t pQuery ) {
    Bind( *pQuery.get() );
  }

  bool Execute( QueryBase& qb ) {
    IDatabase::structStatementState& StatementState 
      = dynamic_cast<IDatabase::structStatementState&>( qb );
    if ( !qb.IsPrepared() ) {
      m_db.PrepareStatement( StatementState, qb.UpdateQueryText() );
      qb.SetPrepared();
    }
    return m_db.ExecuteStatement( StatementState );
  }

  bool Execute( QueryBase::pQueryBase_t pQuery ) {
    return Execute( *pQuery.get() );
  }

  template<class F, class C>
  void Columns( typename QueryFields<F>::pQueryFields_t pQuery, C& columns ) {
    IDatabase::structStatementState& StatementState 
      = *dynamic_cast<IDatabase::structStatementState*>( pQuery.get() );
    IDatabase::Action_Extract_Columns action( StatementState );
    columns.Fields( action );
  }

  void Reset( QueryBase::pQueryBase_t pQuery ) {
    IDatabase::structStatementState& StatementState 
      = *dynamic_cast<IDatabase::structStatementState*>( pQuery.get() );
    m_db.ResetStatement( StatementState );
  }

  template<class F> // T: Table Class with TableDef member function
  typename Query<typename IDatabase::structStatementState, F, session_t>& RegisterTable( const std::string& sTableName ) {

    MapRowDefToTableName<F>( sTableName );

    mapTableDefs_iter_t iter = m_mapTableDefs.find( sTableName );
    if ( m_mapTableDefs.end() != iter ) {
      throw std::runtime_error( "table name already has definition" );
    }

    // test template getting at type without instantiating variable: complains about static call to non static function
    // use full specialization or partial specialization

    typedef Query<IDatabase::structStatementState, F, session_t> query_t;

    F f;  // warning, this variable goes out of scope before the query is destroyed
    query_t* pQuery = new query_t( *this, f );

    IDatabase::Action_Assemble_TableDef action( sTableName );
    f.Fields( action );
    if ( 0 < action.FieldCount() ) pQuery->SetHasFields();
    action.ComposeCreateStatement( pQuery->UpdateQueryText() );

    iter = m_mapTableDefs.insert( 
      m_mapTableDefs.begin(), 
      mapTableDefs_pair_t( sTableName, pQuery) );

    m_vQuery.push_back( pQuery );

    return *pQuery;

  }

  void CreateTables( void );

  template<class F>  // do reset, auto bind when doing execute
  typename Query<typename IDatabase::structStatementState, F, session_t>& Insert( F& f ) {
    return ComposeSql<F, IDatabase::Action_Compose_Insert>( f );
  }

  template<class F>  // do reset, auto bind when doing execute
  typename Query<typename IDatabase::structStatementState, F, session_t>& Update( F& f ) {
    return ComposeSql<F, IDatabase::Action_Compose_Update>( f );
  }

  template<class F>  // do reset, auto bind when doing execute
  typename Query<typename IDatabase::structStatementState, F, session_t>& Delete( F& f ) {
    return ComposeSql<F, IDatabase::Action_Compose_Delete>( f );
  }

  // also need non-F specialization as there may be no fields involved in some queries
  // todo:  need to do field processing, so can get field count, so need a processing action
  template<class F>  // do reset, auto bind if variables exist
  typename Query<typename IDatabase::structStatementState, F, session_t>& SQL( const std::string& sSqlQuery, F& f ) {

    typedef Query<IDatabase::structStatementState, F, session_t> query_t;

    query_t* pQuery = new query_t( *this, f );

    pQuery->UpdateQueryText() = sSqlQuery;

    pQuery->SetExecuteOneTime();
    
    m_vQuery.push_back( pQuery );

    return *pQuery;
  }

  // query with no parameters
  template<class F>
  typename Query<typename IDatabase::structStatementState, F, session_t>& SQL( const std::string& sSqlQuery ) {
    F f;  // warning, this variable goes out of scope before the query is destroyed
    return SQL( sSqlQuery, f );
  }

  template<class F>
  void MapRowDefToTableName( const std::string& sTableName ) {
    std::string sF( typeid( F ).name() );
    mapFieldsToTable_iter_t iter = m_mapFieldsToTable.find( sF );
    if ( m_mapFieldsToTable.end() == iter ) {
      m_mapFieldsToTable[ sF ] = sTableName;
    }
    else {
      if ( iter->second != sTableName ) {
        throw std::runtime_error( "type already defined" );
      }
    }
  }

protected:

  template<class F>
  //const std::string& MapFieldsToTable( void ) {
  const std::string& GetTableName( void ) {
    std::string t( typeid( F ).name() );
    mapFieldsToTable_iter_t iter = m_mapFieldsToTable.find( t );
    if ( m_mapFieldsToTable.end() == iter ) {
      throw std::runtime_error( "type not found" );
    }
    return iter->second;
  }

  template<class F, class Action>  // do reset, auto bind when doing execute
  typename Query<typename IDatabase::structStatementState, F, session_t>& ComposeSql( F& f ) {

    typedef Query<IDatabase::structStatementState, F, session_t> query_t;

    query_t* pQuery = new query_t( *this, f );
    
    Action action( GetTableName<F>() );
    f.Fields( action );
    if ( 0 < action.FieldCount() ) pQuery->SetHasFields();
    action.ComposeStatement( pQuery->UpdateQueryText() );

    pQuery->SetExecuteOneTime();

    m_vQuery.push_back( pQuery );

    return *pQuery;
  }

private:

  bool m_bOpened;
  
  IDatabase m_db;

  typedef QueryBase::pQueryBase_t pQueryBase_t;

  typedef std::map<std::string, pQueryBase_t> mapTableDefs_t;  // map table name to table definition
  typedef typename mapTableDefs_t::iterator mapTableDefs_iter_t;
  typedef std::pair<std::string, pQueryBase_t> mapTableDefs_pair_t;
  mapTableDefs_t m_mapTableDefs;

  typedef std::vector<pQueryBase_t> vQuery_t;
  typedef vQuery_t::iterator vQuery_iter_t;
  vQuery_t m_vQuery;

  typedef std::map<std::string, std::string> mapFieldsToTable_t;
  typedef mapFieldsToTable_t::iterator mapFieldsToTable_iter_t;
  typedef std::pair<std::string, std::string> mapFieldsToTable_pair_t;
  mapFieldsToTable_t m_mapFieldsToTable;

};

// Constructor
template<class IDatabase>
CSession<IDatabase>::CSession( void ): m_bOpened( false ) {
}

// Destructor
template<class IDatabase>
CSession<IDatabase>::~CSession(void) {
  Close();
}

// Open
template<class IDatabase>
void CSession<IDatabase>::Open( const std::string& sDbFileName, enumOpenFlags flags ) {
  if ( m_bOpened ) {
    std::string sErr( "Session already opened" );
    throw std::runtime_error( sErr );
  }
  else {
    m_db.Open( sDbFileName, flags );
    m_bOpened = true;
  }
  
}

// Close
template<class IDatabase>
void CSession<IDatabase>::Close( void ) {
  if ( m_bOpened ) {
    m_mapTableDefs.clear();
    for ( vQuery_iter_t iter = m_vQuery.begin(); iter != m_vQuery.end(); ++iter ) {
      m_db.CloseStatement( *dynamic_cast<IDatabase::structStatementState*>( iter->get() ) );
    }
    m_vQuery.clear();
    m_db.Close();
    m_bOpened = false;
  }
}

// CreateTables
template<class IDatabase>
void CSession<IDatabase>::CreateTables( void ) {
  // todo: need to add a transaction around this set of instructions
  for ( mapTableDefs_iter_t iter = m_mapTableDefs.begin(); m_mapTableDefs.end() != iter; ++iter ) {
    Execute( iter->second );
  }
}

} // db
} // ou