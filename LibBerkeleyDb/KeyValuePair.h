#pragma once

#include <string>
#include <assert.h>

#include "CommonDatabaseFunctions.h"

#include <stdexcept>

class CKeyValuePair : public CCommonDatabaseFunctions<CKeyValuePair> {
public:
  CKeyValuePair(void);
  virtual ~CKeyValuePair(void);

  const static char nMaxKeySize = 30;

  void Save( const std::string &key, char value );
  void Save( const std::string &key, const std::string &value );
  void Save( const std::string &key, short value );
  void Save( const std::string &key, unsigned short value );
  void Save( const std::string &key, long value );
  void Save( const std::string &key, unsigned long value );
  void Save( const std::string &key, float value );
  void Save( const std::string &key, double value );
  void Save( const std::string &key, const structValue &value ); // blob
  char GetChar( const std::string &key );
  void GetString( const std::string &key, std::string *pvalue );
  short GetShort( const std::string &key );
  unsigned short GetUnsignedShort( const std::string &key );
  long GetLong( const std::string &key );
  unsigned long GetUnsignedLong( const std::string &key );
  float GetFloat( const std::string &key );
  double GetDouble( const std::string &key );
  structValue GetBlob( const std::string &key );
protected:
  enum enumValueType : char { Char, String, UInt16, Int16, UInt32, Int32, Float, Double, Blob, Unknown, _Count }; 
  struct structKey {
    char nValueType;
    char nKeySize; // excludes terminating 0
    char chKey[ nMaxKeySize ];
    structKey( void ) : nValueType( Unknown ), nKeySize( 0 ) {};
    structKey( enumValueType vt, size_t size, const char *pkey ) : nValueType( vt ), nKeySize( 0 ) {
      assert( size <= nMaxKeySize );
      assert( 0 < size );
      nKeySize = (char) size;
      memcpy( chKey, pkey, size );
    }
  };
  void Save( const structKey &key, const structValue &value ) throw( std::runtime_error );
  void Get( const structKey &key, void **pVoid, size_t *pSize ) throw( std::runtime_error );  // validate KeyType
private:
};