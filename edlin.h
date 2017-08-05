/* -*- c++ -*-
 *
 * Edlin          ���ׂ͂̈̊�{���i�B�[����ˑ��B�������z�N���X
 *  �� Edlin2     ANSI�G�X�P�[�v�V�[�P���X�ɂ������B����Ȃ̑Ή��܂ށB
 *      �� Shell  �v�����v�g������A�L�[�o�C���h�����܂ށu�V�F���v
 *
 *  �� private�p��    �� public�p��
 *
 * �ۑ�_�F�{�p�����̋����ʂ�����܂������Ȃ��B���܂����B
 */

#ifndef EDLIN_H
#define EDLIN_H

#include <stdio.h>
#include "macros.h"

#ifdef NEW_HISTORY
#  include "Histories.h"
#endif

/* ================ �Q�ƃN���X ================         */
class Complete;		/* �t�@�C�����⊮�ׂ̈̃N���X   */
class jrKanjiStatus;	/* ����ȕW�����C�u�����̍\���� */

class Edlin{
public:
  enum Status {
    CONTINUE,	// �ҏW���s
    TERMINATE,	// ^M ^J (�q�X�g���ɓo�^����)
    CANCEL,	// ^M ^J (�q�X�g���ɓo�^���Ȃ�)
    QUIT  = -1,	// ^D
    ABORT = -2,	// ^C
    FATAL = -3, // ���m�̃g���u��
  };
  enum{ DEFAULT_BUFFER_SIZE = 80 };
protected:
  char *strbuf;        /* �A�X�L�[�R�[�h              */
  char *atrbuf;        /* �����t���O�R�[�h            */
  
  int pos;             /* �J�[�\���̂��镶���̌��ʒu  */
  int len;             /* �S�̂� byte��               */
  int max;             /* strbuf��max                 */
  int markpos;         /* �}�[�N�̂��錅�ʒu          */

  int msgsize;         /* ����ȓ��̃C�����C���̃��b�Z�[�W�̃T�C�Y */
  int bottom_msgsize;  /* ����ȓ��̍ŉ��i�̃��b�Z�[�W�̃T�C�Y */

  bool has_marked;     /* �}�[�N������Ă�����Atrue  */
  /*
   * ================ �o�b�t�@����n���\�b�h ================ 
   */
  /* �ꏊ�����/�팸����(�o�b�t�@����̂�)�B�߂�l != 0 �Ŏ��s */
  int makeRoom(int at,int bytes);

  /* �J�[�\���ʒu�ɁA���p���������㏑�� */
  void writeSBChar(int c){
    strbuf[pos] = c ; atrbuf[pos] = SBC ; putnth(pos++);
  }

  /* �J�[�\���ʒu�ɁA�S�p����c1:c2���㏑��(�o�b�t�@����̂�) */
  void writeDBChar(int c1,int c2){
    strbuf[pos]=c1; atrbuf[pos] = DBC1ST; putnth(pos++);
    strbuf[pos]=c2; atrbuf[pos] = DBC2ND; putnth(pos++);
  }

  /* �J�[�\���ʒu�̒P��̐擪���ʒu�𓾂� */
  int  seek_word_top();
  
  /* ================ �\���X�V�n���\�b�h ================
   *   termclear �� 1�ȏ�ɂ���ƁA���������̌�������������B
   */
  void after_repaint(int termclear=-1);   /* �J�[�\���ȍ~�̂ݍX�V */
  void repaint(int termclear=-1);         /* �S�s repaint �X�V    */

  virtual void putchr(int c)=0; /* �ꕶ���o��               */
  virtual void putel()=0;       /* �J�[�\���ʒu�ȍ~���N���A */
  virtual void putbs(int i)=0;  /* �J�[�\���������߂�       */
  virtual void alert()=0;       /* �x��(���ʂ�beep��)       */

  void putnth(int nth);         /* n �Ԗڂ̕������o��
				 * ���̈ʒu�Ƀ}�[�N������΁A
				 * �����ƐF��ς��� */
  void putchrs(const char *s);  /* putchr �̕����� */

  static int setMarkAttr(const char *start,const char *end);

public:
  Edlin();
  virtual ~Edlin();
  bool operator ! () const { return strbuf==0 || atrbuf==0 ; }
  
  enum{ SBC , DBC1ST , DBC2ND , PAD };
  
  void init();
  void pack(); /* ���͂������䕶����1byte�`���֒u������B */

  void insert(int ch);                     /* �P�����}�� */
  void insert_and_forward(const char *s);  /* ������}�� */
  void quoted_insert(int ch);              /* ���䕶���}�� */

  void cut();
  void erase();               /* ^D �ꕶ���폜         */
  int  forward();             /* ^F �J�[�\���E�ړ�     */
  int  forward(int x);        /*    �������E�ړ�       */
  int  backward();            /* ^B �J�[�\�����ړ�     */
  int  backward(int x);        /*    ���������ړ�       */
  void forward_word();        /* @F �J�[�\���E�P��ړ� */
  void backward_word();       /* @B �J�[�\�����P��ړ� */

  // void go_ahead();            /* ^A �擪��             */

  //void go_tail();             /* ^E ������             */
  void clean_up();            /* ^U ���͔j��           */
  void erasebol();            /*    �J�[�\����O������ */
  void eraseline();           /* ^K �J�[�\���ȍ~������ */
  void swapchars();           /* ^T �J�[�\����O�񕶎�����ꊷ���� */
  virtual void cls(){};       /* ^L ��ʃN���A(�������Ȃ�) */

  /* �����́A���o�N���X�ֈڍ����ׂ����� */
  virtual int complete();	/* TCSH�^�̕⊮ */
  virtual int completeFirst();	/* �ϊ��^�̕⊮ */
  virtual int complete_to_fullpath(const char *header);
  /* �t���p�X�ւ̕⊮ */

  virtual void complete_list(){}    /* ^D �t�@�C�������X�g   */
  virtual int complete_hook(Complete &){ return 0; }
  /* �� �t�@�C�����̑��ɉ������₪����΁A���̃t�b�N�֐��𓱏o����B*/
  
  /* �J�[�\����K�؂Ȉʒu�Ɉړ����ē��͑҂� */
  virtual int getkey(void){ return ::getkey(); }
  
  /* ���͕�����ȊO�̃��b�Z�[�W��\�����郁�\�b�h */
  int message(const char *fmt,...);
  void cleanmsg();

  void bottom_message( const char *fmt , ...);
  void clean_bottom();

  void locate(int x);
  void marking(void);

  /*
   * -------- ���|�[�g�֐� --------
   */
  int getPos()          const throw() { return pos; }
  int getMarkPos()      const throw() { return markpos; }
  int getLen()          const throw() { return len; }
  const char *getText() const throw() { return strbuf; }
  const char *getAttr() const throw() { return atrbuf; }
  int operator[](int n) const throw() { return strbuf[n] & 255; }

  static int complete_tail_char;
  
  // -------- �ϊ��^�t�@�C�����⊮ --------
public:
  enum CompleteFunc {
    COMPLETE_CANCEL,
    COMPLETE_PREV,
    COMPLETE_NEXT,
    COMPLETE_FIX,
    COMPLETE_FIX_PLUS,
  };
private:
  static CompleteFunc completeBindmap[ 0x200 ];
public:
  Status go_ahead();
  Status go_tail();
  static void initComplete();
  static int  bindCompleteKey(const char *key,const char *func);
};

class Edlin2 : public Edlin {
  static int canna_inited;       /* ����������Ă����� not 0 */
  int print_henkan_koho( jrKanjiStatus &status , const char *mode_string );
protected:
  FILE *fp;
  const char *cursor_on;
  const char *cursor_off;
  
  void putchr(int c);
  void putel();
  void putbs(int i);
  void alert(){ putchr('\a'); }
public:
  Edlin2(FILE *_fp=stdout) : fp(_fp) , cursor_on("") , cursor_off(""){}
  int getkey(void);
  
  void setcursor(char *on,char *off="\x1B[0m")
    { cursor_on = on ; cursor_off = off; }

  static int option_canna;
  static void canna_to_alnum();  /* �����I�ɉp�����[�h��  */
};

extern char dbcstable[256];
int dbcs_table_init();

/* �q�X�g�����ǃv����
 *	�ŏI�I�ɂ́A�q�X�g���͊O���I�u�W�F�N�g(Histories)�ŊǗ��B
 *	Shell �́A�q�X�g���C���X�^���X���Q�Ƃ݂̂���悤�ɂ���B
 *	�]���āAShell �̃R���X�g���N�^�Ƀq�X�g���I�u�W�F�N�g��
 *	����邱�ƂɂȂ�B
 *
 *	�����Ȃ�A����͓������AShell �ň����Ă���q�X�g����
 *	�N���X Histories �ň����悤�ɕύX����B
 */

class BindCommand0;

class Shell : private Edlin2 {
  const char *prompt;
  bool topline_permission;

  void complete_list();
  int complete_hook(Complete &com);
  void cls();
  void re_prompt();
  void paste_to_clipboard(int at,int length);
  
  void alert(){ if( beep_ok ) putchr('\a'); }
public:
#ifndef NEW_HISTORY
  struct History{
    History *prev,*next;
    char buffer[1];
  };
#endif
  /* $I �ׂ̈Ƀg�b�v���C������㏑�����邩�ۂ��̐ݒ胁�\�b�h */
  void  allow_use_topline(){ topline_permission = true;  }
  void forbid_use_topline(){ topline_permission = false; }

  /* �� Shell �̃p�[�g */
private:
  bool changed;		/* �ύX�t���O   */
  bool overwrite;	/* �㏑�����[�h */
  int ch;
  int prevchar;
  int prev_complete_num;

  enum{ NUMOF_BINDMAP = 0x200 };
  static void bindkey_base();
  static BindCommand0 *bindmap[ NUMOF_BINDMAP ];
  
  Status search_engine(int isrev);
  int line_input(const char *prompt);
public:
  void setcursor(char *on,char *off="\x1B[0m")
    { Edlin2::setcursor(on,off); }

  static int ctrl_d_eof;
  static void bindkey_wordstar();
  static void bindkey_tcshlike();
  static void bindkey_nyaos();
  static int bindkey(const char *key,const char *funcname);
  static Status (Shell::*get_bindkey_function(int key))();

  static int bind_hotkey(const char *key,const char *program,int opt);
  static void bindlist(FILE *fp);

  Shell( FILE *fp=stdout );
  ~Shell(){}
  bool operator ! () const { return Edlin2::operator !(); }

  int line_input(const char *prompt1,const char *prompt2,const char **str);
private:
#ifdef NEW_HISTORY
  static Histories histories;
#else
  static History *history;
  static int nhistories;
  History *cur;
#endif
public:
  // �ŐV�̃q�X�g�����e�������̓��e�ƒu��������.
  static int replace_last_history(const char *s);
  int regist_history(const char *s=0);
  static int append_history(const char *s);

  bool isOverWrite(){ return overwrite; }

  /* ================ �L�[�Ƀo�C���h�\�ȃR�}���h ================ */

  Status i_search();
  Status rev_i_search();
  Status self_insert();
  Status previous_history();
  Status next_history();
  Status bye();
  Status forward_char();
  Status backward_char();
  Status tcshlike_ctrl_d();
  Status backspace();
  Status tcshlike_complete();
  Status yaoslike_complete();
  Status complete_to_fullpath();
  Status complete_to_url();
  Status input_terminate();
  Status repaint();
  Status go_ahead();
  Status go_forward();
  Status go_tail();
  Status flip_over();
  Status cancel();
  Status erasebol();
  Status eraseline();
  Status forward_word();
  Status backward_word();
  Status simple_delete();
  Status abort(){ return ABORT; }
  Status swapchars(){ Edlin2::swapchars(); return CONTINUE; }
  Status vz_prev_history();
  Status vz_next_history();
  Status quoted_insert();
  Status keyname_insert();
  Status hotkey(const char *cmdline,int opt);
  Status copy();
  Status cut();
  Status paste();
  Status marking();
  Status paste_test();

  /* option���ߗp�BEdlin ����Q�Ƃ����̂� */
  static int beep_ok;
};

/* ������u�֐��I�u�W�F�N�g�v�Ƃ������́B
 * �L�[���^�C�v�����x�ɁA���̃L�[�Ƀo�C���h����Ă���
 * BindCommand0 �̔h���N���X�̃C���X�^���X��
 * operator() (Shell &) ���Ăяo�����̂��I
 */
class BindCommand0 {
public:
  typedef Edlin::Status (Shell::*cmd_t)();

  virtual Edlin::Status operator() (Shell &s)=0;
  virtual void printUsage(FILE *fp)=0;
  virtual ~BindCommand0(){}
  virtual operator cmd_t ()=0;
};

inline Edlin::Status (Shell::*Shell::get_bindkey_function(int key))()
{  return (unsigned(key) < NUMOF_BINDMAP ) ? *bindmap[ key ] : NULL ; }

#endif /* EDLIN_H */
