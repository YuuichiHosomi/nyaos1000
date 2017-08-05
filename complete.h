/* -*- c++ -*- */
#ifndef COMPLETE_H
#define COMPLETE_H

#include <sys/types.h>
#include <dirent.h>

#include "finds.h"

int which_suffix(const char *path,...);

/* Complete : �⊮���ƂȂ�t�@�C���̃��X�g��񋓁E�L������N���X
 * �N���X Files �̔h���N���X(���Č���ᕪ���邩)�B
 *
 * // �g�p��
 * Complete com;			// �C���X�^���X�쐬
 *
 * com.makelist("c:/hoge");		// �⊮���X�g�̍쐬
 * printf("c:/hoge%s",com.nextchar());	// �⊮���ׂ�������
 *
 * // ���Ώۂ̗�
 * for( Complete::Cursor cur(com) ; cur.isOk() ; cur++ ){
 *	printf("%s ",cur->get_name() );
 * }
 *
 */

#ifndef MALLOC_ERROR
#define MALLOC_ERROR
class MallocError{ };
#endif

class Complete : public Files {
  char directory[ FILENAME_MAX ];	/* ���t�@�C�����̃f�B���N�g���� */
  char fname[FILENAME_MAX ];		/* ���t�@�C�����̔�f�B���N�g���� */
  int common_length;			/* ���̋��ʕ����̒��� */
  int max_length;			/* �ł��������O�̌��̒��� */
  int typed_split_char ;		/* ���͂��ꂽ�p�X��������(0,/ or \ )*/
  int makelist_core(int command_complete, int is_with_dir );
public:
  enum{
    NOT_COMPLETED ,
    COMMAND_COMPLETED ,
    FILENAME_COMPLETED ,
    SIMPLE_COMMAND_COMPLETED ,
    ERROR
  } status;

  Complete() : common_length(0) , typed_split_char(0) 
    , status(NOT_COMPLETED) {  }
  ~Complete(){ clear(); }

  int makelist          (const char *path);	/* �����t�@�C�����̕⊮ */
  int makelist_with_path(const char *path);	/* �����R�}���h���̕⊮ */
  int makelist_with_wildcard(const char *path);	/* ���C���h�J�[�h�̕⊮ */

  int add_buildin_command(const char *name); /* after makelist only */
  
  char *nextchar();
  void unique();
  
  /* ���|�[�g�֐� */
  int get_max_name_length() const { return max_length; }
  int get_split_char(void) const { return typed_split_char; }
  const char *get_real_name1() const ;
  int get_fname_common_length() const { return common_length; }

  /* �I�v�V����(�ÓI�����o)�ϐ� */
  static int directory_split_char;
  static int complete_tail_tilda;
  static int complete_hidden_file;
  
  /* �R�}���h���⊮�ׂ̈̃L���b�V�����쐬/�X�V���� */
  static void make_command_cache() throw(MallocError);
  static unsigned queryBytes(); /* �c �L���b�V���̃������g�p�ʂ𓾂� */
  static unsigned queryFiles(); /* �c �L���b�V���ɂ���t�@�C�����̐� */
  
  /* ����q�I�u�W�F�N�g(�C�^���[�^�Ƃ����قǂ̂��̂ł͂Ȃ�!?)
   */
  class Cursor{
    Complete &list;
    FileListT *findptr;
  public:
    Cursor &operator++(){ findptr=findptr->next; return *this;}
    Cursor &operator--(){ findptr=findptr->prev; return *this;}
    Cursor &utimawari() /* �����(�I�薳���I) */
      { findptr=(  findptr->next != 0 ? findptr->next : list.get_top() ); 
	return *this; }
    Cursor &sotomawari() /* �O���(�I�薳���I) */
      { findptr=(  findptr->prev != 0 ? findptr->prev : list.get_tail() );
	return *this;
      }
   
    bool isOk() const { return findptr != NULL; }
    FileListT *toFileListT(){ return findptr; }
    FileListT *operator->(){ return findptr; }

    Cursor( Complete &c ) : list(c) , findptr(c.get_top()) { }
    ~Cursor(){ }
  };
};

#endif
