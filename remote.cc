#include "remote.h"
#include <process.h>

#define INCL_DOSNMPIPES
#include <os2.h>

int RemoteNyaos::create( const char *baseName )
{
  /* �L���[�̖��O����� */
  free(name);
  StrBuffer abuf;
  abuf << "\\PIPE\\" << baseName << '.' << getpid();
  name = abuf.finish();

  errCode=DosCreateNPipe(  (unsigned char*) name
			 , &handle
			 , NP_ACCESS_INBOUND | NP_NOINHERIT
			 , NP_NOWAIT
			 | NP_TYPE_BYTE
			 | NP_READMODE_BYTE
			 | 1
			 , 0
			 , 512
			 , 0 );
  return errCode;
};

int RemoteNyaos::connect()
{
  if( ! ok() ) return 0;
  connectFlag = true;
  return errCode=DosConnectNPipe( handle );
}

int RemoteNyaos::disconnect()
{
  if( ! ok() ) return 0;
  connectFlag = false;
  return errCode=DosDisConnectNPipe( handle );
}

/* ���O�t���p�C�v���A1�����ǂ݂Ƃ�B
 * return
 *	>= 0 :�ǂ݂Ƃ���1�����̃R�[�h
 *	<  0 :�G���[ or EOF
 */
int RemoteNyaos::readchar() throw()
{
  for(;;){
    if( pointor >= left ){
      errCode=DosRead( handle , buffer , sizeof(buffer) , &left );
      pointor = 0;
      if( errCode != 0 )return -errCode;
      if( left <= 0 )	return -1;
    }
    if( buffer[ pointor ] != '\r' )
      break;
    ++pointor;
  } /* \r �𖳎����邽�߂̃��[�v */
  return buffer[ pointor++ ] & 255;
}

/* ���O�t���p�C�v���A1�s�ǂ݂Ƃ�B(\n �͊܂܂�)
 *	sbuf - �ǂ݂Ƃ��̃o�b�t�@
 * return
 *	����( < 0 : �G���[ or EOF );
 * throw
 *	CantExpand - �o�b�t�@�I�[�o�[�t���[
 */
int RemoteNyaos::readline( StrBuffer &sbuf ) throw( StrBuffer::MallocError )
{
  int ch;
  int count=0;
  
  if( (ch=readchar()) < 0 )
    return -1;

  if( ch == '\n' )
    return 0;
  
  do{
    sbuf << char(ch);
    ++count;
  }while( (ch=readchar()) >= 0 && ch != '\n' );
  
  return count;
}

int RemoteNyaos::close()
{
  if( handle != ~0u  &&  connectFlag  ){
    int rc=DosDisConnectNPipe( handle );
    handle = ~0u;
    return rc;
  }
  return 0;
}
