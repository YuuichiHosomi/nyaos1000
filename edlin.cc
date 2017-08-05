#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/kbdscan.h>

#define INCL_VIO
#include <os2.h>

#include "edlin.h"
#include "complete.h"
#include "macros.h"
#include "keyname.h"
#include "strbuffer.h"

const char *getShellEnv(const char *);

#define KEY(x)	(0x100 | K_##x )
#define CTRL(x)	((x) & 0x1F )

extern void debugger(const char *,...);

Edlin::Edlin()
{
  pos = len = markpos = msgsize = 0;
  max = DEFAULT_BUFFER_SIZE;
  has_marked = false;

  strbuf = (char*)malloc(max);
  atrbuf = (char*)malloc(max);

  /* �����ŁAstrbuf,atrbuf �� NULL ���ۂ��𒲂ׂĂ��Ȃ����A
   * ����͕ϐ��錾����ɁA
   * Edlin edlin;
   * if( !edlin ){
   *    :
   * }�Ƃ����`�Ń��[�U�[���Ɍ��o���Ă��炤
   */
}

Edlin::~Edlin()
{
  if( strbuf ) free(strbuf);
  if( atrbuf ) free(atrbuf);
}

void Edlin::putchrs(const char *s)
{
  while( *s != '\0' )
    putchr(*s++);
}

static char mark_on[32]="\x1B[45m";
static char mark_off[32]="\x1B[40m";

void Edlin::init()
{
  markpos=pos=len=0;
  has_marked = false;
  strbuf[0]=0;
  
  const char *env_markattr = getShellEnv("NYAOSMARKCOLOR");
  if( env_markattr == NULL )
    return;

  char *dempos=strchr(env_markattr,'/');
  if( dempos == NULL )
    return;

  int len_on = dempos-env_markattr;
  snprintf(  mark_on ,sizeof(mark_on) ,"\x1B[%*.*sm"
	   , len_on , len_on , env_markattr);
  snprintf(  mark_off,sizeof(mark_off),"\x1B[%sm"
	   , dempos+1 );
}

void Edlin::putnth(int nth)
{
  if( ! has_marked ){
    putchr( strbuf[nth] );
    return;
  }

  if( nth==markpos  &&  atrbuf[nth]  != DBC2ND )
    putchrs( mark_on );
  
  putchr( strbuf[nth] );
  
  if(   (nth==markpos   && atrbuf[nth] != DBC1ST )
     || (nth==markpos+1 && atrbuf[nth] == DBC2ND ) )
    putchrs( mark_off );
}

void Edlin::marking(void)
{
  bool prev_has_marked = has_marked;
  int prev_markpos = markpos;

  has_marked = true;
  markpos = pos;
  
  if( prev_has_marked  &&  prev_markpos < pos ){
    /* �O��̃}�[�N���J�[�\���ʒu��荶�ɂ���ꍇ�ɁA�}�[�N������ */
    putbs( pos-prev_markpos );
    for(int i=prev_markpos ; i<pos ; i++ )
      putchr( strbuf[i] );
  }
  /* �}�[�N��\��(�����I�Ɏ��s���Ă��Ȃ����Aputnth ���������f���Ă����) */
  putnth(pos);
  if( atrbuf[pos] == SBC ){
    putbs(1);
  }else{
    putnth(pos+1);
    putbs(2);
  }
  /* �O��̃}�[�N���J�[�\���ʒu���E�ɂ���ꍇ�ɁA�}�[�N������ */
  if( prev_has_marked  &&  prev_markpos > pos )
    after_repaint(0);
}

/* at �̈ʒu�� bytes �������̃X�y�[�X���m�ۂ���B
 * �󔒕���������킯�ł͂Ȃ��A��Ԃ����Ƃ����Ӗ��B
 * �߂�l��K�����Ȃ��ƁA�I�[�o�[�t���[/�A���_�[�t���[�����m�ł��Ȃ��B
 *	at	�X�y�[�X���쐬����ʒu
 *	bytes	�X�y�[�X�̃T�C�Y(�o�C�g)�B���̒l�ł��悢�B
 * return 0:����  -1:���s(�I�[�o�[�t���[/�A���_�[�t���[)
 */
int Edlin::makeRoom(int at,int bytes)
{
  if( bytes > 0 ){
    while( len+bytes >= max ){
      char *new_strbuf=(char*)realloc(strbuf,max*2);
      if( new_strbuf == NULL )
	return -1;
      char *new_atrbuf=(char*)realloc(atrbuf,max*2);
      if( new_atrbuf == NULL ){
	free(new_strbuf);
	return -1;
      }
      strbuf = new_strbuf;
      atrbuf = new_atrbuf;
      max *= 2;
    }
    if( markpos > at )
      markpos += bytes;

    strbuf[ len+bytes ] = '\0';
    atrbuf[ len+bytes ] = SBC;
    for(int i=len-1 ; i >= at ; i-- ){
      strbuf[ i+bytes ] = strbuf[ i ];
      atrbuf[ i+bytes ] = atrbuf[ i ];
    }
  }else if( bytes < 0 ){
    if( at+(-bytes) > len )
      return -1;
    
    if( markpos > at ){
      if( markpos >= at+(-bytes) ){
	/* �}�[�N���폜�͈͂��E�Ȃ�A���֍폜�o�C�g������� */
	markpos -= (-bytes);
      }else{
	/* �}�[�N���폜�͈͓��Ȃ�A�폜�͈͂̐擪�Ɉړ����� */
	markpos = at;
      }
    }

    for(int i=at ; i<len+bytes ; i++ ){
      strbuf[ i ] = strbuf[ i+(-bytes) ];
      atrbuf[ i ] = atrbuf[ i+(-bytes) ];
    }
    strbuf[ len+bytes ] = '\0';
    atrbuf[ len+bytes ] = SBC;
  }
  len += bytes;
  return 0;
}

int Edlin::complete_tail_char='\\';

/* tcsh �� C-t �ɑ������鏈�����s���B
 * �J�[�\�����O�̓񕶎������ς���B
 */
void Edlin::swapchars()  /* DOS���[�h���Ή����\�b�h */
{
  if( pos < len )
    forward();

  if( pos < 2 ) return;
  
  if( atrbuf[pos-1]==SBC ){
    if( atrbuf[pos-2] == SBC ){
      /* ���p���p */
      int tmp=strbuf[pos-2];
      putbs( 2 );
      strbuf[pos-2] = strbuf[pos-1]; putnth(pos-2);
      strbuf[pos-1] = tmp;           putnth(pos-1);
    }else{
      if( markpos == pos-1 ) /* �S�p�� 2byte�ڂɃ}�[�N���ړ����Ȃ��悤�� */
	markpos = pos-2;

      /* �S�p���p -> ���p�S�p */
      int tmp1=strbuf[pos-3];
      int tmp2=strbuf[pos-2];
      putbs( 3 );

      strbuf[pos-3] = strbuf[pos-1];
      atrbuf[pos-3] = SBC;
      putnth(pos-3);

      strbuf[pos-2] = tmp1;
      atrbuf[pos-2] = DBC1ST;
      putnth(pos-2);

      strbuf[pos-1] = tmp2;
      atrbuf[pos-1] = DBC2ND;
      putnth(pos-1);
    }
  }else if( pos >= 3 ){
    if( atrbuf[pos-3] == SBC ){
      if( markpos == pos-2 ) /* �S�p�� 2byte�ڂɃ}�[�N���ړ����Ȃ��悤�� */
	markpos = pos-1;

      /* ���p�S�p -> �S�p���p */
      int tmp=strbuf[pos-3];
      putbs( 3 );
      strbuf[pos-3] = strbuf[pos-2];
      atrbuf[pos-3] = DBC1ST;
      putnth( pos-3 );
      
      strbuf[pos-2] = strbuf[pos-1];
      atrbuf[pos-2] = DBC2ND;
      putnth( pos-2 );

      strbuf[pos-1] = tmp;
      atrbuf[pos-1] = SBC;
      putnth( pos-1 );
    }else{
      /* �S�p�S�p */
      int tmp1=strbuf[pos-4];
      int tmp2=strbuf[pos-3];
      putbs(4);
      strbuf[pos-4] = strbuf[pos-2]; putnth(pos-4);
      strbuf[pos-3] = strbuf[pos-1]; putnth(pos-3);
      strbuf[pos-2] = tmp1;          putnth(pos-2);
      strbuf[pos-1] = tmp2;          putnth(pos-1);
    }
  }
}

/* �ꕶ���}�����āA�����ɕ\���ɔ��f������B
 * 0x0000 �` 0x00FF : SBCS �Ƃ݂Ȃ��B
 * 0x0100 �` 0x01FF : ��������(�}�����Ȃ�)�B
 * 0x0200 �` 0xFFFF : DBCS �Ƃ݂Ȃ��B
 */
void Edlin::insert(int ch)
{
  if( (unsigned)ch > 0x1FF ){	/* DBCS */
    if( makeRoom(pos,2) != 0 )
      return;
    
    strbuf[ pos   ] = ch >> 8 ;
    atrbuf[ pos   ] = DBC1ST;
    strbuf[ pos+1 ] = ch & 255;
    atrbuf[ pos+1 ] = DBC2ND;
  }else if( (unsigned)ch < 0x100 ){ /* SBCS */
    if( makeRoom(pos,1) != 0 )
      return;
    
    strbuf[pos] = ch;
    atrbuf[pos] = SBC;
  }else{
    return;
  }
  after_repaint(0);  /* �}�������Ƃ��́A�E�֓����̂Ŗ��[�̃N���A�͂���Ȃ� */
}

/* ���䕶���� 2bytes �������� strlen�B
 * �R�s�[����ۂɁu^H�v�Ƃ����`�ɂ���ׁA�{�֐����K�v�B
 *	s ������
 * return ������
 */
static int strlen2(const char *s)
{
  int len=0;
  while( *s ){
    if( 0 < *s  &&  *s < ' ' )
      len++;
    len++;
    s++;
  }
  return len;
}

/* ��������J�[�\���ʒu�֑}�����A�\�����X�V����B
 * �J�[�\���͈ړ����Ȃ��B
 */
void Edlin::insert_and_forward(const char *s)
{
  if( makeRoom(pos,strlen2(s)) != 0 )
    return;
  
  while( *s != '\0' ){
    if( is_kanji(*s & 255) ){
      writeDBChar( *s , *(s+1) );
      s+=2;
    }else if( 0 < *s && *s < ' ' ){
      writeDBChar( '^' , *s++ + '@' );
    }else{
      writeSBChar( *s++ );
    }
  }
  after_repaint(0);  /* �}�������Ƃ��́A�E�֓����̂Ŗ��[�̃N���A�͂���Ȃ� */
}

/* �R���g���[���L�����N�^����͂���ׂ̃��\�b�h
 *	ch �c �L�����N�^�[�R�[�h
 */
void Edlin::quoted_insert(int key)
{
  if( key >= 0x200 ){		/* �{�p���� */
    if( makeRoom(pos,2) != 0 )
      return;
    writeDBChar( (key>>8)& 0xFF , key & 0xFF );
    
  }else if( key >= 0x100 ){	/* ���䕶���ł��L�����R�[�h�������Ȃ����� */
    return;
  }else if( key >= 0x20 ){	/* ���p���� */
    if( makeRoom(pos,1) != 0 )
      return;
    writeSBChar( key );
  }else if( key >= 0 ){		/* ���䕶�� */
    if( makeRoom(pos,2) != 0 )
      return;
    writeDBChar( '^' , key + '@' );
  }else{
    return;
  }
  after_repaint(0);
}

/* C-v �ɂ���ē��͂��ꂽ���䕶����{����1byte�`���ɕϊ�����B
 * �����I�ɂ�
 *	strbuf ... '^',('@'+key)
 *	atrbuf ... DBC1ST,DBC2ND
 * �Ɣ{�p���������ɂȂ��Ă���B
 * ���̃��\�b�h�͕ҏW�I�����ɂ̂݌ĂԁB
 * ����Ȍ�̕ҏW�͐��������삵�Ȃ��B
 */
void Edlin::pack()
{
  int si=0,di=0;
  int oldLength=len;

  while( si < oldLength ){
    if( strbuf[si] == '^'  &&  atrbuf[si]==DBC1ST  ){
      strbuf[di] = strbuf[++si] & 0x1F;
      atrbuf[di] = SBC;
      --len;
    }else{
      strbuf[di] = strbuf[si];
      atrbuf[di] = atrbuf[si];
    }
    ++si;  ++di;
  }
  strbuf[di] = '\0';
}

/* �J�[�\���ʒu�̖��O�̐擪�ʒu�����߂�B
 * �������A���O�́A�⊮���̃t�@�C������O��Ƃ��Ă���̂ŁA
 * �u<�v��u;�v�̒�����P��擪�Ƃ݂Ȃ��B
 *	return �擪�ʒu
 */
int Edlin::seek_word_top()
{
  int wrdtop=0;
  int p=0;

  const char *punct=getShellEnv("NOTPATHCHAR");
  
  for(;;){
    // �󔒂�ǂ݂Ƃ΂��B
    while( isspace(strbuf[p] & 255) ){
      if( p >= pos ){
	return wrdtop;
      }
      if( atrbuf[p] != SBC )
	p++;
      p++;
    }
    // �P�ꋫ�E��ݒ肷��B 
    if(  strbuf[p]=='<' || strbuf[p]=='>' 
       || ( punct != NULL && strchr(punct,strbuf[p]) != NULL ) ){
      ++p;
    }
    wrdtop = p;
    
    // �󔒈ȊO��ǂ݂Ƃ΂��B 
    while( !isspace(strbuf[p] & 255) ){
      if( p >= pos )
	return wrdtop;

      // �u+�v��u;�v�̒�����P�ꋫ�E�Ƃ݂Ȃ���̂ŁAwrdtop ���X�V����B 
      if(   (punct != NULL  && strchr(punct,strbuf[p]) != NULL )
	 && strbuf[p+1] != '\0' ){
	wrdtop = ++p;
	continue;
      }

      if( strbuf[p] == '"' ){
	do{
	  if( atrbuf[p] != SBC )
	    p++;
	  p++;
	  if( p >= pos )
	    return wrdtop;
	}while( strbuf[p] != '"' );
      }
      if( atrbuf[p] != SBC )
	p++;
      p++;
    }
  }
}

Edlin::CompleteFunc Edlin::completeBindmap[ 0x200 ];

/* �ϊ��^�⊮�́A�L�[�o�C���h������������B
 */
void Edlin::initComplete()
{
  static int firstcalled=1;
  if( ! firstcalled  )
    return;

  firstcalled = 0;

  for(unsigned int i=0;i<numof(completeBindmap);i++)
    completeBindmap[ i ] = COMPLETE_FIX_PLUS;
  
  struct{
    int key;
    CompleteFunc func;
  } defaultBindmap[] = {
    { CTRL('G')			, COMPLETE_CANCEL },
    { CTRL('[')			, COMPLETE_CANCEL },
    { KEY(LEFT)			, COMPLETE_CANCEL },

    { KEY(UP)			, COMPLETE_PREV },
    { KEY(ALT_BACKSPACE)	, COMPLETE_PREV },
    
    { KEY(DOWN)			, COMPLETE_NEXT },
    { KEY(CTRL_TAB)		, COMPLETE_NEXT },
    { KEY(ALT_RETURN)		, COMPLETE_NEXT },
    { '\t'			, COMPLETE_NEXT },

    { KEY(RIGHT)		, COMPLETE_FIX },
    { '\r'			, COMPLETE_FIX },
    { '\n'			, COMPLETE_FIX },
  };
  
  for(unsigned int i=0;i<numof(defaultBindmap);i++){
    completeBindmap[ defaultBindmap[i].key ] = defaultBindmap[i].func;
  }
}

static struct CompleteFuncName {
  const char *name;
  Edlin::CompleteFunc func;
} completeFuncName[] = {
  { "complete_cancel"	, Edlin::COMPLETE_CANCEL },
  { "complete_fix"	, Edlin::COMPLETE_FIX },
  { "complete_next"	, Edlin::COMPLETE_NEXT },
  { "complete_prev"	, Edlin::COMPLETE_PREV },
  { "complete_default"	, Edlin::COMPLETE_FIX_PLUS },
};

/* �⊮���[�h���̃L�[�o�C���h��ݒ肷��(�ÓI�����o�֐�)
 *	key	�L�[���̕�����
 *	func	�@�\���̕�����
 * return  0:����I�� 1:�L�[���̕s�K 2:�@�\���̕s�K
 */
int Edlin::bindCompleteKey(const char *key,const char *func )
{
  initComplete();

  KeyName *keyinfo=KeyName::find(key);
  if( keyinfo == NULL )
    return 1;

  CompleteFuncName *funcPtr
    = (CompleteFuncName*)bsearch(  func
				 , completeFuncName
				 , numof(completeFuncName)
				 , sizeof(completeFuncName[0])
				 , &KeyName::compareWithTop );
  if( funcPtr == NULL )
    return 2;

  completeBindmap[ keyinfo->code ] = funcPtr->func;
  return 0;
}

/* �ϊ��^�̕⊮
 * return 0:�⊮���Ȃ����� 1:�⊮����
 */
int Edlin::completeFirst()
{
  initComplete();

  /* ���t�@�C����(�⊮�O�̃t�@�C����)�̐擪�ʒu�E���������߂Ă��� */
  int fntop=seek_word_top();
  int basesize=pos-fntop;

  /* ��⃊�X�g�񋓃I�u�W�F�N�g�Ɛl�͌��� */
  Complete com;

  /* ���t�@�C�������s�̐擪�Ȃ�΁A����̓R�}���h���⊮�Ɛl�͌��� */
  bool command_complete = (fntop <= 1);

  /* ���t�@�C�������N�H�[�g�ň͂܂�Ă��邩�ۂ�
   * �͂܂�Ă��Ȃ���΁A��X�A�ꍇ�ɂ���Ă�
   * �͂�������Ƃ��K�v�ƂȂ�킯���B
   */
  bool quoted=false;
  if( strbuf[fntop] == '"' ){
    fntop++;
    basesize--;
    quoted = true;
  }

  /* �⊮�O�̃t�@�C�������ȉ��u���t�@�C�����v�Ƃ���B
   *   fntop    �c ���t�@�C�����̐擪(�N�H�[�g���܂܂Ȃ�)
   *   basesize �c ���t�@�C�����̒���(�N�H�[�g���܂܂Ȃ�)
   */

  /* �t�@�C�������A\0 �ŏI���`�փR���o�[�g */
  char *buffer=(char*)alloca(basesize+1);
  memcpy( buffer , &strbuf[fntop] , pos-fntop );
  buffer[ pos-fntop ] = '\0';
  
  /* �⊮���X�g�����B
   * �t���O�ɂ���āA�R�}���h���⊮���A�t�@�C�����⊮����I������
   */
  int nfiles = (  command_complete 
		? com.makelist_with_path( buffer )
		: com.makelist( buffer ) );

  /* �����R�}���h�̖��O�Ȃǂ��⊮���X�g�ɉ����Ă��� 
   */
  nfiles += complete_hook(com);
  
  /* ��₪�������A���O���Ƀ��C���h�J�[�h���܂܂�Ă���ꍇ�́A
   * ���C���h�J�[�h�Ƀ}�b�`����t�@�C������⊮���X�g�։�����
   * �����Ȃ���΁A�����܂��B
   */
  if( nfiles <= 0 ){
    if(    strpbrk(buffer,"*?") == NULL
       || (nfiles+=com.makelist_with_wildcard( buffer )) <= 0 ){
      alert();
      return 0;
    }
  }
  com.sort();

  /* basesize �́A���t���p�X���̒����ƂȂ�B
   * com.get_fname_common_length() �́A�t�@�C�����̋��ʕ����̒����B
   */
  backward( com.get_fname_common_length() );

  /* �t�@�C��������A�\�����Ă䂭���[�v�c����ᕪ������� */
  Complete::Cursor cur(com);
  for(;;){
    if( cur->attr & A_DIR ){
      message(  "%s%c"
	      , cur->name
	      , com.get_split_char() ?: complete_tail_char );
    }else{
      message("%s",cur->name );
    }
    CompleteFunc completeFunc;
    unsigned int key=::getkey();
    if( key >= numof(completeBindmap) )
      completeFunc = COMPLETE_FIX_PLUS;
    else
      completeFunc = completeBindmap[ key ];
    
    switch( completeFunc ){
    case COMPLETE_CANCEL:
      cleanmsg();
      forward( com.get_fname_common_length() );
      
      return 0;
      
    case COMPLETE_PREV:
      cur.sotomawari();
      break;
      
    case COMPLETE_NEXT:
      cur.utimawari();
      break;
      
    default:
      ::ungetkey(key);
	/* continue to next case */
      
    case COMPLETE_FIX:
      cleanmsg();
      
      if( !quoted && strpbrk( cur->name , " ^!") != NULL ){
	insert('"');
	quoted = true;
	forward();
      }
      /* �⊮�̃x�[�X��������A�啶���E�����������킹�邽�߂�
       * �㏑�����s�� */
      const char *sp=cur->name;
      for(int i=0;i<com.get_fname_common_length();i++ ){
	strbuf[pos] = *sp++; putnth(pos++);
      }
      
      insert_and_forward( sp );
      
      if( cur->attr & A_DIR ){
	insert( com.get_split_char() ?: complete_tail_char );
	forward();
      }
      if( quoted ){
	insert('"');
	forward();
      }
      return 1;
    } /* end-switch */
  }/* end-for(;;) */
}

int Edlin::complete()
{
  int fntop=seek_word_top();
  int basesize=pos-fntop;

  Complete com;

  int  command_complete = (fntop <= 1);
  int  quoted=false;
  
  if( strbuf[fntop] == '"' ){
    fntop++;
    basesize--;
    quoted = 1;
  }
  /* �t�@�C�������A\0 �ŏI���`�փR���o�[�g */
  char *buffer=(char*)alloca(basesize+1);
  memcpy( buffer , &strbuf[fntop] , pos-fntop );
  buffer[ pos-fntop ] = '\0';

  /* �⊮���X�g�����B
   * �t���O�ɂ���āA�R�}���h���⊮���A�t�@�C�����⊮����I������
   */

  int nfiles = (  command_complete
		? com.makelist_with_path( buffer ) 
		: com.makelist( buffer ) );

  nfiles += complete_hook(com);

  if( nfiles <= 0 ){
    alert();
    return 0;
  }

  com.sort();

  backward( basesize );
  
  const char *nextstr=com.nextchar();

  if( !quoted && strpbrk(nextstr," ^!") != NULL ){
    insert('"');
    quoted = 1;
    forward();
  }

  forward( basesize-com.get_fname_common_length() );

  const char *realname=com.get_real_name1();
  for(int i=0 ; i<com.get_fname_common_length(); i++ ){
    strbuf[pos] = *realname++;
    putnth(pos++);
  }

  insert_and_forward(nextstr);

  if( nfiles == 1 ){
    Complete::Cursor cursor(com);
    
    if( cursor->attr & A_DIR ){
      insert( com.get_split_char() ?: complete_tail_char );
    }else{
      if( quoted ){
	insert('"');
	forward(); 
      }
      insert(' ');
    }
    forward();
  }
  return nfiles;
}

/* �t���p�X�ϊ��Ƃ����z�B
 * ����Ȃ񂩁A�V�F���N���X(Shell)�ɓ��ꂽ�����悳�����ȁc)
 */
int Edlin::complete_to_fullpath(const char *header)
{
  /* �unyaos ���v�̂悤�ɊԂɋ󔒂�����ꍇ�ɁA
   * ���̋󔒂𖳎�����킯�� */
  int spaces=0;
  if( strbuf[pos-1] == ' ' )
    spaces=backward();

  int fntop=seek_word_top();
  int basesize=pos-fntop;
  bool quoted=false;
  
  StrBuffer sbuf;
  // char *buffer=(char*)alloca(basesize*3);

  if( strbuf[fntop] == '"' ){
    fntop++;
    basesize--;
    quoted = true;
  }
  // char *bp=buffer;
  /* ���[�J���o�b�t�@�Ɍ��t�@�C������W�J����B
   * ���̎��A�`���_�� ... ���W�J����B
   */
  if( strbuf[fntop] == '~' ){
    const char *home=getShellEnv("HOME");
    if( home != NULL ){
      // ++bp;
      while( *home != '\0' ){
	// *bp++ = *home++;
	sbuf << *home++;
      }
      ++fntop; // skip tilda.
    }
  }else if( strbuf[fntop]=='.' && strbuf[fntop+1]=='.' ){
    // *bp++ = strbuf[fntop++];
    // *bp++ = strbuf[fntop++];
    sbuf << strbuf[fntop++];
    sbuf << strbuf[fntop++];
    while( strbuf[fntop] == '.' ){
      sbuf << "\\..";
      // *bp++ = '\\';
      // *bp++ = '.';
      // *bp++ = '.';
      ++fntop;
    }
  }
  while( fntop < pos ){
    sbuf << strbuf[fntop++];
    // *bp++ = strbuf[fntop++];
  }
  // *bp = '\0';

  // �t���p�X�𓾂�B�����Ȃ�������A�I��
  char fullpath[ FILENAME_MAX ];
  if( _fullpath( fullpath , (const char *)sbuf , sizeof(fullpath) ) != 0 ){
    while( spaces > 0 )
      spaces -= forward();
    return 0;
  }

  int len_fullpath = strlen(fullpath);
  int delta = len_fullpath-basesize;

  // �t���p�X�̒������o�b�t�@�Ɏ��܂�Ȃ��ꍇ���I��
  if( max-len <= delta-3 ){
    while( spaces > 0 )
      spaces -= forward();
    return 0;
  }
  
  // �J�[�\����P��擪�ֈړ�
  for(int i=0 ; i<basesize ; )
    i += backward();

  // �S�̒����𒲐�
  if( makeRoom(pos,delta) != 0 )
    return 0;
  
  // �P��擪�Ɂu�h�v����������ǂ��u�h�v�ň͂܂Ȃ��Ă͂����Ȃ�����������
  // �ꍇ�A�����Łu�h�v��������B
  // header �� NULL �Ŗ����ꍇ�́A| �� : �̑���ɓ����Ă���̂ŕK�{�ƂȂ�B
  if( !quoted && (strpbrk(fullpath," ^!") != NULL || header != NULL ) ){
    insert('"');
    quoted = 1;
    forward();
  }

  // URL �^�ɂ���ꍇ�Ȃǂ̏���
  if( header != NULL )
    insert_and_forward(header);
  
  // �V�����p�X����������
  for(int i=0 ; i<len_fullpath ; i++ ){
    if( is_kanji(fullpath[ i ] ) ){
      writeDBChar( fullpath[i] , fullpath[i+1] );
      ++i;
    }else{
      writeSBChar( fullpath[i] );
    }
  }

  // �V�����p�X�ȍ~�̕�����������ŕ\��
  after_repaint(delta >= 0 ? 0 : -delta );
  
  if( header != NULL ){
    insert('"');
    forward();
  }

  while( spaces > 0 )
    spaces -= forward();
  return 1;
}

#if 0
void Edlin::insert(int ch1,int ch2)
{
  if( makeRoom(pos,2) != 0 )
    return;

  strbuf[ pos   ] = ch1;
  atrbuf[ pos   ] = DBC1ST;
  strbuf[ pos+1 ] = ch2;
  atrbuf[ pos+1 ] = DBC2ND;
  
  after_repaint(0);
}
#endif

void Edlin::erase()
{
  /* �����ł͓��삹�� */
  if( pos >= len )
    return;

  int ndels=(atrbuf[pos]==SBC ? 1 : 2);
  
  if( makeRoom(pos,-ndels) != 0 )
    return;

  after_repaint(ndels);
}

void Edlin::repaint(int termclear)
{
  /* �\�����Ă���ꕶ���ڂ܂ŃJ�[�\����߂� */
  putbs( pos );

  int i=0;
  while( i < len )
    putnth( i++ );
  
  if( has_marked  &&  markpos == len  ){
    putchrs(mark_on);
    putchr(' ');
    putchrs(mark_off);
  }else{
    putchr(' ');
    --termclear;
  }
  ++i;

  if( termclear >= 0 ){
    while( termclear-- > 0 ){
      putchr( ' ' );
      i++;
    }
  }else{
    putel();
  }
  putbs( i - pos );
}

void Edlin::after_repaint(int termclear)
{
  int i=0;
  if( pos < len ){
    while( pos+i < len )
      putnth( pos+i++ );
  }

  if( has_marked  &&  markpos == len ){
    putchrs(mark_on);
    putchr(' ');
    putchrs(mark_off);
  }else{
    putchr(' ');
    --termclear;
  }
  ++i;

  if( termclear >= 0 ){
    while( termclear-- > 0 ){
      putchr( ' ' );
      i++;
    }
  }else{
    putel();
  }
  putbs( i );
}
/* cmd.exe��keys on����Ctrl-Home���l�ɁA�J�[�\���ʒu��O����s���������B
 * �f�B�t�H���g�ł̓L�[�o�C���h���Ȃ��B����������ɂ́A�Ⴆ�΁A
 *	bindkey CTRL_END  kill_line
 *	bindkey CTRL_HOME kill_top_of_line
 */
void Edlin::erasebol()
{
  if( pos == 0 )
    return;

  int i=pos;  
  if( makeRoom(0,-pos) != 0 )
    return;

  pos = 0;
  putbs(i);
  after_repaint(i);
}

void Edlin::eraseline()
{
  /* 2�s�ɂ܂�����ꍇ�AEraseline�R�[�h��1�s�����������Ȃ� */
  int i=0;
  while( pos+i < len+1 ){ /* '+1' �́A�����̃}�[�N�̈� */
    putchr(' ');
    i++;
  }
  putbs(i);

  len = pos ;
  if( markpos > pos ){
    markpos = pos;
    putchrs(mark_on);
    putchr(' ');
    putchrs(mark_off);
    putbs(1);
  }

  strbuf[ pos ] = '\0';
  atrbuf[ pos ] = SBC;
}

void Edlin::forward_word()
{
  int nextpos = pos;
  /* �P��̓ǂݔ�΂� */
  while( nextpos < len  &&  !isspace(strbuf[nextpos] & 255) )
    ++nextpos;

  /* �󔒂̓ǂݔ�΂� */
  while( nextpos < len  &&  isspace(strbuf[nextpos] & 255) )
    ++nextpos;
  
  while( pos < nextpos )
    putnth( pos++ );
}

void Edlin::backward_word()
{
  int nextpos=pos;
  /* �󔒂̓ǂݔ�΂� */
  while( nextpos > 0  &&  is_space(strbuf[--nextpos]) )
    ;
  /* ��󔒕����̓ǂݔ�΂� */
  while( nextpos > 0  &&  !is_space(strbuf[nextpos-1]) )
    --nextpos;

  putbs( pos-nextpos );
  pos = nextpos;
}

int Edlin::forward()
{
  /* ���������̓�x�ł��ɂ��E�ړ� */
  if( pos+1 <= len  &&  atrbuf[pos] != DBC1ST ){
    /* ���p */
    putnth( pos++ );
    return 1;
  }else if( pos+2 <= len ){
    /* �S�p */
    putnth( pos++ );
    putnth( pos++ );
    return 2;
  }
  return 0;
}

int Edlin::forward(int w)
{
  int i;
  for(i=0 ; i<w ; i+=forward() ){
    if( pos >= len )
      break;
  }
  return i;
}

int Edlin::backward(int w)
{
  int i;
  for(i=0 ; i<w ; i+=backward() ){
    if( pos <= 0 )
      break;
  }
  return i;
}


int Edlin::backward()
{
  if( 0 < pos  &&  atrbuf[pos-1] == SBC ){
    --pos;
    putbs(1);
    return 1;
  }else if( 2 <= pos ){
    pos -= 2;
    putbs(2);
    return 2;
  }
  return 0;
}

Edlin::Status Edlin::go_ahead()
{
  putbs( pos );
  pos = 0;
  return CONTINUE;
}

Edlin::Status Edlin::go_tail()
{
  /* �S�����񂪁A��ʒ��ɂłĂ���ꍇ�A�E�ړ������ł悢 */
  while( pos < len )
    putnth( pos++ );
  return CONTINUE;
}

void Edlin::clean_up()
{
  int cleaning_size;
  if( msgsize != 0 ){
    cleaning_size = msgsize;
    putbs( msgsize );
    msgsize = 0;
  }else{
    putbs( pos );
    cleaning_size = len;
    if( has_marked )
      ++cleaning_size;
  }

  for(int i=0; i<cleaning_size ; i++ )
    putchr(' ');
  
  putbs( cleaning_size );
  markpos = len = pos = 0;
  has_marked = false;
  strbuf[ 0 ] = '\0';
  atrbuf[ 0 ] = SBC;
}

int Edlin::message(const char *fmt,...) /* �E�C���h�E���[�h���Ή� */
{
  char msg[1024];
  va_list vp;
  va_start(vp,fmt);

  /* msgsize �͈ȑO�ɕ\���������b�Z�[�W�̒��� */
  if( msgsize > 0 )
    putbs(msgsize);

  (void)vsnprintf(msg,sizeof(msg),fmt,vp);  
  va_end(vp);

  int columns=0; /* ���ۂ̕\������(�G�X�P�[�v�V�[�P���X����������) */
  int escape=0;  /* �G�X�P�[�v�V�[�P���X���Ȃ� not 0 */

  /* ���b�Z�[�W�{�̂�\�� */
  for(const char *sp=msg ; *sp != '\0' ; sp++ ){
    if( *sp == '\x1b' )
      escape = 1;
    if( ! escape )
      columns++;
    if( isalpha(*sp & 255) )
      escape = 0;

    if( 0 < *sp && *sp < ' '  &&  ! escape ){
      putchr('^');
      putchr('@'+*sp);
      columns++;
    }else{
      putchr(*sp);
    }
  }

  /* �ߋ��̃��b�Z�[�W�̖������폜 */
  if( msgsize > columns ){
    if( pos+columns < len  &&  atrbuf[pos+columns] != DBC2ND )
      putnth( pos+columns );
    else
      putchr( ' ' );
    
    for(int i=columns+1 ; i < msgsize ; i++ ){
      if( pos+i < len  )
	putnth( pos+i );
      else
	putchr(' ');
    }
    putbs( msgsize - columns );
  }
  msgsize = columns;
  
  fflush(stdout);
  return len;
}

void Edlin::cleanmsg() /* �E�C���h�E���[�h���Ή� */
{
  /* �ꎞ�I�ɕ\�����Ă������b�Z�[�W���������A
     �{���\�����ׂ��A���͕�������ĕ\������ */
  
  if( msgsize > 0 ){
    putbs( msgsize );
    
    int i=0;
    while( pos+i < len  &&  i<msgsize ){
      if( atrbuf[pos+i] != SBC )
	putnth( pos+i++ );
      putnth( pos+i++ );
    }

    while( i < msgsize ){
      putchr(' ');
      i++;
    }
    putbs( i );
  }
  msgsize = 0;
}

void Edlin::bottom_message( const char *fmt ,...)
{
  extern int screen_width , screen_height , option_vio_cursor_control;
  int bs=0;

  if( option_vio_cursor_control ){
    unsigned short X,Y;

    VioGetCurPos( &Y , &X , 0 );
    if( Y >= screen_height-1 ){
      static BYTE cell[2]={ ' ',0x00 };
      VioScrollUp(0,0,screen_height-1,screen_width-1,1, cell , 0);
      fputs("\033[1A",stdout);
    }

  }else{
    /* ��ʃT�C�Y���J�[�\����i�߂邱�Ƃɂ���āA
     * ���̍s�ֈړ�����B
     */
    
    if( pos+msgsize < len && atrbuf[pos+msgsize] == DBC2ND ){
      putnth( pos+msgsize );
      ++bs;
    }
    
    for( ; bs < screen_width ; bs++ ){
      if( pos+msgsize+bs < len )
	putnth( pos+msgsize+bs );
      else
	putchr( ' ' );
    }    
    fflush(stdout);
  }
  printf("\033[s\033[%d;1H" , screen_height );
  
  va_list vp;
  va_start(vp,fmt);
  vprintf(fmt,vp);
  va_end(vp);

  printf("\033[K\033[u");
  fflush(stderr);

  if( ! option_vio_cursor_control )
    putbs( bs );

  bottom_msgsize = 1;
}

void Edlin::clean_bottom()
{
  extern int screen_height;
  
  if( bottom_msgsize > 0 ){
    printf( "\033[s\033[%d;1H\033[K\033[u" , screen_height );
    bottom_msgsize = 0;
  }
}

void Edlin::cut()
{
  int at,length;

  if( pos < markpos ){
    at = pos;
    length = markpos - pos;
    markpos = pos;
  }else if( pos > markpos ){
    at = markpos;
    length = pos - markpos;
    putbs( length );
    pos = markpos;
  }else{
    return;
  }
  makeRoom( at , -length );
  after_repaint( length );
}

void Edlin::locate(int x)
{
  if( x > pos ){
    while( pos < x )
      putnth( pos++ );
  }else if( x < pos ){
    putbs( pos-x );
  }
  pos = x;
}
