/* -*- c++ -*- */
#ifndef HISTORIES_H
#define HISTORIES_H

class InterfaceHistory{
  virtual void goHome()=0;		/* �z�[���֖߂�(Enter������������) */
  virtual void operator++()=0;		/* �ߋ�(�Â���)�ֈړ� */
  virtual void operator--()=0;		/* ����(�V������)�ֈړ� */
  virtual const char *operator*()=0;	/* �q�X�g���̓��e���Q�Ƃ��� */
  virtual bool operator !() const=0;	/* ����ȏ�ߋ����������ǂ��� */
  virtual operator void *() { return !*this ? NULL : this; }
};

class Histories {
  struct Node {
    Node *prev; /* �ߋ����� */
    Node *next;	/* �������� */
    char buffer[1];
  } pole; /* first �ł�����Alast �ł�����_�~�[�I�u�W�F�N�g */
  int n;
public:
  Node *getPole(){ return &pole; }
  int append(const char *s); /* last �̖����ɒǉ�����B*/

  class Cursor{
    Histories &histories;
    Node *ptr;
    int n;
  public:
    Cursor(Histories &h) : histories(h) , ptr( h.getPole() ){ }
    void operator++(){ 
      if( (ptr=ptr->next) != histories.getPole() )
	++n;
      else
	n=0;
    }
    void operator--(){
      if( (ptr=ptr->prev) != histories.getPole() )
	--n;
      else
	n=0;
    }
    char *operator *() { return ptr->buffer; }

    const char *getNth(int n);
    
    operator const void*() const { return ptr != histories.getPole() 
				     ? this : NULL ; }
    bool operator !() const { return ptr != histories.getPole(); }
    int replace(const char *s);
    int getN();
  };

  Histories() : n(0) { pole.prev=pole.next=NULL; pole.buffer[0]='\0'; }
  ~Histories();
};

#endif
