/* -*- C++ -*-
 * Vz Editor like �� �q�X�g���Q�Ƃ��s���B
 */

#include <assert.h>
#include <alloca.h>
#include <ctype.h>
#include "edlin.h"
#include "quoteflag.h"

//#define DEBUG1(x) (x)
#define DEBUG1(x) /**/

extern int option_single_quote;

struct WHist{
  WHist *prev,*next;
  const char *buffer;
};

Edlin::Status Shell::vz_next_history()
{
  return CONTINUE;
}

/* ���̒P���\�����āA���[�U�[�Ɏw���𐿂��B
 * �m��L�[�������ꂽ��A�P��̎c�蕔���̒ǉ������ۂ�
 * �s���B
 *
 *	tmp		���̒P���ێ����郊�X�g�v�f
 *	rightmove	�J�[�\���ʒu����A���͂����P�ꖖ���܂ł̒���
 * return
 *	0	�m�肵��/���~�����B
 *	1	�{�P��͋p�����ꂽ�B���̒P���f�ŁA�v������B
 */
static int history_core(WHist *tmp,int rightmove,Edlin &ed)
{
  /* �J�[�\�����E�ֈړ������邽�߂̕�������쐬����B
   * �G�X�P�[�v�V�[�P���X�ɂ�炸�A�J�[�\�����ʉ߂���ʒu��
   * ��������ĕ`�悷�邱�Ƃɂ���āA�E�ֈړ������邽�߁B
   *
   * message �֐��́AFEP �̃C�����C���ϊ��̂悤�ɁA
   * ���͕�����̏�ɁA�ʂ̕�������d�˂ĕ\�������A
   * cleanmsg �֐��ɂ���āA���̕�������񕜂�����悤��
   * �Ȃ��Ă���B
   *
   * ���ׂ̈ɁAmessage �֐��ɂāA�㏑������������̈ʒu
   * �ƒ������L�^���Ă���̂����A�G�X�P�[�v�V�[�P���X��
   * �g�����̂ł́A���ꂪ������Ȃ��Ȃ��Ă��܂��킯���B
   *
   * �Ə����Ă���������ȁB
   */
  assert( rightmove >= 0 );
  char *cursor_right=(char*)alloca( rightmove + 1 );
  memcpy( cursor_right , ed.getText()+ed.getPos() , rightmove );
  cursor_right[ rightmove ] = '\0';

  for(;;){ /* �L�[���� */
    ed.message( "%s%s" , cursor_right , tmp->buffer );

    int key=::getkey();
    ed.cleanmsg();
    
    Edlin::Status (Shell::*function)() = Shell::get_bindkey_function(key);
    
    if(   function==&Shell::next_history
       || function==&Shell::vz_next_history ){
      /* ����� */
      if( tmp->next != NULL ){
	/*     AAA(1) �� BBB(2) �� BBB(3) �� BBB(4:����)
	 * �Ɠo�^����Ă��鎞�A�ŏ��̃��[�v�ŁA(2) �܂ňړ�
	 * �p���ŁA(1) �ֈړ� */
	tmp = tmp->next;
      }else{
	return 0;
      }
    }else if(   function != &Shell::vz_prev_history 
	     && function != &Shell::previous_history ){
      /* �P����o�b�`�\�B��c�~ */
      ed.forward( rightmove );
      ed.insert_and_forward(tmp->buffer);
      ungetkey(key);
      return 0;
    }else if( tmp->prev != NULL ){
      /* �O��� */
      tmp = tmp->prev;
    }else{
      return 1;
    }
  }
}

/* �󔒂�ǂݔ�΂��B
 * return
 *	 0 �P��̓��܂ŁA�|�C���^���ړ�����
 *	-1 '\0' �����ꂽ�B
 */
static int skipSpace(const char *&sp)
{
  for(;;){
    if( *sp=='\0' )
      return -1;
    if( !isspace(*sp & 255) )
      return 0;
    if( is_kanji(*sp) )
      ++sp;
    ++sp;
  }
}

/* �P���ǂݔ�΂��B
 *	sp �P��̔C�ӂ̈ʒu�������|�C���^�B
 *	   �R�[����A�����ֈړ�����B
 * return
 *	 0 �P��̖����܂ŁA�|�C���^���ړ������B
 *	-1 '\0'�����ꂽ�B
 */
static int skipWord(const char *&sp)
{
  QuoteFlag qf;
  while( *sp != '\0' ){
    if( isspace(*sp & 255 ) && !qf.isInQuote() )
      return 0;
    qf.eval( *sp );
    if( is_kanji(*sp) )
      ++sp;
    ++sp;
  }
  return -1;
}

/* ���݃J�[�\���ʒu�ɂ���P��̐擪�ʒu�ƒ����𓾂�B
 *	wordtop �� �P��̐擪�ʒu
 *	wordlen �� �P��̒���
 *	ed : �V�F���I�u�W�F�N�g
 */
static void get_current_word(int &wordtop,int &wordlen,Edlin &ed)
{
  for(int i=0;;){
    /* �󔒃X�L�b�v */
    for(;;){
      if( i >= ed.getPos() ){
	wordtop = i;
	wordlen = 0;
	return;
      }
      if( !isspace(ed[i] & 255) )
	break;
      i++;
    }
    /* �����񕔕��̎擾 */
    wordtop = i;
    wordlen = 0;
    QuoteFlag qf;
    
    while( i < ed.getLen() ){
      if( isspace(ed[i] & 255 ) && !qf.isInQuote() )
	break;
      
      qf.eval( ed[i] );
      if( is_kanji(ed[i]) ){
	i++;
	wordlen++;
      }
      i++;
      wordlen++;
    }
    if( i >= ed.getPos() )
      return;
  }
}

Edlin::Status Shell::vz_prev_history()
{
  /* �⊮�ΏۂƂȂ�A���͍ςݕ�����͈̔� */
  int targetTop;
  int targetLen;

  /* �J�[�\������������Ă���P��̈ʒu�ƒ����𓾂� */
  get_current_word(targetTop,targetLen,*this);

  /* �����A�ŏ��̒P��ł������ꍇ�͔�r�Ώۂ́A�s�S�̂ɂȂ�̂ŁA
   * targetLen ���s�̒����ɕς��Ă����B*/
  if( targetTop == 0 )
    targetLen = getLen();

  /* �J�[�\���ʒu�`�P�ꖖ�̒��������߂Ă��� */
  int rightmove=targetLen-(getPos()-targetTop);

  /* �P��P�ʂ̌������s�� */

  WHist *whist=NULL , *tmp;
  
  /* �匟�������� */
  // for(Histories::Cursor cur(histories) ;; ++cur )
  for(History *cur=history ;; cur=cur->prev ){ /* �s���x���̃��[�v */
      
    if( cur==NULL ){
      if( whist==NULL )
	return CONTINUE;
      else
	cur=history;
    }
    const char *sp=cur->buffer;
    
    if( targetTop == 0 ){
      /******** �s�P�ʂł̌��� *********/
      for(int i=0 ;; i++){
	if( i >= targetLen ){
	  /* �s�̊��ɓ��͂��������ƈ�v������c */
	  tmp = (WHist*)alloca(sizeof(WHist));
	  tmp->buffer = sp;
	  tmp->next = whist;
	  tmp->prev = NULL;
	  if( whist != NULL )
	    whist->prev = tmp;
	  whist=tmp;

	  if( history_core( whist , targetLen-getPos() , *this )==0 ){
	    after_repaint(0);
	    return CONTINUE;
	  }else{
	    break;
	  }
	}
	if( *sp == '\0' || *sp++ != getText()[i] )
	  goto nextline;
      }
      
    }else{ 
      /*
       ******* �P��P�ʂł̌��� *********
       */
      
      /* �ŏ��̒P��A���Ȃ킿�A�R�}���h���𖳎����� */
      if( skipSpace(sp) != 0  ||  skipWord(sp) != 0 )
	goto nextline;

      /* �ȉ��ŁA�q�X�g���̕�������A�P��P�ʂŐ؂�o���Ă䂭�B*/
      for(;;){
	/* ��s�̒P�ꃌ�x���̃��[�v */

	/* �󔒃X�L�b�v */
	if( skipSpace(sp) != 0 )
	  goto nextline;

	/* �����P�ʂŔ�r���Ă䂭���[�v�B
	 * ���[�v�������Ă���Ԃ́A�P��̊e�����ƈ�v���Ă���B
	 */
	for(int i=0 ;; i++,sp++){
	  if( i >= targetLen ){
	    /* �� �P�ꒆ�Ɍ���������L��!(���[�v�I������)
	     *    ����āA��������A�c��̕�������R�s�[���Ă䂭�B
	     *    ���̂��߂ɁA�܂��͎c��̕�����̒����𒲂ׂ�B
	     */

	    const char *tail=sp;
	    skipWord(tail);
	    int completeLen=tail-sp;

	    /* �s���̒P����A�X�^�b�N�փR�s�[ */
	    char *dp=(char*)alloca(completeLen+1);
	    memcpy( dp , sp , completeLen );
	    dp[completeLen] = '\0';

	    /* �R�s�[�����P����A���X�g�ɉ����� */
	    tmp=(WHist*)alloca( sizeof(WHist) );
	    tmp->buffer = dp;
	    tmp->next = whist;
	    tmp->prev = NULL;
	    if( whist != NULL )
	      whist->prev = tmp;
	    whist=tmp;
	    
	    if( history_core( whist , rightmove , *this ) == 0 ){
	      after_repaint(0);
	      return CONTINUE;
	    }
	    break;
	  }
	  if( *sp == '\0' )
	    goto nextline;
	  if( *sp != getText()[targetTop+i] )
	    break;
	}/* �������x����r���[�v */

	skipWord( sp );

      }/* �P�ꃌ�x����r���[�v */

    }/* �s���x����r���[�v */

  nextline:

    /* �d���s���X�L�b�v���� */
    while(    cur->prev != NULL  
	  &&  strcmp(cur->buffer,cur->prev->buffer)==0 ){
      cur = cur->prev;
    }
  }
  return CONTINUE;
}
