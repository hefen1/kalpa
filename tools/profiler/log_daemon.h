#pragma once

#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"

#include "ipc/ipc_channel.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_listener.h"

//using IPC::Channel;
//using IPC::Message;
//using IPC::SyncChannel;
//using IPC::ChannelProxy;

//logdaemon������mainthread��ʼ��,msg�Ĵ���Ҳ��mainthread��ͨ��proxy
//����дһ��messagefilter���ػ�ipcthread�ϵ���Ϣ������ipcthread�ϴ���
//
class LogDaemon : public IPC::Listener, public IPC::Sender{
public:
  LogDaemon();
  virtual ~LogDaemon();

public:
  bool Init();
  bool HasConnection() { return channel_.get() != NULL; }
  void Attach(IPC::Listener* listener, int routing_id);
  void Release(int listener_id);
  typedef IDMap<IPC::Listener>::const_iterator listeners_iterator;

  listeners_iterator ListenersIterator() {
    return listeners_iterator(&listeners_);
  }

  IPC::Listener* GetListenerByID(int routing_id) {
    return listeners_.Lookup(routing_id);
  }

  IPC::ChannelProxy* channel() { return channel_.get(); }

  // IPC::Channel::Sender via LogDaemon.��mainthread��
  virtual bool Send(IPC::Message* msg);

  // IPC::Channel::Listener via LogDaemon.
  virtual bool OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnChannelError();

private:
  void ReceivedBadMessage();

private:
  void OnSyncChannelTestMsg();
  void OnChannelTestMsg();
  void OnProfilerActionMsg(int64,int64,int64,const string16& action,const string16& data);

private:
  //ChannelProxy��Channel�ϲ��ܷ�ͬ����Ϣ
  //Ҫ��ͬ����Ϣ��ֻ����SyncChannel
  scoped_ptr<IPC::ChannelProxy> channel_;
  IDMap<IPC::Listener> listeners_;

  std::queue<IPC::Message*> queued_messages_;
  bool deleting_soon_;
  bool on_channel_error_called_;

  DISALLOW_COPY_AND_ASSIGN(LogDaemon);
};