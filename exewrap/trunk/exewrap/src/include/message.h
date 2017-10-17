#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#define _(MSG_ID) get_message(MSG_ID)

#define MSG_LANG_ID_COUNT 2
#define MSG_LANG_ID_EN 0
#define MSG_LANG_ID_JA 1

#define MSG_ID_COUNT 35
#define MSG_ID_ERR_CREATE_JVM_UNKNOWN            1
#define MSG_ID_ERR_CREATE_JVM_EDETACHED          2
#define MSG_ID_ERR_CREATE_JVM_EVERSION           3
#define MSG_ID_ERR_CREATE_JVM_ENOMEM             4
#define MSG_ID_ERR_CREATE_JVM_EEXIST             5
#define MSG_ID_ERR_CREATE_JVM_EINVAL             6
#define MSG_ID_ERR_CREATE_JVM_ELOADLIB           7
#define MSG_ID_ERR_UNCAUGHT_EXCEPTION            8
#define MSG_ID_ERR_RESOURCE_NOT_FOUND            9
#define MSG_ID_ERR_TARGET_VERSION                10
#define MSG_ID_ERR_DEFINE_CLASS                  11
#define MSG_ID_ERR_FIND_CLASS                    12
#define MSG_ID_ERR_GET_FIELD                     13
#define MSG_ID_ERR_GET_METHOD                    14
#define MSG_ID_ERR_GET_CONSTRUCTOR               15
#define MSG_ID_ERR_NEW_OBJECT                    16
#define MSG_ID_ERR_NULL_OBJECT                   17
#define MSG_ID_ERR_FIND_CLASSLOADER              18
#define MSG_ID_ERR_REGISTER_NATIVE               19
#define MSG_ID_ERR_LOAD_MAIN_CLASS               20
#define MSG_ID_ERR_FIND_MAIN_CLASS               21
#define MSG_ID_ERR_FIND_MAIN_METHOD              22
#define MSG_ID_ERR_FIND_METHOD_SERVICE_START     23
#define MSG_ID_ERR_FIND_METHOD_SERVICE_STOP      24
#define MSG_ID_ERR_SERVICE_ABORT                 25
#define MSG_ID_ERR_SERVICE_NOT_STOPPED           26
#define MSG_ID_SUCCESS_SERVICE_INSTALL           27
#define MSG_ID_SUCCESS_SERVICE_REMOVE            28
#define MSG_ID_SERVICE_STARTING                  29
#define MSG_ID_SERVICE_STOPING                   30
#define MSG_ID_SUCCESS_SERVICE_START             31
#define MSG_ID_SUCCESS_SERVICE_STOP              32
#define MSG_ID_CTRL_SERVICE_STOP                 33
#define MSG_ID_CTRL_SERVICE_TERMINATE            34
#define MSG_ID_CTRL_BREAK                        35

extern const char* get_message(int msg_id);

#endif
