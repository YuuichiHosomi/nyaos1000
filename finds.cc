#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <fnmatch.h>
#include <stdio.h> /* for sprintf */

#define INCL_DOSNLS
#include "macros.h"
#include "finds.h"

#define DEBUG(x) x

int dbcs_fnmatch (const char *mask, const char *name, int flags);
#define _fnmatch dbcs_fnmatch

int convroot(char *&dp,int &size,const char *sp) throw(size_t)
{
  assert( sp != NULL );
  assert( dp != NULL );

  int lastchar=0;
  while( *sp != '\0' ){
    lastchar = *sp;
    if( *sp == '/' ){
      lastchar = *dp++ = '\\';
      ++sp;
    }else{
      if( is_kanji(*sp) ){
	*dp++ = *sp++;
	if( --size <= 0 )
	  throw (size_t)-2;
      }
      *dp++ = *sp++;
    }
    if( --size <= 0 )
      throw (size_t)-1;
  }
  *dp = '\0';
  return lastchar;
}


int get_current_cp()
{
  ULONG CpList[8],CpSize;
  if( DosQueryCp(sizeof(CpList),CpList,&CpSize) == 0 ){
    return CpList[0];
  }else{
    return 0;
  }
}

Dir::Dir(): handle(0xFFFFFFFF), count(1)
{
  /* �p�ꃂ�[�h�œ��{��̃t�@�C�������擾����ƁA
   * �R�A���Ă��܂��̂ŁA���������{�ꃂ�[�h�ɂ��Ă���B
   */
  codepage = get_current_cp();
  DosSetProcessCp( 932 );
}

Dir::Dir(const char *path,int attr=ALL) : handle(0xFFFFFFFF),count(1)
{
  /* �p�ꃂ�[�h�œ��{��̃t�@�C�������擾����ƁA
   * �R�A���Ă��܂��̂ŁA���������{�ꃂ�[�h�ɂ��Ă���B
   */
  codepage = get_current_cp();
  DosSetProcessCp( 932 );
  this->findfirst(path,attr); 
}

Dir::~Dir()
{ 
  DosFindClose(handle);
  
  /* �ύX�����R�[�h�y�[�W�����ɖ߂� */
  DosSetProcessCp( codepage );
}

// �����̓f�B���N�g�����̂݁B������ opendir �ɑΉ�����B
// �f�B���N�g�����͖����� \ �� / �����Ă��Ă��悢�B
int Dir::findfirst(const char *fname,int attr)
{
  int size=strlen(fname)+3;
  char *path=static_cast<char*>(alloca(size));
  char *p=path;
  try{
    int lastchar = convroot(p,size,fname);
    if( lastchar != '\\'  &&  lastchar != ':' )
      *p++ = '\\';
    *p++ = '*';
  }catch(...){
    ;
  }
  *p = '\0';
  
  return dosfindfirst(path,attr);
}

/* ���C���h�J�[�h�t�@�C�������󂯓���� findfirst�B
 * �p�X������ϊ�����̂�
 */
int Dir::findfirst_with_wildcard(const char *fname,int attr)
{
  int size=strlen(fname)+1;
  char *path=static_cast<char*>(alloca(size));
  char *p=path;
  try{
    (void)convroot(p,size,fname);
    *p = '\0';
  }catch( ... ){
    return -1;
  }

  assert( size > 0 );
  return dosfindfirst(path,attr);
}

void fnexplode2_free(char **buffer)
{
  if( buffer != NULL ){
    for(char **p=buffer ; *p != NULL ; p++ ){
      free(*p);
      *p = NULL;
    }
    free(buffer);
  }
}

static int strcmp2( const void *x , const void *y )
{
  return *(char**)x-*(char**)y ?: strcmp( *(char**)x , *(char**)y );
}


/* emx �֐��� _fnexplode �̓Ǝ���
 */
char **fnexplode2(const char *path)
{
  assert( path != NULL );
  
  const char *lastroot=NULL;
  int finalchar = 0;
  bool have_wildcard = false;
  
  /* �p�X�̃f�B���N�g���������ƃt�@�C���������̕���
   * ���C���h�J�[�h���g���Ă��邩�ۂ��̒���
   */
  for(const char *p=path; *p != '\0' ; p++ ){
    finalchar = *p;
    if( *p=='\\' || *p=='/' || *p==':' ){
      lastroot = p;
    }else if( *p=='?' || *p=='*' ){
      have_wildcard = true;
    }
    if( is_kanji(*p) ){
      ++p;
      assert(*p != '\0');
    }
  }
  
  /* ���C���h�J�[�h���g���Ă��Ȃ��ꍇ�͏I�� */
  if(   have_wildcard==false || finalchar=='\\'
     || finalchar == '/' || finalchar == ':')
    return NULL;
  
  bool dotprint=false;

  int lendir = 0;
  char *dirname = "";
  if( lastroot != NULL ){
    assert( lastroot >= path );
    lendir=lastroot-path+1;

    // alloca �ɂ͕��G�Ȉ�����n���Ă͂����Ȃ��B
    int memsiz=lendir+1;
    dirname = (char*)alloca(memsiz);
    
    memcpy( dirname , path , lendir );
    dirname[lendir] = '\0';
    if( lastroot[1]=='.' )
      dotprint = true;
  }else{
    if( path[0] == '.' )
      dotprint = true;
  }
  Dir dir;

  dir.dosfindfirst(path,Dir::ALL);
  if( dir == NULL )
    return NULL;

  int nfiles=0;
  char **result = (char**)malloc(sizeof(char**));
  if( result == NULL )
    return NULL;
  
  do{
    /* �u.�v�Ŏn�܂�t�@�C���͊�{�I�ɕ\�����Ȃ��B
     *	- �t�@�C�������̂̎w��Łu.�v�Ŏn�܂�ꍇ�Ε�
     *	- �u.�v���̂ɂ͓W�J���Ȃ�
     */
    if(  dir.get_name()[0] == '.'
       && ( dotprint == false || dir.get_name()[1]=='\0' ) )
      continue;

    /* OS/2 �̃��C���h�J�[�h�W�J�ł́A�uhoge.���v�Łuhoge�v���}�b�`���Ă��܂��B
     * �����ŁAemx �̊֐��ŁA���������P�[�X�������Ă��B
     */
    if(  lastroot != NULL  
       ? _fnmatch(lastroot+1,dir.get_name(),_FNM_POSIX | _FNM_IGNORECASE)!=0 
       : _fnmatch(path,dir.get_name()      ,_FNM_POSIX | _FNM_IGNORECASE)!=0)
      continue;
	
    result[ nfiles ] = 
      (char*)malloc( lendir + dir.get_name_length()+1 );
    sprintf( result[ nfiles ] , "%s%s" , dirname , dir.get_name() );
    result = (char**)realloc( result , (++nfiles+1) * sizeof(char*) );
  }while( ++dir != NULL );
  result[ nfiles ] = NULL;

  if( nfiles <= 0 ){
    free(result);
    return NULL;
  }

  qsort(result,nfiles,sizeof(char*),strcmp2);

  return result;
}

void numeric_sort(char **array)
{
  if( *array == NULL )
    return;

  /* �ł��Y���̏��������̂���m�肳���Ă䂭�A�I���\�[�g�@ */
  while( *(array+1) != NULL ){
    if( isdigit(**array & 255) ){
      /* ���l�\�[�g������ꍇ�����鎞 */
      for( char **cur=array+1 ; *cur != NULL ; ++cur ){
	if(  isdigit(**cur & 255)
	   ? strnumcmp(*array,*cur) >= 0
	   : (**array >= **cur && strcmp(*array,*cur) >= 0 ) ){
	  char *tmp = *array;
	  *array = *cur;
	  *cur   = tmp;
	}
      }
    }else{
      /* ���l�\�[�g�͂��肦�Ȃ��ꍇ */
      for( char **cur=array+1 ; *cur != NULL ; ++cur ){
	if( **array >= **cur  &&  strcmp(*array,*cur) >= 0 ){
	  char *tmp = *array;
	  *array = *cur;
	  *cur   = tmp;
	}
      }
    }
    ++array;
  }
}

int strnumcmp(const char *s1,const char *s2)
{
  const unsigned char *p1=(const unsigned char *)s1;
  const unsigned char *p2=(const unsigned char *)s2;

  for(;;){
    if( isdigit(*p1) && isdigit(*p2) ){
      int n1=0,len1=0;
      do{
	n1 = n1*10+(*p1-'0');
	len1++;
      }while( isdigit(*++p1) );

      int n2=0,len2=0;
      do{
	n2 = n2*10+(*p2-'0');
	len2++;
      }while( isdigit(*++p2) );

      if( n1 != n2 ){
	return n1-n2;
      }else if( len1 != len2 ){
	/* �����������������擪�� 0 �̐��������ƍl������̂ŁA
	 * �t����������B*/
	return len1-len2;
      }else if( *p1 == '\0' || *p2 == '\0' )
	return 0;
    }else{
      if( *p1 != *p2 )
	return *p1 - *p2;
      if( *p1 == '\0' )
	return 0;
    }
    ++p1;++p2;
  }
}
