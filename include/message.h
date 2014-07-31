#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#define _(MSG_ID) get_message(MSG_ID)

#define MSG_LANG_ID_COUNT 2
#define MSG_LANG_ID_EN 0
#define MSG_LANG_ID_JA 1

#define MSG_ID_COUNT 27
#define MSG_ID_ERR_CREATE_JVM_UNKNOWN            1
#define MSG_ID_ERR_CREATE_JVM_EDETACHED          2
#define MSG_ID_ERR_CREATE_JVM_EVERSION           3
#define MSG_ID_ERR_CREATE_JVM_ENOMEM             4
#define MSG_ID_ERR_CREATE_JVM_EEXIST             5
#define MSG_ID_ERR_CREATE_JVM_EINVAL             6
#define MSG_ID_ERR_CREATE_JVM_ELOADLIB           7
#define MSG_ID_ERR_TARGET_VERSION                8
#define MSG_ID_ERR_DEFINE_CLASS                  9
#define MSG_ID_ERR_GET_METHOD                    10
#define MSG_ID_ERR_NEW_OBJECT                    11
#define MSG_ID_ERR_FIND_CLASSLOADER              12
#define MSG_ID_ERR_REGISTER_NATIVE               13
#define MSG_ID_ERR_LOAD_MAIN_CLASS               14
#define MSG_ID_ERR_FIND_MAIN_METHOD              15
#define MSG_ID_ERR_FIND_METHOD_SERVICE_START     16
#define MSG_ID_ERR_FIND_METHOD_SERVICE_STOP      17
#define MSG_ID_ERR_SERVICE_ABORT                 18
#define MSG_ID_SUCCESS_SERVICE_INSTALL           19
#define MSG_ID_SUCCESS_SERVICE_REMOVE            20
#define MSG_ID_SERVICE_STARTING                  21
#define MSG_ID_SERVICE_STOPING                   22
#define MSG_ID_SUCCESS_SERVICE_START             23
#define MSG_ID_SUCCESS_SERVICE_STOP              24
#define MSG_ID_CTRL_SERVICE_STOP                 25
#define MSG_ID_CTRL_SERVICE_TERMINATE            26
#define MSG_ID_CTRL_BREAK                        27

extern const char* get_message(int msg_id);

#endif
