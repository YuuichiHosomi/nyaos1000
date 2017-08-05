#include <assert.h>
#include <io.h>
#include <sys/types.h>

#include <string.h> /* for memset */
#include <stdlib.h> /* for _osmode */
#include <dos.h>    /* for int86 */

#include <sys/kbdscan.h> /* for translate CSI sequence */
#include <ctype.h>

#include <termios.h>
#include <termio.h>

#include "macros.h"

#define KEY(a)	(K_##a | 0x100)

int option_esc_key_sequences=0;
int option_direct_key=0;

#ifndef S2NYAOS
static int tty=-255;
static struct termios orig,s;
#endif

void raw_mode(void)
{
#ifndef S2NYAOS
  if( _osmode == DOS_MODE  || option_direct_key )
#endif
    return;
#ifndef S2NYAOS

  if( tty == -255 ){
    tcgetattr(tty=0,&s);
    orig = s;
  }else{
    tcgetattr(tty,&s);
  }

  /* �@�\�� off �ɂ��� */
  s.c_iflag &= ~(  ISTRIP /* Clear bit 7 of all input characters */
		 | INLCR  /* Translate linefeed into carriage return */
		 | IGNCR  /* Ignore carrige return */
		 | ICRNL  /* Translate carriage return into linefeed */
		 | IUCLC  /* Convert upper case letters into lower case */
		 );

  /* �@�\�� off �ɂ��� */
  s.c_lflag &= ~(  ICANON /* line editing */
		 | ECHO   /* echo input */
		 | ECHOK  /* echo CR/LF after VKILL */
		 | ISIG   /* Enable signal processing */
		 | ECHONL /* not support ? */
		 );
  /* emx�ł́Ac_oflag���T�|�[�g����Ă��Ȃ��c�݊����̂��߂Ƀw�b�_�����͒�`��
   * ��Ă���c�̂ŁA�ȉ���c_oflag���w�肷��K�v���͂Ȃ��B�� �Q��: termios (3)
   */
  s.c_oflag |=	(  TAB3	  /* expands tabs to spaces			     */
		 | OPOST  /* enable implementation-defined output processing */
		 | ONLCR  /* map NL to CR-NL on output			     */
		 );
  s.c_oflag &= ~(  OCRNL  /* map CR to NL on output	 */
		 | ONOCR  /* don't output CR at column 0 */
		 | ONLRET /* don't output CR             */
		 );

  /* �ꕶ���P�ʂŁA0�b�Ŕ��������� */
  s.c_cc[VMIN] = 1;
  s.c_cc[VTIME] = 0;
  
  tcsetattr(tty,TCSADRAIN,&s);
#endif
}

void cocked_mode(void)
{
#ifndef S2NYAOS
  if( tty != -255 &&  _osmode != DOS_MODE  &&  option_direct_key==0 ){
    tcsetattr(tty,TCSANOW,&orig);
    tty = -255;
  }
#endif
}

int get86key(void)
{
#ifndef S2NYAOS
  if( _osmode == DOS_MODE ){
    /* DOS�ł�_read_kbd�ŁA�����̑��o�C�g�ڂ��擾�ł��Ȃ��B*/
    union REGS regs;
    regs.h.ah = 0x7;
    return _int86( 0x21, &regs , &regs ) & 0xFF ;
  }else if( option_direct_key ){
#endif
    return _read_kbd(0,1,0);
#ifndef S2NYAOS
  }else{
    unsigned char key;
    int rc;

    enum{ READ_INTR = -2 };
    assert( tty != -255 );
    
    do{
      if( isatty( tty ) ){
	fd_set readfds;
	FD_ZERO( &readfds );
	FD_SET(tty,&readfds );
	if( select(tty+1,&readfds,NULL,NULL,NULL) == -1 )
	  return 0;
     
      }
      rc= read( tty , &key , sizeof(char) );
      if( rc < 0 )
	return 0;
    }while( rc != 1 );

    return key & 0xFF;
  }
#endif
}

static int keybuf[16],left=0;

void ungetkey(int key)
{
  keybuf[ left++ ] = key;
}

int getkey(void)
{
  if( left > 0 )
    return keybuf[ --left ];

  int ch = (get86key() & 0xFF );
  switch(ch){
  default: 
    if( is_kanji(ch) )
      ch = ((ch << 8)|(get86key() & 0xFF)) & 0xFFFF;
    break;

  case 0:  
    ch = (get86key()|0x100);
    break;
    
  case 27:
    if(!option_esc_key_sequences)
      return ch;	/* �� �f�B�t�H���g�ł́A�����ŕԂ� */

    /*
     * ESC�L�[�E�V�[�P���X��raw���x���ɖ߂��̂́A�A���S�Ƃ���
     * �������Ȃ����ǁA����Ƃ葁�� (^^;
     */
    
    int par;
    switch(ch=get86key()){
    case '[':
      
      /*
       * CSI sequences
       */
      for(par=0;;){
	/*
	 * CSI�p�����^�̎擾
	 */
	while(isdigit(ch=get86key()))
	  par=par*10+ch-'0';
	if(ch!=';')
	  break;
	/*
	 * �����̃p�����^����������A�����Ȓl���Z�b�g����
	 * ����������
	 */
	par=500;
      }
      if(par<=1/*�p�����^1��1���Ȃ��ꂽ��*/)
	switch(ch){
	case 'A':
	  ch=KEY(UP); break;
	case 'B':
	case 'e':
	  ch=KEY(DOWN);  break;
	case 'C':
	case 'a':
	  ch=KEY(RIGHT); break;
	case 'D': ch=KEY(LEFT);  break;
	case 'H':
	case 'f': ch=KEY(HOME);  break;
	case 'F': /* �{����CSI F�ƈӖ����Ⴄ���A���ۂ�
		   * END�L�[�������Ă݂�� CSI F�������Ⴄ
		   */
	  ch=KEY(END);   break;
	default: ch=0;
	}
      else if(ch=='~')
	switch(par){
	case  2:
	  ch=KEY(INS); break;
	case  3: ch=KEY(DEL); break;
	  /*
	   * ���ɂ�CSI n~�ŗ���L�[�������ς����邯��
	   * nyaos�̂��߂Ɏg�����Ƃ͖ő��ɂȂ����낤��
	   * �ʓ|���������疳��
	   */
	default:
	  ch=0;
	}
      else
	ch=0; /* ��ɊY�����Ȃ�CSI�V�[�P���X�𖳎� */
      break;
      
    case '#': /* DEC test sequences			      */
    case '%': /* UTF control sequences		      */
    case '(': /* G0 sequences		 �����̃V�[�P���X�� */
    case ')': /* G1 sequences		 ���1����������      */
      get86key();	/*	 ����𖳎�����	      */
    default:
      ch=0;
      break;
    }
    break;
  }
  return ch & 0xFFFF;
}
