/* -*- c++ -*- */

class ErrMsg{
public:
  enum MsgNo {
    ChangeDriveError,		/* �ux:�v�ŁA���̃h���C�u�ֈړ��ł��Ȃ� */
    TooNearTerminateChar,	/* �u&�v��u|�v���߂����� */
    InternalError,		/* ��̓����G���[ */
    CantRemoveFile,		/* �t�@�C�����폜�ł��Ȃ� */
    CantMakeDir,		/* �f�B���N�g�������Ȃ� */

    CantWriteSubject,		/* .SUBJECT �֏������߂Ȃ� */
    CantWriteComments,		/* .COMMENTS �֏������߂Ȃ� */
    InvalidKeyName,		/* �L�[�̖��O�����Ⴄ����[�� */
    InvalidKeySetName,		/* �L�[�Z�b�g�̖��O���Ⴄ */
    InvalidFuncName,		/* �@�\�����Ⴄ */

    TooLongVarName,		/* ���ϐ������������� */
    MustNotInputRedirect,	/* �����R�}���h�ł͓��̓��_�C���N�g�ł��Ȃ� */
    InvalidVarName,		/* ���ϐ������� */
    CantOutputRedirect,		/* �o�̓��_�C���N�g�ł��Ȃ����� */
    TooFewArguments,		/* ����������Ȃ� */

    TooManyNesting,		/* �l�X�g��������(source) */
    NoSuchFile,			/* ����ȃt�@�C����������[�� */
    NoSuchAlias,		/* ����ȕʖ��n�߂���Ȃ��Ł[ */
    BadHomeDir,			/* %HOME% ���������� */
    NoSuchDir,			/* ����ȃf�B���N�g����������[�� */
				   
    UnknownOption,		/* ����ȃI�v�V�����m���Ł[ */
    DirStackEmpty,		/* �X�^�b�N�͋�������[�� */
    NoSuchFileOrDir,		/* �t�@�C�����f�B���N�g������������[�� */
    CantGetCurDir,		/* �����͂ǂ��A���͒N */
    TooLargeStackNo,		/* �X�^�b�N�ԍ����傫���� */
    
    Missing,			/* ����͂��̕������ˁ[�悧 */
    NoSuchEnvVar,		/* ����Ȋ��ϐ��ˁ[�� */
    ErrorInForeach,		/* foreach ���s�v���O�����ɃG���[ */
    NotAvailableInRexx,		/* REXX���ł͎g���Ȃ� */
    MemoryAllocationError,	/* malloc �Ń~�X�b���̂� */

    NotSupportVDM,		/* ���͂� VDM �̓T�|�[�g���Ă��Ȃ� */
    DBCStableCantGet,		/* DBCS�\��OS���擾�ł��Ȃ� */
    WhereIsCmdExe,		/* CMD.EXE �������� */
    BadParameter,		/* �Ƃɂ����p�����[�^������(���̂�������) */
    AmbiguousRedirect,		/* �ǂ����Ƀ��_�C���N�g����˂� */

    EventNotFound,		/* ����ȃq�X�g���͂˂����� */
    TooManyErrors,		/* �G���[���������� */
    FileExists,			/* �t�@�C�����㏑�����悤�Ƃ��Ă��� */
#ifdef VMAX
    BadCommandOrFileName	/* �R�}���h�܂��̓t�@�C�������Ⴂ�܂��B*/
#endif
    };
  static void say(int x,...);
  static void mount(int x,const char *s);
};

extern int source_errmsg(const char *fname);

