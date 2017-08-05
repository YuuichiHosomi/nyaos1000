/* -*- c++ -*- */
#ifndef HASH_H
#define HASH_H

/* Hash --- �����L�[��char[]�^������Ɍ��肵���n�b�V���R���e�i(template)
 *
 * �y�n�b�V���̐錾�z
 * Hash <Object> hashbuf; // �錾����B
 * Object *object; // �|�C���^�ł���̂ɒ���
 *
 * hasubuf.insert("�L�[",object);
 *	�o�^�F����L�[������ꍇ�A�Ō�ɓo�^�������̂�D�悳����B�d�������B
 *
 * hashbuf.append("�L�[",object);
 *	�o�^�F����L�[������ꍇ�A�ŏ��ɓo�^�������̂�D�悳����B�d�������B
 *
 * hashbuf["�L�["]
 *	�����F�L�[�Ƀ}�b�`����I�u�W�F�N�g��Ԃ��B�����ꍇ�� NULL
 *
 * hashbuf.remove("�L�[");
 *	�L�[�Ƀ}�b�`����ŏ��̃I�u�W�F�N�g���e�[�u����菜���Bdelete ���Ȃ��B
 *
 * hashbuf.destruct("�L�[");
 *	�L�[�Ƀ}�b�`����ŏ��̃I�u�W�F�N�g���e�[�u�����珜���Adelete ����B
 *
 * hashbuf.remove_all();
 *	�S�ẴI�u�W�F�N�g���e�[�u�����珜���B
 *
 * hashbuf.destruct_all();
 *	�S�ẴI�u�W�F�N�g���e�[�u�����珜���āAdelete ����B
 *
 * insert/append �ŗp����L�[�ɂ��ẮAHash���� strdup/free ����(�V�ł��)�B
 *
 * HashPtr --- Hash �̑S�Ă� �L�[�~�I�u�W�F�N�g�̑΂�񋓂��邽�߂̃N���X�B
 *             Iterator �Ƃ����قǂ̂��̂ł͂Ȃ��B
 */

class Substr;
class HashPtr;

class HashB{
friend class HashPtr;
  struct Bullet{
    Bullet *next;
    char *key;
    void *rep;

    Bullet() : next(0),key(0),rep(0){ }
    ~Bullet();
  }**table;
  int size;

  int get_index(const char *key,int len=1000);
  int get_index_without_cases(const char *key,int len=1000);
protected:
  virtual void delete_node(void *){ };
  int add(const char *key, void *rep,int flag);
  int insert(const char *key, void *rep){ return add(key,rep,0); }
  int append(const char *key, void *rep){ return add(key,rep,1); }
  void *operator[](const char *key);
  void *operator[](const Substr &s);
  void *lookup_tolower(const Substr &s);
  void *lookup_tolower(const char *s);
public:
  int remove(const char *key   ,bool do_delete=false );
  int remove(const Substr &key ,bool do_delete=false );
  void remove_all(bool do_delete=false);
  
  HashB(int s) : table((Bullet**)NULL) , size(s) { }
  virtual ~HashB(){ }
};

template <class T>
class Hash : public HashB{
  void delete_node(void *one){ delete (T*)one; }
public:
  int insert(const char *key,T *rep){ return add(key,rep,0); }
  int append(const char *key,T *rep){ return add(key,rep,1); }
  T *operator[](const char *key){ return (T*)HashB::operator[](key); }
  T *operator[](const Substr &key){ return (T*)HashB::operator[](key); }
  T *lookup_tolower(const Substr &key){ return (T*)HashB::lookup_tolower(key);}
  T *lookup_tolower(const char *key){ return (T*)HashB::lookup_tolower(key);}
  
  int  destruct(const char   *key){ return remove(key,true); }
  int  destruct(const Substr &key){ return remove(key,true); }
  void destruct_all(){ remove_all(true); }
  
  Hash(int i) : HashB(i) { }
};

/* Hash �ɓo�^����Ă���S�ẴI�u�W�F�N�g���Q�Ƃ��邽�߂̃N���X�B
 * 
 */

class HashPtr{
  static void *preptr;
  HashB &hash;
  HashB::Bullet *ptr;
  int index;
  HashPtr(void);
public:
  HashPtr(HashB &h);
  
  void *operator*(){ return ptr != NULL ? ptr->rep : NULL ; }
  HashPtr &operator++();
  void **operator++(int)
    { preptr=**this; ++*this; return &preptr; }
};

template <class T>
class HashIndex : public HashPtr{
public:
  HashIndex(Hash<T> &h) : HashPtr(h) {}
  T *operator*(){ return (T*)HashPtr::operator*(); }
  T *operator->(){ return (T*)HashPtr::operator*(); }
};

#endif
