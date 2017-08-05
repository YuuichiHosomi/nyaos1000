#include <ctype.h>
// #include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define INCL_WIN
#define INCL_VIO
// #include <os2.h>

#include "keyname.h"
#include "hash.h"
#include "edlin.h"
#include "complete.h"
#include "nyaos.h"

extern HAB hab;

/* option ���߂� �N���XEdlin����Q�Ƃ����̂� */
int Shell::beep_ok=1;

/* 1.39 �Œǉ����� <WP_CONFIG> �Ȃǂ��A�⊮�Ώۂɉ�����I�v�V�����B
 * �������A�g���Ă݂�ƁA���ہA�g���ɂ����Ȃ邾���Ȃ̂ŁA�f�t�H���g�I�t�B
 */
int option_complete_etc=0;

int Shell::complete_hook(Complete &com)
{
  int n=0;
  if( com.status == Complete::SIMPLE_COMMAND_COMPLETED ){
    /* build-in command */
    for( const Command *p=jumptable; p->name != NULL ; p++ ){
      if( com.add_buildin_command(p->name) == 0 )
	n++;
    }

    /* alias */
    extern Hash <Alias> alias_hash;

    for(HashIndex <Alias> hi(alias_hash) ; *hi != NULL ; hi++ ){
      if( com.add_buildin_command(hi->name) == 0 )
	n++;
    }
  }else if( option_complete_etc ){
    const char *workplace[]={
      "<WP_CONFIG>",	/* �V�X�e���ݒ� */
      "<WP_DESKTOP>",	/* �f�X�N�g�b�v */
      "<WP_DRIVE>",	/* �h���C�u�ݒ� */
      "<WP_INFO>",	/* ��� */
      "<WP_NOWHERE>",	/* ���̑� */
      "<WP_START>",	/* �n�� */
      "<WP_SYSTEM>",	/* �V�X�e�� */
      "<WP_TEMPS>",	/* �e���v���[�g */
      NULL,
    };
    for(const char **p=workplace ; *p != NULL ; p++ ){
      if( com.add_buildin_command(*p) == 0 )
	n++;
    }
    struct Option{
      const char *name;
      int *pointor;
      int true_value;
      int false_value;
    } extern optlist[];
    
    for(const Option *p=optlist; p->name != NULL ; p++ ){
      if( com.add_buildin_command(p->name) == 0 )
	n++;
    }
  }
  return n;
}

/* �v�����v�g���ĕ\������ */

void Shell::re_prompt()
{
  putchrs(prompt);
  int i=0;
  while( i<len )
    putnth( i++ );
  putbs( i-pos );
}


/* ^D �� [TAB]^2 �ȂǁA�⊮���X�g�̕\�����s���L�[���\�b�h
 */
void Shell::complete_list()
{
  Complete com;

  int fntop=seek_word_top();
  int basesize=pos-fntop;
  int command_complete=(fntop <= 0) ;
  int has_a_wildcard_letter = 0;
  char *buffer=(char*)alloca(basesize+6);

  if( strbuf[fntop] == '"' ){
    fntop++;
    basesize--;
  }
  char *bp=buffer;
  while( fntop < pos ){
    if( strbuf[fntop] == '?' || strbuf[fntop] == '*' )
      has_a_wildcard_letter = 1;
    *bp++ = strbuf[fntop++];
  }
  *bp = '\0';
  
  int nfiles=(  command_complete
	      ? com.makelist_with_path( buffer ) 
	      : com.makelist( buffer )
	      );
  
  nfiles += complete_hook(com);
  
  if( nfiles <= 0 ){
    /* �����A�}�b�`����t�@�C�����������āA�����C���h�J�[�h�������g���
     * �Ă���ꍇ�́A���̃��C���h�J�[�h�Ƀ}�b�`����t�@�C�����ꗗ����
     */
    if(    ! has_a_wildcard_letter 
       || (nfiles+=com.makelist_with_wildcard( buffer )) <= 0 )
      return;
  }

  com.sort();
  
  Complete::Cursor cur(com);
  putchr('\n');
  
  int scrnsize[2];
  get_scrsize(scrnsize);

  int files_per_line = 
    scrnsize[0]-1 < com.get_max_name_length()+2
      ? 1 : (scrnsize[0]-1)/(com.get_max_name_length()+2);
  int files_per_column = (nfiles+files_per_line-1)/files_per_line;

  struct filelist **ptr =
    (struct filelist**)alloca(files_per_line*sizeof(struct filelist *));

  for(int i=0 ; i<files_per_line; i++ )
    ptr[i] = NULL;
  
  for(int i=0; i<files_per_line-1 && cur.isOk() ; i++ ){
    ptr[i] = cur.toFileListT();
    for(int j=0 ; cur.isOk() && j<files_per_column ; j++){
      ++cur;
    }
  }
  ptr[files_per_line-1] = cur.toFileListT();
  
  for(int j=0; j<files_per_column ; j++ ){
    for(int i=0; i<files_per_line  &&  ptr[i] != NULL ; i++ ){
      int n=fprintf(fp,"%s%c ",
		    ptr[i]->name,
		    ptr[i]->attr & A_DIR 
		    ? Complete::directory_split_char : ' '
		    );
      if( (i+1) < files_per_line && ptr[i+1] != NULL )
	while( n++ < com.get_max_name_length()+2 )
	  putchr(' ');
      
      ptr[i] = ptr[i]->next;
    }
    putchr('\n');
  }
  re_prompt();
}

void Shell::cls()
{
  fprintf(  fp 
	  , topline_permission ? "\x1B[2J\x1B[H%s":"\x1B[2J\x1B[H\n%s"
	  , prompt );
  int i=0;
  while( i < len )
    putnth( i++ );
  putbs( i - pos );
}

int Shell::ctrl_d_eof=0;

/* �㏑�����[�h�Ƒ}�����[�h�̐؂芷�����s�� 
 * return ��� CONTINUE
 */
Edlin::Status Shell::flip_over(){
  overwrite = !overwrite;

  if( option_vio_cursor_control ){
    VIOCURSORINFO info;
    
    if( this->isOverWrite() ){ /* �㏑�����[�h�̎��͔����T�C�Y */
      info.yStart = (unsigned short)-50;
    }else{			 /* �}�����[�h�̎��̓t���T�C�Y */
      info.yStart = 0;
    }
    info.cEnd   = (unsigned short)-100;
    info.cx = info.attr = 0;
    
    VioSetCurType( &info , 0 );
  }
  return CONTINUE;
}

Edlin::Status Shell::self_insert()
{
  if( (ch >= ' '  &&  ch < 0x100) || ch >= 0x200  ){
    if( overwrite )
      erase();
    Edlin::insert(ch);
    Edlin::forward();
    changed = true;
  }
  return CONTINUE;
}

Edlin::Status Shell::quoted_insert()
{
  Edlin::quoted_insert( getkey() );
  return CONTINUE;
}

Edlin::Status Shell::keyname_insert()
{
  KeyName *name=KeyName::find( getkey() );
  if( name != 0 )
    Edlin::insert_and_forward( name->name );
  return CONTINUE;
}

Edlin::Status Shell::previous_history()
{
  if( cur==NULL || changed ){
    cur = history;
  }else{
    cur=cur->prev;
    while(   cur != NULL
	  && cur->prev != NULL
	  && strcmp(cur->buffer,cur->prev->buffer)==0 )
      cur=cur->prev;
  }
  clean_up();
  if( cur != NULL )
    insert_and_forward(cur->buffer);
  changed = false;
  return CONTINUE;
}

Edlin::Status Shell::next_history()
{
  clean_up();
  if( cur != NULL  && (cur=cur->next) != NULL ){
    while( cur->next != NULL  &&  strcmp(cur->buffer,cur->next->buffer)==0 )
      cur = cur->next;

    insert_and_forward(cur->buffer);
  }
  changed = false;
  return CONTINUE;
}

Edlin::Status Shell::bye()
{
  return QUIT;
}

Edlin::Status Shell::forward_char()
{
  Edlin::forward();
  return CONTINUE;
}
Edlin::Status Shell::backward_char()
{
  Edlin::backward();
  return CONTINUE;
}
Edlin::Status Shell::simple_delete()
{
  erase();
  return CONTINUE;
}

Edlin::Status Shell::tcshlike_ctrl_d()
{
  if( getPos() < getLen() ){
    changed = true;
    erase();
  }else if( getLen() == 0 ){
    return ctrl_d_eof ? QUIT : CONTINUE ;
  }else{
    complete_list();
  }
  return CONTINUE;
}

Edlin::Status Shell::backspace()
{
  backward();
  erase();
  changed = true;
  return CONTINUE;
}
Edlin::Status Shell::tcshlike_complete()
{
  if( prevchar=='\t'  &&  prev_complete_num != 1 ){
    if( getLen() != 0 )
      complete_list();
  }else{
    prev_complete_num = complete();
    changed = true;
  }
  return CONTINUE;
}
Edlin::Status Shell::yaoslike_complete()
{
  prev_complete_num = completeFirst();
  return CONTINUE;
}

Edlin::Status Shell::complete_to_fullpath()
{
  prev_complete_num = Edlin::complete_to_fullpath(NULL);
  return CONTINUE;
}

Edlin::Status Shell::complete_to_url()
{
  prev_complete_num = Edlin::complete_to_fullpath("file:///");
  return CONTINUE;
}

extern int are_spaces(const char *s);

int Shell::regist_history(const char *s)
{
#ifdef NEW_HISTORY
  histories.append(s); /* bindkey.cc �ł̂݁A���p����Ă���悤�� */
#else
  int length;
  const char *text;
  if( s == NULL ){
    text = Edlin::getText();
    length = len;
  }else{
    text = s;
    length = strlen(s);
  }

  /* ------- �q�X�g���ɓo�^���� ---------- */
  History *tmp=(History*)malloc(sizeof(History)+length);
  if( tmp == NULL )
    return -1;
  
  memcpy(tmp->buffer,text,length);
#if 0
  for(int i=0;i<length;i++){
    if( text[i] == '\n' )
      tmp->buffer[i] = ' ';
    else
      tmp->buffer[i] = text[i];
  }
#endif
  tmp->buffer[length] = '\0';

  /* �Â����� prev , �V�������� next�� */
  tmp->prev = history;
  tmp->next = NULL;
  
  if( history != NULL )
    history->next = tmp;
  history = tmp;
  
  /* cur�� NULL �̎��A����̃q�X�g���Q�ƂŁA
   * �ŏ��Ɍ���镶���񂪃g�b�v�ɂȂ�B */
  
  cur = NULL;
  nhistories++;
  return 0;
#endif
}


Edlin::Status Shell::input_terminate()
{
  go_tail();
  int len=getLen();
  if( len <= 0  ||  are_spaces( getText() ) )
    return CANCEL;
  else
    return TERMINATE;
}

/* �V�F���̃q�X�g���ɒǉ�����B
 *	s �q�X�g��������
 * return 0:���� , -1:���s
 */
int Shell::append_history(const char *s)
{
#ifdef NEW_HISTORY
  histories.append(s); /* prepro2.cc �ł̂ݎg�p */
#else
  int len=strlen(s);
  History *tmp=(History*)malloc(sizeof(History)+len);
  if( tmp == NULL )
    return -1;

  strcpy( tmp->buffer , s );
  if( history != NULL )
    history->next = tmp;
  tmp->prev = history;
  tmp->next = NULL;
  history = tmp;
#endif
  return 0;
}


int Shell::replace_last_history(const char *s)
{
#ifdef NEW_HISTORY
  Histories::Cursor cur(histories); /* bindkey.cc �ł̂ݎg�p */
  ++cur; cur->replace(s);
#else
  int len=strlen(s);
  History *tmp=(History*)malloc(sizeof(History)+len);
  if( tmp == NULL )
    return -1;
  
  strcpy( tmp->buffer , s );
  if( history != NULL ){
    tmp->next = history->next;
    if( history->next != NULL )
      history->next->prev = tmp;
    tmp->prev = history->prev;    
    if( history->prev != NULL )
      history->prev->next = tmp;
    free( history );
  }else{
    tmp->next = NULL;
    tmp->prev = NULL;
  }
  history = tmp;
#endif
  return 0;
}


Edlin::Status Shell::repaint()
{
  cls();
  return CONTINUE;
}
Edlin::Status Shell::go_ahead()
{
  Edlin::go_ahead();
  return CONTINUE;
}

Edlin::Status Shell::go_tail()
{
  Edlin::go_tail();
  return CONTINUE;
}

Edlin::Status Shell::cancel()
{
  /* Edlin2::canna_to_alnum(); */
  clean_up();
  changed = false;
  cur = NULL;
  return CONTINUE;
}

Edlin::Status Shell::erasebol()
{
  if( getPos() > 0 ){
    Edlin::erasebol();
    changed = true;
  }
  return CONTINUE;
}

void Shell::paste_to_clipboard(int at,int length)
{
  char *buffer;
  DosAllocSharedMem( (void**)(&buffer) 
		    , NULL
		    , (ULONG)length+1
		    , PAG_COMMIT | PAG_READ | PAG_WRITE 
		    | OBJ_TILE   | OBJ_GIVEABLE );
  
  char *dp=buffer;
  for(int i=at; i<at+length; i++ ){
    if( strbuf[i]=='^' && atrbuf[i]==DBC1ST ){
      *dp++ = strbuf[++i] & 0x1F;
    }else{
      *dp++ = strbuf[i];
    }
  }
  *dp = '\0';

  WinOpenClipbrd(hab);
  WinSetClipbrdOwner(hab,NULLHANDLE);
  WinEmptyClipbrd(hab);
  WinSetClipbrdData( hab, (ULONG)buffer, CF_TEXT, CFI_POINTER );
  WinCloseClipbrd(hab);
}


Edlin::Status Shell::eraseline()
{
  int pos=getPos();
  int len=getLen();
  if( pos < len ){
    paste_to_clipboard( pos , len-pos );
    Edlin::eraseline();
    changed = true;
  }
  return CONTINUE;
}
Edlin::Status Shell::forward_word()
{
  Edlin::forward_word();
  return CONTINUE;
}

Edlin::Status Shell::backward_word()
{
  Edlin::backward_word();
  return CONTINUE;
}

static Shell::History *i_search_core( Shell::History *cur,const char *sekstr
				     ,int &findpos)
{
  for(;;){
    char *findptr;
    if( cur == NULL ){
      putc('\a',stderr);
      return NULL;
    }
    if( (findptr=strstr(cur->buffer,sekstr)) != NULL ){
      findpos = findptr - cur->buffer;
      return cur;
    }
    cur = cur->next;
  }
}
  
static Shell::History *rev_i_search_core(  Shell::History *cur
					 , const char *sekstr
					 , int &findpos )
{
  for(;;){
    char *findptr;
    if( cur == NULL ){
      putc('\a',stderr);
      return NULL;
    }
    if( (findptr=strstr(cur->buffer,sekstr)) != NULL ){
      findpos = findptr - cur->buffer;
      return cur;
    }
    cur = cur->prev;
  }
}

Edlin::Status Shell::search_engine(int isrev=true)
{
  if( history == NULL )
    return CONTINUE;

  /* �C���N�������^���T�[�`���[�h�J�n */
  History *cur=NULL,*tmp=NULL;
  char sekstr[256]="\0";
  int seklen=0;
  int findpos=0;
  
  for(;;){
    message("(%si-search)`%s':%s"
	       ,isrev ? "reverse-" : ""
	       ,sekstr
	       ,(cur==NULL ? "" : cur->buffer) );

    unsigned key=getkey();

    if( key < 0 || key > 0x1FF || *bindmap[key] == &self_insert 
       && isprint(key & 255) ){

      /* ������̒ǉ�(increment) */
      if( key > 0x1ff || key < 0 ){
	sekstr[ seklen++ ] = (key >> 8);
      }
      sekstr[ seklen++ ] = (key & 0xFF);
      sekstr[ seklen   ] = '\0';

      if( isrev ){
	tmp=rev_i_search_core(cur != NULL ? cur : history 
				, sekstr , findpos );
      }else if( cur != NULL ){
	tmp = i_search_core(cur , sekstr , findpos );
      }
      if( tmp != NULL )
	cur = tmp;
      
    }else if( key >= 0 && *bindmap[ key ] == &rev_i_search ){
      isrev = 1;

      if( cur==NULL || cur->prev==NULL )
	tmp = rev_i_search_core(history , sekstr , findpos );
      else
	tmp = rev_i_search_core(cur->prev , sekstr , findpos );
      
      if( tmp != NULL )
	cur = tmp;

    }else if( key >= 0 && *bindmap[ key ] == &i_search ){
      isrev = 0;

      if( cur != NULL  &&  cur->next != NULL )
	tmp = i_search_core(cur->next , sekstr , findpos );
      
      if( tmp != NULL )
	cur = tmp;

    }else{ /* ����ȊO�̋@�\�L�[�̏ꍇ�́A�T�[�`���I������B*/
      if( cur != NULL  &&  key != '\007' ){
	/* ���̓o�b�t�@�̒��Ƀy�[�X�g����B*/
	clean_up();
	insert_and_forward( cur->buffer );
	locate(findpos+seklen);
      }else{
	cleanmsg();
      }
      return (key < 0 || key > numof(bindmap) 
	      || *bindmap[key]==&input_terminate
	      ? CONTINUE
	      : (*bindmap[key])( *this ) );
    }
  }
}

Edlin::Status Shell::i_search()
{
  return search_engine(false);
}

Edlin::Status Shell::rev_i_search()
{
  return search_engine(true);
}


Edlin::Status Shell::copy()
{
  int markpos=getMarkPos();

  if( markpos < 0  || markpos > getLen() )
    return CONTINUE;

  int pos =getPos();

  int at,length;
  if( pos < markpos ){
    at = pos;
    length = markpos - pos;
  }else if( pos > markpos ){
    at = markpos;
    length = pos - markpos;
  }else{
    return CONTINUE;
  }
  paste_to_clipboard( at , length );
  return CONTINUE;
}

Edlin::Status Shell::cut()
{
  Shell::copy();
  Edlin::cut();
  return CONTINUE;
}

Edlin::Status Shell::paste()
{
  /* �N���b�v�{�[�h����擾����e�X�g	*/
  WinOpenClipbrd(hab);
  WinSetClipbrdOwner(hab,NULLHANDLE);
  ULONG	ulFmtInfo;
  if( WinQueryClipbrdFmtInfo( hab, CF_TEXT, &ulFmtInfo ) ){
    char *text = (char*)WinQueryClipbrdData( hab, CF_TEXT );
    if( text != NULL )
      insert_and_forward( text );
  }
  WinCloseClipbrd(hab);
  return CONTINUE;
}

Edlin::Status Shell::marking()
{
  Edlin::marking();
  return CONTINUE;
}

Edlin::Status Shell::paste_test()
{
  /* �N���b�v�{�[�h����擾����e�X�g	*/
  WinOpenClipbrd(hab);
  WinSetClipbrdOwner(hab,NULLHANDLE);
  ULONG	ulFmtInfo;
  if( WinQueryClipbrdFmtInfo( hab, CF_TEXT, &ulFmtInfo ) ){
    char *text = (char*)WinQueryClipbrdData( hab, CF_TEXT );
    if( text != NULL ){
      putchr('\n');
      while( *text ){
	if( *text == '\r' && *(text+1) == '\n' ){
	  putchr(*++text);
	}else if( *text < ' ' && *text >= 0 ){
	  putchr('^');
	  putchr('@'+*text);
	}else
	  putchr(*text);
	++text;
      }
      putchr('\n');
      re_prompt();
    }
  }
  WinCloseClipbrd(hab);
  return CONTINUE;
}

