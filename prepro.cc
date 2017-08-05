#include <ctype.h>
#include <io.h>
#include <stdlib.h>
// #include <stdio.h>

#include "parse.h"
#include "Edlin.h"
#include "macros.h"
#include "autofileptr.h"
#include "quoteflag.h"
#include "errmsg.h"

const char *getShellEnv(const char *);

int option_tilda_is_home=1;
int option_tilda_without_root=0;
int option_replace_slash_to_backslash_after_tilda=1;
int option_tcshlike_history=0;
int option_dots=1;
int option_history_in_doublequote=0;
int option_same_history=1;

extern int are_spaces(const char *s); /* �� edlin2.cc */

static struct PublicHistory {
  char *string;
  PublicHistory *prev,*next;

  PublicHistory() : string(0) , prev(0) , next(0) { }
} Oth , *public_history=&Oth;

int nhistories = 0;

/* 00h ���� 1Fh �܂ł̐��䕶���� ^H �Ƃ����`�ŕ\������fputs�B
 * �o�͐悪�[���łȂ��A�t�@�C�����̎��́A�ϊ����s��Ȃ��B
 *	p    ������
 *	fout �o�͐�t�@�C���|�C���^
 */
static void fputs_ctrl(const char *p , FILE *fout)
{
  for( ; *p != '\0' ; p++ ){
    if( 0 < *p && *p < ' ' && isatty(fileno(fout)) ){
      putc( '^' , fout );
      putc( '@'+*p , fout );
    }else{
      putc( *p , fout );
    }
  }
}

/* �usource -h �t�@�C�����v���s���B
 *	fname �ǂݍ��ރt�@�C����
 *
 * return 0:���� , -1:���s(�t�@�C�������݂��Ȃ�)
 *	���������m�ۂł��Ȃ��ꍇ�Ȃǂ̓G���[�ɂȂ炸�A
 *	�ǂݍ��܂�Ȃ������B
 */
int source_history( const char *fname )
{
  AutoFilePtr fp(fname,"rt");
  if( fp==NULL )
    return -1;

  char buffer[1024];
  while( fgets(buffer,sizeof(buffer),fp) != NULL ){
    /* �擪�̐����Ɓu:�v������ */
    char *p = strchr(buffer,':');
    if( p == NULL || *++p == '\0' || *++p == '\0' )
      continue;

    /* ������ \n ������ */
    char *q= strchr(p,'\n');
    if( q != NULL )
      *q = '\0';

    /* ---- public history ----- */
    PublicHistory *tmp=new PublicHistory;
    if( tmp != NULL ){
      tmp->string = strdup(p);
      if( tmp->string == NULL ){
	delete tmp;
      }else{
	tmp->prev = public_history;
	tmp->next = NULL;
	if( public_history != NULL )
	  public_history->next = tmp;
	public_history = tmp;
	++nhistories;
      }
    }
    
    /* ---- shell history ---- */
    Shell::append_history(p);
  }
  return 0;
}

/* �q�X�g������������B
 *	str ... ����������B�������A�擪 len ���������L��
 *	len ... ����������̑ΏۂƂȂ镶����
 * return �}�b�`�����q�X�g���s�B�}�b�`���Ȃ����� NULL�B
 */
static const char *seek_hist_top(const char *str,int len)
{
  PublicHistory *cur=public_history;
  while( cur != NULL && cur != &Oth ){
    if( cur->string[0] == str[0]  &&  memcmp(cur->string,str,len)==0 )
      return cur->string;
    cur = cur->prev;
  }
  return NULL;
}

/* �q�X�g������������B
 *	str ... ����������
 * return �}�b�`�����q�X�g���s
 */
static const char *seek_hist_mid(const char *str)
{
  PublicHistory *cur=public_history;
  while( cur != NULL && cur != &Oth ){
    if( strstr( cur->string , str ) != NULL )
      return cur->string;
    cur = cur->prev;
  }
  return NULL;
}

/* �q�X�g���̓��e���A�Â����̔ԍ��œ���B
 *	n ... �ԍ�
 *
 * return �q�X�g�����e�̕�����B�������� NULL�B
 */
static const char *get_hist_f(int n)
{
  PublicHistory *cur=Oth.next;
  if( cur==NULL )
    return NULL;
  
  /* ���[�U�[�w��̃q�X�g���ԍ��́u1�v����n�܂�̂ŁAi=1 */
  for(int i=1; i<n ; i++ ){
    if( cur == NULL )
      return NULL;
    // fprintf(stderr,"[%d]%s ",i,cur->string ?: "(null)" );
    cur = cur->next;
  }
  return (cur != NULL  && cur != &Oth ) ? cur->string : 0 ;
}

/* �ߋ������փq�X�g������������ 
 *	n ... �k��q�X�g���̐�
 */
static const char *get_hist_r(int n)
{
  PublicHistory *cur=public_history;
  if( cur==NULL || cur==&Oth )
    return NULL;
  for(int i=0; i<n ; i++ ){
    if( cur == NULL || cur==&Oth )
      return NULL;
    cur = cur->prev;
  }
  return cur ? cur->string : 0 ;
}

/* �p�X���I�v�V�����̒l�ɂ���āA�؂荏�ރ��[�`��
 * option
 *	'h' : �f�B���N�g������
 *	't' : ��f�B���N�g������
 *	'r' : ��g���q����
 *	'e' : �g���q����
 * return
 *	��NULL : ���o���������̐擪�ʒu
 *	NULL   : option �� h,t,r,e �ȊO
 */
char *cut_with_designer(char *path,int option)
{
  switch( option ){
  case 'H':
  case 'h': /* �f�B���N�g������ */
    *( _getname( path ) ) = '\0';
    return path;

  case 'T':
  case 't': /* �f�B���N�g�������ȊO */
    return _getname( path );

  case 'R':
  case 'r': /* �g���q�����ȊO */
    *( _getext2( path ) ) = '\0';
    return path;
    
  case 'E':
  case 'e': /* �g���q���� */
    return _getext2( path );
    
  default:
    return NULL;
  }
}


/* ���ϐ��̓��e���R�s�[����
 *	env ... ���ϐ���(NULL��)
 *	dp ... �R�s�[��(�X�}�[�g�|�C���^)
 * return �R�s�[��̖���
 */
static void insert_env(const char *env,StrBuffer &buf)
{
  const char *sp;
  const char *opt=strchr(env,':');
  if( opt != NULL  &&  opt[1] != '\0' ){
    /* %bar:h% ... %bar%�̃f�B���N�g������
     * %bar:t% ... %bar%�̃f�B���N�g�������ȊO
     * %bar:r% ... %bar%�̊g���q�ȊO
     * %bar:e% ... %bar%�̊g���q
     */

    /* %bar:x% �� bar �̕��������A�����o�� */
    char *envtmp = (char*)alloca( opt-env+1 );
    memcpy( envtmp , env , opt-env );
    envtmp[ opt-env ] = '\0';

    /* %bar% �̒l�𓾂�B����`�Ȃ�ΏI�� */
    const char *value = getShellEnv(envtmp);
    if( value == NULL )
      return;
    
    /* %bar% �̒l�����H����ׂɁA�ʂ̗̈�ɃR�s�[����B*/
    int vallen=strlen(value);
    char *valtmp=(char*)alloca(vallen+1);
    memcpy( valtmp , value , vallen );
    valtmp[ vallen ] = '\0';

    /* %bar:x% �𓾂�B
     * �������A�u:x�v�̕������s�K�ȏꍇ�́ubar:x�v�S�̂����ϐ��Ƃ݂Ȃ��B
     */
    if( opt[2]!='\0' || (sp=cut_with_designer( valtmp , opt[1] ))==NULL ){
      sp = getShellEnv(env);
    }
  }else{
    /* �� �P���� %bar% �̏ꍇ */
    sp = getShellEnv(env);
  }
  buf << sp;
}

/* �P��C���q�A���Ȃ킿�u!�v�̌�ɑ����u$�v�u^�v�ȂǂŁA
 * �s����P���؂�o����Ƃ��s���B
 *	sp ... �u:�v�̒���������B
 *	histring ... �؂�o���ꂤ��s
 *	dp ... �R�s�[��(�X�}�[�g�|�C���^)
 */

static void word_designator(const char *&sp , const char *histring ,
			    StrBuffer &buf )
{
  /* sp �́A':' �̌�ɂ���Ƃ��� */

  Parse argv(histring);
  int argc=argv.get_argc();

  if( *sp=='$' ){
    ++sp;

    buf.paste( argv[ argc-1 ].ptr , argv[ argc-1 ].len );
    return ;
  }else if( *sp=='^' ){
    ++sp;

    if( 1 < argc )
      buf.paste( argv[ 1 ].ptr , argv[ 1 ].len );

    return;

  }else if( *sp=='*' ){
    ++sp;
    
    for( int i=1 ; i<argc ; i++ ){
      buf.paste( argv[ i ].ptr , argv[ i ].len );
      buf << ' ';
    }
    return;

  }else if( *sp=='-' && isdigit(sp[1] & 255) ){
    ++sp;
    
    int n=0;
    do{
      n = n*10 + (*sp-'0');
    }while( isdigit( *++sp & 255 ) );
    
    if( n >= argc )
      n = argv.get_argc()-1;

    for(int i=0 ; i<=n ; i++ ){
      buf.paste( argv[ i ].ptr , argv[ i ].len );
      buf << ' ';
    }
    return;

  }else if( isdigit(*sp) ){

    int n=0;
    do{
      n = n*10 + (*sp-'0');
    }while( isdigit(*++sp & 255) );
      
    if( n < argc )
      buf.paste( argv[ n ].ptr , argv[ n ].len );

    if( *sp == '-' ){
      int end=0;
      if( isdigit( *++sp & 255 ) ){
	do{
	  end = end*10 + (*sp-'0');
	}while( isdigit( *++sp & 255) );
	if( end >= argc-1 )
	  end = argc-1;
      }else{
	end = argc-1;
      }
      while( ++n <= end ){
	buf << ' ';
	buf.paste( argv[ n ].ptr , argv[ n ].len );
      }
    }
    return;
  }
  buf << histring;
  return;
}
/* �u!�v�Ŏn�܂�q�X�g���Q�Ǝq���A�Ή�����q�X�g�����e�ɒu������B
 *	sp ... �u���O�́u!�v�������|�C���^
 *	dp ... �u����̌��ʂ��R�s�[����X�}�[�g�|�C���^
 * return �R�s�[���������������X�}�[�g�|�C���^
 */
static void history_copy(const char *&sp, StrBuffer &buf)
{
  /* ���� sp �́A�u!�v���w���Ă���Ɖ��� */
  const char *histring=0;
  const char *event=sp; /* �� �G���[���b�Z�[�W�p */
  
  switch( *++sp ){

  case '!':
    sp++;
  case '*':
  case ':':
  case '$':
  case '^':

    histring = get_hist_r(0);
    if( histring == NULL )
      ErrMsg::say(ErrMsg::EventNotFound,"!",0);
    break;

  default:
    int minus=0;
    if( *sp == '-' ){
      minus=1;
      ++sp;
    }
    if( is_digit(*sp) ){
      int n=0;
      do{
	n = n*10+(*sp-'0');
      }while( is_digit(*++sp) );
      
      if( minus ){
	histring = get_hist_r(n>0 ? n-1 : 0 );
	if( histring == NULL )
	  ErrMsg::say(ErrMsg::EventNotFound,event,0);
      }else{
	histring = get_hist_f(n);
	if( histring == NULL )
	  ErrMsg::say(ErrMsg::EventNotFound,event,0);
      }
      

    }else if( *sp == '?' ){
      char buffer[1024] , *bp = buffer;
      ++sp; /* skip '?' */
      while( *sp != '\0' ){
	if( *sp == '?' ){
	  ++sp; /* skip '?' */
	  break;
	}
	*bp++ = *sp++;
      }
      *bp = '\0';

      histring = seek_hist_mid(buffer);
      if( histring == NULL )
	ErrMsg::say(ErrMsg::EventNotFound,buffer,0);
	
    }else{
      char buffer[1024] , *bp = buffer;
      int len=0;
      while( *sp != '\0' && !isspace(*sp) ){
	*bp++ = *sp++;
	len++;
      }
      *bp = '\0';
      histring = seek_hist_top(buffer,len);
      if( histring == NULL )
	ErrMsg::say(ErrMsg::EventNotFound,buffer,0);
    }
    break;
  }/* end of switch */

  if( histring == NULL )
    return;

  switch( *sp ){
  case ':':
    ++sp;
  case '^':
  case '$':
  case '*':
    word_designator( sp , histring , buf );
    return;
    
  default:
    buf << histring;
    return;
  }
}

/* �q�X�g���ϊ����s���v���v���Z�b�T
 *	sp ... �u���O������
 *	_dp .. �u�����ʂ�����o�b�t�@
 *	max .. �o�b�t�@�T�C�Y
 */
char *replace_history(const char *sp) throw(StrBuffer::MallocError)
{
  StrBuffer buf;
  
  int is_history_refered=0;
  int quote=0;
  int prevchar=' ';

  if( *sp=='!' ){
    history_copy(sp,buf);
    is_history_refered = 1 ;
  }

  // ---------------- CD �� DIR �ɑ΂����O���� -------------
  
  //�ucd/usr/local/bin�v�ȂǂƂ������͂ɑΉ����邽�߂̏���
  // ���̏ꍇ�Acd �Ɓu/�v�̊Ԃɋ󔒂�}������B
  
  if(   (sp[0]=='c' || sp[0]=='C')
     && (sp[1]=='d' || sp[1]=='D')
     && (sp[2]=='.' || sp[2]=='\\' || sp[2]=='/' ) ){
    buf << sp[0] << sp[1] << ' ' << sp[2];
    sp += 3;
  }else if(   (sp[0]=='d' || sp[0]=='D')
	   && (sp[1]=='i' || sp[1]=='I')
	   && (sp[2]=='r' || sp[2]=='R')
	   && (sp[3]=='.' || sp[3]=='\\' || sp[3]=='/' ) ){
    buf << sp[0] << sp[1] << sp[2] << ' ' << sp[3];
    sp += 4;
  }
  
  while( *sp != '\0' ){
    switch( *sp ){
    case '\'':
      if( (quote & 1)==0 )
	quote ^= 2;
      break;
      
    case '"':
      if( (quote & 2)==0 )
	quote ^= 1;
      break;

    case '>': /* ���_�C���N�g�Ɋւ��u!�v���Ђ�������Ȃ��悤�ɂ��� */
      if( sp[1] == '!' ){
	buf << ">!";
	sp+=2;
      }else if( sp[1]=='&'  &&  sp[2] == '!' ){
	buf << ">&!";
	sp+=3;
      }else if( sp[1]=='>'  &&  sp[2] == '!' ){
	buf << ">>!";
	sp+=3;
      }else if( sp[1]=='>'  &&  sp[2] == '&' &&  sp[3] == '!' ){
	buf << ">>&!";
	sp+=4;
      }else{
	break;
      }
      continue;
      
    case '!':
      if(  option_tcshlike_history
	 && (   option_history_in_doublequote
	     ?  (quote & 2)==0  :  quote == 0 ) ) {
	/* history_in_doublequote ���L��(not 0)�Ȃ�΁A
	 *    "�`!�`"�̓q�X�g���ϊ�����B
	 */
	
	history_copy(sp,buf);
	/* is_history_refered = 1; */
      }
      break;
    } // end of switch

    buf << (char)(prevchar = *sp++);
    if( is_kanji(prevchar) )
      buf << *sp++;
  } // end of while

  // �q�X�g�����Q�Ƃ���Ă���ꍇ�́A�ϊ��㕶�������ʂɕ\������B
  if( is_history_refered ){
    fputs_ctrl( (const char *)buf , stdout );
    putc( '\n' , stdout );
  }

  /* �q�X�g����o�^����B*/
  PublicHistory *tmp=new PublicHistory;
  if(   tmp != NULL
     && !are_spaces((const char*)buf)
     && (tmp->string=strdup((const char*)buf))!=NULL ){
    /* (prev)   ��public_history �� tmp �� public_history->next  (next) 
     *  �Â�                     (1)    (2)   (=NULL)            �V����
     */
    tmp->prev = public_history ;

    if( public_history != NULL ){
      tmp->next = public_history->next ; /*  ���� == NULL */
      if( public_history->next != NULL ){
	public_history->next->prev = tmp;
      }
      public_history->next = tmp;
    }else{
      tmp->next = NULL;
    }
    public_history = tmp;

#if 0
    public_history = public_history->next = tmp ;
#endif
    nhistories++;
    if( option_same_history )
      Shell::replace_last_history( tmp->string );
  }
  return buf.finish();
}
void replace_history(const char *sp, char *_dp , int max )
{
  try{
    char *result=replace_history(sp);
    strncpy( _dp , result , max );
    free(result);
  }catch( StrBuffer::MallocError ){
    strncpy( _dp , sp , max );
  }
  _dp[ max-1 ] = '\0';
}


/* �v���v���Z�X�F���ϐ��A�`���_�A�u...�v�Ȃǂ̓W�J���s���B
 *	sp �ϊ��O������
 *	_dp �R�s�[��o�b�t�@
 *	max �o�b�t�@�T�C�Y
 */
char *preprocess(const char *sp) throw (StrBuffer::MallocError)
{
  StrBuffer buf;
  QuoteFlag qf;
  int prevchar=' ';

  if( *sp == '@' )
    ++sp;
  
  while( *sp != '\0' ){
    switch( *sp ){
    case '\'':
    case '"':
      qf.eval( *sp );
      break;
      
    case ';': /* �󔒁{�u�G�v���u&;�v�ɕϊ����� */
      ++sp;
      if(   Parse::option_semicolon_terminate 
	 && !qf.isInQuote() && is_space(prevchar) ){
	buf << '&';
      }
      buf << ';';
      continue;
      
    case '.': /* �󔒁{�u...�v���u..\..�v�ɕϊ����� */
      if(   option_dots 
	 && !qf.isInQuote()
	 && is_space(prevchar) && sp[1]=='.' && sp[2]=='.' ){
	
	++sp;
	/* sp �͓�ڂ� . �������Ă���B*/
	for(;;){
	  buf << "..";
	  if( *++sp != '.' )
	    break;
	  buf << '\\';
	}
	continue;
      }
      break;
      
    case '~':
      if(    option_tilda_is_home  &&  !qf.isInQuote()
	 &&  is_space(prevchar) ){
	if( *(sp+1) != '\\' && *(sp+1) != '/' && !option_tilda_without_root){
	  /* option tilda_without_root �� off �̎���
	   * "~hogehoge" �ŕϊ����Ȃ��B 
	   * ������ƁA����ȏ������A�X�����ǁc�B
	   */
	  break;
	}else if( *(sp+1) == ':' ){ /* `~:' ���u�[�g�h���C�u�ɒu������ */
	  ++sp;
	  const char *system_ini = getShellEnv("SYSTEM_INI");
	  if( system_ini == NULL ){
	    buf << '?';
	  }else{
	    buf << *system_ini;
	  }
	}else{ /* ���ʂ� UNIX �I�`���_�̕ϊ� */
	  insert_env("HOME",buf);
	  if( isalnum(*++sp&255) || is_kanji(*sp&255) ){
	    buf << (char) Edlin::complete_tail_char << ".."
	      << (char)(prevchar=Edlin::complete_tail_char) ;
	  }else{
	    prevchar = '~';
	  }
	}
	if( option_replace_slash_to_backslash_after_tilda ){
	  /* �`���_�̌�́u/�v��S�āu\�v�ɕϊ�����B */
	  for(;;){
	    if( *sp == '\0' )
	      return buf.finish();
	    if( is_space(*sp) )
	      break;
	    if( is_kanji(*sp) ){
	      buf << (char)(prevchar=*sp++);
	      buf << *sp++;
	    }else if( *sp=='/' ){
	      ++sp;
	      buf << (char)(prevchar='\\');
	    }else{
	      buf << (char)(prevchar = *sp++);
	    }
	  }
	}
	continue;
      }
      break;
      
    case '%':
      
      if( !qf.isInDoubleQuote() ){
	StrBuffer envname;
	
	++sp; /* �ŏ��́���ǂ݂Ƃ΂� */
	while( *sp != '\0' ){
	  if( *sp=='%' ){
	    /* �Ō�� sp ��ǂ݂Ƃ΂� */
	    prevchar = *sp++;
	    break;
	  }
	  envname << (char)(prevchar = toupper(*sp & 255) );
	  ++sp;
	}
	if( envname[0] == '\0' ){
	  buf << '%';	/* �u%%�v�͈�́u%�v�֕ϊ����� */
	}else{
	  insert_env((const char*)envname,buf);
	}
	continue;
      }
      break;
      
    } /* end of switch () */
    if( is_kanji(*sp) ){
      buf << (char)(prevchar=*sp++);
      buf << *sp++;
    }else{
      buf << (char)(prevchar=*sp++);
    }
  }
  return buf.finish();
}

void preprocess(const char *sp, char *_dp , int max )
{
  try{
    char *result=preprocess(sp);
    strncpy( _dp , result , max );
    free(result);
  }catch( StrBuffer::MallocError ){
    strncpy( _dp , sp , max );
  }
  _dp[ max-1 ] = '\0';
}


/* �R�}���h�uhistory�v
 *	source ... �R�}���h���g�������Ă����X�g���[��
 *	param .... �p�����[�^���X�g�I�u�W�F�N�g
 */
int cmd_history(FILE *source,Parse &param)
{
  int n=nhistories;
  /* �p�����[�^(�Q�Ƃ���q�X�g���̐�)������ꍇ�A���̐��l���擾�B
   */
  if( param.get_argc() >= 2 ){
    char *arg1=(char*)alloca(param.get_length(1)+1);
    param.copy(1,arg1);
    if( (n=atoi(arg1)) < 1 )
      n = nhistories;
  }
  FILE *fout=param.open_stdout();

  PublicHistory *cur=public_history;
  if( cur != NULL ){
    int i;
    for( i=0 ; i<n  && cur != NULL && cur != &Oth ; i++ )
      cur = cur->prev;

    for( ; i > 0  && cur !=NULL ; i-- ){
      cur = cur->next;
      fprintf( fout , "%4d : " , 1+nhistories-i );
      fputs_ctrl( cur->string , fout );
      putc('\n',fout);
    }
  }
  return 0;
}

/* ����܂ł̃q�X�g������|���āA���ɖ߂��Ă��܂��B
 * main�֐��ŁA.nyaos ����ǂݍ��܂�Ă��܂����q�X�g���������̂ɁA
 * ��x�Ă΂��̂�
 */
void killAllPublicHistory(void)
{
  PublicHistory *cur=public_history;
  while( cur != NULL && cur != &Oth ){
    PublicHistory *trash=cur;
    cur = cur->prev;
    free( trash->string );
    delete trash;
  }
  public_history = &Oth;
  Oth.next = NULL;
  Oth.prev = NULL;
  nhistories = 0;
}
