/*
    Purpose:
    Author: Reece W.
    License: All Rights Reserved J. Reece Wilson (See License.txt)
*/
#pragma once

enum ThreadMessageType_e
{
    kMsgThreadCreate,
    kMsgThreadStart,
    kMsgThreadExit
};

typedef struct ThreadMsg_s
{
    ThreadMessageType_e type;
    union
    {
        struct
        {
            void *  data;            // IN: "void * data" argument as presented to SpawnOThread
            int32_t code;            // OUT: thread exit code [assuming the thread doesn't get killed]
        } start;
        struct
        {
            void *    data;          // IN: "void * data" argument as presented to SpawnOThread 
            OThread * thread;        // IN: life span = whenever the SpawnOThreads callee causes the OOutlivableRefs internal reference counter to hit zero; the container backing OOutlivableRef<OThread> has not obtained control yet.
            uint32_t  thread_id;
        } create;
        struct
        {
            int32_t  code;           // IN: thread exit code
            uint32_t thread_id;      // IN: thread id
        } exit;
    };
} *ThreadMsg_p,
*ThreadMsg_ref,
ThreadMsg_t;

typedef void(*OThreadEP_t)(ThreadMsg_ref);
error_t LIBLINUX_SYM SpawnOThread(const OOutlivableRef<OThread> & thread, OThreadEP_t entrypoint, const char * name, void * data);
