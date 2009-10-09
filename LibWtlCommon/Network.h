/************************************************************************
 * Copyright(c) 2009, One Unified. All rights reserved.                 *
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

#include <codeproject/thread.h>  // class inbound messages

#include <boost/asio.hpp>  // class outbound processing
#include <boost/thread.hpp>  // separate thread for asio run processing
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/array.hpp>

#include <string>
#include <vector>

#include "LibCommon/ReusableBuffers.h"

// http://www.codeproject.com/KB/wtl/WTLIntellisense.aspx

// use asio post messages to send commands 
// use CThread PostMessages to return responses
// use circular buffer of 20 messages
// post message when circular buffer is empty
// based upon Lock-Free Queues? http://www.ddj.com/hpc-high-performance-computing/208801974
// input streams are cr/lf delimited
// LOG received buffer length to see if we are actually using big buffers on reception
// constructor uses static init structure, rather than dynamic update one message types
// maybe do dynamic message pointer updates later
// three threads:
//   * external thread for routines that send message to this class
//   * asio thread for processing outbound and inbound
//   * windows thread for processing windows messages
// when running with socket async io, is a timer needed to keep asio running?
// collect statistics to see if network side input buffers are being broken up
//   if they are, then remain with character by character delivery
//   if they are not, then can use begin/end iterators to pass information
//   or come up with a different scheme of managing iterators over multiple buffers for use by Spirit

class CNetwork: public CGuiThreadImpl<CNetwork> {
public:

  // message ids for messages delivered to this class from external caller
  enum enumMessageTypes { 
    WM_NETWORK_CONNECT = WM_USER + 1,  // open the port
    WM_NETWORK_DISCONNECT,  // close the port
    WM_NETWORK_SEND,  // send string over network port
    WM_NETWORK_PROCESSED // message has been processed, return buffer to queue for re-use
  };

  enum enumMessageErrorTypes {
    OK = 0,
    ERROR_WRITE,
    ERROR_SOCKET,
    ERROR_CONNECT
  };

#define NETWORK_INPUT_BUF_SIZE 2048

  // pre-initialized message ids for messages delivered to and accepted by external caller
  struct structMessages {  // information needed by port processing thread
    HWND hWnd;        // handle of window to which message is sent
    UINT msgInitialized;   // message id when class thread has been  initialized  (wparam=null, lparam=null)
    UINT msgClosed;   // message id when class thread has ended   (wparam=null, lparam=null)
    UINT msgConnected;    // message id when socket has been connected  (wparam=null, lparam=null)
    UINT msgDisconnected;  // message id when socket has been disconnected/closed  (wparam=null, lparam=null)
    UINT msgProcess;  // message id when sending line of text to process  (wparam=linebuffer_t*, lparam=null)
    UINT msgSendDone; // returns buffer used by the send message  (wparam=linebuffer_t*, lparam=null)
    UINT msgError;    // message id when error is encountered  (wparam=enumMessageErrorTypes, lparam=null)
    structMessages(void): hWnd( 0 ), msgProcess( 0 ), msgInitialized( 0 ), msgClosed( 0 ), msgError( 0 ) {};
    structMessages(HWND hWnd_, UINT msgProcess_, UINT msgInitialized_, UINT msgClosed_, UINT msgError_ )
      : hWnd( hWnd_ ), msgProcess( msgProcess_ ), msgInitialized( msgInitialized_ ), msgClosed( msgClosed_ ), msgError( msgError_ ) {};
  };

  struct structConnection {
    std::string sAddress;
    unsigned short nPort;
  };

  typedef unsigned char bufferelement_t;
  typedef boost::array<bufferelement_t, NETWORK_INPUT_BUF_SIZE> inputbuffer_t; // bulk input buffer via asio
  typedef CBufferRepository<inputbuffer_t> inputrepository_t;
  typedef std::vector<bufferelement_t> linebuffer_t;  // used for composing lines of data for processing
  typedef CBufferRepository<linebuffer_t> linerepository_t;

  CNetwork(CAppModule* pModule, const structMessages&);
  ~CNetwork(void);

protected:
  BEGIN_MSG_MAP_EX(CNetwork)
//    MSG_WM_TIMER(OnTimer)
    MESSAGE_HANDLER(WM_NETWORK_CONNECT, OnConnect)  //(wparam=const structConnection&, lparam=null)
    MESSAGE_HANDLER(WM_NETWORK_SEND, OnSend)  // command to be transmitted to network  (wparam=linebuffer_t*, lparam=null)
    MESSAGE_HANDLER(WM_NETWORK_PROCESSED, OnProcessed)  // line buffer is returned to repository  (wparam=linebuffer_t*, lparam=null)
    MESSAGE_HANDLER(WM_NETWORK_DISCONNECT, OnDisconnect)  //(wparam=null, lparam=null)
  END_MSG_MAP()

  LRESULT OnConnect( UINT, WPARAM, LPARAM, BOOL &bHandled);
  LRESULT OnSend( UINT, WPARAM, LPARAM, BOOL &bHandled);
  LRESULT OnProcessed( UINT, WPARAM, LPARAM, BOOL &bHandled);
  LRESULT OnDisconnect( UINT, WPARAM, LPARAM, BOOL &bHandled);

  BOOL InitializeThread( void );
  void CleanupThread( DWORD );

  BOOL PostMessage( UINT, WPARAM = NULL, LPARAM = NULL );

private:

  structMessages m_Messages;

  bool m_bKeepTimerActive;
  bool m_bSocketOpen;

  boost::thread m_asioThread;

  boost::asio::io_service m_io;
  boost::asio::ip::tcp::socket* m_psocket;

  boost::asio::deadline_timer m_timer;  // used to keep asio.run something to keep busy with

  inputrepository_t m_reposInputBuffers;
  linerepository_t m_reposLineBuffers;

  linebuffer_t* m_line;

  unsigned int m_cntBytesTransferred_input;
  unsigned int m_cntAsyncReads;
  unsigned int m_cntSends;
  unsigned int m_cntBytesTransferred_send;

  void ConnectHandler( const boost::system::error_code& error );
  void TimerHandler( const boost::system::error_code& error );
  void WriteHandler( const boost::system::error_code& error, std::size_t bytes_transferred, linebuffer_t* );
  void ReadHandler( const boost::system::error_code& error, std::size_t bytes_transferred, inputbuffer_t* );
  void AsyncRead( void );

  void AsioThread( void );

};