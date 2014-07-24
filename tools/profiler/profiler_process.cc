#include "tools/profiler/profiler_process.h"
#include "tools/profiler/profiler_thread.h"

//////////////////////////////////////////////////////////////////////////
ProfilerProcess* g_profiler_process = NULL;

//////////////////////////////////////////////////////////////////////////
ProfilerProcess::ProfilerProcess(const CommandLine& command_line)
:created_io_thread_(false),created_launcher_thread_(false),
created_db_thread_(false),created_file_thread_(false)
{
  g_profiler_process = this;
}

ProfilerProcess::~ProfilerProcess()
{
  //×¢ÒâË³Ðò
  launcher_thread_.reset();
  file_thread_.reset();
  db_thread_.reset();
  io_thread_.reset();

  g_profiler_process = NULL;
}

void ProfilerProcess::CreateIoThread()
{
  DCHECK(!created_io_thread_ && io_thread_.get() == NULL);
  created_io_thread_ = true;

  scoped_ptr<base::Thread> thread(new ProfilerThread(ProfilerThread::IO));
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  if (!thread->StartWithOptions(options))
    return;
  io_thread_.swap(thread);
}

base::Thread* ProfilerProcess::io_thread()
{
  DCHECK(CalledOnValidThread());
  if (!created_io_thread_)
    CreateIoThread();
  return io_thread_.get();
}

void ProfilerProcess::CreateFileThread()
{
  DCHECK(!created_file_thread_ && file_thread_.get() == NULL);
  created_file_thread_ = true;

  scoped_ptr<base::Thread> thread(new ProfilerThread(ProfilerThread::FILE));
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  if (!thread->StartWithOptions(options))
    return;
  file_thread_.swap(thread);
}

base::Thread* ProfilerProcess::file_thread()
{
  DCHECK(CalledOnValidThread());
  if (!created_file_thread_)
    CreateFileThread();
  return file_thread_.get();
}

void ProfilerProcess::CreateDbThread()
{
  DCHECK(!created_db_thread_ && db_thread_.get() == NULL);
  created_db_thread_ = true;

  scoped_ptr<base::Thread> thread(new ProfilerThread(ProfilerThread::DB));
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  if (!thread->StartWithOptions(options))
    return;
  db_thread_.swap(thread);
}

base::Thread* ProfilerProcess::db_thread()
{
  DCHECK(CalledOnValidThread());
  if (!created_db_thread_)
    CreateDbThread();
  return db_thread_.get();
}

void ProfilerProcess::reset_dbthread(){
  DCHECK(CalledOnValidThread());

  db_thread_.reset();
  created_db_thread_ = false;
  CreateDbThread();
}

void ProfilerProcess::CreateLauncherThread()
{
  DCHECK(!created_launcher_thread_ && launcher_thread_.get() == NULL);
  created_launcher_thread_ = true;

  scoped_ptr<base::Thread> thread(new ProfilerThread(ProfilerThread::PROCESS_LAUNCHER));
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_DEFAULT;
  if (!thread->StartWithOptions(options))
    return;
  launcher_thread_.swap(thread);
}

base::Thread* ProfilerProcess::launcher_thread()
{
  DCHECK(CalledOnValidThread());
  if (!created_launcher_thread_)
    CreateLauncherThread();
  return launcher_thread_.get();
}

ProfilerData* ProfilerProcess::profiler_data()
{
  if(!profiler_data_.get()){
    profiler_data_ = new ProfilerData;
  }

  return profiler_data_.get();
}
//////////////////////////////////////////////////////////////////////////