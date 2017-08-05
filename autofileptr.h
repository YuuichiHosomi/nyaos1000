/* -*- c++ -*- */
#ifndef AUTOFILEPTR_H
#define AUTOFILEPTR_H
#include <stdio.h>

/* �����N���[�Y����A�t�@�C���|�C���^�B
 *
 * AutoFilePtr fp("hogehoge","r");
 * if( fp == NULL ) puts("error");
 *
 * �����ŁA���Ƃ͕��ʂ� FILE * �Ɠ����B
 * �������Afclose �����͏���ɂ��Ă͂����Ȃ��B
 * (�����N���[�Y���邩��)
 *
 * �ǂ����Ă��N���[�Y���������́A
 * ���\�b�h AutoFilePtr::close ���g�����ƁI�I
 */

class AutoFilePtr {
  FILE *fp;
public:
  int isOk() const { return fp != 0; }
  operator FILE* () { return fp; }
  
  /* �I�[�v���Ffopen �̑���Ɏg���ׂ������A
   * ���ʂ́A�����t���R���X�g���N�^�̕��ł悢���낤 */
  FILE *open(const char *name,const char *mode){
    if( fp != 0 ) fclose(fp);
    return fp = fopen(name,mode);
  }

  /* �N���[�Y�Ffclose �̑���Ɏg�� */
  void close(){ 
    if( fp != 0 ) fclose(fp);
    fp = 0;
  }

  /* ������Z�q�B��̃t�@�C���|�C���^��
   * ������ AutoFilePtr �C���X�^���X��
   * ���L���邱�Ƃ͂ł��Ȃ��I
   */
  AutoFilePtr &operator = (AutoFilePtr &x){
    if( fp != 0 ) fclose(fp);
    fp = x.fp;
    x.fp = 0;
    return *this;
  }

  /* �R���X�g���N�^(�t�@�C�����I�[�v�����Ȃ�) */
  explicit AutoFilePtr(FILE *p) : fp(p) { }

  /* �R���X�g���N�^(�t�@�C���𓯎��ɃI�[�v������) */
  AutoFilePtr(const char *name,const char *mode){
    fp = fopen(name,mode);
  }

  /* �f�X�g���N�^(�t�@�C���������N���[�Y����) */
  ~AutoFilePtr() {
    if( fp != 0 ) fclose(fp); 
  }
};

#endif
