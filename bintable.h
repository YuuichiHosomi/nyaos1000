/* -*- c++ -*- 
 * bsearch �֐������b�s���O����ׂ̃N���X�B
 * �e�[�u���̗v�f�ƂȂ�I�u�W�F�N�g�̃N���X��
 * �uname�v�Ƃ������O�� char* / char[] �^�����o�������Ƃ��K�v�B
 */

#ifndef BINTABLE_H

#include <string.h> /* for str(i)cmp */
#include <stdlib.h> /* for bsearch */

template <class T> class BinTable {
  static int compare(void *key,void *el){
    return stricmp(  static_cast <const char *>(key)
		   , (static_cast <T *>(el) )->name  );
  }
  T *table;
  int n;
public:
  T *find( const char *key ){
    return static_cast <T*> (bsearch(  name
				     , table
				     , n
				     , sizeof(T)
				     , BinTable<T>::compare ) );
  }
  
  BinTable(T *_table,int _n) : table(_table) , n(_n) { }
  ~BinTable();
};

#endif
