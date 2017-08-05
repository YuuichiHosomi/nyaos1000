/* -*- c++ -*- */

#ifndef SUBSTR_H
#define SUBSTR_H

class SmartPtr;

/* Substr : ����������Q�Ɨp�N���X(Pascal�^������) */
class Substr{
public:
  int len;
  const char *ptr;
  
  char operator[](int n) const
    { return ptr[n] & 255; }

  /* ������ */
  Substr(void) : len(0), ptr(0) { }
  Substr(const char *p,int l) :  len(l) ,ptr(p) { }
  void set(const char *p,int l)
    { ptr=p; len=l; }
  void reset()
    { ptr=0; len=0; }

  /* �e�X�g */
  operator const void* () const
    { return ptr; }
  int operator ! () const
    { return ptr == 0; }

  /* �P���R�s�[ */
  void operator >> (char *dp) const
    { memcpy(dp,ptr,len); dp[len] = '\0'; }
  
  void operator >> (SmartPtr dp) const;
  
  /* ���p�R�s�[ */
  char *quote(char *dp) const;
};

#endif
