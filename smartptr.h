#ifndef SMARTPTR_H
#define SMARTPTR_H

/* SmartPtr : (char *)�̌݊��N���X�B
 *	char buffer[1000];
 *	SmartPtr dp(buffer,sizeof(buffer));
 * �Ɛ錾����ƁA������ ++dp ���Ă��A
 * dp �� buffer+10000 ���Ήz���Ȃ��B
 * �z�����ꍇ�A��O SmartPtr::BorderOut ����������B
 *
 * (�{�̂� smartptr.cc �͖����B�C�����C���֐��̂�)
 */

class SmartPtr{
  char *ptr;
  char *border; /* ptr �̏�� , *border �ɂ́u\0�v��������̂݁B*/
public:
  class BorderOut{}; /* ptr �� border ���z�������ɔ��������O */
  
  SmartPtr(char *p,int max) : ptr(p) , border(p+max-1) { }

  SmartPtr &operator++() throw(BorderOut) {
    if( ++ptr > border ){
      throw BorderOut();
    }
    return *this;
  }
  char *operator++(int) throw(BorderOut) {
    if( ptr+1 > border ) throw BorderOut();
    return ptr++;
  }
  SmartPtr &operator << (char c){
    *ptr++ = c;
    if( ptr+1 > border ) throw BorderOut();
    return *this;
  }
  SmartPtr &operator << (const char *s){
    while( *s != '\0' )
      *this << *s++;
    return *this;
  }

  char &operator*() throw()
    { return *ptr; }
  operator const char*() const throw()
    { return ptr; }
  SmartPtr &operator += (int n) throw(BorderOut) {
    if( (ptr+=n) > border ) throw BorderOut();
    return *this;
  }
  SmartPtr operator + (int n) const
    { SmartPtr tmp(*this); return tmp += n ; }
  char *rawptr() throw()
    { return ptr; }
  int operator !() const throw()
    { return ptr > border; }
  int ok() const throw()
    { return ptr <= border; }
  int ng() const throw()
    { return ptr >= border; }
  void set(char *p,int max)
    { ptr=p ; border = p+max-1; }
  char &operator[](int n) throw(BorderOut){
    if( ptr+n > border ) throw BorderOut();
    return ptr[n];
  }
  void terminate() { *(ptr=border) = '\0'; }
};


#endif
