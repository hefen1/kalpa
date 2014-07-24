#include "tools/profiler_dll/log_client.h"

#include "base/process/kill.h"

#include "tools/profiler_dll/profiler_thread.h"
#include "tools/profiler_dll/profiler_ipc_messages.h"

//////////////////////////////////////////////////////////////////////////
LogClient::LogClient()
:deleting_soon_(false),
shutdown_event_(true, false),
check_with_browser_before_shutdown_(false),
on_channel_error_called_(false)
{
}

LogClient::~LogClient()
{
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
bool LogClient::Init()
{
  if (channel_.get())
    return true;

  check_with_browser_before_shutdown_=false;
  on_channel_error_called_ = false;

  channel_.reset(new IPC::SyncChannel(
    "profiler_logd",
    IPC::Channel::MODE_CLIENT, 
    this, 
    ProfilerThread::GetMessageLoopProxyForThread(ProfilerThread::IO),true,&shutdown_event_));

  return true;
}

bool LogClient::Send(IPC::Message* msg)
{
  if(channel_.get()){
    return channel_->Send(msg);
  }

  queued_messages_.push(msg);
  return true;
}

bool LogClient::OnMessageReceived(const IPC::Message& msg) {
  if (deleting_soon_)
    return false;

  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    bool msg_is_ok = true;
    IPC_BEGIN_MESSAGE_MAP_EX(LogClient, msg, msg_is_ok)
      IPC_MESSAGE_HANDLER(SyncChannelTestMsg_NoArgs,OnSyncChannelTestMsg)
      IPC_MESSAGE_HANDLER(ChannelTestMsg_NoArgs,OnChannelTestMsg)
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
void LogClient::OnChannelConnected(int32 peer_pid) {
  while (!queued_messages_.empty()) {
    Send(queued_messages_.front());
    queued_messages_.pop();
  }
}

void LogClient::OnChannelError() {
  if (!channel_.get())
    return;

  on_channel_error_called_ = true;

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
}

void LogClient::ReceivedBadMessage() {
  NOTREACHED();
  base::KillProcess(NULL,0,false);
}

void LogClient::Attach(IPC::Listener* listener,
                       int routing_id) 
{
  listeners_.AddWithID(listener, routing_id);
}

void LogClient::Release(int listener_id) {
  DCHECK(listeners_.Lookup(listener_id) != NULL);
  listeners_.Remove(listener_id);

  if (listeners_.IsEmpty()) {
    base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
    deleting_soon_ = true;
  }
}
//////////////////////////////////////////////////////////////////////////

void LogClient::OnSyncChannelTestMsg()
{
}

//如果发同步消息这里会死锁,都proxy到ui上来，然后client在ui上发了一个同步消息，server通过proxy
//也要得到ui，死锁
void LogClient::OnChannelTestMsg()
{
  //Send(new SyncChannelTestMsg_NoArgs);
  //Send(new ProfilerAction(base::TimeTicks::HighResNow().ToInternalValue(),
  //  L"action-test",L"from-test",L"data-test"));
}

//////////////////////////////////////////////////////////////////////////