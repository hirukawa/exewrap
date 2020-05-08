#include <windows.h>
#include <locale.h>
#include <mbctype.h>
#include "include/message.h"

static const DWORD LCID_ja_JP = MAKELCID(MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN), SORT_DEFAULT);

const wchar_t* get_message(int msg_id);

static int   get_lang_index(void);
static void  init_message(void);

static int   lang_index = -1;
static const wchar_t** msg;

const wchar_t* get_message(int msg_id)
{
	if(lang_index < 0)
	{
		lang_index = get_lang_index();
		init_message();
	}

	return msg[MSG_ID_COUNT * lang_index + msg_id];
}

static int get_lang_index()
{
	DWORD lcid = GetUserDefaultLCID();

	if(lcid == LCID_ja_JP)
	{
		return MSG_LANG_INDEX_JA;
	}
	return MSG_LANG_INDEX_EN;
}

static void init_message()
{
	size_t size = MSG_LANG_INDEX_COUNT * (MSG_ID_COUNT + 1);

	msg = (const wchar_t**)calloc(size, sizeof(wchar_t*));

	//
	// ENGLISH
	//
	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_CREATE_JVM_UNKNOWN] =
	L"Failed to create the Java Virtual Machine. An unknown error has occurred.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_CREATE_JVM_EDETACHED] =
	L"Failed to create the Java Virtual Machine. Thread detached from the Java Virtual Machine.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_CREATE_JVM_EVERSION] =
	L"Failed to create the Java Virtual Machine. JNI version error.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_CREATE_JVM_ENOMEM] =
	L"Failed to create the Java Virtual Machine. Not enough memory available to create the Java Virtual Machine.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_CREATE_JVM_EEXIST] =
	L"Failed to create the Java Virtual Machine. The Java Virtual Machine already exists.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_CREATE_JVM_EINVAL] =
	L"Failed to create the Java Virtual Machine. An invalid argument was supplied.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_CREATE_JVM_ELOADLIB] =
	L"Failed to create the Java Virtual Machine. Failed to load jvm.dll.\r\n"
	L"%d-bit Java Virtual Machine required.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_UNCAUGHT_EXCEPTION] =
	L"Uncaught exception occured.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_RESOURCE_NOT_FOUND] =
	L"Resource not found: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_TARGET_VERSION] =
	L"%ls or higher is required to run this program.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_DEFINE_CLASS] =
	L"Class not found: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_FIND_CLASS] =
	L"Class not found: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_GET_CONSTRUCTOR] =
	L"Constructor not found: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_GET_FIELD] =
	L"Field not found: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_GET_METHOD] =
	L"Method not found: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_NEW_OBJECT] =
	L"Failed to create object: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_NULL_OBJECT] =
	L"Object is null: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_FIND_CLASSLOADER] =
	L"Class Loader not found.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_REGISTER_NATIVE] =
	L"Failed to register native methods: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_LOAD_MAIN_CLASS] =
	L"Failed to load the Main Class: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_FIND_MAIN_CLASS] =
	L"Main class not found.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_FIND_MAIN_METHOD] =
	L"Main method is not implemented.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_FIND_METHOD_SERVICE_START] =
	L"The service could not be started. Requires to define a method within main class: public static void start(String[] args)";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_FIND_METHOD_SERVICE_STOP] =
	L"The service could not be started. Requires to define a method within main class: public static void stop()";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_SERVICE_ABORT] =
	L"%ls service was terminated abnormally.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_ERR_SERVICE_NOT_STOPPED] =
	L"%ls service can't be deleted because it is not stopped.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_SUCCESS_SERVICE_INSTALL] =
	L"%ls service installed.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_SUCCESS_SERVICE_REMOVE] =
	L"%ls service was removed.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_SERVICE_STARTING] =
	L"%ls service is starting.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_SERVICE_STOPING] =
	L"%ls service is stopping.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_SUCCESS_SERVICE_START] =
	L"The %ls service was started successfully.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_SUCCESS_SERVICE_STOP] =
	L"The %ls service was stopped successfully.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_CTRL_SERVICE_STOP] =
	L"%ls: The service is stopping.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_CTRL_SERVICE_TERMINATE] =
	L"%ls: The service is terminating.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_EN + MSG_ID_CTRL_BREAK] =
	L"%ls: ";

	//
	// JAPANESE
	//
	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_CREATE_JVM_UNKNOWN] =
	L"JavaVM���쐬�ł��܂���ł����B�s���ȃG���[���������܂����B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_CREATE_JVM_EDETACHED] =
	L"JavaVM���쐬�ł��܂���ł����B�X���b�h��JavaVM�ɐڑ�����Ă��܂���B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_CREATE_JVM_EVERSION] =
	L"JavaVM���쐬�ł��܂���ł����B�T�|�[�g����Ă��Ȃ��o�[�W�����ł��B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_CREATE_JVM_ENOMEM] =
	L"JavaVM���쐬�ł��܂���ł����B���������s�����Ă��܂��B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_CREATE_JVM_EEXIST] =
	L"JavaVM���쐬�ł��܂���ł����BJavaVM�͊��ɍ쐬����Ă��܂��B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_CREATE_JVM_EINVAL] =
	L"JavaVM���쐬�ł��܂���ł����B�������s���ł��B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_CREATE_JVM_ELOADLIB] =
	L"JavaVM���쐬�ł��܂���ł����Bjvm.dll�����[�h�ł��܂���ł����B\r\n"
	L"���̃v���O�����̎��s�ɂ� %d-bit JavaVM ���K�v�ł��B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_UNCAUGHT_EXCEPTION] =
	L"�⑫����Ȃ���O���������܂����B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_RESOURCE_NOT_FOUND] =
	L"���\�[�X��������܂���: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_TARGET_VERSION] =
	L"���̃v���O�����̎��s�ɂ� %ls �ȏオ�K�v�ł��B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_DEFINE_CLASS] =
	L"�N���X��������܂���: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_FIND_CLASS] =
	L"�N���X��������܂���: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_GET_CONSTRUCTOR] =
	L"�R���X�g���N�^�[��������܂���: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_GET_FIELD] =
	L"�t�B�[���h��������܂���: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_GET_METHOD] =
	L"���\�b�h��������܂���: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_NEW_OBJECT] =
	L"�I�u�W�F�N�g�̍쐬�Ɏ��s���܂���: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_NULL_OBJECT] =
	L"�I�u�W�F�N�g�Q�Ƃ�����܂���: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_FIND_CLASSLOADER] =
	L"�N���X���[�_�[��������܂���B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_REGISTER_NATIVE] =
	L"�l�C�e�B�u���\�b�h�̓o�^�Ɏ��s���܂���: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_LOAD_MAIN_CLASS] =
	L"���C���N���X�̃��[�h�Ɏ��s���܂���: %ls";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_FIND_MAIN_CLASS] =
	L"���C���N���X��������܂���B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_FIND_MAIN_METHOD] =
	L"main���\�b�h����������Ă��܂���B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_FIND_METHOD_SERVICE_START] =
	L"�T�[�r�X���J�n�ł��܂���BWindows�T�[�r�X�́A���C���N���X�� public static void start(String[] args) ����������K�v������܂��B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_FIND_METHOD_SERVICE_STOP] =
	L"�T�[�r�X���J�n�ł��܂���BWindows�T�[�r�X�́A���C���N���X�� public static void stop() ����������K�v������܂��B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_SERVICE_ABORT] =
	L"%ls �T�[�r�X�ُ͈�I�����܂����B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_ERR_SERVICE_NOT_STOPPED] =
	L"%ls �T�[�r�X����~���Ă��Ȃ����߁A�폜�ł��܂���B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_SUCCESS_SERVICE_INSTALL] =
	L"%ls �T�[�r�X���C���X�g�[�����܂����B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_SUCCESS_SERVICE_REMOVE] =
	L"%ls �T�[�r�X���폜���܂����B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_SERVICE_STARTING] =
	L"%ls �T�[�r�X���J�n���܂�.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_SERVICE_STOPING] =
	L"%ls �T�[�r�X���~���ł�.";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_SUCCESS_SERVICE_START] =
	L"%ls �T�[�r�X�͐���ɊJ�n����܂����B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_SUCCESS_SERVICE_STOP] =
	L"%ls �T�[�r�X�͐���ɒ�~����܂����B";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_CTRL_SERVICE_STOP] =
	L"%ls: �T�[�r�X���~���ł�...";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_CTRL_SERVICE_TERMINATE] =
	L"%ls: �����I�����܂�...";

	msg[MSG_ID_COUNT * MSG_LANG_INDEX_JA + MSG_ID_CTRL_BREAK] =
	L"%ls: ";
}
