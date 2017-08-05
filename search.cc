#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
// #include <stdlib.h>
#include "macros.h"
#include "finds.h"

const char *getShellEnv(const char *);

#undef CACHE
#ifdef CACHE
extern PathCache *script_cache;
#endif

/* �t�@�C�������݂��Ă��āA�������f�B���N�g�����łȂ���΁A�^(1)��Ԃ��B*/

static int is_file_not_dir(const char *path)
{
  if( access(path,0) != 0 )
    return 0;

  struct stat statbuf;
  if( stat(path,&statbuf)==0  && (statbuf.st_attr & 0x10)==0 )
    return 1;
  else
    return 0;
}

static int cmdexe_check(const char *path, char *tail )
{
  /* exe , cmd �ɂ��āA�ʂɑ��݂��邩���`�F�b�N���� */
  tail[0]='.'; tail[4]='\0';
  
  tail[1]='e';  tail[2]='x';  tail[3]='e';
  if( is_file_not_dir(path) )
    return EXE_FILE;
  
  tail[1]='c';  tail[2]='m';  tail[3]='d';
  if( is_file_not_dir(path) )
    return CMD_FILE;

  tail[2]='o'; tail[3]='m';
  if( is_file_not_dir(path) )
    return COM_FILE;
  
  tail[0]='\0';
  if( is_file_not_dir(path) )
    return FILE_EXISTS;

  return NO_FILE;
}

static int _SearchEnv(const char *fname,const char *envname,char *path)
{
  int absolute_path=0;

  const char *period = NULL;
  char *dp=path;
  int fnlen=0;
  
  for( const char *p=fname ; *p != '\0' ; p++,fnlen++ ){
    if( *p=='\\' || *p=='/' || *p==':' ){
      absolute_path = 1;
      period = NULL;
    }else if( *p=='.' ){
      period = p;
    }
    
    if( is_kanji(*p) )
      *dp++ = *p++;
    *dp++ = *p;
  }
  *dp = '\0';
  
  /* �g���q�����݂��Ă���ꍇ�A���̃^�C�v�𒲂ׂĂ��� */
  int suffix_type = FILE_EXISTS;
  if( period != NULL  &&  period[4] == '\0' ){
    if(   (period[1]=='e' || period[1]=='E' )
       && (period[2]=='x' || period[2]=='X' )
       && (period[3]=='e' || period[3]=='E' ) ){
      
      suffix_type = EXE_FILE ;
      
    }else if( period[1]=='c' || period[1]=='C' ){
      // �ꕶ���ڂ� C �Ȃ�� CMD �� COM
      
      if(   (period[2]=='m' || period[2]=='M' )
	 && (period[3]=='d' || period[3]=='D' ) ){
	
	suffix_type = CMD_FILE ;
	
      }else if(   (period[2]=='o' || period[2]=='O' )
	       && (period[3]=='m' || period[3]=='M' ) ){

	suffix_type = COM_FILE;
      }
    }
  }

  /* �܂��A���݂̃f�B���N�g���Œ��ׂ� */
     
  if( period==NULL ){
    /* �g���q�������ꍇ�A�⊮���Ē��ׂ� */
    int rc=cmdexe_check(path,dp);
    if( rc != NO_FILE )
      return rc;
  }else{
    /* �g���q���L��ꍇ�A���̂܂܂Œ��ׂ� */
    if( is_file_not_dir(path) )
      return suffix_type;
  }
  
  /* ��΃p�X�w��ŁA���̎��_�Ō��t�����Ă��Ȃ��ꍇ�͏I�� */
  if( absolute_path )
    return NO_FILE;

  /* �t�@�C�����݂̂̋L�q�̏ꍇ�A�������� */
#ifdef CACHE
  if( script_cache != NULL ){
    /* �L���b�V���L��̏ꍇ */
    if( period == NULL ){
      char *buf=(char*)alloca(fnlen+5);
      const char *sp=fname;
      char *dp=buf;

      while( *sp != '\0' )
	*dp++ = *sp++;

      dp[0]='.'; dp[1]='E'; dp[2]='X'; dp[3]='E'; dp[4]='\0';
      const char *result=script_cache->find(fname);
      if( result != NULL ){
	strcpy( path , result );
	return EXE_FILE;
      }
      dp[1]='C'; dp[2]='M'; dp[3]='D';
      result = script_cache->find(fname);
      if( result != NULL ){
	strcpy( path, result );
	return CMD_FILE;
      }
      dp[2]='O'; dp[3]='M';
      result = script_cache->find(fname);
      if( result != NULL ){
	strcpy( path, result );
	return COM_FILE;
      }
      dp[0] = '\0';
      result = script_cache->find(fname);
      if( result != NULL ){
	strcpy( path , result );
	return FILE_EXISTS;
      }
      return NO_FILE;
    }else{
      const char *p = script_cache->find(fname);
      if( p != NULL){
	strcpy( path , p);
	return suffix_type;
      }
      return NO_FILE;
    }
  }else{
#endif
    /* �L���b�V�������̏ꍇ */
    const char *env=getShellEnv(envname);
    if( env == NULL )
      return NO_FILE;
    
    int lastchar=0;
    dp=path;
    
    for(;;){
      if( *env != '\0' && *env != ';' ){
	if( is_kanji(lastchar=*env) )
	  *dp++ = *env++;
	*dp++ = *env++;
	continue;
      }
      if( lastchar != '/' && lastchar != '\\' )
	*dp++= '\\';
      
      /* �t�@�C�������R�s�[ */
      for( const char *sp=fname ; *sp != '\0' ; sp++ ){
	*dp++ = *sp;
      }
      *dp = '\0';
      
      if( period != NULL ){
	/* �g���q������ꍇ�́A���̂܂܂� Ok! */
	if( is_file_not_dir(path) )
	  return suffix_type;
      }else{
	/* �g���q�������ꍇ�́AEXE , CMD , �g���q�����ɂ��Ē��ׂ� */
	int rc=cmdexe_check(path,dp);
	if( rc != NO_FILE )
	  return rc;
      }
      if( *env == '\0' )
	return NO_FILE;
      
      ++env;           /* ; ���X�L�b�v */
      dp = path;
      lastchar = 0;
    }
#ifdef CACHE
  }/* �L���b�V�������̏ꍇ�̏I�� */
#endif
}

int SearchEnv(const char *fname,const char *envname,char *path)
{
  int rc=_SearchEnv(fname,envname,path);
  if( rc == NO_FILE )
    strcpy( path , fname );
  return rc;
}

#if 0
#include <stdio.h>
int main(int argc,char **argv)
{
  for(int i=1;i<argc;i++){
    char buffer[FILENAME_MAX];
    int rc=SearchEnv(argv[i],"SCRIPTPATH",buffer);
    if( rc != NO_FILE ){
      printf("%d : %s on SCRIPTPATH\n",rc,buffer);
    }
    rc=SearchEnv(argv[i],"PATH",buffer);
    if( rc != NO_FILE ){
      printf("%d : %s on PATH\n",rc,buffer);
    }
  }
  return 0;
}
#endif
