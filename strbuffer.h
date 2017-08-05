/* This header file is for -*- c++ -*- 
 *
 * StrBuffer : �ꕶ�����������镶������Ǘ�����N���X�B
 *	StrBuffer buf;
 *	buf << 'a' << "hogehoge";
 * �����s����ƁA�q�[�v�Ɋm�ۂ��ꂽ�̈�ɍŏ��̂��̂���
 * ��������ł䂭�B�r���̕�����́A
 *	(const char *)buf
 *	buf.getTop()
 * �ȂǂŎQ�Ɖ\�B�������A����� const �ł���A�I�u�W�F�N�gbuf
 * �̏����Ƌ��Ɏ�����B�����A
 *	char *s=buf.finish();
 * �����s����ƁA�q�[�v�̕������ buf �ƓƗ��Ɏ��o�����Ƃ��ł���B
 *
 * s �� strdup �֐��Ȃǂɂ���č��ꂽ���ʂ� �q�[�v������Ɖ���
 * �ς��Ƃ���͂Ȃ��B�������AStrBuffer �̃C���X�^���X�̓q�[�v
 * ����������S�Ɏ�����̂ŁA(const char *)�ւ̃L���X�g���œ�����
 * ������͒����O�ɖ߂��Ă��܂�(NULL�ł͂Ȃ�)�Bs �͎g�p��A�����I��
 * free ���Ă��K�v������B
 *
 * ��F
 *	try{
 *	    StrBuffer buf;
 *	    for(int i=0 ; i<argc ; i++ ){
 *		buf << argv[i] << ' ';
 *		printf("buf==[%s]\n",(const char *)buf);
 *	    }
 *	    char *s=buf.finish();
 *	    printf("buf==[%s]\nbuf.finish()==[%s]\n",(const char *)buf,s);
 *	    free(s);
 *	}catch( MallocError ){
 *	    fputs("Memory allocation error\n",stderr);
 *	}
 *
 * ��̗�ł́A�Ō�� printf �� �ubuf==[]�v�ƕ\�������B
 *
 * StrBuffer �� malloc/realloc �ŕ�������Ǘ����Ă���̂ŁA���
 * �q�[�v�m�ێ��s���N�������ꂪ����B����́A��O MallocError
 * �� catch ���邱�ƂőΉ��ł���BMallocError �̓����o�[�������Ȃ�
 * �N���X�B
 *
 * �s���ӓ_�t
 * StrBuffer �� 0�����̒i�K(this->isZero()!=0)�ł�
 *    (const char *)   �� "\0" (�Œ蕶���� , const )
 *    getTop  ���\�b�h �� "\0" (�Œ蕶���� , const )
 *    finish  ���\�b�h �� NULL
 *    finish2 ���\�b�h �� "\0" (�q�[�v�F�vfree������)
 * ��Ԃ��B
 */

#ifndef STRBUFFER_H
#define STRBUFFER_H

#ifndef MALLOC_ERROR
#define MALLOC_ERROR
class MallocError{ };
#endif

class StrBuffer {
public:
  class MallocError : ::MallocError { };	/* ��O�F�������m�ێ��s */
  class OverFlow { };				/* ��O�F�z��Y���G���[ */
private:
  int length;
  char *buffer;
  int max;	/* ���� max �� length �� max �Ȃ̂ŁA�T�C�Y�� +1 �K�v */
  int inc;
  
  void grow(int x) throw(MallocError);
  static char zero[1];

public:
  bool isZero() const { return length==0; }
  StrBuffer &operator << ( const char *s ) throw(MallocError);
  StrBuffer &operator << ( char c ) throw(MallocError){
    if( length+1 >= max )
      grow(length+1+inc);
    buffer[ length++ ] = c;
    buffer[ length ] = 0;
    return *this;
  }
  StrBuffer &operator << ( int n ) throw(MallocError);
  
  /* �������̈�(�擪�A�h���X�{�o�C�g��)��ǉ�����B*/
  StrBuffer &paste( const void *s , int size ) throw(MallocError);

  /* ���l���E�l�߂ŏo�͂��� */
  StrBuffer &putNumber(int num,int width,char fillchar=' ',char sign='\0');

  /* ��������q�[�v������Ƃ��Ď��o���B
   * ����ɃC���X�^���X�͋�ɂȂ�B*/
  char *finish() throw();             /* 0 �����ł� NULL ��Ԃ��B*/
  char *finish2() throw(MallocError); /* 0 �����ł̓q�[�v������"\0"��Ԃ� */

  /* �o�b�t�@���̂Ă� */
  void drop() throw();

  /* n�����ڈȍ~��؂�̂Ă�B*/
  void back(int n) throw() { buffer[ length=n ] = '\0'; }

  /* n�����ڂ𓾂�B�z��T�C�Y�̃`�F�b�N�͂��Ȃ� */
  int operator[](int x)   const throw(){ return buffer[x] & 255; }

  /* n�����ڂ𓾂�B�z��T�C�Y���I�[�o�[����Ɨ�O���΂� */
  int at(int x) const throw(OverFlow){ 
    if( x < length )	return buffer[x] & 255;
    else		throw OverFlow();
  }

  int getLength()         const throw(){ return length; }
  const char *getTop()    const throw(){ return buffer; }
  operator const char *() const throw(){ return buffer; }

  StrBuffer() : length(0),buffer(zero),max(0),inc(100){ }
  StrBuffer(int x) : length(0),buffer(zero),max(0),inc(x){ }
  ~StrBuffer();
};


#endif
