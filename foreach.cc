#include <io.h>
#include <ctype.h>

#include "finds.h"
#include "edlin.h"
#include "nyaos.h"
#include "parse.h"
#include "strbuffer.h"
#include "errmsg.h"
#include "prompt.h"

extern volatile int ctrl_c;

struct Line{
  Line *next;
  char buffer[1];
};

enum{
  OPTION_I = 1,
  OPTION_N = 2,
  OPTION_V = 4,
};
static unsigned option=0;

/* value �� opt �̓��e�ɏ]���ĉ��H���A���ʂ� line �ɗ������ށB
 *	value ���o�����x�[�X
 *	opt ���e�͎��̂��̂��󂯂���B
 *		NULL�� �S��
 *		h �f�B���N�g������
 *		t �f�B���N�g�������ȊO
 *		r �g���q�����ȊO
 *		e �g���q����
 * return
 *	 0 : ����
 *	-1 : opt �̓��e���s�K�ł���B
 */
int word_design(StrBuffer &line ,const char *value,int option)
     throw(StrBuffer::MallocError)
{
  switch( option ){
  case 'e':
  case 'E': /* �u:e�v�g���q�̂� */
    line << _getext2(value);
    break;

  case 'r':
  case 'R': /* �u:r�v�g���q���� */
    {
      const char *ext=_getext(value);
      if( ext != 0 ){
	line.paste( value , ext-value );
      }else{
	line << value;
      }
    }
    break;

  case 'h':
  case 'H': /* �u:h�v�f�B���N�g���̂� */
    {
      const char *name=_getname(value);
      if( name != 0 )
	line.paste( value , name-value );
    }
    break;

  case 't':
  case 'T':/* �u:t�v�f�B���N�g������ */
    line << _getname(value);
    break;

  default:
    return -1;
  }
  return 0;
}

static int word_design(StrBuffer &line ,const char *value,const char *opt)
     throw(StrBuffer::MallocError)
{
  if( opt==NULL  ||  opt[0] == '\0' ||  !isalpha(opt[0])  || opt[1] !='\0' )
    return -1;

  return word_design(line,value,opt[0]);
}

static int eachcmd(FILE *srcfil, const char *var, const char *str, Line *src )
     throw(StrBuffer::MallocError)
{
  int rv=0;
  
  /* �V�F���ϐ��ɁAargv[1] �̓��e��ݒ肷�� */
  const char *orgEnvValue=getShellEnv(var);
  char *orgEnvValueDup=(orgEnvValue ? strdup(orgEnvValue) : 0 );
  setShellEnv(var,str);

  /* �e���ߖ��Ƀ��[�v */
  for( ; src != NULL ; src=src->next ){
    StrBuffer line;
    const char *sp=src->buffer;
	
    while( *sp != '\0' ){
      if( *sp != '$' ){	/* �ʏ�̕��� */
	line << *sp++;
      }else if( *(sp+1) == '$' ){ /* �u$$�v�́u$�v�֕ϊ� */
	line << '$';
	sp += 2;
      }else{
	/* $... �́A���ϐ��A���邢�́A�p�����[�^�ւ̒u�� */

	StrBuffer word;	/* $(VAR:OPT) �� VAR ��ۑ����� */
	StrBuffer opt;	/* $(VAR:OPT) �� OPT ��ۑ����� */

	if( *++sp == '{' ){		/**** ${...} �̃P�[�X ****/

	  ++sp; /*  '{'��ǂݔ�΂� */
	  while( *sp != '}' ){
	    if( *sp == '\0' ){
	      ErrMsg::say( ErrMsg::Missing , "foreach" , "}" , 0 );
	      rv = -1;
	      goto exit;
	    }else if( *sp == ':' ){ /* ${VAR:OPT} �̏ꍇ */
	      ++sp;
	      while( *sp != '}' ){
		if( *sp == '\0' ){
		  ErrMsg::say( ErrMsg::Missing , "foreach" , "}" , 0 );
		  rv = -1;
		  goto exit;
		}
		opt << *sp++;
	      }
	      break;
	    }
	    word << *sp++;
	  }
	  ++sp; /* '}'��ǂݔ�΂� */

	}else if( *sp == '(' ){		/**** $(...) �̃P�[�X ****/
	  
	  ++sp; /*  '('��ǂݔ�΂� */
	  while( *sp != ')' ){
	    if( *sp == '\0' ){
	      ErrMsg::say(ErrMsg::Missing,"foreach",")",0);
	      rv = -1;
	      goto exit;
	    }else if( *sp == ':' ){ /* $(VAR:OPT) �̏ꍇ */
	      ++sp;
	      while( *sp != ')' ){
		if( *sp == '\0' ){
		  ErrMsg::say(ErrMsg::Missing,"foreach",")",0);
		  rv = -1;
		  goto exit;
		}
		opt << *sp++;
	      }
	      break;
	    }
	    word << *sp++;
	  }
	  ++sp; /* ')'��ǂݔ�΂� */

	}else{				/**** $AAAA �̃P�[�X *****/
	  if( *sp != '\0'  &&  (is_alpha(*sp) || *sp=='_' ) ){
	    do{
	      word << *sp++;
	      if( *sp == ':' ){ /**** $VAR:OPT �̃P�[�X ****/
		++sp;
		while( *sp != '\0' && is_alpha(*sp) )
		  opt << *sp++;
		break;
	      }
	    }while( *sp != '\0' && ( is_alnum(*sp) || *sp=='_' ) );
	  }
	}
	
	const char *env;
	const char *value=0;
	if( strcmp(word,var)==0 ){
	  /* foreach �̕ϐ��̏ꍇ */
	  value = str;
	}else if( (env=getShellEnv(word)) != NULL ){
	  /* ���ϐ��̏ꍇ */
	  value = env;
	}else{
	  /* �����Ȃ���΁A�G���[������ */
	  ErrMsg::say( ErrMsg::NoSuchEnvVar , "foreach" , word.getTop() , 0 );
	  rv = -1;
	  goto exit;
	}

	if( opt.getLength() <= 0 ){
	  /* �u�F�I�v�V�����������ꍇ�́A���̂܂ܓW�J���� */
	  line << value;
	}else{
	  if( word_design( line , value , opt ) != 0 ){
	    ErrMsg::say( ErrMsg::UnknownOption , "foreach",opt.getTop() ,0);
	    rv = -1;
	    goto exit;
	  }
	}
      }
    }
    
    if( ctrl_c ){
      puts( "\nCtrl-C Hit." );
      ctrl_c = 0;
      rv = -1;
      goto exit;
    }

    /* �u�����č쐬�����A�e�R�}���h�����s����B
     * �I�v�V���� -n ���w�肳��Ă��鎞�́A
     * �\���̂ݍs���A���ۂɂ͎��s���Ȃ��B
     */
    if( option & OPTION_N ){
      puts(line);
    }else{
      /* execute �̋A��l�́Aprompt �ŕ\������ׂɃO���[�o���ϐ��ɕۑ�����B
       * ���̎d�l�A�Ȃ�Ƃ��������������ˁI
       */
      extern int execute_result; /* �� nyaos.cc */
      if( option & OPTION_V ){
	fputs(line,stderr);
	if( isatty(fileno(srcfil)) )
	  fputc('\n',stderr);
      }
      execute_result = execute(srcfil,line,1);
      
      if( execute_result != 0  &&  (option & OPTION_I)==0 ){
	ErrMsg::say( ErrMsg::ErrorInForeach , 0 );
	rv = -1;
	goto exit;
      }
    }
  }/* ���߃��[�v */

 exit:
  setShellEnv( var , orgEnvValueDup );
  free( orgEnvValueDup );

  return rv;
}

static int foreach(FILE *srcfil,const char *parameter, int argc, char **argv)
{
  if( srcfil == NULL ){
    ErrMsg::say( ErrMsg::NotAvailableInRexx , "foreach" , 0 );
    return 0;
  }
  
  if( argc < 3 ){
    fputs("foreach [-ivn] var param1 param2 ... paramN\n",stderr);
    return 0;
  }

  option = 0;

  while( argv[1][0] == '-' ){
    const char *p=argv[1]+1;
    while( *p != '\0' ){
      switch(*p){
      case 'i':
      case 'I':
	option |= OPTION_I;
	break;
      case 'n':
      case 'N':
	option |= OPTION_N;
	break;
      case 'v':
      case 'V':
	option |= OPTION_V;
	break;
      }
      p++;
    }
    argv++;
    argc--;
  }

  struct Line dummyfirst , *cur=&dummyfirst;

  dummyfirst.next = NULL;

  int org_fd1 = -1;
  Parse *args=NULL;


  /** �J��Ԃ����ߌQ��S�ē��͂�����B **/
  if( isatty(fileno(srcfil)) ){
    /* �L�[�{�[�h���� */
    Prompt prompt;
    Shell shell;
    const char *promptenv=getShellEnv("NYAOSPROMPT2");
    int rc;
    
    for(;;){
      Prompt prompt( promptenv ? promptenv : "? " );

      if( prompt.isTopUsed() )
	shell.forbid_use_topline();
      else
	shell.allow_use_topline();

      shell.setcursor( cursor_on_color_str , cursor_off_color_str );
      
      const char *buffer;
      rc=shell.line_input(prompt.get2(),"and..",&buffer);
      putchar('\n');
      if( rc < 0 )
	break;
      else if( rc == 0 )
	continue;
      
      if(   (buffer[0] == 'e' || buffer[0] == 'E' )
	 && (buffer[1] == 'n' || buffer[1] == 'N' )
	 && (buffer[2] == 'd' || buffer[2] == 'D' )
	 && (   buffer[3] =='\0' || buffer[3] == '>'
	     || buffer[3] == '|' || isspace(buffer[3] & 255) ) ){
	args=new Parse(buffer);
	FILE *fout=args->open_stdout();
	if( fout != NULL && fout != stdout ){
	  org_fd1 = dup(1);
	  dup2( fileno(fout) , 1 );
	}
	break;
      }
      if( buffer[0] != '\0' ){
	cur = cur->next = 
	  (struct Line*)alloca(sizeof(struct Line)+strlen(buffer));
	strcpy( cur->buffer  , buffer );
      }
    }/* end:for */
    if( rc==Edlin::ABORT || rc==RC_ABORT ){
      puts("^C");
      return 1;
    }
    if( rc==Edlin::FATAL ){
      ErrMsg::say( ErrMsg::InternalError , "foreach" , 0 );
      return 1;
    }
  }else{
    /* �t�@�C������ */
    for(;;){
      char buffer[1024];
      if( fgets_chop(buffer,sizeof(buffer),srcfil) == NULL )
	break;
      
      char *sp=buffer;

      while( *sp != '\0' && is_space(*sp) )
	sp++;
      
      if (   (sp[0] == 'e' || sp[0] == 'E' )
	  && (sp[1] == 'n' || sp[1] == 'N' )
	  && (sp[2] == 'd' || sp[2] == 'D' )
	  && (sp[3] =='\0' || is_space(sp[3]) 
	      || sp[3] == '>' || sp[3]=='|' ) ){
	args = new Parse(sp);
	FILE *fout=args->open_stdout();
	if( fout != NULL && fout != stdout ){
	  org_fd1 = dup(1);
	  close(1);
	  dup2( fileno(fout) , 1 );
	}
	break;
      }

      if( *sp != '\0' ){
	cur = cur->next =
	  (struct Line*)alloca(sizeof(struct Line)+strlen(buffer));
	strcpy( cur->buffer  , buffer );
      }/* ��s�͖������� */
    }
  }
  cur->next   = NULL;

  /* �e�������Ƀ��[�v */
  int rv=0;
  for(int i=2;i<argc;i++){
    /* �W�J�����t�@�C�������Ƃ̃��[�v */
    char **list = 0;
    list = (char**)fnexplode2(argv[i]);
    try{
      if( list==NULL ){
	if( eachcmd(srcfil,argv[1],argv[i],dummyfirst.next) != 0 )
	  goto exit;
      }else{
	numeric_sort(list);
	for(char **listptr=list ; *listptr != NULL ; listptr++ ){
	  int rv=eachcmd(srcfil,argv[1],*listptr,dummyfirst.next);
	  if( rv != 0 ){
	    fnexplode2_free(list);
	    goto exit;
	  }
	}
	fnexplode2_free(list);
      }/* �W�J��̖��O���[�v */
    }catch(StrBuffer::MallocError){
      if( list != NULL )
	fnexplode2_free(list);
      ErrMsg::say( ErrMsg::MemoryAllocationError , "foreach" , 0 );
      break;
    }
  }/* �p�����[�^���[�v */

 exit:
  if( org_fd1 != -1 ){
    close( 1 );
    dup2( org_fd1 , 1 );
    close( org_fd1 );
    delete args;
  }
  return rv;
}

static int compatible(FILE *source , Parse &params ,
		      int (*routine)(  FILE * , const char*,int,char**) )
{
  int argc=params.get_argc();
  char **argv=(char**)alloca( (argc+1)*sizeof(char*) );

  for(int i=0 ; i<argc ; i++){
    argv[i] = (char*)alloca( params[i].len + 1 );
    memcpy( argv[i] , params[i].ptr , params[i].len );
    argv[i][ params[i].len ] = '\0';
  }
  return (*routine)( source , params.get_parameter() , argc , argv );
}

int cmd_foreach(FILE *source, Parse &params )
{
  return compatible(source,params,foreach); 
}
