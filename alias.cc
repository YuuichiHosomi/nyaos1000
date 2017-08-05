#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <io.h>
#include "hash.h"
#include "nyaos.h"
#include "parse.h"
#include "finds.h"
#include "strbuffer.h"
#include "errmsg.h"

extern int option_noclobber;

Hash <Alias> alias_hash(1024);

static void translate_copy( const Substr &arg , StrBuffer &buf )
{
  buf << (arg[0] == '-' ? '/' : (char)arg[0] );
  
  int quote=(arg[0] == '"' ? 1 : 0);
  for(int i=1;i<arg.len;i++){
    if( arg[i] == '"' )
      quote ^= 1;

    buf << ( quote && arg[i] == '/' ? '\\' : arg[i] );
  }

  if( arg[arg.len-1] == '/' || arg[arg.len-1] == '\\' )
    buf << '.';
}

/*  ���C���h�J�[�h�W�J���s���B
 *	arg		���C���h�J�[�h���܂ތ��t�@�C����
 *	dp		�W�J��
 *	translate_flag	
 */
static void wildcard_expand_copy( const Substr &arg , StrBuffer &buf
				 ,int translate_flag )
{
  int quote=0;
  int wildcard=0;
  
  for(int i=0;i<arg.len;i++){
    if( arg[i] == '"' )
      quote ^= 1;
    
    if( quote==0  && (arg[i] == '*' || arg[i] == '?') ){
      wildcard = 1;
      break;
    }
  }
  if( wildcard == 0 ){
    /* ���C���h�J�[�h�W�J�����̏ꍇ */
    if( translate_flag ){
      translate_copy( arg , buf );
    }else{
      for(int i=0;i<arg.len ; i++)
	buf << arg[i];
    }
    return;
  }
  char *buffer=(char*)alloca(arg.len+1);
  {
    char *q=buffer;
    for(int i=0;i<arg.len;i++){
      if( arg[i] != '"' )
	*q++ = arg[i];
    }
    *q = '\0';
  }
  char **filelist=fnexplode2(buffer);
  if( filelist == NULL ){
    if( translate_flag ){
      translate_copy( arg , buf );
    }else{
      for(int i=0 ; i<arg.len ; i++)
	buf << arg[i];
    }
    return;
  }
  numeric_sort(filelist);
  for(char **ptr=filelist ; *ptr != NULL ; ++ptr ){
    int need_quote=0;
    for(const char *sp=*ptr ; *sp != '\0' ; ++sp ){
      if( isspace(*sp & 255) ){
	need_quote = 1;
	buf << '"';
	break;
      }
    }
    for(const char *sp=*ptr ; *sp != '\0' ; ++sp )
      buf << ( translate_flag  &&  *sp == '/' ? '\\' : *sp );

    if( need_quote )
      buf << '"';
    buf << ' ';
  }
  fnexplode2_free(filelist);
}

/* %1:h , %2:r �Ȃǂ���������ׂ̉��H���[�`��
 * foreach2.cc �� word_design �� SubStr/SmartPtr�ŁB
 *	dp	�c �W�J��
 *	argv	�c �������g(SubStr):�T�C�Y��������
 *	option	�c 'h','t','r' or 'e'
 * return
 *	true  �c option ���K�؁B������������R�s�[�����B
 *	false �c option ���s�K�B������S�̂��R�s�[�����B
 */
static bool word_design(StrBuffer &buf,const Substr &argv,int option)
{
  /* cut_with_designer �� prepro2.cc �Œ�`����Ă���֐��B
   * �^����ꂽ�������؂荏��ŁA�f�B���N�g���Ƃ��A
   * �g���q�Ƃ��𔲂��o���B����䂦�A���̕�����͖�����
   * �A���Ă��Ȃ��B������A�g���� alloca �ŁA�܂����B
   */
  extern char *cut_with_designer(char *path,int option);

  char *buffer=(char*)alloca(argv.len+1);
  argv.quote( buffer );

  /* �g��肪�ł�������A���������؂荏��ł��炨�� */
  char *part=cut_with_designer( buffer , option );

  if( part != NULL ){
    /* �� option �l���K�؂ŁA�����ƕ��������񂪐؂�o�����B
     */
    while( *part != '\0' )
      buf << *part++;
    return true;
  }else{
    /* �� option �l���s�K���A���������A�؂荏��łق����Ȃ��ꍇ��
     *    ������S�̂��R�s�[����B
     */
    for( int i=0 ; i<argv.len ; i++ )
      buf << argv.ptr[i];
    return false;
  }
}

char *replace_alias(const char *sp) throw(Noclobber)
{
  try{
    StrBuffer buf;
    for(;;){ /* �e�R�}���h�P�� */
      Parse params(sp);
      
      /* ���߂���̏ꍇ�A�������ɂ�蒼���B
       * �u&&�v�� start�ɕϊ�����u&�v�Ȃǂł́A���ꂪ�K�v�炵�� 
       */
      if( params.get_argc() == 0 ){
	sp = params.get_tail();
	if( *sp == '\0' )
	  break;
	while( sp < params.get_nextcmds() )
	  buf << *sp++;
	if( *sp == '\0' )
	  break;
	
	continue;
      }
      
      Alias *ptr = alias_hash[ params[0] ];
      if( ptr == NULL ){
	/* �G�C���A�X�ɒ�`����Ă��Ȃ��̂ŁA
	 * ���̓��͕���������̂܂ܔ��f���� */
	for(int i=0;i<params.get_argc();i++){
	  buf.paste( params[i].ptr, params[i].len );
	  buf << ' ';
	}
      }else{
	/* �G�C���A�X�ɒu������ */
	const char *spa=ptr->base;
	bool percent_used=false;
	
	while( *spa != '\0' ){
	  if( *spa == '%' ){
	    /* ����(���̓W�J)�̓W�J���s�� */
	    
	    /* ���C���h�J�[�h�W�J���s�����A
	     * %+1 , %+2 �ƂȂ��Ă��邩���`�F�b�N���� */
	    int wildcard_flag = 0;
	    if( *++spa == '+' ){
	      wildcard_flag = 1;
	      ++spa;
	    }
	    
	    switch( *spa ){
	    default:
	      if( is_digit(*spa) ){
		percent_used = true;
		int n=0;
		do{
		  n *= 10;
		  n += (*spa-'0');
		}while( is_digit(*++spa) );
		
		if( n < params.get_argc() ){
		  if( wildcard_flag ){
		    /* ���C���h�J�[�h�W�J����ꍇ */
		    wildcard_expand_copy( params[n] , buf ,*spa=='@' ? 1 : 0);
		  }else if( spa[0]==':'  &&  is_alpha(spa[1]) ){
		    /* �u:x�v�ȂǁA�p�X�𕔕��I�Ɏ��o���ꍇ */
		    if( word_design( buf , params[n] , spa[1] ) ){
		      spa += 2; /* �u:x�v��ǂݔ�΂� */
		    }
		  }else{
		    /* ������S�̂�f���Ɏ��o���ꍇ */
		    params.copy(n,buf);
		  }
		}
		
		if( *spa == '*' ){
		  while( ++n < params.get_argc() ){
		    buf << ' ';
		    if( wildcard_flag )
		      wildcard_expand_copy( params[n] , buf , 0 );
		    else
		      params.copy(n,buf);
		  }
		  ++spa;
		}else if( *spa == '@' ){
		  while( ++n < params.get_argc() ){
		    buf << ' ';
		    if( wildcard_flag )
		      wildcard_expand_copy( params[n] , buf , 1 );
		    else
		      params.copy(n,buf,Parse::QUOTE_COPY);
		  }
		  ++spa;
		}
	      }
	      break;
	      
	    case '*':
	      percent_used = true;
	      spa++;
	      if( wildcard_flag ){
		for(int i=1;i<params.get_argc();i++)
		  wildcard_expand_copy( params[i] , buf , 0 );
	      }else{
		params.copyall(1,buf);
	      }
	      break;
	      
	    case '@':
	      percent_used = true;
	      spa++;
	      if( wildcard_flag ){
		for(int i=1;i<params.get_argc() ; i++)
		  wildcard_expand_copy( params[i] , buf , 1 );
	      }else{
		params.copyall(1,buf,Parse::REPLACE_SLASH);
	      }
	      break;
	      
	    case '%':
	      spa++;
	      buf << '%';
	      break;

	   case '\\':case '/':
	     if(   buf.getLength() <= 0
		|| (   buf[ buf.getLength()-1 ] != '\\' 
		    && buf[ buf.getLength()-1 ] != '/' ) )
	       buf << *spa;
	     spa++;
	   }
	 }else{
	   buf << *spa++;
	 }
       }
       if( percent_used == false ){
	 buf << ' ';
	 params.copyall(1,buf);
       }
      }
      
      params.restoreRedirects(buf);

      sp = params.get_tail();
      if( *sp == '\0' )
	break;
      
      while( sp < params.get_nextcmds() )
	buf << *sp++;
      
      if( *sp == '\0' )
	break;
    }/* for(;;) */
    return buf.finish();
  }catch( Noclobber ){
    throw;
  }catch( StrBuffer::MallocError ){
    throw Noclobber();
  }
}

void replace_alias(const char *sp , char *destinate , int max )
{
  try{
    char *result=replace_alias(sp);
    strncpy( destinate , result , max );
    destinate[max-1] = '\0';
    free(result);
  }catch( StrBuffer::MallocError  ){
    strncpy( destinate , sp , max );
  }catch( Noclobber ){
    destinate[0] = '\0';
  }
}

int cmd_unalias(FILE *fin, Parse &params)
{
  int argc=params.get_argc();

  if( argc < 2 )
    return 0;
  
  if( alias_hash.remove( params[1] ) != 0 ){
    char name[128];
    params.copy(1,name);
    
    ErrMsg::say(ErrMsg::NoSuchAlias,"unalias",name,0);
    
    return 1;
  }
  return 0;
}

/* �ʖ� name �̒�`���e���uNAME=VALUE\n�v�`���� fout �֏o�͂���B
 *	name �ʖ�����
 *	fout �o�͐�
 * return
 *	0 �c ���̕ʖ������݂��āA�\�������B
 *	1 �c ���̕ʖ��͑��݂��Ȃ������B
 */
static int print_one_alias(const char *name,FILE *fout=stdout)
{
  Alias *ptr=alias_hash[ name ];
  if( ptr != NULL ){
    fprintf(fout,"%s=\"%s\"\n",ptr->name,ptr->base);
    return 0;
  }
  return 1;
}

int cmd_alias(FILE *fp, Parse &params)
{
  const char *sp=params.get_argv(1);
  int argc=params.get_argc();

  if( argc < 2  ||  sp==NULL ){
    /* �����������ꍇ�A�G�C���A�X�̃��X�g��\������B*/

    FILE *fout=params.open_stdout();
    if( fout == NULL ){
      ErrMsg::say(ErrMsg::CantOutputRedirect,"alias",0);
      return 1;
    }
    for( HashIndex <Alias> cur(alias_hash) ; *cur != NULL ; ++cur )
      fprintf( fout,"%s=\"%s\"\n" , cur->name , cur->base );
  }else if( sp[0]=='-'  &&  sp[1]=='s'  ){
    /* alias -s ... ���_�C���N�g�̏o�͂����̂܂� source �ł���`�ɂ���B*/
    FILE *fout=params.open_stdout();
    if( fout == NULL ){
      ErrMsg::say(ErrMsg::CantOutputRedirect,"alias",0);
      return 1;
    }
    for( HashIndex <Alias> cur(alias_hash) ; *cur != NULL ; ++cur ){
      fprintf( fout,"alias %s=\"" , cur->name );
      /* ��̈��p�����ɕϊ����� */
      for(const char *sp=cur->base ; *sp != '\0' ; ++sp ){
	if( *sp == '"' )
	  putc( '"' , fout);
	putc( *sp , fout );
      }
      fprintf( fout,"\"\n" );
    }
  }else{
    /* ����������̂ŁA�G�C���A�X���`�A���邢�́A�\������ */
    int length=strlen(sp);
    Alias *tmp=(Alias*)malloc(sizeof(struct Alias)+length);
    if( tmp == NULL )
      return -1;

    char *dp=tmp->name;
    while( !is_space(*sp) ){
      if( sp >= params.get_tail() ){
	*dp = '\0';
	FILE *fout=params.open_stdout();
	if( fout == NULL ){
	  ErrMsg::say(ErrMsg::CantOutputRedirect,"alias",0);
	  return 1;
	}
	print_one_alias(tmp->name,fout);
	free(tmp);
	return 0;
      }
      if( *sp == '=' ){
	++sp;
	break;
      }
      if( is_space(*sp) ){
	while( is_space(*sp) )
	  sp++;
	if( *sp == '=' ){
	  ++sp;
	  break;
	}
      }
      *dp++ = *sp++;
    }
    *dp++ = '\0';
    
    /* �󔒃X�L�b�v */
    while( is_space(*sp) )
      sp++;

    if( sp >= params.get_tail() ){
      FILE *fout=params.open_stdout();
      print_one_alias(tmp->name,fout);
      free(tmp);
      return 0;
    }
    
    tmp->base = dp;

    /* alias ahaha="ufufuf ""ohoho""" �̏ꍇ�B
     * ���p����͋󕶎��ɁA�A��������p����͈��p����ɒu�������B
     */
    int quote = 1;
    for(;;){
      if( *sp == '"' ){
	if( *++sp == '"' ){
	  *dp++ = *sp++;
	  continue;
	}else{
	  quote ^= 1;
	}
      }
      if( sp >= params.get_tail() )
	break;
      *dp++ = *sp++;
    }
    *dp = '\0';
    
    alias_hash.destruct( tmp->name );
    alias_hash.insert( tmp->name , tmp );
  }
  return 0;
}
