/* -*- c++ -*- */
#ifndef MACROS_H
#define MACROS_H

/* dbcs.c */

extern char dbcs_table[128+256];
int dbcs_table_init();
extern char toupper_table[128+256];
extern char tolower_table[128+256];

#define is_kanji(x) (dbcs_table+128)[x]
#define to_upper(x) (toupper_table+128)[x]
#define to_lower(x) (tolower_table+128)[x]
#define is_space(x) isspace((x)& 255)
#define is_digit(x) isdigit((x)& 255)
#define is_alpha(x) isalpha((x)& 255)
#define is_xdigit(x) isxdigit((x)& 255)
#define is_lower(x) islower((x)& 255)
#define is_upper(x) isupper((x)& 255)
#define is_alnum(x) isalnum((x)& 255)

#undef numof
#define numof(A) (sizeof(A)/sizeof((A)[0]))
#define tailof(A) ((A)+numof(A))

enum{
  NO_FILE = 0,           /* �t�@�C���͑��݂��Ȃ�        */
  EXE_FILE = 1,          /* �o�C�i�����s�t�@�C��	*/
  CMD_FILE = 2,          /* OS/2 �R�}���h�t�@�C��	*/
  COM_FILE = 3,	         /* COM(SOS) �t�@�C��		*/
  FILE_EXISTS = 4,       /* ���̑��̃t�@�C��		*/
};

int SearchEnv(const char *fname,const char *envname,char *path);

void raw_mode(void);
void cocked_mode(void);
int get86key(void);
int getkey(void);
void ungetkey(int key);


/* dup2alloca(var,src,len);
 *
 * src[0]�csrc[len-1]�܂ł̗̈�� �X�^�b�N�̈�� dup ����B
 * ����(src[len])�� '\0' ���ݒ肳���B
 * dup ���ꂽ�̈�́A�������̃|�C���^�̏����l�Ƃ����B
 * �܂�A�}�N�����ŁA����|�C���^���錾�����̂ŁA
 * ���łɐ錾���ꂽ�|�C���^�ϐ���������ʖځB
 * �����A�����������c (^^;  �ȉ��̖{�̂�����������������낤�āc
 *
 * �}�N���̂����ɁAC++ �łȂ��Ǝ��p�ɂȂ�Ȃ��Ƃ�����V�ȑ㕨�ł�����B
 */
#define dup2alloc(var,src,len) \
  char *var=(char*)alloca((len)+1);(memcpy(var,src,len),var[size]='\0')


#ifndef MALLOC_ERROR
#define MALLOC_ERROR
class MallocError{};
#endif

#endif
