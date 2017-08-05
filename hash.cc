#include <cctype>
#include <cstring>
#include <cstdlib>

#include "substr.h"
#include "hash.h"

inline HashB::Bullet::~Bullet()
{
  free(key);
}

/* HashB::get_index --- �L�[�����񂩂�A�n�b�V���l���v�Z����
 *	p   - �L�[������
 *	len - �L�[������̒���('\0'��F������̂� �� �ł��悢)
 * return
 *	�n�b�V���l(�n�b�V���e�[�u���̃T�C�Y�ŏ��Z�ς݂̒l)
 */

int HashB::get_index(const char *p,int len)
{
  int index=0;
  while( len-- > 0  &&  *p != '\0'){
    index += (*p++ & 255);
  }
  return index % size;
}

/* HashB::get_index_without_cases
 * --- �啶���E�������𖳎����� get_index�B
 */
int HashB::get_index_without_cases(const char *p,int len)
{
  int index=0;
  while( len-- > 0  &&  *p != '\0' ){
    index += tolower(*p & 255) ; p++;
  }
  return index % size;
}

/* HashB::add 
 * �I�u�W�F�N�g rep �� key �Ƃ��ēo�^����B
 * �{���\�b�h�ł́A�d���������B
 *	flag == 0 : ���X�g�̐擪�ɑ}������B(insert)
 *	flag != 0 : ���X�g�̖����ɒǉ�����B(append)
 * return
 *	0 : ���� , ��0 : ���s(�������G���[)
 */
int HashB::add(const char *key, void *rep, int flag)
{
  if( table == NULL ){
    if( (table=new Bullet*[size]) == NULL )
      return 1;
    for(int i=0;i<size;i++)
      table[i] = NULL;
  }

  int index=get_index(key);
  Bullet *tmp=new Bullet;
  if( tmp == NULL )
    return 1;

  if( (tmp->key = strdup(key)) == NULL ){
    delete tmp;
    return 1;
  }

  tmp->rep = rep;

  if( table[index]==(Bullet*)NULL || flag==0 ){
    tmp->next = table[index];
    table[index] = tmp;
  }else{
    tmp->next = NULL;
    Bullet *pre=table[index];
    while( pre->next != NULL )
      pre = pre->next;
    pre->next = tmp;
  }
  return 0;
}

/* HashB �� [] ���Z�q --- �L�[�l����I�u�W�F�N�g����������B
 *	key �L�[�l 
 * return
 *	��NULL �c �I�u�W�F�N�g�ւ̃|�C���^
 *	NULL   �c �}�b�`����I�u�W�F�N�g�͖��������B
 */
void *HashB::operator[](const char *key)
{
  if( table == NULL ) return NULL;
  int index=get_index(key);
  
  for(Bullet *ptr=table[index] ; ptr != NULL ; ptr=ptr->next ){
    if( ptr->key[0]==key[0]  &&  strcmp(ptr->key,key)==0  ){
      return ptr->rep;
    }
  }
  return NULL;
}

/* HashB �� [] ���Z�q (SubStr�Łcchar�łƂ���Ă邱�Ƃ͓���)
 */
void *HashB::operator[](const Substr &key)
{
  if( table==NULL  ||  key.len==0 ) return NULL;
  int index=get_index(key.ptr,key.len);
  
  for(Bullet *cur=table[index] ; cur != NULL ; cur=cur->next ){
    if( cur->key[0]==key.ptr[0]
       && memcmp(cur->key , key.ptr , key.len)==0 
       && cur->key[key.len] == '\0' ){
      return cur->rep;
    }
  }
  return NULL;
}

/* �啶������������ʂ����Ɍ������郁�\�b�h�B
 * �啶���������̋�ʂ��邵�Ȃ��ȊO�� [] �Ɠ���
 */
void *HashB::lookup_tolower(const char *key)
{
  if( table==NULL  ) return NULL;
  int index=get_index_without_cases(key);

  int firstletter=tolower(*key & 255 );
  for(Bullet *cur=table[index] ; cur != NULL ; cur=cur->next ){
    if(   cur->key[0]==firstletter  && stricmp(cur->key,key) == 0  ){
      return cur->rep;
    }
  }
  return NULL;
}

/* �啶������������ʂ����Ɍ������郁�\�b�h(SubStr��)�B
 * �啶���������̋�ʂ��邵�Ȃ��ȊO�� [] �Ɠ���
 */
void *HashB::lookup_tolower(const Substr &key)
{
  if( table==NULL  ||  key.len == 0 ) return NULL;
  int index=get_index_without_cases(key.ptr,key.len);

  int firstletter=tolower(key.ptr[0] & 255 );
  for(Bullet *cur=table[index] ; cur != NULL ; cur=cur->next ){
    if(   cur->key[0]==firstletter 
       && memicmp(cur->key , key.ptr , key.len )==0
       && cur->key[key.len] == '\0' ){
      return cur->rep;
    }
  }
  return NULL;
}

/* �L�[�l�Ƀ}�b�`����I�u�W�F�N�g��Hash���珜�����\�b�h�B
 * destruct_flag 
 *	0   �I�u�W�F�N�g�� Hash ���珜�������B
 *	��0 �I�u�W�F�N�g�� Hash ���珜������ŁAdelete ����B
 *	    (���ۂ� delete �͉��z�֐��� delete_node ���Ăяo��)
 * return
 *	0 �폜�ł����I
 *	1 �폜�ł��Ȃ������I(�}�b�`����I�u�W�F�N�g����������)
 */
int HashB::remove(const char *key, bool destruct_flag)
{
  if( table == NULL ) return 1;
  int index=get_index(key);
  
  if( table[index] == NULL ){
    return 1;
  }else if(   table[index]->key[0] == key[0] 
	   && strcmp(table[index]->key,key)==0 ){
    
    Bullet *tmp=table[index]->next;
    delete table[index];
    table[index] = tmp;
    return 0;
  }else{
    Bullet *cur;
    for(Bullet *pre=table[index] ; (cur=pre->next) != NULL ; pre=cur ){
      if( cur->key[0] == key[0] && strcmp(cur->key,key)==0 ){
	pre->next = cur->next;
	if( destruct_flag )
	  delete_node( cur->rep );
	delete cur;
	return 0;
      }
    }
    return 1;
  }
}

/* �L�[�l�Ƀ}�b�`����I�u�W�F�N�g��Hash���珜�����\�b�h�B
 * SubStr ���L�[�Ɏg���ȊO�́Achar[]�łƓ����B
 */
int HashB::remove(const Substr &key,bool destruct_flag)
{
  if( table == NULL ) return 1;
  int index=get_index(key.ptr,key.len);
  
  if( table[index] == NULL ){
    return 1;
  }else if( table[index]->key[0] == key.ptr[0] 
	   && memcmp(table[index]->key,key.ptr , key.len )==0
	   && table[index]->key[key.len] == '\0' ){
    
    Bullet *tmp=table[index]->next;
    if( destruct_flag )
      delete_node( table[index]->rep );
    delete table[index];
    table[index] = tmp;
    return 0;
  }else{
    Bullet *cur;
    for(Bullet *pre=table[index] ; (cur=pre->next) != NULL ; pre=cur ){
      if(   cur->key[0] == key.ptr[0] 
	 && memcmp(cur->key,key.ptr,key.len)==0
	 && cur->key[key.len] == '\0' ){
	pre->next = cur->next;
	if( destruct_flag )
	  delete_node( cur->rep );
	delete cur;
	return 0;
      }
    }
    return 1;
  }
}

/* �n�b�V������A�S�ẴC���X�^���X�����O����B
 * destruct_flag
 *	0    ���O���邾��
 *	��0  ���O������ŁA�C���X�^���X�ɑ΂��Adelete �����s����B
 */
void HashB::remove_all(bool destruct_flag)
{
  if( table != NULL ){
    for(int i=0; i<size; i++){
      Bullet *tmp;
      for(Bullet *ptr=table[i]; ptr != NULL ; ptr=tmp ){
	tmp=ptr->next;
	if( destruct_flag )
	  delete_node(ptr->rep);
	delete ptr;
      }
    }
    delete table;
    table = NULL;
  }
}

void *HashPtr::preptr = NULL;

HashPtr::HashPtr(HashB &h) : hash(h)
{
  if( hash.table != NULL ){
    for(index = 0 ; index < hash.size ; index++){
      if( hash.table[index] != NULL ){
	ptr = hash.table[index];
	return;
      }
    }
  }
  ptr = NULL;
}

HashPtr &HashPtr::operator++()
{
  if( ptr == NULL )
    return *this;

  if( ptr->next != NULL ){
    ptr = ptr->next;
    return *this;
  }
  while( ++index < hash.size ){
    if( hash.table[index] != NULL ){
      ptr = hash.table[index];
      return *this;
    }
  }
  ptr = NULL;
  return *this;
}
