#include <assert.h>
#include <ctype.h>
#include <io.h>
#include <stdio.h>

#include "nyaos.h"
#include "macros.h"
#include "parse.h"
#include "errmsg.h"

int Parse::option_semicolon_terminate=1;

extern int option_noclobber;

void Parse::restoreRedirects(StrBuffer &buf) throw(Noclobber)
{
  if( redirect[0].isRedirect() ){
    buf << " <";
    buf.paste( redirect[0].path.ptr , redirect[0].path.len );
  }
  for(int i=1 ; i <= 2 ; i++ ){
    if( redirect[i].isToFile() ){
      buf << ' ' << "012"[i] << '>';
      if( redirect[i].isAppend() )
	buf << '>';
      
      char *fname=(char*)alloca(redirect[i].path.len+1);
      redirect[i].path >> fname;
      
      if(   ! redirect[i].isForced()
	 && ! redirect[i].isAppend()
	 && option_noclobber
	 && access(fname,0444)==0 )
	throw Noclobber();
      
      buf << fname << ' ';
    }
    if( redirect[i].isToHandle() ){
      buf << ' ' << "0123456789"[i] << ">&" 
	<< "0123456"[ redirect[i].getHandle() & 7] << ' ';
    }
  }
}


void Parse::RedirectInfo::close()
{
  if( fp != 0 ){
    if( flag & PIPE )
      pclose(fp);
    else
      fclose(fp);
    fp = 0;
  }
}

FILE *Parse::RedirectInfo::openFileToWrite()
{
  this->close();
  flag &= ~PIPE;
  
  char *fname = (char*)alloca( path.len+1 );
  path.quote(fname);
  
  return fp=fopen(fname,isAppend() ? "a":"w");
}

FILE *Parse::RedirectInfo::openFileToRead()
{
  this->close();
  flag &= ~PIPE;
  
  char *fname = (char*)alloca( path.len+1 );
  path.quote(fname);
  
  return fp=fopen(fname,"r");
}

FILE *Parse::RedirectInfo::openPipe(const char *cmds,const char *mode)
{
  this->close();
  flag |= PIPE;
  return fp=popen(cmds,mode);
}


/* char�z��ցA�R�s�[����B
 *	�E�P��̓�d���p���͖�������B
 *	�E��̘A�������d���p���́A��̓�d���p���֕ϊ�����B
 * return
 *	�R�s�[��� '\0' �̈ʒu�B
 */
char *Substr::quote(char *dp) const
{
  const char *tail=ptr+len;
  const char *sp=ptr;
  
  while( sp < tail ){
    /* �P��́u"�v�͖������邪�A�A������u""�v�́u"�v�ɕϊ����� */
    if( *sp == '"' ){
      if( *++sp == '"' ){
	*dp++ = '"';
	sp++;
      }
    }else{
      if( is_kanji(*sp) )
	*dp++ = *sp++;
      *dp++ = *sp++;
    }
  }
  *dp = '\0';
  return dp;
}

void Substr::operator >> (SmartPtr dp) const
{
  for(int i=0;i<len;i++)
    *dp++ = ptr[i];
  *dp = '\0';
}

extern volatile int ctrl_c;

Parse::~Parse()
{
  if( args != argbase  &&  args != NULL )
    delete args;
}

int Parse::parseRedirect()
{
  int ionum = 1;
  
  if( *sp=='0' || *sp=='1' || *sp=='2' )
    ionum = *sp++ - '0';

  if( *sp == '<' ){
    ionum = 0;
    ++sp;
  }else if( *sp == '>' ){
    if( *++sp == '>' ){
      redirect[ ionum ].setAppend();
      // appendflag[ ionum ] = true;
      ++sp;
    }
    // sp points next of '>'
    if( *sp == '&' ){
      ++sp; // sp points next of '&'

      if( isdigit(*sp & 255) ){ /* 2>&1 or 2>&1 */
	redirect[ ionum ].setHandle(*sp++ -'0');
	return 0;
      }else{ /* �P�Ȃ�u>&�v�`�� */
	if( ionum == 1 )
	  redirect[ 2 ].setHandle( 1 );
	else
	  redirect[ 1 ].setHandle( 2 );
      }
    }
    if( *sp=='!' ){
      redirect[ ionum ].setForced();
      ++sp;
    }
  }else{
    ErrMsg::say(ErrMsg::InternalError,"nyaos",0);
    return 1;
  }
  
  while( isspace(*sp & 255) )
    ++sp;
  
  redirect[ ionum ].path.ptr = sp;
  
  if( tailcheck() )
    return err=-1;

  do{
    if( is_kanji(*sp) )
      ++sp;
    ++sp;
    if( *sp=='"' ){
      do{
	if( is_kanji(*sp) )
	  ++sp;
	++sp;
	if( *sp == '\0' )
	  goto exit;
      }while( *sp != '"' );
      sp++;
    }
  }while( *sp != '\0'  &&  !isspace(*sp & 255) );

 exit:
  redirect[ ionum ].path.len = sp - redirect[ ionum ].path.ptr ;
  return 0;
}


/* �����łȂ��Ƃ��́A�l \0 ��Ԃ��B
 * �����̎��́A�u&�v�u|�v�u0�v(0��\0�ł͂Ȃ�)��Ԃ��B
 *
 * terminal �� �u&�v�u|�v�u\0�v���ݒ肳���B
 */
int Parse::tailcheck ()
{
  if( *sp=='&' ){
    tail = sp++;
    if( *sp == '&' ){
      nextcmds = ++sp;
      return terminal = AND_TERMINAL;
    }else if( *sp == ';' ){
      nextcmds = ++sp;
      return terminal = SEMI_TERMINAL;
    }else{
      nextcmds = sp;
      return terminal = AMP_TERMINAL;
    }
  }

  if( *sp=='|' ){
    tail = sp++;
    if( *sp=='|' ){
      nextcmds = ++sp;
      return terminal=OR_TERMINAL;
    }else if( *sp=='&' ){
      nextcmds = ++sp;
      return terminal=PIPEALL_TERMINAL;
    }else{
      nextcmds = sp;
      return terminal=PIPE_TERMINAL;
    }
  }

  if( *sp=='\0' ){
    tail=nextcmds=sp;
    return terminal=NULL_TERMINAL;
  }
  return terminal=NOT_TERMINAL;
}

/* �R�}���h���C���̂P�R�}���h���A�P�ꂲ�Ƃ� Substr �^�֕�������B
 *
 * return �R�}���h�̏I������ '\0','&'�c
 */
int Parse::parseAll()
{
  argc = 0;

  redirect[0].reset();
  redirect[1].reset();
  redirect[2].reset();

  terminal = NOT_TERMINAL;

  for(;;){
    if( argc+1 >= limit ){
      if( args == argbase ){
	args = new Substr[limit+=30];
	assert( args != NULL );
	for(int i=0;i<limit;i++)
	  args[i] = argbase[i];
      }else{
	Substr *tmp=new Substr[limit + 30];
	for(int i=0;i<limit;i++)
	  tmp[i] = args[i];
	delete args;
	args = tmp;
	limit += 30;
	assert( args != NULL );
      }
    }

    /* �󔒂�ǂݔ�΂� */
    while( *sp != '\0' &&  is_space(*sp) )
      ++sp;

    if( tailcheck() != 0 ){
      args[ argc ].reset();
      return terminal;
    }

    if( isRedirectMark(sp) ){
      if( parseRedirect() != 0 )
	return err=-1;
      continue;
    }
    
    args[ argc ].ptr = sp;
    while( !is_space(*sp) ){
      if( tailcheck() ){
	args[ argc ].len = tail - args[argc].ptr;
	++argc;
	return terminal;
      }
      if( *sp=='<' || *sp=='>' 
	 || ((*sp=='1' || *sp=='2') && sp[1] == '>' ) )
	break;

      if( *sp == '^' && *(sp+1) != '\0' ){
	/* �L�����b�g�̓k���ȊO�̎��̋@�\�����𖳌�������B*/
	if( is_kanji(*++sp) ){
	  sp++;
	  assert(*sp != '\0');
	}
	sp++;
      }else if( *sp == '"' ){
	/* �N�H�|�g�͎��̃N�H�|�g������܂ŁA
	 * �k���ƃN�H�|�g�ȊO�̑S�Ă̋@�\�����𖳌�������B
	 * �L�����b�g�������������B
	 */
	do{
	  if( is_kanji(*sp) ){
	    ++sp;
	    assert(*sp != '\0' );
	  }
	  ++sp;
	  if( *sp=='\0' ){
	    terminal = NULL_TERMINAL;
	    tail=nextcmds=sp;
	    goto exit;
	  }
	}while( *sp != '"' );
	++sp;
      }else{
	/* ANK�Ȃ�2byte,�����Ȃ�1byte���炷 */
	if( is_kanji(*sp) )
	  ++sp;
	++sp;
      }
    }
    args[ argc ].len = sp - args[argc].ptr;
    ++argc;
  }
 exit:
  args[ argc ].len = sp - args[argc].ptr;
  ++argc;
  return terminal;
}

FILE *Parse::open_stdout()
{
  if( redirect[1].isRedirect() ){
    /* ���_�C���N�g�悪�A�t�@�C���Ɏw�肳��Ă���ꍇ
     * ������ '|' �ł́A��������
     */
    if( terminal==PIPE_TERMINAL ){
      ErrMsg::say(ErrMsg::AmbiguousRedirect,0);
      return NULL;
    }
    return redirect[1].openFileToWrite();
  }else if( terminal==PIPE_TERMINAL || terminal==PIPEALL_TERMINAL ){
    return redirect[1].openPipe( nextcmds , "w" );
  }else{
    return stdout;
  }
}

void Parse::close_stdout()
{
  redirect[1].close();
}

int Parse::call_as_main(int (*routine)(int argc,char **argv
				       ,FILE *fout,Parse &parser))
{
  int i;
  char **argv=(char**)alloca(sizeof(char*)*(argc+5));
  for(i=0;i<argc;i++){
    argv[i]=(char *)alloca(args[i].len+5);
    copy(i,SmartPtr(argv[i],args[i].len+5));
  }
  argv[i] = NULL;

  FILE *fout=open_stdout();
  if( fout==NULL ){
    ErrMsg::say(ErrMsg::CantOutputRedirect,"nyaos",0);
    return -1;
  }
  return (*routine)(argc,argv,fout,*this);
}

SmartPtr Parse::copy(int n, SmartPtr dp, int flag ) throw()
{
  if( n >= argc )
    return dp;

  const char *sp   = args[n].ptr ;
  const char *tail = sp + args[n].len ;

  /* ��{�I�Ɉ��p���ƃL�����b�g�̓R�s�|���Ȃ��B
   * ��d�L�����b�g�u^^�v�́u^�v�Ƃ��ăR�s�|����B
   * �������A���p���Ɉ͂܂ꂽ�L�����b�g�͂��̂܂܃R�s�|����B
   */
  
  bool quote=false;
  
  /* UNIX���C�N�ȃp�X/�I�v�V�����w��@�� OS/2 ���C�N�ɕϊ����鏈��
   * (1) s|^-|/|;
   */
  
  if( (flag & REPLACE_SLASH)  &&  *sp == '-' ){
    *dp++ = '/';
    ++sp;
  }

  int lastchar = -1;

  try{
    while( sp < tail ){
      if( *sp == '"' ){
	/* ���p���̏ꍇ�́A�t���O�𔽓]�����āA�|�C���^��i�߂邾���B*/
      
	if( (flag & QUOTE_COPY)==0 && *(sp+1) == '"' ){
	  /* �A�������̈��p���́A�P��̈��p���ɕϊ�����B*/
	  *dp++ = '"';
	  sp += 2;
	}else{
	  quote = !quote;
	  ++sp;
	  if( flag & QUOTE_COPY )
	    *dp++ = '"';
	}

      }else if( *sp == '^' && !quote ){
	/* �L�����b�g�̎��̕����𖳏����� put ����B */
	if( *++sp != '\0' ){
	  if( is_kanji(lastchar=*sp) )
	    *dp++ = *sp++;
	  *dp++ = *sp++;

	}else{
	  *dp = '\0';
	  return dp;
	}
      }else if( (flag & REPLACE_SLASH) && !quote && *sp == '/' ){
	/* UNIX���C�N�ȃp�X/�I�v�V�����w��@�� OS/2 ���C�N�ɕϊ����鏈��
	 * (2) s|/|\|g; (���������p���Ɉ͂܂�Ă��Ȃ�����)
	 */

	lastchar = *dp++ = '\\';
	sp++;
      }else{
	/* ����ȊO�̓R�s�| */
	if( is_kanji(lastchar=*sp) ){
	  *dp++ = *sp++;
	  assert(*sp != '\0' );
	}
	*dp++ = *sp++;
      }
    }
    if(   (flag & REPLACE_SLASH) != 0  
       && (lastchar=='/' || lastchar=='\\' ) )
      *dp++ = '.';

    *dp = '\0';
  }catch(SmartPtr::BorderOut){
    dp.terminate();
  }
  return dp;
}

void Parse::copy(int n , StrBuffer &buf,int flag) throw(MallocError)
{
  if( n >= argc )
    return;

  const char *sp   = args[n].ptr ;
  const char *tail = sp + args[n].len ;

  /* ��{�I�Ɉ��p���ƃL�����b�g�̓R�s�|���Ȃ��B
   * ��d�L�����b�g�u^^�v�́u^�v�Ƃ��ăR�s�|����B
   * �������A���p���Ɉ͂܂ꂽ�L�����b�g�͂��̂܂܃R�s�|����B
   */
  
  bool quote=false;
  
  /* UNIX���C�N�ȃp�X/�I�v�V�����w��@�� OS/2 ���C�N�ɕϊ����鏈��
   * (1) s|^-|/|;
   */
  
  if( (flag & REPLACE_SLASH)  &&  *sp == '-' ){
    buf << '/';
    ++sp;
  }

  int lastchar = -1;

  while( sp < tail ){
    if( *sp == '"' ){
      /* ���p���̏ꍇ�́A�t���O�𔽓]�����āA�|�C���^��i�߂邾���B*/
      
      if( (flag & QUOTE_COPY)==0 && *(sp+1) == '"' ){
	/* �A�������̈��p���́A�P��̈��p���ɕϊ�����B*/
	buf << '"';
	sp += 2;
      }else{
	quote = !quote;
	++sp;
	if( flag & QUOTE_COPY )
	  buf << '"';
      }
      
    }else if( *sp == '^' && !quote ){
      /* �L�����b�g�̎��̕����𖳏����� put ����B */
      if( *++sp != '\0' ){
	if( is_kanji(lastchar=*sp) )
	  buf << *sp++;
	buf << *sp++;
      }else{
	return;
      }
    }else if( (flag & REPLACE_SLASH) && !quote && *sp == '/' ){
      /* UNIX���C�N�ȃp�X/�I�v�V�����w��@�� OS/2 ���C�N�ɕϊ����鏈��
       * (2) s|/|\|g; (���������p���Ɉ͂܂�Ă��Ȃ�����)
       */
      
      buf << '\\';
      lastchar = '\\';
      sp++;
    }else{
      /* ����ȊO�̓R�s�| */
      if( is_kanji(lastchar=*sp) ){
	buf << *sp++;
	assert(*sp != '\0' );
      }
      buf << *sp++;
    }
  }
  if(   (flag & REPLACE_SLASH) != 0  
     && (lastchar=='/' || lastchar=='\\' ) )
    buf << '.';
  
}


SmartPtr Parse::betacopy(SmartPtr dp,int n)
{
  const char *ssp=args[n].ptr;

  while( ssp < tail )
    *dp++ = *ssp++;

  *dp = '\0';
  return dp;
}

void Parse::betacopy(StrBuffer &buf,int n)
{
  const char *ssp=args[n].ptr;
  while( ssp < tail )
    buf << *ssp++;
}

SmartPtr Parse::copyall(int n, SmartPtr dp, int flag)
{
  if( n < argc ){
    
    /* ��{�I�Ɉ��p���͕��ʂ̕����Ɠ��l�ɃR�s�|����B
     * �������A�L�����b�g�̎��̋@�\�����𖳌�������B
     * �L�����b�g���g�̓R�s�[���Ȃ�(�u^^�v�͕�)
     */
    
    const char *ssp=args[n].ptr;
    bool quote=false;

    /* UNIX���C�N�ȃp�X/�I�v�V�����w��@�� OS/2 ���C�N�ɕϊ����鏈��
     * (1) s|^-|/|;
     */
    if( (flag & REPLACE_SLASH) &&  *ssp == '-' ){
      *dp++ = '/';
      ++ssp;
    }

    int lastchar = -1;

    while( ssp < tail ){
      if( *ssp == '"' ){
	/* ���p���́A�t���O�𔽓]������B*/

	if( (flag & QUOTE_COPY)==0 && *(ssp+1) == '"' ){
	  /* �A�������̈��p���́A�P��̈��p���ɕϊ�����B*/
	  *dp++ = '"';
	  ssp += 2;
	}else{
	  quote = !quote;
	  ++ssp;
	  if( flag & QUOTE_COPY )
	    *dp++ = '"';
	}
	continue;
      }

      if( *ssp=='^' && !quote ){
	/* ���p���̒��ɂȂ��L�����b�g�͎��̓��ꕶ���̋@�\��
	 * ����������B
	 */
	if( *++ssp != '\0' ){
	  if( is_kanji(lastchar=*ssp) )
	    *dp++ = *ssp++;
	  *dp++ = *ssp++;
	}else{
	  *dp++ = '\0';
	  return dp;
	}
	continue;
      }
      
      if( (flag & REPLACE_SLASH) && !quote ){
	/* UNIX���C�N�ȃp�X/�I�v�V�����w��@�� OS/2 ���C�N�ɕϊ����鏈�� */
	
	if( *ssp == '/' ){
	  /* (2) s|/|\|g; (���������p���Ɉ͂܂�Ă��Ȃ�����) */
	  lastchar = *dp++ = '\\';
	  ssp++;
	  if( *ssp == '\0' || is_space(*ssp) )
	    *dp++ = '.';
	  continue;
	}else if( *ssp == '-' && is_space(lastchar) ){
	  lastchar = *dp++ = '/';
	  ssp++;
	  continue;
	}else if( *ssp=='\\' && (ssp[1]=='\0' || is_space(ssp[1]) )){
	  *dp++ = *ssp++;
	  lastchar = *dp++ = '.';
	  continue;
	}
      }
      /* ����ȊO�̓R�s�| */
      if( is_kanji(lastchar=*ssp) )
	*dp++ = *ssp++;
      *dp++ = *ssp++;
    }
    *dp = '\0';
  }
  return dp;
}

void Parse::copyall(int n, StrBuffer &buf, int flag) throw( MallocError )
{
  if( n < argc ){
    
    /* ��{�I�Ɉ��p���͕��ʂ̕����Ɠ��l�ɃR�s�|����B
     * �������A�L�����b�g�̎��̋@�\�����𖳌�������B
     * �L�����b�g���g�̓R�s�[���Ȃ�(�u^^�v�͕�)
     */
    
    const char *ssp=args[n].ptr;
    bool quote=false;

    /* UNIX���C�N�ȃp�X/�I�v�V�����w��@�� OS/2 ���C�N�ɕϊ����鏈��
     * (1) s|^-|/|;
     */
    if( (flag & REPLACE_SLASH) &&  *ssp == '-' ){
      buf << '/';
      ++ssp;
    }

    int lastchar = -1;

    while( ssp < tail ){
      if( *ssp == '"' ){
	/* ���p���́A�t���O�𔽓]������B*/

	if( (flag & QUOTE_COPY)==0 && *(ssp+1) == '"' ){
	  /* �A�������̈��p���́A�P��̈��p���ɕϊ�����B*/
	  buf << '"';
	  ssp += 2;
	}else{
	  quote = !quote;
	  ++ssp;
	  if( flag & QUOTE_COPY )
	    buf << '"';
	}
	continue;
      }

      if( *ssp=='^' && !quote ){
	/* ���p���̒��ɂȂ��L�����b�g�͎��̓��ꕶ���̋@�\��
	 * ����������B
	 */
	if( *++ssp != '\0' ){
	  if( is_kanji(lastchar=*ssp) )
	    buf << *ssp++;
	  buf << *ssp++;
	}else{
	  return;
	}
	continue;
      }
      
      if( (flag & REPLACE_SLASH) && !quote ){
	/* UNIX���C�N�ȃp�X/�I�v�V�����w��@�� OS/2 ���C�N�ɕϊ����鏈�� */
	
	if( *ssp == '/' ){
	  /* (2) s|/|\|g; (���������p���Ɉ͂܂�Ă��Ȃ�����) */
	  buf << '\\';
	  lastchar = '\\';
	  ssp++;
	  if( *ssp == '\0' || is_space(*ssp) )
	    buf << '.';
	  continue;
	}else if( *ssp == '-' && is_space(lastchar) ){
	  buf << '/';
	  lastchar = '/';
	  ssp++;
	  continue;
	}else if( *ssp=='\\' && (ssp[1]=='\0' || is_space(ssp[1]) )){
	  buf << *ssp++;
	  buf << '.';
	  lastchar = '.';
	  continue;
	}
      }
      /* ����ȊO�̓R�s�| */
      if( is_kanji(lastchar=*ssp) )
	buf << *ssp++;
      buf << *ssp++;
    }
  }
}
