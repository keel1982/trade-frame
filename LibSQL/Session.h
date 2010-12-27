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

#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include <typeinfo>

#include "IDatabase.h"
#include "Sql.h"
#include "TableDef.h"

namespace ou {
namespace db {

//
// CSession code is at bottom of file.
//

// ==========

// this function should probably disappear at some point
template<typename K, typename M, typename P, typename F>
void LoadObject(
  const std::string& sErrPrefix, const std::string& sSqlSelect,
  K& key,
  sqlite3* pDb,
  sqlite3_stmt** pStmt,
  M& map,  // map
  P& p, // shared ptr
  F fConstructInstance  // function to construct instance of class
  )
  {

  M::iterator iter = map.find( key );
  if ( map.end() != iter ) {
    p = iter->second;
  }
  else {
    if ( NULL == pDb ) {
      std::string sErr( sErrPrefix );
      sErr += ":  no record available";
      throw std::runtime_error( sErr );
    }
    else {
      int rtn;

//      PrepareStatement( sErrPrefix, sSqlSelect, pStmt );

      rtn = p->BindDbKey( *pStmt );
      if ( SQLITE_OK != rtn ) {
        std::string sErr( sErrPrefix );
        sErr += ":  error in bind";
        throw std::runtime_error( sErr );
      }
      rtn = sqlite3_step( *pStmt );
      if ( SQLITE_ROW == rtn ) {
        p.reset( fConstructInstance( key, *pStmt ) );
      }
      else {
        std::string sErr( sErrPrefix );
        sErr += ":  error in load";
        throw std::runtime_error( sErr );
      }

      map.insert( std::pair<K,P>( key, p ) );
    }
  }
}

// this function should probably disappear at some point
template<typename K, typename P>
void SqlOpOnObject(
  const std::string& sErrPrefix, const std::string& sSqlOp,
  sqlite3* pDb,
  sqlite3_stmt** pStmt,
  K& key,
  P& p
  )
{
  if ( NULL != pDb ) {
    int rtn;

//    PrepareStatement( sErrPrefix, sSqlOp, pDb, pStmt );

    rtn = p->BindDbKey( *pStmt );
    if ( SQLITE_OK != rtn ) {
      std::string sErr( sErrPrefix );
      sErr += ": error in BindDbKey";
      throw std::runtime_error( sErr );
    }
    rtn = p->BindDbVariables( *pStmt );
    if ( SQLITE_OK != rtn ) {
      std::string sErr( sErrPrefix );
      sErr += ": error in BindDbVariables";
      throw std::runtime_error( sErr );
    }
    rtn = sqlite3_step( *pStmt );
    if ( SQLITE_DONE != rtn ) {
      std::string sErr( sErrPrefix );
      sErr += ": error in step";
      throw std::runtime_error( sErr );
    }
  }

}

// this function should probably disappear at some point
template<typename K, typename P>
void DeleteObject(
  const std::string& sErrPrefix, const std::string& sSqlOp,
  sqlite3* pDb,
  sqlite3_stmt** pStmt,
  K& key,
  P& p
  )
{
  if ( NULL != pDb ) {

    int rtn;

//    PrepareStatement( sErrPrefix, sSqlOp, pDb, pStmt );

    rtn = p->BindDbKey( *pStmt );
    if ( SQLITE_OK != rtn ) {
      std::string sErr( sErrPrefix );
      sErr += ": error in BindDbKey";
      throw std::runtime_error( sErr );
    }

    rtn = sqlite3_step( *pStmt );
    if ( SQLITE_DONE != rtn ) {
      std::string sErr( sErrPrefix );
      sErr += ": error in step";
      throw std::runtime_error( sErr );
    }

  }
}

//
// CSession
//

class CSession {
public:

   CSession( void );
   CSession( const std::string& sDbFileName, enumOpenFlags flags = EOpenFlagsZero );
  ~CSession( void );

  void Open( const std::string& sDbFileName, enumOpenFlags flags = EOpenFlagsZero );
  void Close( void );

  // need to convert for generic library call
  void PrepareStatement(
    const std::string& sErrPrefix, const std::string& sSqlOp, sqlite3_stmt** pStmt );

  template<typename T> // T derived from CStatementBase
  void Prepare( T& stmt ) {
    stmt.Prepare( m_db );
  }

  template<typename T> // T: Table Class with TableDef member function
  void RegisterTable( const std::string& sTableName );

  template<typename F>
  typename ou::db::CSql<F>::pCSql_t RegisterFields( void );

  void CreateTables( void );

protected:
private:
  
  bool m_bDbOpened;

  sqlite3* m_db;

  std::string m_sDbFileName;

  typedef ou::db::CSqlBase::pCSqlBase_t pCSqlBase_t;  // track use_count on exit to ensure all removed properly

  typedef std::map<std::string, pCSqlBase_t> mapTableDefs_t;  // map table name to table definition
  typedef mapTableDefs_t::iterator mapTableDefs_iter_t;
  typedef std::pair<std::string, pCSqlBase_t> mapTableDefs_pair_t;
  mapTableDefs_t m_mapTableDefs;

  typedef std::vector<pCSqlBase_t> vSql_t;
  typedef vSql_t::iterator vSql_iter_t;
  vSql_t m_vSql;

};

template<typename T>
void CSession::RegisterTable( const std::string& sTableName ) {
  mapTableDefs_iter_t iter = m_mapTableDefs.find( sTableName );
  if ( m_mapTableDefs.end() != iter ) {
    throw std::runtime_error( "table name already has definition" );
  }
  typename ou::db::CTableDef<T>::pCTableDef_t pTableDef;
  pTableDef.reset( new CTableDef<T>( sTableName ) );  // add empty table definition
  iter = m_mapTableDefs.insert( m_mapTableDefs.begin(), mapTableDefs_pair_t( sTableName, pTableDef ) );
}

template<typename F>
typename ou::db::CSql<F>::pCSql_t CSession::RegisterFields( void ) {
  typename ou::db::CSql<F>::pCSql_t pCSql;
  m_vSql.push_back( pCSql );
  pCSql.reset( new CSql<F>() );
  return pCSql;
}


} // db
} // ou
