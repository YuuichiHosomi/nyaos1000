#include <stddef.h>
#include "macros.h"
#include "strtok.h"

/* p[0] (�����̏ꍇ��p[0],p[1])�ɂ��镶�����Adem �̒��Ɋ܂܂�Ă���
 * �Ȃ�� 1 �A�����Ȃ���� 0 ��Ԃ�
 */
static int has_a_char(const char *p,const char *dem)
{
  while( *dem != '\0' ){
    if( *p == *dem  &&  ( ! is_kanji(*p) || p[1] == dem[1] ) )
      return 1;
    
    if( is_kanji(*dem) )
      ++dem;
    ++dem;
  }
  return 0;
}

/* strtok �Ƃ�������B�������Astrtok�̑������ɂ����镶�����
 * �C���X�^���X���L�����Ă���B
 *  in	dem �f�~���^
 */
char *Strtok::cut_with(const char *dem)
{
  if( p==NULL )
    return NULL;
  
  // �擪�̃f�~���^�������ǂ݂Ƃ΂��B
  for(;;){
    if( *p=='\0' )
      return p=NULL;

    if( ! has_a_char(p,dem) )
      break;
    
    ++p;
  }
  // ���̃f�~���^������T�������B
  char *top=p;
  for(;;){
    if( *p == '\0' ){
      p = NULL;
      return top;
    }
    if( has_a_char(p,dem) ){
      *p++ = '\0';
      return top;
    }
    if( is_kanji(*p) )
      ++p;
    ++p;
  }
}
