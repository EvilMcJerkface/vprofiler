//#include "my_global.h"
#include "trace_tool.h"
#include <algorithm>
#include <pthread.h>
#include <fstream>
#include <time.h>
#include <cstring>
#include <sstream>
#include <cstdlib>
#include <cassert>

#define TARGET_PATH_COUNT 0
#define NUMBER_OF_FUNCTIONS 200
#define LATENCY
#define MONITOR

#define NEW_ORDER_MARKER "SELECT C_DISCOUNT, C_LAST, C_CREDIT, W_TAX  FROM CUSTOMER, WAREHOUSE WHERE"
#define PAYMENT_MARKER "UPDATE WAREHOUSE SET W_YTD = W_YTD"
#define ORDER_STATUS_MARKER "SELECT C_FIRST, C_MIDDLE"
#define DELIVERY_MARKER "SELECT NO_O_ID FROM NEW_ORDER WHERE NO_D_ID ="
#define STOCK_LEVEL_MARKER "SELECT D_NEXT_O_ID FROM DISTRICT WHERE D_W_ID ="

using namespace std;
//using std::endl;
//using std::ifstream;
//using std::ofstream;
//using std::vector;
//using std::stringstream;
//using std::sort;
//using std::getline;

ulint transaction_id = 0;

TraceTool *TraceTool::instance = NULL;
pthread_mutex_t TraceTool::instance_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_rwlock_t TraceTool::data_lock = PTHREAD_RWLOCK_INITIALIZER;
__thread ulint TraceTool::current_transaction_id = 0;

timespec TraceTool::global_last_query;
pthread_mutex_t TraceTool::last_query_mutex = PTHREAD_MUTEX_INITIALIZER;

__thread int TraceTool::path_count = 0;
__thread bool TraceTool::is_commit = false;
__thread bool TraceTool::commit_successful = true;
__thread bool TraceTool::new_transaction = true;
__thread timespec TraceTool::trans_start;
__thread transaction_type TraceTool::type = NONE;

unique_ptr<SynchronizationTraceTool> SynchronizationTraceTool::instance = nullptr;
mutex SynchronizationTraceTool::singletonMutex;

thread_local OperationLog SynchronizationTraceTool::currOpLog;
thread_local FunctionLog SynchronizationTraceTool::currFuncLog;
std::mutex SynchronizationTraceTool::dataMutex;
pid_t SynchronizationTraceTool::lastPID;

static const size_t NEW_ORDER_LENGTH = strlen(NEW_ORDER_MARKER);
static const size_t PAYMENT_LENGTH = strlen(PAYMENT_MARKER);
static const size_t ORDER_STATUS_LENGTH = strlen(ORDER_STATUS_MARKER);
static const size_t DELIVERY_LENGTH = strlen(DELIVERY_MARKER);
static const size_t STOCK_LEVEL_LENGTH = strlen(STOCK_LEVEL_MARKER);

/* Define MONITOR if needs to trace running time of functions. */
#ifdef MONITOR
static __thread timespec function_start;
static __thread timespec function_end;
static __thread timespec call_start;
static __thread timespec call_end;
#endif

void TRACE_FUNCTION_START()
{
#ifdef MONITOR
  if (TraceTool::should_monitor())
  {
    clock_gettime(CLOCK_REALTIME, &function_start);
  }
#endif
}

void TRACE_FUNCTION_END()
{
#ifdef MONITOR
  if (TraceTool::should_monitor())
  {
    clock_gettime(CLOCK_REALTIME, &function_end);
    TraceTool::get_instance()->add_record(0, function_start, function_end);
  }
#endif
}

bool TRACE_START()
{
#ifdef MONITOR
  if (TraceTool::should_monitor())
  {
    clock_gettime(CLOCK_REALTIME, &call_start);
  }
#endif
  return false;
}

bool TRACE_END(int index)
{
#ifdef MONITOR
  if (TraceTool::should_monitor())
  {
    clock_gettime(CLOCK_REALTIME, &call_end);
    TraceTool::get_instance()->add_record(index, call_start, call_end);
  }
#endif
  return false;
}

void SESSION_START()
{
#ifdef LATENCY
  TraceTool::get_instance()->new_transaction = true;
  TraceTool::get_instance()->start_new_query();
#endif
}

void SESSION_END(bool successfully)
{
#ifdef LATENCY
  TraceTool::get_instance()->is_commit = true;
  TraceTool::get_instance()->commit_successful = successfully;
  TraceTool::get_instance()->end_query();
#endif
}

void SYNCHRONIZATION_CALL_START(int op, void* obj) {
    SynchronizationTraceTool::SynchronizationCallStart(static_cast<Operation>(op), obj);
}

void SYNCHRONIZATION_CALL_END() {
    SynchronizationTraceTool::SynchronizationCallEnd();
}

/********************************************************************//**
Get the current TraceTool instance. */
TraceTool *TraceTool::get_instance()
{
  if (instance == NULL)
  {
    pthread_mutex_lock(&instance_mutex);
    /* Check instance again after entering the ciritical section
       to prevent double initilization. */
    if (instance == NULL)
    {
      instance = new TraceTool;
#ifdef LATENCY
      /* Create a background thread for dumping function running time
         and latency data. */
      pthread_t write_thread;
      pthread_create(&write_thread, NULL, check_write_log, NULL);
#endif
    }
    pthread_mutex_unlock(&instance_mutex);
  }
  return instance;
}

TraceTool::TraceTool() : function_times()
{
  /* Open the log file in append mode so that it won't be overwritten */
  log_file.open("logs/trace.log");
  const int number_of_functions = NUMBER_OF_FUNCTIONS + 1;
  vector<vector<FunctionLog>> function_time;
  for (int index = 0; index < number_of_functions; index++)
  {
    function_times.push_back(function_time);
    function_times[index].reserve(500000);
    for (int j = 0; j < 500000; j++) {
        function_times[index][j].reserve(500);
    }
  }
  transaction_start_times.reserve(500000);
  transaction_start_times.push_back(0);
  transaction_types.reserve(500000);
  transaction_types.push_back(NONE);
  
  srand(time(0));
}

bool TraceTool::should_monitor()
{
  return path_count == TARGET_PATH_COUNT;
}

void *TraceTool::check_write_log(void *arg)
{
  /* Runs in an infinite loop and for every 5 seconds,
     check if there's any query comes in. If not, then
     dump data to log files. */
  while (true)
  {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    timespec now = get_time();
    if (now.tv_sec - global_last_query.tv_sec >= 5 && transaction_id > 0)
    {
      /* Create a back up of the debug log file in case it's overwritten. */
      std::ifstream src("logs/trace.log", std::ios::binary);
      std::ofstream dst("logs/trace.bak", std::ios::binary);
      dst << src.rdbuf();
      src.close();
      dst.close();
      
      /* Create a new TraceTool instnance. */
      TraceTool *old_instace = instance;
      instance = new TraceTool;
      
      /* Reset the global transaction ID. */
      transaction_id = 0;
      
      /* Dump data in the old instance to log files and
         reclaim memory. */
      // Should this section be locked?
      old_instace->write_log();
      delete old_instace;
    }
  }
  return NULL;
}

timespec TraceTool::get_time()
{
  timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  return now;
}

long TraceTool::difftime(timespec start, timespec end)
{
  return (end.tv_sec - start.tv_sec) * 1000000000 + (end.tv_nsec - start.tv_nsec);
}

ulint TraceTool::now_micro()
{
  timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  return now.tv_sec * 1000000 + now.tv_nsec / 1000;
}

/********************************************************************//**
Start a new query. This may also start a new transaction. */
void TraceTool::start_new_query()
{
  is_commit = false;
  /* This happens when a log write happens, which marks the end of a phase. */
  if (current_transaction_id > transaction_id)
  {
    current_transaction_id = 0;
    new_transaction = true;
    commit_successful = true;
  }
#ifdef LATENCY
  /* Start a new transaction. Note that we don't reset the value of new_transaction here.
     We do it in set_query after looking at the first query of a transaction. */
  if (new_transaction)
  {
    trans_start = get_time();
    commit_successful = true;
    /* Use a write lock here because we are appending content to the vector. */
    pthread_rwlock_wrlock(&data_lock);
    current_transaction_id = transaction_id++;
    transaction_start_times[current_transaction_id] = now_micro();
    transaction_start_times.push_back(0);
    transaction_types.push_back(NONE);
    pthread_rwlock_unlock(&data_lock);
  }
  pthread_mutex_lock(&last_query_mutex);
  clock_gettime(CLOCK_REALTIME, &global_last_query);
  pthread_mutex_unlock(&last_query_mutex);
#endif
}

void TraceTool::set_query(const char *new_query)
{
  // Look at the first query of a transaction
  if (new_transaction)
  {
    if (strncmp(new_query, NEW_ORDER_MARKER, NEW_ORDER_LENGTH) == 0)
    {
      type = NEW_ORDER;
    }
    else if (strncmp(new_query, PAYMENT_MARKER, PAYMENT_LENGTH) == 0)
    {
      type = PAYMENT;
    }
    else if (strncmp(new_query, ORDER_STATUS_MARKER, ORDER_STATUS_LENGTH) == 0)
    {
      type = ORDER_STATUS;
    }
    else if (strncmp(new_query, DELIVERY_MARKER, DELIVERY_LENGTH) == 0)
    {
      type = DELIVERY;
    }
    else if (strncmp(new_query, STOCK_LEVEL_MARKER, STOCK_LEVEL_LENGTH) == 0)
    {
      type = STOCK_LEVEL;
    }
    else
    {
     type = NONE;
     commit_successful = false;
    }
    
    pthread_rwlock_rdlock(&data_lock);
    transaction_types[current_transaction_id] = type;
    pthread_rwlock_unlock(&data_lock);
    /* Reset the value of new_transaction. */
    new_transaction = false;
  }
}

void TraceTool::end_query()
{
#ifdef LATENCY
  if (is_commit)
  {
    end_transaction();
  }
#endif
}

void TraceTool::end_transaction()
{
#ifdef LATENCY
  timespec now = get_time();
  pthread_rwlock_rdlock(&data_lock);
  function_times.back()[current_transaction_id].push_back(FunctionLog(current_transaction_id, trans_start, now));
  if (!commit_successful)
  {
    transaction_start_times[current_transaction_id] = 0;
  }
  pthread_rwlock_unlock(&data_lock);
#endif
  new_transaction = true;
  type = NONE;
}

void TraceTool::add_record(int function_index, 
                           timespec &start_time, 
                           timespec &end_time)
{
  if (current_transaction_id > transaction_id)
  {
    current_transaction_id = 0;
  }
  pthread_rwlock_rdlock(&data_lock);
  function_times[function_index][current_transaction_id].push_back(FunctionLog(current_transaction_id, start_time, end_time));
  pthread_rwlock_unlock(&data_lock);
}

void TraceTool::write_latency(string dir)
{
  ofstream tpcc_log;
  ofstream new_order_log;
  ofstream payment_log;
  ofstream order_status_log;
  ofstream delivery_log;
  ofstream stock_level_log;
  
  tpcc_log.open(dir + "tpcc");
  new_order_log.open(dir + "new_order");
  payment_log.open(dir + "payment");
  order_status_log.open(dir + "order_status");
  delivery_log.open(dir + "delivery");
  stock_level_log.open(dir + "stock_level");
  
  pthread_rwlock_wrlock(&data_lock);
  for (ulint index = 0; index < transaction_start_times.size(); ++index)
  {
    ulint start_time = transaction_start_times[index];
    if (start_time > 0)
    {
      tpcc_log << index << ',' << start_time << endl;
      switch (transaction_types[index])
      {
        case NEW_ORDER:
          new_order_log << start_time << endl;
          break;
        case PAYMENT:
          payment_log << start_time << endl;
          break;
        case ORDER_STATUS:
          order_status_log << start_time << endl;
          break;
        case DELIVERY:
          delivery_log << start_time << endl;
          break;
        case STOCK_LEVEL:
          stock_level_log << start_time << endl;
          break;
        default:
          break;
      }
    }
  }
  
  int function_index = 0;
  for (vector<vector<vector<FunctionLog>>>::iterator iterator = function_times.begin(); iterator != function_times.end(); ++iterator)
  {
    ulint number_of_transactions = iterator->size();
    for (ulint index = 0; index < number_of_transactions; ++index)
    {
      if (transaction_start_times[index] > 0)
      {
        for (vector<FunctionLog>::iterator innerIt = (*iterator)[index].begin(); innerIt != (*iterator)[index].end(); innerIt++) {
            tpcc_log << function_index << ',' << (*innerIt);
            switch (transaction_types[index])
            {
              case NEW_ORDER:
                new_order_log << function_index << ',' << (*innerIt);
                break;
              case PAYMENT:
                payment_log << function_index << ',' << (*innerIt);
                break;
              case ORDER_STATUS:
                order_status_log << function_index << ',' << (*innerIt);
                break;
              case DELIVERY:
                delivery_log << function_index << ',' << (*innerIt);
                break;
              case STOCK_LEVEL:
                stock_level_log << function_index << ',' << (*innerIt);
                break;
              default:
                break;
            }
        }
      }
    }
    function_index++;
    iterator->clear();
  }
  function_times.clear();
  pthread_rwlock_unlock(&data_lock);
  tpcc_log.close();
  new_order_log.close();
  payment_log.close();
  order_status_log.close();
  delivery_log.close();
  stock_level_log.close();
}

void TraceTool::write_log()
{ 
  write_latency("latency/");
}


SynchronizationTraceTool::SynchronizationTraceTool() {
    opLogs = new vector<OperationLog>;
    funcLogs = new vector<FunctionLog>;
    doneWriting = false;

    opLogs->reserve(1000000);
    funcLogs->reserve(1000000);

    lastPID = ::getpid();

    logFile.open("latency/SynchronizationLog_" + std::to_string(lastPID), std::ios_base::trunc);

    writerThread = thread(writeLogWorker);
}

SynchronizationTraceTool::~SynchronizationTraceTool() {
    
    // opLogs and funcLogs are deleted in here.  Maybe move that
    // functionality to a cleanup function.
    writeLogs(instance->opLogs, instance->funcLogs);

    dataMutex.lock();
    doneWriting = true;
    dataMutex.unlock();

    writerThread.join();
}

void SynchronizationTraceTool::SynchronizationCallStart(Operation op, void *obj) {
    if (instance == nullptr) {
        maybeCreateInstance();
    }

    dataMutex.lock();
    instance->opLogs->push_back(OperationLog(obj, op));
    dataMutex.unlock();

    currFuncLog = FunctionLog(TraceTool::current_transaction_id);

    timespec startTime;
    clock_gettime(CLOCK_REALTIME, &startTime);

    currFuncLog.setFunctionStart(startTime);
}

void SynchronizationTraceTool::SynchronizationCallEnd() {
    timespec endTime;
    clock_gettime(CLOCK_REALTIME, &endTime);
    currFuncLog.setFunctionEnd(endTime);

    dataMutex.lock();
    instance->opLogs->push_back(currOpLog);
    instance->funcLogs->push_back(currFuncLog);
    dataMutex.unlock();
}

void SynchronizationTraceTool::maybeCreateInstance() {
    singletonMutex.lock();

    if (instance == nullptr) {
        instance = unique_ptr<SynchronizationTraceTool>(new SynchronizationTraceTool());
    }

    singletonMutex.unlock();
}

void SynchronizationTraceTool::checkFileClean() {
    pid_t currPID = ::getpid();

    if (currPID != lastPID) {
        instance->logFile.close();

        instance->logFile.open("latency/SynchronizationLog_" + std::to_string(currPID), std::ios_base::trunc);
    }
}

void SynchronizationTraceTool::writeLogWorker() {
    bool stopLogging = false;

    // Loop forever writing logs
    while (!stopLogging) {
        this_thread::sleep_for(chrono::seconds(5));
        if (instance != nullptr) {
            checkFileClean();

            vector<OperationLog> *newOpLogs = new vector<OperationLog>;
            vector<FunctionLog> *newFuncLogs = new vector<FunctionLog>;

            // TODO TODO TODO experiment with this size!!!!
            newOpLogs->reserve(instance->opLogs->size() * 4);
            newFuncLogs->reserve(instance->funcLogs->size() * 4);

            dataMutex.lock();

            vector<OperationLog> *oldOpLogs = instance->opLogs;
            vector<FunctionLog> *oldFuncLogs = instance->funcLogs;

            instance->opLogs = newOpLogs;
            instance->funcLogs = newFuncLogs;

            if (instance->doneWriting) {
                stopLogging = true;
            }

            dataMutex.unlock();

            writeLogs(oldOpLogs, oldFuncLogs);
        }
    }
}

void SynchronizationTraceTool::writeLogs(vector<OperationLog> *opLogs, 
                                         vector<FunctionLog> *funcLogs) {
    for (OperationLog &opLog : *opLogs) {
        instance->logFile << "0," << opLog;
    }

    for (FunctionLog &funcLog : *funcLogs) {
        instance->logFile << "1," << funcLog;
    }

    delete opLogs;
    delete funcLogs;
}

std::ostream& operator<<(std::ostream &os, const Operation &op) {
    switch (op) {
        case MUTEX_LOCK:
            os << "ML";
            break;
        case MUTEX_UNLOCK:
            os << "MU";
            break;
        case CV_WAIT:
            os << "CVW";
            break;
        case CV_BROADCAST:
            os << "CVB";
            break;
        case CV_SIGNAL:
            os << "CVS";
            break;
        case QUEUE_ENQUEUE:
            os << "QE";
            break;
        case QUEUE_DEQUEUE:
            os << "QD";
            break;
        case MESSAGE_SEND:
            os << "MS";
            break;
        case MESSAGE_RECEIVE:
            os << "MR";
            break;
    }

    return os;
}

