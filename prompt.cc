#include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
#include <ctype.h>
#include <time.h>
// #include <sys/ea.h>
#include <sys/video.h>

#define INCL_DOSMISC
// #include <os2.h>

#include "nyaos.h"
#include "finds.h"
#include "strtok.h"
#include "strbuffer.h"
#include "prompt.h"

extern int nhistories;
extern int execute_result;
extern char *get_asciitype_ea(const char *fname,const char *eatype,int *l=0);

char *strcpytail(char *dp,const char *sp)
{
  while( *sp != '\0' )
    *dp++ = *sp++;
  *dp = '\0';
  return dp;
}

/* �p�X���A�z�[���f�B���N�g�������܂�ł���΁A�u�`�v�ɕϊ�����B
 *	sp �t�@�C����
 * return �����ς����t�@�C����(NULL�̎��́u�`�v���܂܂Ȃ������B
 */
static char *to_tilda_name(const char *sp)
{
  const char *home=getShellEnv("HOME");
  if( home == NULL || *home == '\0' )
    return NULL;
  
  /* ��r���� */
  while( *home != '\0' ){
    int x=tolower(*home & 255); if( x == '\\' ) x='/';
    int y=tolower(*sp   & 255); if( y == '\\' ) y='/';
    if( x != y )
      return NULL;
    ++home ; ++sp;
  }

  StrBuffer sbuf;
  sbuf << '~' << sp;

  return sbuf.finish();
}

/* �^�̃t�@�C�����𓾂�(�t�@�C�����̂݁A�f�B���N�g���͊܂܂�)
 *   return �������݌�� dp
 * �t�@�C�����������Ȃ��������� NULL ��Ԃ��B
 */
static char *paste_true_name(char *dp,const char *cwd)
{
  Dir dir;
  if( dir.dosfindfirst( cwd ) != 0 )
    return NULL;

  return strcpytail(dp,dir.get_name());
}

/* �啶���E����������ʂ������m�ȃt�@�C�����𓾂邪�A
 * ���� SRC ��j�󂵂Ă��܂��B�j�󂵂��������ꍇ��
 * correct_case() ���g���ׂ��B
 *    in	src �I���W�i���t�@�C����(�j�󂳂��)
 *    out	dst �啶���E�������𐳊m�ɂ����t�@�C����
 * return dst �̖����ւ̃|�C���^
 */
static char *_correct_case(char *src,char *dst)
{
  /* ���̕������
   *    x:\hoge\hoge
   *    x:\
   * �̂ǂ��炩�̃P�[�X�B
   */
  
  char *dp=dst;

  /* �h���C�u�������� */
  if( isalpha(*src & 255)  &&  *(src+1)==':' ){
    *dp++ = *src++;
    *dp++ = *src++;
  }
  /* ���[�g�f�B���N�g������ */
  if( *src=='\\' || *src=='/' ){
    ++src;
    *dp++ = '\\';
  }
  /* �T�u�f�B���N�g������؂�o���B*/
  Strtok tzer(src);
  char *token=tzer.cut_with("\\/");
  if( token != NULL ){
    for(;;){
      /* �܂��A�f�̃t�@�C�������R�s�[���Ă����B */
      char *tail_at_normal=strcpytail(dp,token);

      /* �啶���E�������̐��m�ȃt�@�C����������ꂽ��A
       * ��������ɃR�s�[������ɏ㏑������B */

      if( (dp=paste_true_name(dp,dst) ) == NULL )
	dp = tail_at_normal;

      if( (token=tzer.cut_with("\\/")) == NULL )
	break;

      *dp++ = '\\';
    }
  }
  *dp = '\0';
  return dp;
}

/* SRC �̑啶��/�������𐳂����C�������p�X���� dst �֓���B
 * SRC �͔j�󂵂Ȃ��B(open.cc ����g����)
 *	src      ���̃p�X��
 *	dst,size �ϊ���̃o�b�t�@�Ƃ��̃T�C�Y
 */
void correct_case( char *dst , const char *src , int size )
{
  char *tmp=(char*)alloca(size);
  _fullpath( tmp , src , size );
  _correct_case( tmp , dst );
}

/* �p�X���́u���v���u/�v�֕ϊ�����
 */
void anti_convroot(char *dst)
{
  while( *dst != '\0' ){
    if( *dst == '\\' ){
      *dst++ = '/';
    }else{
      if( is_kanji(*dst) )
	++dst;
      ++dst;
    }
  }
}

/* �J�����g�h���C�u�̃J�����g�f�B���N�g����啶���E�����������m�ɓ���
 * in/out - dst �t�@�C����(�㏑�������)
 * return �t�@�C�����̖����̈ʒu
 */
char *getcwd_case(char *dst)
{
  char cwd[ FILENAME_MAX ];

  cwd[0] = _getdrive();
  cwd[1] = ':';

  DosError( FERR_DISABLEHARDERR );
  char *rc = _getcwd( cwd+2 , sizeof(cwd)-2 );
  DosError( FERR_ENABLEHARDERR );
  if( rc == NULL ){
    *dst++ = cwd[0];
    *dst++ = cwd[1];
    *dst   = '\0';
    return dst;
  }
  
  char *tail = _correct_case(cwd,dst); // �p�X���̑啶���E���������C��
  anti_convroot(dst);	  // �p�X���́u���v���u/�v�֏C��
  return tail;
}

/* �t�@�C���V�X�e���𒲂ׂ�B
 *	drivenum : �h���C�u�ԍ�
 * return
 *   0:FAT  , 1:HPFS , 2:CDFS
 */
int query_filesystem(int drivenum)
{
  static unsigned char buffer[ sizeof(FSQBUFFER2)+(3*CCHMAXPATH) ];
  static unsigned char devname[3]="?:";

  ULONG cbBuffer = sizeof(buffer);
  PFSQBUFFER2	pfsqBuffer=(PFSQBUFFER2) buffer;

  devname[0] = toupper(drivenum & 255);
  
  if( DosQueryFSAttach(  devname , 0 , FSAIL_QUERYNAME
		       , pfsqBuffer , &cbBuffer ) != 0 ){
    return -1;
  }else{
    const unsigned char *p = pfsqBuffer->szName + pfsqBuffer->cbName + 1;

    if( p[0]=='F' && p[1]=='A' && p[2]=='T' && p[3]=='\0' )
      return 0;
    else if( p[0]=='H' && p[1]=='P' && p[2]=='F' && p[3]=='S' && p[4]=='\0')
      return 1;
    else if( p[0]=='C' && p[2]=='D' && p[3]=='F' && p[3]=='S' && p[4]=='\0')
      return 2;
    else
      return 3;
  }
}


char *get_cwd_long_name(char *dp)
{
  int rc=0;
  char cwd[ FILENAME_MAX ];
  *dp++ = cwd[0] = _getdrive();
  *dp++ = cwd[1] = ':';
  DosError( FERR_DISABLEHARDERR );    
  rc = _getcwd1( cwd+2 , toupper(cwd[0]) ) ;
  DosError( FERR_ENABLEHARDERR );
  if( rc != 0 )
    return dp;

  try{
    char *p=cwd+2;
    int size=sizeof(cwd)-2;
    (void)convroot(p,size,p);
  }catch(...){
    ; /* ������Ⴀ�A��O�����ɂ����Ӗ��Ȃ��Ȃ��c (^^;;) */
  }
  
  /* A:\
     0123 */
  
  /* ���[�g�f�B���N�g���̏ꍇ�̗�O���� */
  if( (cwd[2] == '/' || cwd[2] == '\\' ) && cwd[3]=='\0' ){
    *dp++ = '/'; *dp = '\0';
    return dp;
  }

  int filesystem=query_filesystem(cwd[0]);

  for(char *sp=cwd+3 ; ; sp++ ){
    if( *sp == '/' || *sp == '\\' || *sp == '\0' ){
      int org=*sp;
      *sp = '\0'; /* �ꎞ�I�Ƀ��[�g�� 0 �ɂ��� */
      *dp++ = '/';
      
      char *longname=0;
      if(    filesystem != 2
	 &&  (longname=get_asciitype_ea(cwd,".LONGNAME")) != NULL ){
	/* .LONGNAME �����݂���ꍇ�́A��������g�� */
	const char *sp=longname;
	while( *sp != '\0' ){
	  if( *sp == '\r' ){
	    ++sp;
	  }else if( *sp == '\n' ){
	    *dp++ = '\r';
	    ++sp;
	  }else if( '\0' <= *sp && *sp < ' ' ){
	    *dp++ = '^';
	    *dp++ = '@' + *sp++;
	  }else{
	    *dp++ = *sp++;
	  }
	}
	free(longname);
      }else{
	/* �����Ȃ���΁A�t�@�C������啶���E���������C�����邾�� */
	char *save_dp=dp;
	if( (dp = paste_true_name( dp , cwd )) == NULL ){
	  /* �^�̃t�@�C�����������Ȃ������ꍇ�A
	   * �P���Ƀp�X������؂�o���B
	   */
	  dp = save_dp;
	  const char *sq=_getname(cwd);
	  while( *sq != '\0' )
	    *dp++ = *sq;
	}
      }
      if( (*sp=org) == '\0' )
	break;
    }
    if( is_kanji(*sp) )
      ++sp;
  }
  *dp = '\0';
  return dp;
}


/* �o�b�t�@�� NYAOS ���S(��ʍŏ�i�ɕ\�����������) ���������� */
static void set_logo_to_prompt(StrBuffer &prompt)
{
  int a=0x0F;
  if( option_vio_cursor_control )
    a = v_getattr();

  const char logo[]=
    " Nihongo Yet Another Os/2 Shell "VERSION
      " (c) 1996-99 HAYAMA,Kaoru ";
	
  prompt << "\x1B[s\x1B[1;44;37m\x1B[H";
  prompt << logo << "\x1b[K";
  prompt << "\x1B[u\x1B[0m";
  
  if( option_vio_cursor_control )
    v_attrib(a);
}

/* �o�b�t�@�ɁA�w�肳��Ă���S�h���C�u�̃J�����g�f�B���N�g������������ */
static void set_multi_curdir_to_prompt(  StrBuffer &prompt
				       , const char *&promptenv )
{
  int a=0x0F;
  int curdrv=_getdrive();
  if( option_vio_cursor_control )
    a = v_getattr();
  
  /* �J�[�\���ʒu���L�� �� ��ʍŏ�i�ֈړ� */
  prompt << "\x1b[s\x1B[H";

  /* �����ŕϐ� length �́A�ŏ�i�ł̕\�����������J�E���g���� */
  int length=0;

  while(   *++promptenv != '}' 
	&& *  promptenv != '\0'
	&& length < screen_width-1 ){
    
    /* ${...} �̒��̓h���C�u���^�[�̂݁A���͖��� */
    if( !isalpha(*promptenv) )
      continue;

    char curdir[ FILENAME_MAX ];
    
    int drv=toupper(*promptenv);

    /* �J�����g�h���C�u�Ȃ�ΐԁA�����Ȃ���ΐŕ\������B*/
    prompt << "\x1B[1;" << (drv==curdrv ? "41" : "44")
      << ";37m" << (char) drv << ':';
    length += 3;
    
    DosError( FERR_DISABLEHARDERR );    
    _getcwd1(curdir,drv);
    DosError( FERR_ENABLEHARDERR );
    int len=strlen(curdir);
    
    if( length + len < screen_width-1 ){
      /* �J�����g�f�B���N�g���̃T�C�Y���\���A��ʕ��Ɏ��܂�ꍇ */
      length += len;
      prompt << curdir;
    }else{
      /* �J�����g�f�B���N�g���̃T�C�Y���A��ʕ��ɓ���Ȃ�
       * �� �N���b�s���O���� */
      
      const char *p=curdir;
      for(int i=length ; i<screen_width-4 ; i++ ){
	if( is_kanji(*p) ){
	  prompt << *++p;
	  ++i;
	}
	prompt << *++p;
      }
      if( length < 74 )
	prompt << "...";
      prompt << "\x1b[0m ";
      while( *promptenv != '}' && *promptenv != '\0' )
	++promptenv;
      break;
    }
    prompt << "\x1B[0m ";
  }
  prompt << "\x1b[K\x1b[u";

  if( option_vio_cursor_control )
    v_attrib(a);
}


/* �v�����v�g���쐬����B
 *     promptenv �v�����v�g�̌�������
 */
int Prompt::parse( const char *promptenv )
{
  try{
    StrBuffer prompt;
    used_topline=false;
    
    time_t now;
    time( &now );
    struct tm *thetime = localtime( &now );
    
    while( *promptenv != '\0' ){
      if( *promptenv == '$' ){
	switch( promptenv++ , to_upper(*promptenv) ){
	case '!':
	  prompt << nhistories+1;	break;
	case '@': prompt << _getvol(0);		break; /* �{�����[�����x�� */
	case '$': prompt << '$';			break;
	case '_': prompt << '\n';			break;
	case 'A': prompt << '&';			break;
	case 'B': prompt << '|';			break;
	case 'C': prompt << '(';			break;
	case 'E': prompt << '\x1b'; 		break;
	case 'F': prompt << ')';			break;
	case 'G': prompt << '>';			break;
	case 'H': prompt << '\b';			break;
	case 'L': prompt << '<';			break;
	case 'Q': prompt << '=';			break;
	case 'S': prompt << ' ';			break;
	case 'N': prompt << (char)_getdrive();	break;
	case 'R': prompt << execute_result;	break;
	case 'D':/* ���݂̓��t */
	  prompt.putNumber( thetime->tm_year+1900 , 4 , '0' ) << '-';
	  prompt.putNumber( thetime->tm_mon +1    , 2 , '0' ) << '-';
	  prompt.putNumber( thetime->tm_mday      , 2 , '0' );
	  break;
	case 'T':/* ���݂̎��� */
	  prompt.putNumber( thetime->tm_hour	, 2 , '0' ) << ':';
	  prompt.putNumber( thetime->tm_min	, 2 , '0' ) << ':';
	  prompt.putNumber( thetime->tm_sec	, 2 , '0' );
	  break;
	case 'I':/* ���S */
	  set_logo_to_prompt( prompt );
	  used_topline = true;
	  break;
	case '{':/* �e�h���C�u�̃J�����g�f�B���N�g�� */
	  set_multi_curdir_to_prompt( prompt , promptenv );
	  used_topline = true;
	  break;
	case 'V':/* OS/2�̃o�[�W���� */
	  prompt << "The Operating System/2 Version is "
	    << _osmajor/10 << '.' << _osminor;
	  break;
	case 'P':/* �J�����g�f�B���N�g�� */
	  {
	    char curdir[ FILENAME_MAX ];
	    getcwd_case( curdir );
	    prompt << curdir;
	  }
	  break;
	  
	case 'W':/* �J�����g�f�B���N�g��:�z�[���f�B���N�g�����u~�v�ɕϊ����� */
	  {
	    char curdir[ FILENAME_MAX ];
	    getcwd_case( curdir );
	    char *tilda_name=to_tilda_name(curdir);
	    if( tilda_name != NULL ){
	      prompt << tilda_name;
	      free(tilda_name);
	    }else{
	      prompt << curdir;
	    }
	  }
	  break;
	  
	case 'Z':
	  switch( ++promptenv , to_upper(*promptenv) ){
	  case 'A': 
	    prompt << '\a'; break;
	  case 'H': /* �q�X�g���ԍ� */
	    prompt << nhistories+1;
	    break;
	  case 'V': /* �{�����[�����x�� */
	    prompt << _getvol(0);
	    break;
	  case 'P': /* LONGNAME */
	    {
	      char curdir[FILENAME_MAX];
	      get_cwd_long_name( curdir );
	      prompt << curdir;
	    }
	    break;
	  case '\0':
	    goto promptend;
	  }
	  break;
	}
	promptenv++;
      }else{
	if( is_kanji(*promptenv) )
	  prompt << *promptenv++;
	prompt << *promptenv++;
      }
    }
  promptend:    
    free( promptstr );
    promptstr = prompt.finish();
    if( used_topline ){
      prompt << "\x1B[1A\x1B[1B" << promptstr;
      promptstr = prompt.finish();
    }
    return 0;
  }catch( StrBuffer::MallocError ){
    free( promptstr );
    promptstr = NULL;
    return -1;
  }
}
