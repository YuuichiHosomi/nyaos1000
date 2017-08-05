#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <ctype.h>
#include <process.h>
#include <sys/video.h>

#define USE_SET_WIN_TITLE 0

#define INCL_VIO
#define INCL_WIN
#define INCL_RXSUBCO
#define INCL_RXFUNC
#define INCL_DOSPROCESS

#include <os2.h>

#include "edlin.h"
#include "nyaos.h"
#include "complete.h"
#include "smartptr.h"
#include "errmsg.h"
#include "prompt.h"
#include "heapptr.h"
#include "shared.h"
#include "remote.h"

HAB   hab, hmq;

int prompt_myself=1;
int screen_width=80;
int screen_height=25;
int option_vio_cursor_control=1;
int option_prompt_even_piped=1;
int cursor_start;
int cursor_end;
char *cursor_on_color_str=NULL;
char *cursor_off_color_str=NULL;
int option_nyaos_rc=1;
int option_cmdlike_crlf=0;
int option_icanna=0;

char comspec[128]="COMSPEC=";
char *cmdexe_path=comspec+8;

int execute_result=0;
extern int printexitvalue;
int option_noclobber=0;
char *check_redirect_to_exist_file(const char *sp);
extern int killAllPublicHistory(void);

int do_rexx( const char *progname , LONG argc , RXSTRING *rx_argv );

/* �uVER�v�R�}���h�F 
 * nyaos.cc �����ăR���p�C������΁ANYAOS �ŕ\�������S�Ă�
 * �o�[�W�����i���o���X�V�����悤�A�����Ă����ɒu���Ă���B
 */
int cmd_ver( FILE *source , Parse &argv )
{
  fputs("Nihongo Yet Another Os/2 Shell is "VERSION
#ifdef S2NYAOS
	"\n (static linked version)"
#endif
	"\ncompiled on "__DATE__
	, stdout );
  fflush(stdout);
  return RC_HOOK;
}


/* VIO�v���O��������A�����I�� PM �A�v���P�[�V�����ɉ�����
 * ����ɂ���āAPM �̃N���b�v�{�[�h�̓ǂݏ������\�Ƃ���B
 */
static int pretend_pm_application()
{
  PTIB  ptib = NULL;
  PPIB  ppib = NULL;
  APIRET rc = DosGetInfoBlocks(&ptib, &ppib);
  if (rc != 0)
    return rc;
  ppib->pib_ultype = PROG_PM;
  hab = WinInitialize(0);
  hmq = WinCreateMsgQueue(hab, 0);
  if (hmq == NULLHANDLE)
    return rc;
  return 0;
}

/* ���ϐ����A��ʃT�C�Y���擾����B
 * �擾�ł��Ȃ������ꍇ�́A80x25 �ƂȂ�B
 *	wh[0] ����
 *	wh[1] �s��
 */
static void get_scrsize_with_env(int *wh)
{
  const char *env;

  env=getShellEnv("COLUMNS");
  if( env==NULL || (wh[0]=atoi(env)) <= 1 ) 
    wh[0] = 80;

  env=getShellEnv("LINES");
  if( env==NULL || (wh[1]=atoi(env)) <= 1 )
    wh[1] = 25;
}

/* �X�g���[�� f �̉�ʃT�C�Y���擾����B
 * f ���t�@�C���̏ꍇ�́A���ϐ����擾�A
 * �[���̏ꍇ�́A_scrsize�֐���p���Ď擾����B
 *
 *	wh[0] ����
 *	wh[1] �s��
 *	f �X�g���[��
 */
void get_scrsize(int *wh,FILE *f)
{
  if( f==0 )
    f=stdout;
  
  if( !isatty(fileno(f)) ){
    /* �t�@�C���o�͂Ȃ�΁A���ϐ������𗊂�ɂ��� */
    get_scrsize_with_env(wh);
  }else{
    /* �[���Ȃ�΁A_scrsize�֐����g���Ă݂�B*/
    _scrsize(wh);
    if( wh[0] <= 1  ||  wh[1] <= 1 )
      get_scrsize_with_env(wh);
  }
}

/* fgets �Ɗ�{�͓����B������
 *	�E�����́u\n�v��ǂݍ��܂Ȃ��B
 *	�E�u^\n�v�͖�������(�s�p���������Ƃ݂Ȃ�)
 */
char *fgets_chop(char *dp, int max, FILE *fp)
{
  bool kanji=false;    /* �O��char���A�����̑�1�o�C�g�������B*/
  int ch=0;

  for(;;){
    if( max-- <= 1  ||  (ch=getc(fp)) == EOF ){
      *dp = '\0';
      return NULL;
    }
    if( ch == '\n' )
      break;

    if( ch == '^'  &&  !kanji  ){
      if( (ch=getc(fp) == '\n' ) ){
	ch = getc(fp); /* ���̂܂܁u^\n�v�𖳎�����B
			* �u^\n^\n�v�ƘA������ƑΉ��ł��Ȃ����A
			* ����Ȃ��Ƃ���l�Ԃ͂��Ȃ����낤�c���� */
      }else{
	ungetc( ch , fp );
	ch = '^';
      }
    }
    if( ch==EOF ){
      *dp = '\0';
      return NULL;
    }
    if( kanji ){
      kanji = false;
    }else if( is_kanji(ch) ){
      kanji = true;
    }
    *dp++ = ch;
  }
  *dp = '\0';
  return dp;
}

#if USE_SET_WIN_TITLE
/* �t���O : FCF_TASKLIST �� VIO �E�C���h�E�ŗ����Ă���ꍇ�A
 * �uNYAOS.EXE�v�̑���� set_win_title �̈������E�C���h�E�^�C�g���ɂȂ�B
 * �����ɂ��ustart nyaos.exe�v�ŋN�����������A�A�C�R���Ƀ^�C�g��������
 * �������AFCF_TASKLIST �͗����Ȃ��B
 */
extern "C" void _THUNK_C_FUNCTION(WinSetTitle)(PSZ szTITLE);

void set_win_title( const char *title )
{
  _THUNK_C_PROLOG ( 4 );
  _THUNK_C_FLAT ( const_cast<char*>(title) );
  _THUNK_C_CALL ( WinSetTitle );
}

#endif

static void nyaosAtExit()
{
  const char *envAtExit=getShellEnv("ATEXIT");
  if( envAtExit != NULL ){
    execute(stdin,envAtExit);
  }else{
    fputs("\nGood bye!\n",stdout);
  }
}

bool option_quite_mode=0;	/* ���S��\�����Ȃ� */

int main(int argc, char **argv)
{
  if( _osmode != OS2_MODE ){
    ErrMsg::say( ErrMsg::NotSupportVDM , "nyaos",0 );
    return -1;
  }
#if USE_SET_WIN_TITLE
  set_win_title( "Nihongo Yet Another Os/2 Shell "VERSION );
#endif
  
  // ---- DBCS table �̏����� ----

  if( dbcs_table_init() != 0 ){
    ErrMsg::say( ErrMsg::DBCStableCantGet , "nyaos" , 0 );
    return -1;
  }

  resetCWD();				/* �V�F���ϐ� CWD �ɃJ�����g
					 * �f�B���N�g����ݒ肷�� */
  setvbuf(stdout,NULL,_IOLBF,BUFSIZ);	/* ��ʕ\���͉��s����܂�
					 * �o�b�t�@�����O����B*/
  Shell::bindkey_nyaos();		/* �L�[�o�C���h��ݒ肷�� */

  // ----------------------------------------
  // COMSPEC �ɁANYAOS���g���ݒ肳��Ă���ƁA
  // ���삪���������Ȃ�̂ŁA
  // CMD.EXE �ɐ؂芷��������B 
  // ----------------------------------------
  if( SearchEnv("CMD.EXE","PATH",cmdexe_path) == 0 ){
    ErrMsg::say( ErrMsg::WhereIsCmdExe , "nyaos" , 0 );
    return -1;
  }
  putenv(comspec);

  // -------- �I�v�V�������� ----------

  int warning_mode=0;	/* �x������F!0 �ŉ�ʂ��N���A���Ȃ� */

  for(int i=1;i<argc;i++){
    if( argv[i][0] == '-' || argv[i][0] == '/' ){
      switch(argv[i][1]){
      default:
	ErrMsg::say(ErrMsg::UnknownOption , argv[i] , 0 );
	warning_mode = 1;
	break;

      case 'z':
      case 'Z': /* Vz ���C�N�L�[���[�h */
	Shell::bindkey_wordstar();
	break;

      case 'g': /* �E�C���h�E�T�C�Y�w�� */
      case 'G':
	{
	  const char *p;

	  if( argv[i][2] != '\0' ){
	    p = &argv[i][2];
	  }else if( i+1 < argc ){
	    p = argv[++i];
	  }else{
	    ErrMsg::say(ErrMsg::TooFewArguments,"nyaos -g",0);
	    warning_mode = 1;
	    break;
	  }

	  int x=0,y=0;

	  while( *p != '\0' && isdigit(*p & 255) )
	    x = x*10 + (*p++ -'0');

	  if( (*p != 'x' && *p != 'X' ) || x<80 || x>200 ){
	    ErrMsg::say(ErrMsg::BadParameter,"nyaos -g",argv[i],0);
	    return 1;
	  }
	  ++p;
	  while( *p !='\0' && isdigit(*p & 255) )
	    y = y*10 + (*p++ -'0');

	  if( y<20 || y>100 ){
	    ErrMsg::say(ErrMsg::BadParameter,"nyaos -g",argv[i],0);
	    return 2;
	  }
	  
	  char buffer[40];
	  sprintf(buffer,"co%d,%d", screen_width=x , screen_height=y );
	  spawnlp(P_WAIT,"CMD.EXE","CMD.EXE","/C","MODE",buffer,NULL);

	  static char env_columns[20];
	  sprintf(env_columns,"COLUMNS=%d",x);
	  putenv(env_columns);

	  static char env_lines[20];
	  sprintf(env_lines,"LINES=%d",y);
	  putenv(env_lines);
	}
	break;

      case 'h':
      case 'H':
	if( i+1 < argc ){
	  char *home;
	  int len=strlen(argv[++i]);
	  if( (home = new char[len+7]) == NULL ){
	    perror( argv[0] );
	    return -1;
	  }
	  sprintf( home , "HOME=%s" , argv[i] );
	  putenv( home );
	}
	break;
	
      case 'C':
      case 'c':
      case 'K':
      case 'k':
      case 'e':
	if( i+1 < argc ){
	  char oneline[1024];
	  SmartPtr dp(oneline,sizeof(oneline));
	  try{
	    for(int j=i+1;;){
	      int quote=0;
	      
	      for(const char *p=argv[j] ; *p != '\0' ; p++ ){
		if( isspace(*p & 255) || *p == '^' || *p == '!' )
		  quote = 1;
	      }
	      if( quote ) *dp++ = '"';
	      
	      for( const char *sp=argv[j] ; *sp != '\0' ; sp++ )
		*dp++ = *sp;
	      
	      if( quote ) *dp++ = '"';
	      
	      if( ++j >= argc )  break;
	      *dp++ = ' ';
	    }
	    *dp = '\0';
	  }catch( SmartPtr::BorderOut ){
	    dp.terminate();
	  }

	  int rc=execute(stdin,oneline);
	  if( argv[i][1] == 'c' || argv[i][1] == 'C' || argv[i][1] == 'e' )
	    return rc;
	}
	goto end_argv;
	
      case 'f':
	option_nyaos_rc = 0;
	break;

      case 'q':
	option_quite_mode = 1;
	break;

      case '-':
	{
	  extern int set_option(const char *name,int flag);
	  
	  const char *sp=&argv[i][2];
	  char buffer[40],*dp=buffer;
	  int flag=1;
	  while( *sp != '\0' &&  dp < buffer+sizeof(buffer)-1 ){
	    if( *sp == '-' ){
	      flag = 0;
	      break;
	    }else if( *sp == '+' ){
	      break;
	    }
	    *dp++ = *sp++;
	  }
	  *dp = '\0';
	  
	  if( set_option(buffer,flag) != 0 ){
	    ErrMsg::say(ErrMsg::UnknownOption,"nyaos" , buffer , 0 );
	    warning_mode = 1;
	  }
	}
	break;
      }
    }else{
      if( changeDir(argv[i]) != 0 ){
	ErrMsg::say(ErrMsg::BadParameter,argv[0],argv[i],0);
	return -1;
      }
    }
  }
  pretend_pm_application();

  if( isatty(fileno(stdin)) && !option_quite_mode ){
    extern int get_current_cp(void);
    
    if( ! warning_mode )
      fputs("\x1b[2J\x1b[1m",stdout);

    int cp=get_current_cp();
    if( cp==932 || cp==942 || cp==943 ){
      // Japanese Message.
      fputs("\n  ��������  ��������������������" 
	    "\n  ��������������������  ��������"
	    "\n  ���������������@��������������"
	    ,stdout );
    }else{
      // English Message.
      fputs("\n   //  // //  //  ////   ////   //// "
	    "\n  /// // ////// //  // //  // ///    "
	    "\n // ///    /// ////// //  //    ///  "
	    "\n//  //  ////  //  //  ////  /////    "
	    ,stdout);
    }
    
    fputs("\n     The Open Source Software     "
	  "\n- Nihongo Yet Another Os/2 Shell -"
	  "\n    1996-2001 (c) HAYAMA,Kaoru  "
#ifdef S2NYAOS
	  "\n   Static linked version "VERSION
	  "\n     compiled on "__DATE__
#else
	  "\n Ver."VERSION" compiled on "__DATE__
#endif
	  "\n\n\x1b[0m"
	  , stdout );
  }

 end_argv:
  if( option_nyaos_rc ){
    char buffer[FILENAME_MAX];
    char *path=NULL;
    const char *home;

    if( access(".nyaos",0)==0 ){
      path = ".nyaos";
    }else if( access("_nyaos",0)==0 ){
      path = "_nyaos";
    }else if( access("nyaos.rc",0)==0 ){
      path = "nyaos.rc";
    }else if( (home=getShellEnv("HOME"))!=NULL ){
      char *dp = path = buffer;
      int lastchar=0;
      
      while( *home != '\0' ){
	if( is_kanji(lastchar=*home) )
	  *dp++ = *home++;
	*dp++ = *home++;
      }
      if( lastchar != '\\' && lastchar != '/' && lastchar != ':' )
	*dp++ = '\\';
      
      strcpy(dp,".nyaos");
      if( access(buffer,0) != 0 ){
	strcpy(dp,"_nyaos");
	if( access( buffer,0) != 0 ){
	  strcpy(dp,"nyaos.rc");
	  if( access(buffer,0) != 0 )
	    path = NULL;
	}
      }
    }

    /* ----------------------------------------------------------------
     * �u.nyaos�v�̏���
     * ----------------------------------------------------------------
     */
    FILE *fp;
    if(  path != NULL  && (fp=fopen(path,"r")) != NULL ){
      char buffer[256];
      const char *rc=fgets_chop(buffer,sizeof(buffer),fp);

      if( rc != NULL && buffer[0]=='/' && buffer[1]=='*' ){
	// REXX ���[�h
	fclose(fp);
	
	RXSTRING rx_argv[1];
	MAKERXSTRING(rx_argv[0],"-",1);
	do_rexx( path , 1 , rx_argv );
      }else{
	// �R�}���h���[�h
	while( rc != NULL ){
	  if( execute(fp,buffer) == RC_QUIT )
	    break;
	  rc = fgets_chop(buffer,sizeof(buffer),fp);
	}
	fclose(fp);
      }
    }
  }

  if( option_vio_cursor_control )
    v_init();

  /* ----------------------------------------------------------------
   * �W�����͂��A���_�C���N�g����Ă���ꍇ�̏���(�����Ŋ���)
   * ----------------------------------------------------------------
   */
  if( ! isatty(fileno(stdin)) ){
    for(;;){
      if( option_prompt_even_piped ){
	/* Mule ������ANYAOS �𗘗p����ꍇ�́A
	 * �p�C�v����Ă���ꍇ�ł��A�v�����v�g��\�������Ȃ��Ă�
	 * �����Ȃ� */

	const char *promptenv=getShellEnv("NYAOSPROMPT");
	if( promptenv==NULL && (promptenv=getShellEnv("PROMPT")) == NULL )
	  promptenv = "$p$g";

	Prompt prompt( promptenv );
	fputs( prompt.get2() , stdout );
	fflush(stdout);
      }
      char cmdlin[1024]="";
      if( fgets_chop(cmdlin,sizeof(cmdlin),stdin) == NULL 
	 || execute(stdin,cmdlin) == RC_QUIT )
	return 0;
    }
  }

  killAllPublicHistory();

  if( option_icanna == 0 ){
    extern int canna_init();
    canna_init();
  }

  Shell shell;
  if( !shell ){
    ErrMsg::say(ErrMsg::MemoryAllocationError,"nyaos",0);
    return -1;
  }

  // ======================== �R�}���h���̃��[�v =========================
  for(;;){
    /* �v�����v�g������̍쐬 */
    
    const char *promptenv=getShellEnv("NYAOSPROMPT");
    if( promptenv==NULL && (promptenv=getShellEnv("PROMPT")) == NULL )
      promptenv = "$p$g";
    
    /* �v�����v�g������ɁA�ŏ�i���g�p������̂�����΁A
     * �V�F��(Shell)�ɁA���̎g�p���֎~������B*/

    Prompt prompt( promptenv );
    if( prompt.isTopUsed() )
      shell.forbid_use_topline();
    else
      shell.allow_use_topline();

    /* �J�[�\���� BOX �^�ɂ���B
     */
    if( option_vio_cursor_control ){
      VIOCURSORINFO info;

      if( shell.isOverWrite() ){ /* �㏑�����[�h�̎��͔����T�C�Y */
	info.yStart = (unsigned short)-50;
      }else{			 /* �}�����[�h�̎��̓t���T�C�Y */
	info.yStart = 0;
      }
      info.cEnd   = (unsigned short)-100;
      info.cx = 0;
      info.attr = 0;
      
      VioSetCurType( &info , 0 );
    }
    
    /* ��s���� */
    const char *top;

    // pretend_pm_application();
    int rc = shell.line_input(prompt.get2() ,">",&top);
    
    // ============== �R�}���h�̎��s ====================

    // ---------------------------------------------------------
    // �R�}���h�����s���A�u�I���v�̋A��l��������A�I������B
    // ���s�́A���@�\system �ł��� execute ���悵�Ȃɂ��Ă����B
    // ---------------------------------------------------------
    
    if( rc > 0 ){
      putchar('\n');
      while( *top != '\0' && is_space(*top) )
	++top;
      if( top[0] != '\0' ){
	execute_result = execute(stdin,top);
	
	if( execute_result == RC_QUIT ){
	  // --- exit�R�}���h�Ȃǂɂ��I�� ----
	  nyaosAtExit();
	  return 0;
	}
	if( option_cmdlike_crlf )
	  putchar('\n');
	
	if( execute_result != 0 && printexitvalue !=0 ){
	  printf("Exit %i\n",execute_result);
	}
      }else{
	execute_result = 0;
      }
    }else{
      switch( rc ){
      case Edlin::QUIT:
	// ---- CTRL-Z �Ȃǂɂ��I�� ----
	nyaosAtExit();
	return 0;

      case RC_ABORT:
      case Edlin::ABORT:
	fputs("^C\n",stdout);
	break;

      case 0:
	putchar('\n');
	break;
	
      default:
	fputs("\nUnknown error occuerd.\n"
	      "Please mail to iyamatta.hayama@nifty.ne.jp\n"
	      , stdout );
	break;
      }
    }
  }// ============ �R�}���h���̃��[�v�̖��� ===========
}
