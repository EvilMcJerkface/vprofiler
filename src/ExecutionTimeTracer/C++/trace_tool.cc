// VProf headers
#include "trace_tool.h"

// C headers
#include <sys/mman.h>
#include <sys/stat.h>
#include <limits.h>
#include <pthread.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

// C++ headers
#include <algorithm>
#include <fstream>
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

/*!< Process local transaction ID. */
std::atomic_uint_fast64_t processTransactionID{0};

std::shared_ptr<VProfSharedMemory> VProfSharedMemory::instance = nullptr;
bool VProfSharedMemory::singletonInitialized = false;

TraceTool *TraceTool::instance = NULL;
__thread ulint TraceTool::current_transaction_id = 0;
std::unordered_map<uint, uint> TraceTool::processToGlobalSID{{0, 0}};

timespec TraceTool::global_last_query;

ofstream TraceTool::log_file;

__thread int TraceTool::path_count = 0;
__thread bool TraceTool::is_commit = false;
__thread bool TraceTool::commit_successful = true;
__thread timespec TraceTool::trans_start;

bool TraceTool::should_shutdown = false;
pthread_t TraceTool::back_thread;

int SynchronizationTraceTool::numThingsLogged = 0;
thread_local OperationLog SynchronizationTraceTool::currOpLog;
thread_local FunctionLog SynchronizationTraceTool::currFuncLog;
std::mutex SynchronizationTraceTool::dataMutex;
pid_t SynchronizationTraceTool::lastPID;

std::unique_ptr<SynchronizationTraceTool> SynchronizationTraceTool::instance = nullptr;
mutex SynchronizationTraceTool::singletonMutex;

/* Define MONITOR if needs to trace running time of functions. */
#ifdef MONITOR
static __thread timespec function_start;
static __thread timespec function_end;
static __thread timespec call_start;
static __thread timespec call_end;
#endif

static const size_t NEW_ORDER_LENGTH = strlen(NEW_ORDER_MARKER);
static const size_t PAYMENT_LENGTH = strlen(PAYMENT_MARKER);
static const size_t ORDER_STATUS_LENGTH = strlen(ORDER_STATUS_MARKER);
static const size_t DELIVERY_LENGTH = strlen(DELIVERY_MARKER);
static const size_t STOCK_LEVEL_LENGTH = strlen(STOCK_LEVEL_MARKER);

void set_id(int id) {
    TraceTool::get_instance()->id = id;
    if (!TraceTool::log_file.is_open()) {
        TraceTool::log_file.open("latency/log_file_" + to_string(id));
    }
}

int get_thread_id() {
    return TraceTool::get_instance()->id;
}

pthread_t get_thread() {
    return TraceTool::back_thread;
}

void set_should_shutdown(int shutdown) {
    TraceTool::should_shutdown = shutdown;
}

void log_command(const char *command) {
    TraceTool::get_instance()->log_file << "[Thread " << pthread_self() << "]: " << command << endl;
}
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

void VProfSharedMemory::GetExecutableName(char *filename) const {
    #ifdef WINDOWS
    GetModuleFilename(NULL, filename, MAX_PATH);

    // Eventually need some other directive for Mac OS
    #else
    readlink("/proc/self/exe", filename, PATH_MAX);

    #endif
}

std::atomic_uint_fast64_t* VProfSharedMemory::GetTransactionID() {
    return &shm->transaction_id;
}

VProfSharedMemory::VProfSharedMemory() {
    #ifdef WINDOWS
    wchar_t filename[MAX_PATH];
    #else
    char filename[PATH_MAX];
    #endif
    GetExecutableName(filename);

    int fd = shm_open("/vprofsharedmem", O_RDWR | O_CREAT | O_EXCL, 0666);

    // We're the first process to create the shared memory.
    if (fd != -1) {
        // Set shared memory size.
        // TODO handle error.
        if (ftruncate(fd, sizeof(VProfSharedMemory)) == -1) {
            // noop
            (void)0;
        }

        shm = static_cast<SharedMem*>(mmap(NULL, sizeof(VProfSharedMemory), 
                                                   PROT_READ | PROT_WRITE,
                                                   MAP_SHARED, fd, 0));
        shm->transaction_id = 0;
        shm->sharedMemInitialized = true;
        shm->refCount = 1;
    }
    else {
        fd = shm_open("/vprofsharedmem", O_RDWR, 0666);

        // Wait for file to be truncated
        struct stat fileStats;
        do {
            fstat(fd, &fileStats);
        } while(fileStats.st_size < 1);

        shm = static_cast<SharedMem*>(mmap(NULL, sizeof(VProfSharedMemory), 
                                                   PROT_READ | PROT_WRITE,
                                                   MAP_SHARED, fd, 0));

        // Wait for shm to be initialized. Maybe add cv in shm?
        while (!shm->sharedMemInitialized) {}

        shm->refCount++;
    }
}

VProfSharedMemory::~VProfSharedMemory() {
    if ((shm->refCount.fetch_sub(1) - 1) == 0) {
        shm_unlink("/vprofsharedmem");
    }
}

/********************************************************************//**
Get shared memory instance. */
std::shared_ptr<VProfSharedMemory> VProfSharedMemory::GetInstance() {
    if (!singletonInitialized) {
        instance = std::shared_ptr<VProfSharedMemory>(new VProfSharedMemory());

        singletonInitialized = true;
    }

    return instance;
}

/********************************************************************//**
Get the current TraceTool instance. */
std::shared_ptr<TraceTool> TraceTool::get_instance()
{
  if (instance == nullptr)
  {
    pthread_mutex_lock(&instance_mutex);
    /* Check instance again after entering the ciritical section
       to prevent double initilization. */
    if (instance == nullptr)
    {
      instance = std::shared_ptr<TraceTool>(new TraceTool());
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

TraceTool::TraceTool() : function_times() {
    /* Open the log file in append mode so that it won't be overwritten */
    const int number_of_functions = NUMBER_OF_FUNCTIONS + 1;
    vector<FunctionLog> function_time;
    function_time.push_back(0);
    for (int index = 0; index < number_of_functions; index++) {
        function_times.push_back(function_time);
        function_times[index].reserve(500000);
    }
    transaction_start_times.reserve(500000);
    transaction_start_times.push_back(0);

    processToGlobalSID.reserve(500000);

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
    if (now.tv_sec - global_last_query.tv_sec >= 10 && VProfSharedMemory::GetInstance()->GetTransactionID()->load() > 0) {
        /* Create a new TraceTool instance. */
        TraceTool *old_instance = instance;
        instance = new TraceTool;
        instance->id = old_instance->id;

        /* Reset the global transaction ID. */
        //VProfSharedMemory::GetInstance()->GetTransactionID()->store(0);
        processTransactionID.store(0);


        /* Dump data in the old instance to log files and
           reclaim memory. */
        old_instance->write_log();
        delete old_instance;
    }

    if (now.tv_sec - global_last_query.tv_sec >= 5 && should_shutdown) {
        break;
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
    if (processToGlobalSID[current_transaction_id] > VProfSharedMemory::GetInstance()->GetTransactionID()->load()) {
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
    current_transaction_id = *(VProfSharedMemory::GetInstance()->transaction_id)++;
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

void TraceTool::end_transaction() {
#ifdef LATENCY
    timespec now = get_time();
    long latency = difftime(trans_start, now);
    function_times.back()[current_transaction_id].setFunctionStart(trans_start);
    function_times.back()[current_transaction_id].setFunctionEnd(now);
    if (!commit_successful) {
        transaction_start_times[current_transaction_id] = 0;
    }
#endif
  new_transaction = true;
  type = NONE;
}

void TraceTool::add_record(int function_index, 
                           timespec &start_time, 
                           timespec &end_time)
{
  if (current_transaction_id > *(VProfSharedMemory::GetInstance()->transaction_id))
  {
    current_transaction_id = 0;
  }
  pthread_rwlock_rdlock(&data_lock);
  function_times[function_index][current_transaction_id].setFunctionStart(start_time);
  function_times[function_index][current_transaction_id].setFunctionEnd(end_time);
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
  
  /*int function_index = 0;
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
  }*/
  int function_index = 0;
  for (vector<vector<FunctionLog> >::iterator iterator = function_times.begin();
          iterator != function_times.end(); ++iterator) {
      ulint number_of_transactions = iterator->size();
      for (ulint index = 0; index < number_of_transactions; ++index) {
          if (transaction_start_times[index] > 0) {
              tpcc_log << function_index << ',' << (*iterator)[index] << endl;
          }
      }
      function_index++;
      vector<FunctionLog>().swap(*iterator);
  }
  vector<vector<FunctionLog> >().swap(function_times);
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

