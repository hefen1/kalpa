#include "tools/profiler/log_daemon.h"

#include "base/process/kill.h"

#include "tools/profiler/profiler_thread.h"
#include "tools/profiler/profiler_ipc_messages.h"
#include "tools/profiler/profiler_process.h"

//////////////////////////////////////////////////////////////////////////
LogDaemon::LogDaemon()
:deleting_soon_(false),
on_channel_error_called_(false)
{

}

LogDaemon::~LogDaemon()
{
  deleting_soon_ = true;

  if(channel_.get()){
    channel_->Close();
    channel_->ClearIPCTaskRunner();
  }
  channel_.reset();
  while (!queued_messages_.empty()) {
    delete queued_messages_.front();
    queued_messages_.pop();
  }
}

//////////////////////////////////////////////////////////////////////////
bool LogDaemon::Init()
{
  if (channel_.get())
    return true;

  channel_.reset(new IPC::ChannelProxy(
    "profiler_logd",
    IPC::Channel::MODE_SERVER, 
    this, 
    ProfilerThread::GetMessageLoopProxyForThread(ProfilerThread::IO)));

  return true;
}

bool LogDaemon::Send(IPC::Message* msg)
{
  if(channel_.get()){
    return channel_->Send(msg);
  }

  queued_messages_.push(msg);
  return true;
}

bool LogDaemon::OnMessageReceived(const IPC::Message& msg) {
  if (deleting_soon_)
    return false;

  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    bool msg_is_ok = true;
    IPC_BEGIN_MESSAGE_MAP_EX(LogDaemon, msg, msg_is_ok)
      IPC_MESSAGE_HANDLER(SyncChannelTestMsg_NoArgs,OnSyncChannelTestMsg)
      IPC_MESSAGE_HANDLER(ChannelTestMsg_NoArgs,OnChannelTestMsg)
      IPC_MESSAGE_HANDLER(ProfilerAction,OnProfilerActionMsg)
      IPC_MESSAGE_UNHANDLED_ERROR()
    IPC_END_MESSAGE_MAP_EX()

    if (!msg_is_ok) {
      LOG(ERROR) << "bad message " << msg.type() << " terminating renderer.";
      ReceivedBadMessage();
    }
    return true;
  }

  IPC::Listener* listener = GetListenerByID(msg.routing_id());
  if (!listener) {
    if (msg.is_sync()) {
      IPC::Message* reply = IPC::SyncMessage::GenerateReply(&msg);
      reply->set_reply_error();
      Send(reply);
    }
    return true;
  }
  return listener->OnMessageReceived(msg);
}

//首先主进程给子进程发一个异步的
void LogDaemon::OnChannelConnected(int32 peer_pid) {
  //Send(new ChannelTestMsg_NoArgs());
}

void LogDaemon::OnChannelError() {
  if (!channel_.get())
    return;

  channel_->Close();
  channel_->ClearIPCTaskRunner();
  channel_.reset();

  IDMap<IPC::Listener>::iterator iter(&listeners_);
  while (!iter.IsAtEnd()) {
    //iter.GetCurrentValue()->OnMessageReceived(
    //  ViewHostMsg_RenderViewGone(iter.GetCurrentKey(),
    //  static_cast<int>(status),
    //  exit_code));
    iter.Advance();
  }

  //重新初始化
  if(!deleting_soon_){
    Init();
  }
}

void LogDaemon::ReceivedBadMessage() {
  NOTREACHED();
  base::KillProcess(NULL,0,false);
}

void LogDaemon::Attach(IPC::Listener* listener,
                               int routing_id) 
{
  listeners_.AddWithID(listener, routing_id);
}

void LogDaemon::Release(int listener_id) {
  DCHECK(listeners_.Lookup(listener_id) != NULL);
  listeners_.Remove(listener_id);

  if (listeners_.IsEmpty()) {
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
    deleting_soon_ = true;
  }
}
//////////////////////////////////////////////////////////////////////////

void LogDaemon::OnSyncChannelTestMsg()
{
  //Send(new ChannelTestMsg_NoArgs);
}

//这里会死锁,都proxy到ui上来，然后client在ui上发了一个同步消息，server通过proxy
//也要得到ui，死锁
void LogDaemon::OnChannelTestMsg()
{
}

void LogDaemon::OnProfilerActionMsg(int64 time_ms,int64 delta_ms,int64 id,
                                    const string16& action,const string16& data)
{
  int tid = (id & 0xffffffff);
  int pid = (id >> 32);
  g_profiler_process->profiler_data()->Action(
    time_ms,delta_ms,
    pid,tid,
    action,data);
}
//////////////////////////////////////////////////////////////////////////