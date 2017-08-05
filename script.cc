/* -*- c++ -*-
 * �֐�
 *	replace_script		�X�N���v�g�ϊ��������s���B
 *	is_innser_command	�����R�}���h���ۂ����`�F�b�N����B
 *	getApprecationType	�A�v���^�C�v�𓾂�B
 *	script_to_cache		�X�N���v�g�ƃC���^�v���^�����L���b�V���O����B
 *	read_script_header	�X�N���v�g����C���^�v���^����ǂݏo���B
 *	expand_sos		SOS�X�N���v�g����R�}���h���C���֓W�J����B
 *	read_sos_header		SOS�X�N���v�g�Ȃ�΃t�H�[�}�b�g��ǂݏo���B
 *	copy_args		�������R�s�[����B
 *	copy_filename		�t�@�C�������R�s�[����B
 *
 *	cmd_rehash		rehash �R�}���h�̏���
 *	cmd_cache		cache �R�}���h�̏���
 */

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>

#define INCL_DOSFILEMGR
#define INCL_DOSMISC
#define INCL_DOSSESMGR

#include "macros.h"
#include "finds.h"
#include "hash.h"
#include "Parse.h"
#include "nyaos.h"	/* for Command class */
#include "complete.h"	/* cmd_rehash �ׂ̈����̂� */
#include "strbuffer.h"
#include "autofileptr.h"

int scriptflag=1;
int option_amp_start=1;
int option_amp_detach=0;
int option_sos=0;
int option_script_cache=1;
int option_auto_close=1;

extern void debugger(const char *,...);

struct ScriptCache{
  char *name;
  char *interpreter;

  ScriptCache() : name(0) , interpreter(0){ }
  ~ScriptCache(){ free(name); free(interpreter); }
};

Hash <ScriptCache> script_hash(1024);

extern int option_debug_echo;

/* �R�}���h�ucache�v�F
 * �X�N���v�g�L���b�V���ɕۑ�����Ă���L���b�V������
 * ��ʂɕ\������B
 */
int cmd_cache(FILE *source, Parse &args )
{
  for(HashIndex<ScriptCache> hi(script_hash) ; *hi != NULL ; hi++ ){
    printf("%s = %s\n",hi->name,hi->interpreter);
  }
  return 0;
}

/* �R�}���h�urehash�v�F
 * �X�N���v�g�L���b�V���̏���S�Ĕj��������B
 */
int cmd_rehash(FILE *source , Parse &args )
{
  bool quiet=false;
  bool atzero=false;

  for(int i=0;i<args.get_argc();i++){
    if( args[i][0]=='-' ){
      for(int j=1;j<args[i].len;j++){
	switch( args[i][j] ){
	case 'q':
	case 'Q':
	  quiet = true;
	  break;
	  
	case 'n':
	case 'N':
	  atzero = true;
	  break;
	}
      }
    }
  }
  script_hash.destruct_all();

  if( atzero  &&  Complete::queryFiles() > 0 )
    return 0;

  Complete::make_command_cache();
  if( !quiet ){
    FILE *fout=args.open_stdout();
    if( fout != NULL ){
      fprintf(fout
	      , "%d bytes are used for %d commands'name cache.\n"
	      , Complete::queryBytes() , Complete::queryFiles()
	      );
    }
  }
  return 0;
}


/* copy_filename �F�t�@�C������ StrBuffer �֓ǂݎ��B
 *	buf �ǂݎ��o�b�t�@
 *	sp  �t�@�C�����̓��̈ʒu
 *	flag ����t���O(�ȉ��� enum �Q�� )
 * return
 *	�t�@�C�����̖����̎��̈ʒu('\0'�̈ʒu)
 * throw
 *	NULL �������m�ێ��s(StrBuffer�R���̂���)
 */
enum{
  SPACE_TERMINATE	= 1,	/* �󔒂𖖔��Ƃ݂Ȃ��B	*/
  SLASH_DEMILITOR	= 2,	/* �����^�֕ϊ�����B	*/
  BACKSLASH_DEMILITOR	= 4,	/* �^�����֕ϊ�����B	*/
};

static const char *copy_filename(  StrBuffer &buf
				 , const char *sp 
				 , int flag=SPACE_TERMINATE )
     throw(StrBuffer::MallocError)
{
  for(;;){
    /* �u&�v��u|�v�A�u\0�v�ȂǁA�R�}���h�����̕�����Ȃ�I�� */
    if( Parse::is_terminal_char(*sp) )
      break;

    /* �X�y�[�X�ł��I������ꍇ������ */
    if( (flag & SPACE_TERMINATE) != 0  &&  is_space(*sp) )
      break;

    /* �R�}���h�� : "/"-->"\\"�ɒu�� */
    if( *sp == '"' ){
      do{
	buf << *sp++;
	if( *sp == '\0' )
	  goto exit;
      }while( *sp != '"' );
    }
    if( *sp == '/' && (flag & BACKSLASH_DEMILITOR) !=0 ){
      buf << '\\';
      ++sp;
    }else if( *sp == '\\' && (flag & SLASH_DEMILITOR) !=0 ){
      buf << '/';
      ++sp;
    }else{
      if( is_kanji(*sp) )
	buf << *sp++;
      buf << *sp++;
    }
  }
 exit:
  return sp;
}


/* copy_args �F�S�Ă̈���(|��&���A\0�܂ł̕�����sp)�� BUF �փR�s�[����B
 *	buf �R�s�[��
 *	sp  �R�s�[��
 * return
 *	sp ��ǂ񂾌�̖���('\0'�� & , | �Ȃǂ̈ʒu)
 * throw
 *	NULL �������m�ۃG���[
 */
static const char *copy_args( StrBuffer &buf , const char *sp )
     throw (StrBuffer::MallocError)
{
  while( ! Parse::is_terminal_char(*sp) ){
    if( *sp == '"' ){
      do{
	if( is_kanji(*sp) )
	  buf << *sp++;
	buf << *sp++;
	if( *sp == '\0' )
	  goto exit;
      }while( *sp != '"' );
    }
    if( is_kanji(*sp) )
      buf << *sp++;
    buf << *sp++;
  }
 exit:
  return sp;
}

/* sos_script :
 * �@�t�@�C�� PATH �� SOS�X�N���v�g�Ȃ�΁A
 * �C���^�v���^���L�q�s�� Heap ������ŕԂ��B
 * �����Ȃ���΁ANULL ��Ԃ�
 */
static char *read_sos_header( const char *path ) throw(StrBuffer::MallocError)
{
  AutoFilePtr fp(path,"r");
  if( fp == NULL )
    return NULL;

  int nlines=0;
  for(;;){
    int ch=getc(fp);
    if( ch=='\n' ){
      /* 1�J�����ڂ́u#�v�u;�v�u%�v�u:�v�u'�v�̂�
       * ����ӊO�̏ꍇ�A�X�N���v�g�Ƃ݂͂Ȃ��Ȃ��B
       */
      int ch=getc(fp);
      if( ch !='#' && ch !=';' && ch !='%' && ch !=':' && ch !='\'' )
	break;

      /* SOS�X�N���v�g�͂Q�s�ڂ� soshdr/Nide �Ƃ����T�C��������I*/
      if( ++nlines==1 ){
	for( const char *s="soshdr/Nide" ; *s != '\0' ; s++ ){
	  if( getc(fp) != *s )
	    return NULL;
	}
      }else if( nlines >= 14 ){
	/* SOS.HDR �� 13�s�ł��邱�Ƃ���A���̍s�����A
	 * ���s�v���O���������L�q���ꂽ�s�ł���B
	 * �Ƃ����킯�ŁA���̍s�𔲂��o���ĕԂ��I
	 */
	StrBuffer buf;
	while( (ch=getc(fp)) != EOF  &&  ch != '\n' )
	  buf << ch;

	return buf.finish();
      }
    }else if( !isprint(ch) || ch==EOF ){
      break;
    }
  }
  /* break �ł��̃��[�v��E�o������̂́A
   * ������� SOS�X�N���v�g�łȂ������P�[�X
   */
  return 0;
}

/* expand_sos :
 *   SOS�̃w�b�_����������ɃR�}���h���C���֓W�J����B
 *
 *	buf	�W�J��
 *	fmt	SOS�̃w�b�_������
 *	prog	�X�N���v�g��
 *	argv	����
 * throw NULL �������G���[
 */
static void expand_sos(  StrBuffer &buf 
		       , const char *fmt
		       , const char *prog
		       , const char *argv )
     throw(StrBuffer::MallocError)
{
  for( ; *fmt != '\0' ; ++fmt ){
    if( *fmt != '%' ){
      buf << *fmt;
    }else{
      switch( *++fmt ){
      case '0':
	copy_filename( buf , prog , SLASH_DEMILITOR );
	break;
      case '@':
	buf << argv;
	break;
      case '%':
	buf << '%';
	break;
      default:
	buf << '%' << *fmt;
	break;
      }
    }
  }
}

/* read_script_header :
 * �X�N���v�g�t�@�C�������ۂɃI�[�v�����āA�u#!�v�̌�ɋL�q����Ă���
 * �C���^�[�v���^����ǂݏo���A����� buf �֏����o���B
 *	fname �X�N���v�g�̃t�@�C����
 *	return �C���^�v���^��(�q�[�v�̕�����:free���K�v)
 */
static char *read_script_header( const char *fname )
     throw(StrBuffer::MallocError)
{
  AutoFilePtr fp(fname,"r");
  if( fp==NULL )
    return NULL;
  
  if( getc(fp) != '#' || getc(fp) != '!' )
    return NULL;

  StrBuffer buf; /* �C���^�v���^����ۑ� */

  /* ���ϐ� SCRIPTDRIVE �̍ŏ��̈ꕶ���𕡎� */
  int ch;
  const char *usp=0;
  if( (ch=getc(fp))=='/' && (usp=getShellEnv("SCRIPTDRIVE")) != NULL ){
    while( *usp != '\0' && *usp != ':' ){
      buf << *usp++;
    }
    buf << ':';
  }
  /* perl��awk�Ȃǂ̎��s�t�@�C�����̕��� */
  while( ch != EOF  &&  ch != '\n' ){
    buf << (char)(ch=='/' ? '\\' : ch);
    ch=getc(fp);
  }
  return buf.finish();
}
 
/* �X�N���v�g�ƃC���^�v���^�����L���b�V��
 * (�O���[�o���ϐ� script_hash)�ɕۑ�����B
 *	script		�X�N���v�g��(Heap������)
 *	interpreter	�C���^�[�v���^��(Heap������)
 * throw NULL  �������m�ۃG���[
 *
 */
static void script_to_cache( char *script , char *interpreter ) 
     throw(StrBuffer::MallocError)
{
  ScriptCache *sc=new ScriptCache;
  if( sc == NULL )
    throw StrBuffer::MallocError();

  sc->name = script;
  sc->interpreter = interpreter;
  
  /* �������e�̃n�b�V��������΁A�����j�������Ă���A
   * �V�K�o�^ */
  script_hash.destruct( sc->name );
  script_hash.insert( sc->name , sc );
}

/* �A�v���P�[�V�����̃^�C�v�𓾂�
 *	fname ���s�t�@�C���̖��O
 * return
 *	0 �^�C�v�s��		 
 *	1 ��E�C���h�E�݊�
 *	2 �E�C���h�E�݊��i�u�h�n�j
 *	3 �E�C���h�EAPI�i�o�l�j
 *	-1 �G���[
 * throw ����
 */
static int getApplicationType(const char *fname) throw()
{
  ULONG apptype;
  if( DosQueryAppType(  (const unsigned char *)fname , &apptype ) != 0 )
    return -1;
  return apptype & 7;
}

/* �����R�}���h���A�ǂ������`�F�b�N����B
 *	fname �R�}���h��
 * return
 *	false �����R�}���h�ł͂Ȃ�����
 *	true  �����R�}���h�������B
 */

static bool is_inner_command( const char *name ) throw()
{
  extern Hash <Command> command_hash;
  extern int option_ignore_cases;
  
  Command *buildinCommand;
  if( option_ignore_cases ){
    buildinCommand = command_hash.lookup_tolower( name );
  }else{
    buildinCommand = command_hash[ name ];
  }
  return buildinCommand != NULL;
}

/* �X�N���v�g�ϊ����s���B
 *
 * return
 *	�ϊ���̃e�L�X�g�B�q�[�v������Ȃ̂ŁA�g�p���
 *	free ���邱�Ƃ��K�v�B
 * throw
 *	StrBuffer::MallocError �����ʂ�
 */
char *replace_script( const char *sp ) throw(MallocError)
{
  StrBuffer buf;
  
  for(;;){  /* �R�}���h���̃��[�v */
    while( *sp != '\0'  &&  is_space(*sp) )
      buf << *sp++;

    /* true �Ȃ�΁AVIO�A�v���̎���"/C /F" ���}�������*/
    bool auto_close=false;

    if( option_amp_start || option_amp_detach ){
      // ��s���āA������ & ���ǂ�������ׂ�B
      // �����A�����Ȃ�ΐ擪�Ɂustart�v��ǉ�����B

      const char *p=sp;
      for(;;){
	switch( *p ){
	case '>':		/* >& �Ƃ������_�C���N�g�}�[�N������ */
	  if( *++p == '&' )
	    ++p;
	  break;
	  
	case '&':
	  while( is_space(*++p) )
	    ;
	  if( *p != '&' && *p != ';' ){
	    /* �u&&�v,�u&;�v�łȂ��u&�v�Ȃ� start �܂��� detach ��}�� */
	    if(option_amp_detach){
	      buf << "detach ";
	    }else{
	      buf << "start ";
	      if( option_auto_close )
		auto_close = true;
	    }
	  }
	  goto check_script;

	case '|':
	case '\0':
	  goto check_script;

	case '"':
	  do{
	    if( *++p == '\0' )
	      goto check_script;
	  }while( *p != '"' );
	  ++p;
	  break;

	default:
	  if( is_kanji(*p) )
	    ++p;
	  ++p;
	  break;
	}/* switch() */
      }/* for(;;) */
    } /* if option_amp... */

  check_script:
    if( scriptflag != 0  ){
      // ---------------------------
      // option +script �̏ꍇ
      // ---------------------------

      StrBuffer fname;
      char path[FILENAME_MAX];
      ScriptCache *sc;
      char *header=0;
      int type;
      const char *suffix=0;

      /* �Ƃ肠�����A�R�}���h����ʂ̃o�b�t�@�ɕۑ����Ă����āA 
       * �|�C���^��i�߂�B(�u$0�v�� fname) 
       */
      sp = copy_filename( fname , sp );

      if( is_inner_command( fname.getTop() )) {

	/* ------ �����R�}���h ------*/
	copy_filename( buf , fname.getTop() , BACKSLASH_DEMILITOR );
	sp = copy_args( buf , sp );
	
      }else if( stricmp(suffix=_getext2(fname),".class") == 0 ){
	/*
	 * ------ Java Application (*.class �Ȏ�) -----
	 */
	buf << "java ";
	const char *javaopt=getShellEnv("JAVAOPT");
	if( javaopt != NULL  &&  javaopt[0] != '\0' )
	  buf << javaopt << ' ';
	
	buf.paste( fname , suffix-fname );
	buf << ' ';
	sp = copy_args( buf , sp );

      }else if(    option_script_cache
	      &&  (sc=script_hash[fname.getTop()]) != NULL ){

	/* ------ �X�N���v�g(�L���b�V���q�b�g) ------- */

	if( option_debug_echo ){
	  fputs( "Script cache hit\n",stderr);
	  fflush(stderr);
	}
	type = SearchEnv(fname,"SCRIPTPATH",path);
	if( auto_close )
	  buf << "/C /F ";
	
	buf << sc->interpreter << ' ';
	copy_filename( buf , path , SLASH_DEMILITOR );
	sp = copy_args( buf , sp );

      }else if(   (type=SearchEnv(fname,"SCRIPTPATH",path)) == COM_FILE
	       && (header=read_sos_header(path)) !=0 ){
	/*
	 * -------- SOS�X�N���v�g --------
	 */
	try{
	  StrBuffer argv;
	  sp = copy_args( argv , sp );
	  expand_sos( buf , header , path , argv );
	}catch(...){
	  free(header);
	  throw;
	}
	free(header);
      }else if(    type==FILE_EXISTS 
	       && (header=read_script_header(path)) != 0 ){
	/*
	 * ------- �u#!�v�X�N���v�g --------
	 */
	if( auto_close )
	  buf << "/C /F ";
	
	/* �u#!�v�s���̃C���^�v���^�����y�[�X�g */
	copy_filename( buf , header , BACKSLASH_DEMILITOR );
	buf << ' ';
	/* �X�N���v�g�����g���y�[�X�g */
	copy_filename( buf , path   , SLASH_DEMILITOR );
	
	buf << ' ';

	/* �I�v�V�����������Ă���΁A
	 * �L���b�V���ɁA�ǂݏo�������ʂ�ۑ����� */
	if( option_script_cache ){
	  char *name = strdup( fname );
	  if( name == 0 ){
	    free( header );
	    throw StrBuffer::MallocError();
	  }
	  script_to_cache( name , header );
	}else{
	  free( header );
	}
	sp = copy_args( buf , sp );

      }else{
	/* -------- OS/2 �̎��s�t�@�C�� ---------- */
	if(   auto_close 
	   && (getApplicationType(fname) == 2 || type == CMD_FILE)  ){
	  buf << "/C /F ";
	}
	copy_filename( buf , fname , BACKSLASH_DEMILITOR );
	sp = copy_args( buf , sp );
      }

    }else{
      /* ================= option -script �̏ꍇ =============== */
      StrBuffer fname;
      sp = copy_filename( fname , sp );
      if(   auto_close && getApplicationType(fname)== 2 )
	buf << "/C /F ";
      buf << fname;
      sp = copy_args( buf , sp );
    }

    /* ================= �I�������̏��� (\0, | , & �Ȃ�) =============*/

    if( *sp == '\0' )
      break;
    
    if( *sp == '|' ){
      if( *(sp+1) == '&' ){	/*  `|&' -> '2>&1 |' */
	buf << "2>&1 |";
	sp += 2;
      }else{
	buf << *sp++;
      }
    }else if( *sp == '&' ){
      buf << *sp++;
      if( *sp == '&' )
	buf << *sp++;
      else if( *sp == ';' )
	++sp;
    }
    if( *sp=='\0' )
      break;
  }/* �p�C�v�ŋ�؂�ꂽ�e�R�}���h���̃��[�v */
  return buf.finish();
}

int replace_script( const char *sp , char *dst , int max )
{
  char *kekka;
  try{
    kekka = replace_script(sp);
  }catch(StrBuffer::MallocError){
    strncpy( dst , sp , max );
    return 0;
  }
  if( kekka != NULL ){
    strncpy( dst , kekka , max );
    free(kekka);
  }else{
    strncpy( dst , sp , max );
  }
  return 0;
}
