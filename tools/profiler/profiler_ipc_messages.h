#pragma once

#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START TestMsgStart

IPC_SYNC_MESSAGE_CONTROL0_0(SyncChannelTestMsg_NoArgs)
IPC_SYNC_MESSAGE_ROUTED0_0(SyncRoutedTestMsg_NoArgs)
IPC_MESSAGE_CONTROL0(ChannelTestMsg_NoArgs)
IPC_MESSAGE_ROUTED0(RoutedTestMsg_NoArgs)

//time delta action data pid tid
IPC_MESSAGE_CONTROL5(ProfilerAction,
                     int64,int64,int64,
                     string16,string16)

//pid
IPC_MESSAGE_CONTROL1(ProfilerChannel,
                     int)