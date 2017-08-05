#ifndef QUOTEFLAG_H
#define QUOTEFLAG_H

/* ��������ŏ�����ǂ�ł䂭�ہA���݂̈ʒu�� ���p���̒����ۂ���
 * �m�肽����������B�����������Ɏg���N���X�B
 *
 * QuoteFlag qf;
 * for(const char *sp=src ; *sp != '\0' ; sp++ ){
 *	;  �F�X�ȏ���
 *    qf.eval(*sp);
 * }
 * �ĂȊ����Ŏg���B�u�F�X�ȏ����v�̕����ŁA���݁A�ǂ�������Ԃɂ��邩��
 *	qf.isInSingleQuote() �c �V���O���N�H�[�g�ň͂܂�Ă���B
 *	qf.isInDoubleQuote() �c �_�u���N�H�[�g�ň͂܂�Ă���B
 *	qf.isInQuote() �c �C�ӂ̃N�H�[�g�ň͂܂�Ă���B
 * �ȂǂŒm�邱�Ƃ��ł���B
 */

extern int option_single_quote;

class QuoteFlag {
  enum{ SINGLE=1 , DOUBLE=2 };
  unsigned flag;
public:
  void eval(int ch){
    if( ch == '"'  &&  (flag & SINGLE) == 0 )
      flag ^= DOUBLE;
    else if( ch == '\''  &&  (flag & DOUBLE) == 0  &&  option_single_quote )
      flag ^= SINGLE;
  }
  bool isInSingleQuote() const { return (flag & SINGLE) != 0; }
  bool isInDoubleQuote() const { return (flag & DOUBLE) != 0; }
  bool isInQuote()       const { return flag != 0; }
  QuoteFlag() : flag(0){ }
};

#endif
