// -*- c++ -*-
#ifndef SHARED_H
#define SHARED_H

/* ���L�������̍\��
 *	4 bytes : �V�F�A�[�h�������S�̂̃T�C�Y[bytes]
 *	4 bytes : �S�̂̎g�p�T�C�Y[bytes]
 *	4 bytes : �t�@�C���̐�
 *
 *   12th byte ��� �ȉ��A�J��Ԃ�:
 *	2 bytes : ������̒��� ( 0 �Ȃ�A�I��)
 *	n bytes : ������
 * 
 */

class SharedMem{
  enum { default_size = 4096 };

  struct Header{
    Header *next;
    unsigned long allsize;
    unsigned long usedsize;
  } *first,*last;

  unsigned long hmtx;
  int nlocked;
  bool isvirgin;
  int static_area_size;
public:
  struct Fail{};
  struct SemError{
    int errcode;
    SemError(int n) : errcode(n){}
  }; // �Z�}�t�H�̃^�C���A�E�g�� 

  SharedMem(  const char *semname , const char *memname
	    , int staticAreaSize=0 ) throw(Fail,SemError);
  ~SharedMem();
  void *getStaticArea(){ return first+1; }
  void *alloc(int s) throw(SemError);
  void clear() throw(SemError);
  
  void lock() throw(SemError);
  void unlock();
  bool isVirgin() const { return isvirgin; }
};

class PathCacheShared : public SharedMem {
public:  
  struct Node {
    Node *next;
    unsigned short length;
    char name[1];
  };
  struct Common{
    int nfiles;
    int nbytes;
    Node *first,*last;
  } *common;

  PathCacheShared() throw(SemError);
  void clear() throw(SemError);
  int insert( const char *name ) throw(SemError);
  
  class Cursor {
    PathCacheShared &cache;
    Node *cur;
  public:
    Cursor(PathCacheShared &pc) throw(SemError): cache(pc) {
      pc.lock(); 
      cur=pc.common->first;
    }
    ~Cursor(){ cache.unlock(); }
    Node *operator->(){ return cur; }
    Node *operator*(){ return cur; }
    void operator++(){ if( cur ) cur = cur->next; }
    operator const void*() const {  return (const void*)cur; }
    bool operator ! () const { return cur == 0; }
  };
  unsigned queryBytes() throw(SemError){
    lock(); int rc=common->nbytes; unlock(); return rc;
  }
  unsigned queryFiles() throw(SemError){ 
    lock(); int rc=common->nfiles; unlock(); return rc;
  }
};

#endif
