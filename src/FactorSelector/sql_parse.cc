

#define MYSQL_LEX 1
#include "my_global.h"
#include "sql_priv.h"
#include "unireg.h"                    
#include "sql_parse.h"        
#include "lock.h"             
                              
                              
                              
                              
#include "sql_base.h"         
#include "sql_cache.h"        
#include "sql_show.h"         
                              
#include "mysqld.h"
#include "sql_locale.h"                         
#include "log.h"                                
#include "sql_view.h"         
#include "sql_delete.h"       
#include "sql_insert.h"       
#include "sql_update.h"       
#include "sql_partition.h"    
#include "sql_db.h"           
                              
                              
                              
                              
#include "sql_table.h"        
                              
                              
                              
                              
#include "sql_reload.h"       
#include "sql_admin.h"        
#include "sql_connect.h"      
                              
                              
                              
#include "sql_rename.h"       
#include "sql_tablespace.h"   
#include "hostname.h"         
#include "sql_acl.h"          
                              
                              
                              
                              
                              
                              
#include "sql_test.h"         
#include "sql_select.h"       
#include "sql_load.h"         
#include "sql_servers.h"      
                              
#include "sql_handler.h"      
                              
#include "sql_binlog.h"       
#include "sql_do.h"           
#include "sql_help.h"         
#include "rpl_constants.h"    
#include "log_event.h"
#include "rpl_slave.h"
#include "rpl_master.h"
#include "rpl_filter.h"
#include <m_ctype.h>
#include <myisam.h>
#include <my_dir.h>
#include "rpl_handler.h"

#include "sp_head.h"
#include "sp.h"
#include "sp_cache.h"
#include "events.h"
#include "sql_trigger.h"
#include "transaction.h"
#include "sql_audit.h"
#include "sql_prepare.h"
#include "debug_sync.h"
#include "probes_mysql.h"
#include "set_var.h"
#include "opt_trace.h"
#include "mysql/psi/mysql_statement.h"
#include "sql_bootstrap.h"
#include "opt_explain.h"
#include "sql_rewrite.h"
#include "global_threads.h"
#include "sql_analyse.h"
#include "table_cache.h" 
#include "trace_tool.h"

#include <algorithm>

#include "trace_tool.h"
using std::max;
using std::min;

#define FLAGSTR(V,F) ((V)&(F)?#F" ":"")




#define SP_TYPE_STRING(LP) \
  ((LP)->sphead->m_type == SP_TYPE_FUNCTION ? "FUNCTION" : "PROCEDURE")
#define SP_COM_STRING(LP) \
  ((LP)->sql_command == SQLCOM_CREATE_SPFUNCTION || \
   (LP)->sql_command == SQLCOM_ALTER_FUNCTION || \
   (LP)->sql_command == SQLCOM_SHOW_CREATE_FUNC || \
   (LP)->sql_command == SQLCOM_DROP_FUNCTION ? \
   "FUNCTION" : "PROCEDURE")

static bool execute_sqlcom_select(THD *thd, TABLE_LIST *all_tables);
static bool check_show_access(THD *thd, TABLE_LIST *table);
static void sql_kill(THD *thd, ulong id, bool only_kill_query);
static bool lock_tables_precheck(THD *thd, TABLE_LIST *tables);

const char *any_db="*any*";	

const LEX_STRING command_name[]={
  { C_STRING_WITH_LEN("Sleep") },
  { C_STRING_WITH_LEN("Quit") },
  { C_STRING_WITH_LEN("Init DB") },
  { C_STRING_WITH_LEN("Query") },
  { C_STRING_WITH_LEN("Field List") },
  { C_STRING_WITH_LEN("Create DB") },
  { C_STRING_WITH_LEN("Drop DB") },
  { C_STRING_WITH_LEN("Refresh") },
  { C_STRING_WITH_LEN("Shutdown") },
  { C_STRING_WITH_LEN("Statistics") },
  { C_STRING_WITH_LEN("Processlist") },
  { C_STRING_WITH_LEN("Connect") },
  { C_STRING_WITH_LEN("Kill") },
  { C_STRING_WITH_LEN("Debug") },
  { C_STRING_WITH_LEN("Ping") },
  { C_STRING_WITH_LEN("Time") },
  { C_STRING_WITH_LEN("Delayed insert") },
  { C_STRING_WITH_LEN("Change user") },
  { C_STRING_WITH_LEN("Binlog Dump") },
  { C_STRING_WITH_LEN("Table Dump") },
  { C_STRING_WITH_LEN("Connect Out") },
  { C_STRING_WITH_LEN("Register Slave") },
  { C_STRING_WITH_LEN("Prepare") },
  { C_STRING_WITH_LEN("Execute") },
  { C_STRING_WITH_LEN("Long Data") },
  { C_STRING_WITH_LEN("Close stmt") },
  { C_STRING_WITH_LEN("Reset stmt") },
  { C_STRING_WITH_LEN("Set option") },
  { C_STRING_WITH_LEN("Fetch") },
  { C_STRING_WITH_LEN("Daemon") },
  { C_STRING_WITH_LEN("Binlog Dump GTID") },
  { C_STRING_WITH_LEN("Error") }  
};

const char *xa_state_names[]={
  "NON-EXISTING", "ACTIVE", "IDLE", "PREPARED", "ROLLBACK ONLY"
};


Slow_log_throttle log_throttle_qni(&opt_log_throttle_queries_not_using_indexes,
                                   &LOCK_log_throttle_qni,
                                   Log_throttle::LOG_THROTTLE_WINDOW_SIZE,
                                   slow_log_print,
                                   "throttle: %10lu 'index "
                                   "not used' warning(s) suppressed.");


#ifdef HAVE_REPLICATION

inline bool all_tables_not_ok(THD *thd, TABLE_LIST *tables)
{
  return rpl_filter->is_on() && tables && !thd->sp_runtime_ctx &&
         !rpl_filter->tables_ok(thd->db, tables);
}


inline bool db_stmt_db_ok(THD *thd, char* db)
{
  DBUG_ENTER("db_stmt_db_ok");

  if (!thd->slave_thread)
    DBUG_RETURN(TRUE);

  
  bool db_ok= (rpl_filter->get_do_db()->is_empty() &&
               rpl_filter->get_ignore_db()->is_empty()) ?
              rpl_filter->db_ok_with_wild_table(db) :
              rpl_filter->db_ok(db);

  DBUG_RETURN(db_ok);
}
#endif


static bool some_non_temp_table_to_be_updated(THD *thd, TABLE_LIST *tables)
{
  for (TABLE_LIST *table= tables; table; table= table->next_global)
  {
    DBUG_ASSERT(table->db && table->table_name);
    if (table->updating && !find_temporary_table(thd, table))
      return 1;
  }
  return 0;
}



bool stmt_causes_implicit_commit(const THD *thd, uint mask)
{
  const LEX *lex= thd->lex;
  bool skip= FALSE;
  DBUG_ENTER("stmt_causes_implicit_commit");

  if (!(sql_command_flags[lex->sql_command] & mask))
    DBUG_RETURN(FALSE);

  switch (lex->sql_command) {
  case SQLCOM_DROP_TABLE:
    skip= lex->drop_temporary;
    break;
  case SQLCOM_ALTER_TABLE:
  case SQLCOM_CREATE_TABLE:
    
    skip= (lex->create_info.options & HA_LEX_CREATE_TMP_TABLE);
    break;
  case SQLCOM_SET_OPTION:
    skip= lex->autocommit ? FALSE : TRUE;
    break;
  default:
    break;
  }

  DBUG_RETURN(!skip);
}




uint sql_command_flags[SQLCOM_END+1];
uint server_command_flags[COM_END+1];

void init_update_queries(void)
{
  
  memset(server_command_flags, 0, sizeof(server_command_flags));

  server_command_flags[COM_STATISTICS]= CF_SKIP_QUESTIONS;
  server_command_flags[COM_PING]=       CF_SKIP_QUESTIONS;
  server_command_flags[COM_STMT_PREPARE]= CF_SKIP_QUESTIONS;
  server_command_flags[COM_STMT_CLOSE]=   CF_SKIP_QUESTIONS;
  server_command_flags[COM_STMT_RESET]=   CF_SKIP_QUESTIONS;

  
  memset(sql_command_flags, 0, sizeof(sql_command_flags));

  
  sql_command_flags[SQLCOM_CREATE_TABLE]=   CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_AUTO_COMMIT_TRANS |
                                            CF_CAN_GENERATE_ROW_EVENTS;
  sql_command_flags[SQLCOM_CREATE_INDEX]=   CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_TABLE]=    CF_CHANGES_DATA | CF_WRITE_LOGS_COMMAND |
                                            CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_TRUNCATE]=       CF_CHANGES_DATA | CF_WRITE_LOGS_COMMAND |
                                            CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_TABLE]=     CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_LOAD]=           CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS;
  sql_command_flags[SQLCOM_CREATE_DB]=      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_DB]=        CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_DB_UPGRADE]= CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_DB]=       CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_RENAME_TABLE]=   CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_INDEX]=     CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_VIEW]=    CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_VIEW]=      CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_TRIGGER]= CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_TRIGGER]=   CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_EVENT]=   CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_EVENT]=    CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_EVENT]=     CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;

  sql_command_flags[SQLCOM_UPDATE]=	    CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE |
                                            CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_UPDATE_MULTI]=   CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE |
                                            CF_CAN_BE_EXPLAINED;
  
  sql_command_flags[SQLCOM_INSERT]=	    CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE |
                                            CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_INSERT_SELECT]=  CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE |
                                            CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_DELETE]=         CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE |
                                            CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_DELETE_MULTI]=   CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE |
                                            CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_REPLACE]=        CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE |
                                            CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_REPLACE_SELECT]= CF_CHANGES_DATA | CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE |
                                            CF_CAN_BE_EXPLAINED;
  sql_command_flags[SQLCOM_SELECT]=         CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE |
                                            CF_CAN_BE_EXPLAINED;
  
  
  sql_command_flags[SQLCOM_SET_OPTION]=     CF_REEXECUTION_FRAGILE |
                                            CF_AUTO_COMMIT_TRANS |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE; 
  
  sql_command_flags[SQLCOM_DO]=             CF_REEXECUTION_FRAGILE |
                                            CF_CAN_GENERATE_ROW_EVENTS |
                                            CF_OPTIMIZER_TRACE; 

  sql_command_flags[SQLCOM_SHOW_STATUS_PROC]= CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_STATUS]=      CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_DATABASES]=   CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_TRIGGERS]=    CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_EVENTS]=      CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_OPEN_TABLES]= CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_PLUGINS]=     CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_FIELDS]=      CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_KEYS]=        CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_VARIABLES]=   CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_CHARSETS]=    CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_COLLATIONS]=  CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_BINLOGS]=     CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_SLAVE_HOSTS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_BINLOG_EVENTS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_STORAGE_ENGINES]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_PRIVILEGES]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_WARNS]=       CF_STATUS_COMMAND | CF_DIAGNOSTIC_STMT;
  sql_command_flags[SQLCOM_SHOW_ERRORS]=      CF_STATUS_COMMAND | CF_DIAGNOSTIC_STMT;
  sql_command_flags[SQLCOM_SHOW_ENGINE_STATUS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_ENGINE_MUTEX]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_ENGINE_LOGS]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_PROCESSLIST]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_GRANTS]=      CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_DB]=   CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_MASTER_STAT]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_SLAVE_STAT]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_PROC]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_FUNC]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_TRIGGER]=  CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_STATUS_FUNC]= CF_STATUS_COMMAND | CF_REEXECUTION_FRAGILE;
  sql_command_flags[SQLCOM_SHOW_PROC_CODE]=   CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_FUNC_CODE]=   CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_CREATE_EVENT]= CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_PROFILES]=    CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_SHOW_PROFILE]=     CF_STATUS_COMMAND;
  sql_command_flags[SQLCOM_BINLOG_BASE64_EVENT]= CF_STATUS_COMMAND |
                                                 CF_CAN_GENERATE_ROW_EVENTS;

   sql_command_flags[SQLCOM_SHOW_TABLES]=       (CF_STATUS_COMMAND |
                                                 CF_SHOW_TABLE_COMMAND |
                                                 CF_REEXECUTION_FRAGILE);
  sql_command_flags[SQLCOM_SHOW_TABLE_STATUS]= (CF_STATUS_COMMAND |
                                                CF_SHOW_TABLE_COMMAND |
                                                CF_REEXECUTION_FRAGILE);

  sql_command_flags[SQLCOM_CREATE_USER]=       CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_RENAME_USER]=       CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_DROP_USER]=         CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_ALTER_USER]=        CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_GRANT]=             CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_REVOKE]=            CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_REVOKE_ALL]=        CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_OPTIMIZE]=          CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_CREATE_FUNCTION]=   CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_PROCEDURE]=  CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_SPFUNCTION]= CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_PROCEDURE]=    CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_FUNCTION]=     CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_PROCEDURE]=   CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_FUNCTION]=    CF_CHANGES_DATA | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_INSTALL_PLUGIN]=    CF_CHANGES_DATA;
  sql_command_flags[SQLCOM_UNINSTALL_PLUGIN]=  CF_CHANGES_DATA;

  
  sql_command_flags[SQLCOM_GET_DIAGNOSTICS]= CF_DIAGNOSTIC_STMT;

  
  sql_command_flags[SQLCOM_CALL]=      CF_REEXECUTION_FRAGILE |
                                       CF_CAN_GENERATE_ROW_EVENTS |
                                       CF_OPTIMIZER_TRACE; 
  sql_command_flags[SQLCOM_EXECUTE]=   CF_CAN_GENERATE_ROW_EVENTS;

  
  sql_command_flags[SQLCOM_REPAIR]=    CF_WRITE_LOGS_COMMAND | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_OPTIMIZE]|= CF_WRITE_LOGS_COMMAND | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ANALYZE]=   CF_WRITE_LOGS_COMMAND | CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CHECK]=     CF_WRITE_LOGS_COMMAND | CF_AUTO_COMMIT_TRANS;

  sql_command_flags[SQLCOM_CREATE_USER]|=       CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_USER]|=         CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_RENAME_USER]|=       CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_USER]|=        CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_REVOKE]|=            CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_REVOKE_ALL]|=        CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_GRANT]|=             CF_AUTO_COMMIT_TRANS;

  sql_command_flags[SQLCOM_ASSIGN_TO_KEYCACHE]= CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_PRELOAD_KEYS]=       CF_AUTO_COMMIT_TRANS;

  sql_command_flags[SQLCOM_FLUSH]=              CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_RESET]=              CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CREATE_SERVER]=      CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_ALTER_SERVER]=       CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_DROP_SERVER]=        CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_CHANGE_MASTER]=      CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_SLAVE_START]=        CF_AUTO_COMMIT_TRANS;
  sql_command_flags[SQLCOM_SLAVE_STOP]=         CF_AUTO_COMMIT_TRANS;

  
  sql_command_flags[SQLCOM_CREATE_TABLE]|=    CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_DROP_TABLE]|=      CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_CREATE_INDEX]|=    CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_ALTER_TABLE]|=     CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_TRUNCATE]|=        CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_LOAD]|=            CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_DROP_INDEX]|=      CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_UPDATE]|=          CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_UPDATE_MULTI]|=    CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_INSERT_SELECT]|=   CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_DELETE]|=          CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_DELETE_MULTI]|=    CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_REPLACE_SELECT]|=  CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_SELECT]|=          CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_SET_OPTION]|=      CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_DO]|=              CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_CALL]|=            CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_CHECKSUM]|=        CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_ANALYZE]|=         CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_CHECK]|=           CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_OPTIMIZE]|=        CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_REPAIR]|=          CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_PRELOAD_KEYS]|=    CF_PREOPEN_TMP_TABLES;
  sql_command_flags[SQLCOM_ASSIGN_TO_KEYCACHE]|= CF_PREOPEN_TMP_TABLES;

  
  sql_command_flags[SQLCOM_CREATE_TABLE]|=    CF_HA_CLOSE;
  sql_command_flags[SQLCOM_DROP_TABLE]|=      CF_HA_CLOSE;
  sql_command_flags[SQLCOM_ALTER_TABLE]|=     CF_HA_CLOSE;
  sql_command_flags[SQLCOM_TRUNCATE]|=        CF_HA_CLOSE;
  sql_command_flags[SQLCOM_REPAIR]|=          CF_HA_CLOSE;
  sql_command_flags[SQLCOM_OPTIMIZE]|=        CF_HA_CLOSE;
  sql_command_flags[SQLCOM_ANALYZE]|=         CF_HA_CLOSE;
  sql_command_flags[SQLCOM_CHECK]|=           CF_HA_CLOSE;
  sql_command_flags[SQLCOM_CREATE_INDEX]|=    CF_HA_CLOSE;
  sql_command_flags[SQLCOM_DROP_INDEX]|=      CF_HA_CLOSE;
  sql_command_flags[SQLCOM_PRELOAD_KEYS]|=    CF_HA_CLOSE;
  sql_command_flags[SQLCOM_ASSIGN_TO_KEYCACHE]|=  CF_HA_CLOSE;

  
  sql_command_flags[SQLCOM_CREATE_TABLE]|=     CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_TABLE]|=      CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_TABLE]|=       CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_RENAME_TABLE]|=     CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_INDEX]|=     CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_INDEX]|=       CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_DB]|=        CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_DB]|=          CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_DB_UPGRADE]|= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_DB]|=         CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_VIEW]|=      CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_VIEW]|=        CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_TRIGGER]|=   CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_TRIGGER]|=     CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_EVENT]|=     CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_EVENT]|=      CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_EVENT]|=       CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_USER]|=      CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_RENAME_USER]|=      CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_USER]|=       CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_USER]|=        CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_SERVER]|=    CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_SERVER]|=     CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_SERVER]|=      CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_FUNCTION]|=  CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_PROCEDURE]|= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_CREATE_SPFUNCTION]|=CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_PROCEDURE]|=   CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_DROP_FUNCTION]|=    CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_PROCEDURE]|=  CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_FUNCTION]|=   CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_TRUNCATE]|=         CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_ALTER_TABLESPACE]|= CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_REPAIR]|=           CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_OPTIMIZE]|=         CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_GRANT]|=            CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_REVOKE]|=           CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_REVOKE_ALL]|=       CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_INSTALL_PLUGIN]|=   CF_DISALLOW_IN_RO_TRANS;
  sql_command_flags[SQLCOM_UNINSTALL_PLUGIN]|= CF_DISALLOW_IN_RO_TRANS;
}

bool sqlcom_can_generate_row_events(const THD *thd)
{
  return (sql_command_flags[thd->lex->sql_command] &
          CF_CAN_GENERATE_ROW_EVENTS);
}
 
bool is_update_query(enum enum_sql_command command)
{
  DBUG_ASSERT(command >= 0 && command <= SQLCOM_END);
  return (sql_command_flags[command] & CF_CHANGES_DATA) != 0;
}


bool is_explainable_query(enum enum_sql_command command)
{
  DBUG_ASSERT(command >= 0 && command <= SQLCOM_END);
  return (sql_command_flags[command] & CF_CAN_BE_EXPLAINED) != 0;
}


bool is_log_table_write_query(enum enum_sql_command command)
{
  DBUG_ASSERT(command >= 0 && command <= SQLCOM_END);
  return (sql_command_flags[command] & CF_WRITE_LOGS_COMMAND) != 0;
}

void execute_init_command(THD *thd, LEX_STRING *init_command,
                          mysql_rwlock_t *var_lock)
{
  Vio* save_vio;
  ulong save_client_capabilities;

  mysql_rwlock_rdlock(var_lock);
  if (!init_command->length)
  {
    mysql_rwlock_unlock(var_lock);
    return;
  }

  
  size_t len= init_command->length;
  char *buf= thd->strmake(init_command->str, len);
  mysql_rwlock_unlock(var_lock);

#if defined(ENABLED_PROFILING)
  thd->profiling.start_new_query();
  thd->profiling.set_query_source(buf, len);
#endif

  THD_STAGE_INFO(thd, stage_execution_of_init_command);
  save_client_capabilities= thd->client_capabilities;
  thd->client_capabilities|= CLIENT_MULTI_QUERIES;
  
  save_vio= thd->net.vio;
  thd->net.vio= 0;
  dispatch_command(COM_QUERY, thd, buf, len);
  thd->client_capabilities= save_client_capabilities;
  thd->net.vio= save_vio;

#if defined(ENABLED_PROFILING)
  thd->profiling.finish_current_query();
#endif
}

static char *fgets_fn(char *buffer, size_t size, fgets_input_t input, int *error)
{
  MYSQL_FILE *in= static_cast<MYSQL_FILE*> (input);
  char *line= mysql_file_fgets(buffer, size, in);
  if (error)
    *error= (line == NULL) ? ferror(in->m_file) : 0;
  return line;
}

static void handle_bootstrap_impl(THD *thd)
{
  MYSQL_FILE *file= bootstrap_file;
  char buffer[MAX_BOOTSTRAP_QUERY_SIZE];
  char *query;
  int length;
  int rc;
  int error= 0;

  DBUG_ENTER("handle_bootstrap");

#ifndef EMBEDDED_LIBRARY
  pthread_detach_this_thread();
  thd->thread_stack= (char*) &thd;
#endif 

  thd->security_ctx->user= (char*) my_strdup("boot", MYF(MY_WME));
  thd->security_ctx->priv_user[0]= thd->security_ctx->priv_host[0]=0;
  
  thd->client_capabilities|= CLIENT_MULTI_RESULTS;

  thd->init_for_queries();

  buffer[0]= '\0';

  for ( ; ; )
  {
    rc= read_bootstrap_query(buffer, &length, file, fgets_fn, &error);

    if (rc == READ_BOOTSTRAP_EOF)
      break;
    
    if (rc != READ_BOOTSTRAP_SUCCESS)
    {
      
      thd->get_stmt_da()->reset_diagnostics_area();

      
      char *err_ptr= buffer + (length <= MAX_BOOTSTRAP_ERROR_LEN ?
                                        0 : (length - MAX_BOOTSTRAP_ERROR_LEN));
      switch (rc)
      {
      case READ_BOOTSTRAP_ERROR:
        my_printf_error(ER_UNKNOWN_ERROR, "Bootstrap file error, return code (%d). "
                        "Nearest query: '%s'", MYF(0), error, err_ptr);
        break;

      case READ_BOOTSTRAP_QUERY_SIZE:
        my_printf_error(ER_UNKNOWN_ERROR, "Boostrap file error. Query size "
                        "exceeded %d bytes near '%s'.", MYF(0),
                        MAX_BOOTSTRAP_LINE_SIZE, err_ptr);
        break;

      default:
        DBUG_ASSERT(false);
        break;
      }

      thd->protocol->end_statement();
      bootstrap_error= 1;
      break;
    }

    query= (char *) thd->memdup_w_gap(buffer, length + 1,
                                      thd->db_length + 1 +
                                      QUERY_CACHE_FLAGS_SIZE);
    size_t db_len= 0;
    memcpy(query + length + 1, (char *) &db_len, sizeof(size_t));
    thd->set_query_and_id(query, length, thd->charset(), next_query_id());
    DBUG_PRINT("query",("%-.4096s",thd->query()));
#if defined(ENABLED_PROFILING)
    thd->profiling.start_new_query();
    thd->profiling.set_query_source(thd->query(), length);
#endif

    
    thd->set_time();
    Parser_state parser_state;
    if (parser_state.init(thd, thd->query(), length))
    {
      thd->protocol->end_statement();
      bootstrap_error= 1;
      break;
    }

    mysql_parse(thd, thd->query(), length, &parser_state);

    bootstrap_error= thd->is_error();
    thd->protocol->end_statement();

#if defined(ENABLED_PROFILING)
    thd->profiling.finish_current_query();
#endif

    if (bootstrap_error)
      break;

    free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));
    free_root(&thd->transaction.mem_root,MYF(MY_KEEP_PREALLOC));
  }

  DBUG_VOID_RETURN;
}




pthread_handler_t handle_bootstrap(void *arg)
{
  THD *thd=(THD*) arg;

  mysql_thread_set_psi_id(thd->thread_id);

  do_handle_bootstrap(thd);
  return 0;
}

void do_handle_bootstrap(THD *thd)
{
  bool thd_added= false;
  
  thd->thread_stack= (char*) &thd;
  if (my_thread_init() || thd->store_globals())
  {
#ifndef EMBEDDED_LIBRARY
    close_connection(thd, ER_OUT_OF_RESOURCES);
#endif
    thd->fatal_error();
    goto end;
  }

  mysql_mutex_lock(&LOCK_thread_count);
  thd_added= true;
  add_global_thread(thd);
  mysql_mutex_unlock(&LOCK_thread_count);

  handle_bootstrap_impl(thd);

end:
  net_end(&thd->net);
  thd->release_resources();

  if (thd_added)
  {
    remove_global_thread(thd);
  }
  
  delete thd;

  mysql_mutex_lock(&LOCK_thread_count);
  in_bootstrap= FALSE;
  mysql_cond_broadcast(&COND_thread_count);
  mysql_mutex_unlock(&LOCK_thread_count);

#ifndef EMBEDDED_LIBRARY
  my_thread_end();
  pthread_exit(0);
#endif

  return;
}




void free_items(Item *item)
{
  Item *next;
  DBUG_ENTER("free_items");
  for (; item ; item=next)
  {
    next=item->next;
    item->delete_self();
  }
  DBUG_VOID_RETURN;
}


void cleanup_items(Item *item)
{
  DBUG_ENTER("cleanup_items");  
  for (; item ; item=item->next)
    item->cleanup();
  DBUG_VOID_RETURN;
}

#ifndef EMBEDDED_LIBRARY



bool do_command(THD *thd)
{
  bool return_value;
  char *packet= 0;
  ulong packet_length;
  NET *net= &thd->net;
  enum enum_server_command command;

  DBUG_ENTER("do_command");

  
  thd->lex->current_select= 0;

  
  my_net_set_read_timeout(net, thd->variables.net_wait_timeout);

  
  thd->clear_error();				
  thd->get_stmt_da()->reset_diagnostics_area();

  net_new_transaction(net);

  
  DEBUG_SYNC(thd, "before_do_command_net_read");

  
  thd->m_server_idle= true;
  packet_length= my_net_read(net);
  thd->m_server_idle= false;

  if (packet_length == packet_error)
  {
    DBUG_PRINT("info",("Got error %d reading command from socket %s",
		       net->error,
		       vio_description(net->vio)));

    
    thd->m_statement_psi= MYSQL_REFINE_STATEMENT(thd->m_statement_psi,
                                                 com_statement_info[COM_END].m_key);

    

    
    DBUG_ASSERT(thd->is_error());
    thd->protocol->end_statement();

    
    MYSQL_END_STATEMENT(thd->m_statement_psi, thd->get_stmt_da());
    thd->m_statement_psi= NULL;

    if (net->error != 3)
    {
      return_value= TRUE;                       
      goto out;
    }

    net->error= 0;
    return_value= FALSE;
    goto out;
  }

  packet= (char*) net->read_pos;
  
  if (packet_length == 0)                       
  {
    
    packet[0]= (uchar) COM_SLEEP;
    packet_length= 1;
  }
  
  packet[packet_length]= '\0';                  

  command= (enum enum_server_command) (uchar) packet[0];

  if (command >= COM_END)
    command= COM_END;				

  DBUG_PRINT("info",("Command on %s = %d (%s)",
                     vio_description(net->vio), command,
                     command_name[command].str));

  
  my_net_set_read_timeout(net, thd->variables.net_read_timeout);

  DBUG_ASSERT(packet_length);

  return_value= dispatch_command(command, thd, packet+1, (uint) (packet_length-1));

out:
  
  DBUG_ASSERT(thd->m_statement_psi == NULL);
  DBUG_RETURN(return_value);
}
#endif  



static my_bool deny_updates_if_read_only_option(THD *thd,
                                                TABLE_LIST *all_tables)
{
  DBUG_ENTER("deny_updates_if_read_only_option");

  if (!opt_readonly)
    DBUG_RETURN(FALSE);

  LEX *lex= thd->lex;

  const my_bool user_is_super=
    ((ulong)(thd->security_ctx->master_access & SUPER_ACL) ==
     (ulong)SUPER_ACL);

  if (user_is_super)
    DBUG_RETURN(FALSE);

  if (!(sql_command_flags[lex->sql_command] & CF_CHANGES_DATA))
    DBUG_RETURN(FALSE);

  
  if (lex->sql_command == SQLCOM_UPDATE_MULTI)
    DBUG_RETURN(FALSE);

  const my_bool create_temp_tables= 
    (lex->sql_command == SQLCOM_CREATE_TABLE) &&
    (lex->create_info.options & HA_LEX_CREATE_TMP_TABLE);

  const my_bool drop_temp_tables= 
    (lex->sql_command == SQLCOM_DROP_TABLE) &&
    lex->drop_temporary;

  const my_bool update_real_tables=
    some_non_temp_table_to_be_updated(thd, all_tables) &&
    !(create_temp_tables || drop_temp_tables);


  const my_bool create_or_drop_databases=
    (lex->sql_command == SQLCOM_CREATE_DB) ||
    (lex->sql_command == SQLCOM_DROP_DB);

  if (update_real_tables || create_or_drop_databases)
  {
      
      DBUG_RETURN(TRUE);
  }


  
  DBUG_RETURN(FALSE);
}


bool dispatch_command(enum enum_server_command command, THD *thd,
		      char* packet, uint packet_length)
{
  
	TRACE_FUNCTION_START();
TRACE_START();
SESSION_START();
TRACE_END(1);
  NET *net= &thd->net;
  bool error= 0;
  TRACE_START();
  DBUG_ENTER("dispatch_command");
  TRACE_END(2);
  TRACE_START();
  DBUG_PRINT("info",("packet: '%*.s'; command: %d", packet_length, packet, command));
  TRACE_END(3);

  
#if defined(ENABLED_PROFILING)
  TRACE_START();
  thd->profiling.start_new_query();
  TRACE_END(4);
#endif

  TraceTool::TRACE_START();
get_instance()->start_new_query();
TRACE_END(5);
  
  
  TRACE_START();
  MYSQL_COMMAND_START(thd->thread_id, command,
                      &thd->security_ctx->priv_user[0],
                      (char *) thd->security_ctx->host_or_ip);
  TRACE_END(6);

  
  TRACE_START();
  thd->m_statement_psi= MYSQL_REFINE_STATEMENT(thd->m_statement_psi,
                                               com_statement_info[command].m_key);
  TRACE_END(7);

  TRACE_START();
  thd->set_command(command);
  TRACE_END(8);
  
  thd->enable_slow_log= TRUE;
  thd->lex->sql_command= SQLCOM_END; 
  TRACE_START();
  thd->set_time();
  TRACE_END(9);
  if (TRACE_S_E( !thd->is_valid_time() ,10))
  {
    
    thd->security_ctx->master_access|= SHUTDOWN_ACL;
    command= COM_SHUTDOWN;
  }
  TRACE_START();
  thd->set_query_id(next_query_id());
  TRACE_END(11);
  TRACE_START();
  inc_thread_running();
  TRACE_END(12);

  if (!(server_command_flags[command] & CF_SKIP_QUESTIONS)) {
    TRACE_START();
    statistic_increment(thd->status_var.questions, &LOCK_status);
    TRACE_END(13);
} 

  
  thd->server_status&= ~SERVER_STATUS_CLEAR_SET;

  
  if (TRACE_S_E( unlikely(thd->security_ctx->password_expired &&
               command != COM_QUERY &&
               command != COM_STMT_CLOSE &&
               command != COM_STMT_SEND_LONG_DATA &&
               command != COM_PING &&
               command != COM_QUIT) ,14))
  {
    TRACE_START();
    my_error(ER_MUST_CHANGE_PASSWORD, MYF(0));
    TRACE_END(15);
    goto done;
  }

  switch (command) {
  case COM_INIT_DB:
  {
    LEX_STRING tmp;
    TRACE_START();
    status_var_increment(thd->status_var.com_stat[SQLCOM_CHANGE_DB]);
    TRACE_END(16);
    TRACE_START();
    thd->convert_string(&tmp, system_charset_info,
			packet, packet_length, thd->charset());
    TRACE_END(17);
    if (TRACE_S_E( !mysql_change_db(thd, &tmp, FALSE) ,18))
    {
      TRACE_START();
      general_log_write(thd, command, thd->db, thd->db_length);
      TRACE_END(19);
      TRACE_START();
      my_ok(thd);
      TRACE_END(20);
    }
    break;
  }
#ifdef HAVE_REPLICATION
  case COM_REGISTER_SLAVE:
  {
    if (TRACE_S_E( !register_slave(thd, (uchar*)packet, packet_length) ,21)) {
      TRACE_START();
      my_ok(thd);
      TRACE_END(22);
} 
    break;
  }
#endif
  case COM_CHANGE_USER:
  {
    int auth_rc;
    TRACE_START();
    status_var_increment(thd->status_var.com_other);
    TRACE_END(23);

    TRACE_START();
    thd->change_user();
    TRACE_END(24);
    TRACE_START();
    thd->clear_error();
    TRACE_END(25);                         

    
    net->read_pos= (uchar*)packet;

    TRACE_START();
    USER_CONN *save_user_connect=
      const_cast<USER_CONN*>(thd->get_user_connect());
    TRACE_END(26);
    char *save_db= thd->db;
    uint save_db_length= thd->db_length;
    Security_context save_security_ctx= *thd->security_ctx;

    TRACE_START();
    auth_rc= acl_authenticate(thd, packet_length);
    TRACE_END(27);
    TRACE_START();
    MYSQL_AUDIT_NOTIFY_CONNECTION_CHANGE_USER(thd);
    TRACE_END(28);
    if (auth_rc)
    {
      TRACE_START();
      my_free(thd->security_ctx->user);
      TRACE_END(29);
      *thd->security_ctx= save_security_ctx;
      TRACE_START();
      thd->set_user_connect(save_user_connect);
      TRACE_END(30);
      TRACE_START();
      thd->reset_db(save_db, save_db_length);
      TRACE_END(31);

      TRACE_START();
      my_error(ER_ACCESS_DENIED_CHANGE_USER_ERROR, MYF(0),
               thd->security_ctx->user,
               thd->security_ctx->host_or_ip,
               (thd->password ? ER(ER_YES) : ER(ER_NO)));
      TRACE_END(32);
      thd->killed= THD::KILL_CONNECTION;
      error=true;
    }
    else
    {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      
      if (save_user_connect) {
	TRACE_START();
	decrease_user_connections(save_user_connect);
	TRACE_END(33);
} 
#endif 
      TRACE_START();
      mysql_mutex_lock(&thd->LOCK_thd_data);
      TRACE_END(34);
      TRACE_START();
      my_free(save_db);
      TRACE_END(35);
      TRACE_START();
      mysql_mutex_unlock(&thd->LOCK_thd_data);
      TRACE_END(36);
      TRACE_START();
      my_free(save_security_ctx.user);
      TRACE_END(37);

    }
    break;
  }
  case COM_STMT_EXECUTE:
  {
    TRACE_START();
    mysqld_stmt_execute(thd, packet, packet_length);
    TRACE_END(38);
    break;
  }
  case COM_STMT_FETCH:
  {
    TRACE_START();
    mysqld_stmt_fetch(thd, packet, packet_length);
    TRACE_END(39);
    break;
  }
  case COM_STMT_SEND_LONG_DATA:
  {
    TRACE_START();
    mysql_stmt_get_longdata(thd, packet, packet_length);
    TRACE_END(40);
    break;
  }
  case COM_STMT_PREPARE:
  {
    TRACE_START();
    mysqld_stmt_prepare(thd, packet, packet_length);
    TRACE_END(41);
    break;
  }
  case COM_STMT_CLOSE:
  {
    TRACE_START();
    mysqld_stmt_close(thd, packet, packet_length);
    TRACE_END(42);
    break;
  }
  case COM_STMT_RESET:
  {
    TRACE_START();
    mysqld_stmt_reset(thd, packet, packet_length);
    TRACE_END(43);
    break;
  }
  case COM_QUERY:
  {
    if (TRACE_S_E( alloc_query(thd, packet, packet_length) ,44))
      break;					
    TRACE_START();
    MYSQL_QUERY_START(thd->query(), thd->thread_id,
                      (char *) (thd->db ? thd->db : ""),
                      &thd->security_ctx->priv_user[0],
                      (char *) thd->security_ctx->host_or_ip);
    TRACE_END(45);
    TRACE_START();
    char *packet_end= thd->query() + thd->query_length();
    TRACE_END(46);
    TraceTool::TRACE_START();
get_instance()->set_query(thd->query());
TRACE_END(47);

    if (opt_log_raw) {
      TRACE_START();
      general_log_write(thd, command, thd->query(), thd->query_length());
      TRACE_END(48);
} 

    TRACE_START();
    DBUG_PRINT("query",("%-.4096s",thd->query()));
    TRACE_END(49);

#if defined(ENABLED_PROFILING)
    TRACE_START();
    thd->profiling.set_query_source(thd->query(), thd->query_length());
    TRACE_END(50);
#endif

    TRACE_START();
    MYSQL_SET_STATEMENT_TEXT(thd->m_statement_psi, thd->query(), thd->query_length());
    TRACE_END(51);

    Parser_state parser_state;
    if (TRACE_S_E( parser_state.init(thd, thd->query(), thd->query_length()) ,52))
      break;

    TraceTool::path_count++;
    TRACE_START();
    mysql_parse(thd, thd->query(), thd->query_length(), &parser_state);
    TRACE_END(53);
    TraceTool::path_count--;

    while (!thd->killed && (parser_state.m_lip.found_semicolon != NULL)  && TRACE_S_E( 
           ! thd->is_error() ,54))
    {
      
      char *beginning_of_next_stmt= (char*) parser_state.m_lip.found_semicolon;

      
      TRACE_START();
      thd->update_server_status();
      TRACE_END(55);
      TRACE_START();
      thd->protocol->end_statement();
      TRACE_END(56);
      TRACE_START();
      query_cache_end_of_result(thd);
      TRACE_END(57);

      TRACE_START();
      mysql_audit_general(thd, MYSQL_AUDIT_GENERAL_STATUS,
                          thd->get_stmt_da()->is_error() ?
                          thd->get_stmt_da()->sql_errno() : 0,
                          command_name[command].str);
      TRACE_END(58);

      ulong length= (ulong)(packet_end - beginning_of_next_stmt);

      TRACE_START();
      log_slow_statement(thd);
      TRACE_END(59);

      
      while (length > 0  && TRACE_S_E(  my_isspace(thd->charset(), *beginning_of_next_stmt) ,60))
      {
        beginning_of_next_stmt++;
        length--;
      }


      TRACE_START();
      MYSQL_END_STATEMENT(thd->m_statement_psi, thd->get_stmt_da());
      TRACE_END(61);
      thd->m_statement_psi= NULL;


      if (TRACE_S_E( MYSQL_QUERY_DONE_ENABLED() ,62))
      {
        TRACE_START();
        MYSQL_QUERY_DONE(thd->is_error());
        TRACE_END(63);
      }


#if defined(ENABLED_PROFILING)
      TRACE_START();
      thd->profiling.finish_current_query();
      TRACE_END(64);
#endif


#if defined(ENABLED_PROFILING)
      TRACE_START();
      thd->profiling.start_new_query("continuing");
      TRACE_END(65);
      TRACE_START();
      thd->profiling.set_query_source(beginning_of_next_stmt, length);
      TRACE_END(66);
#endif


      TRACE_START();
      MYSQL_QUERY_START(beginning_of_next_stmt, thd->thread_id,
                        (char *) (thd->db ? thd->db : ""),
                        &thd->security_ctx->priv_user[0],
                        (char *) thd->security_ctx->host_or_ip);
      TRACE_END(67);


      TRACE_START();
      thd->m_statement_psi= MYSQL_START_STATEMENT(&thd->m_statement_state,
                                                  com_statement_info[command].m_key,
                                                  thd->db, thd->db_length,
                                                  thd->charset());
      TRACE_END(68);
      TRACE_START();
      THD_STAGE_INFO(thd, stage_init);
      TRACE_END(69);
      TRACE_START();
      MYSQL_SET_STATEMENT_TEXT(thd->m_statement_psi, beginning_of_next_stmt, length);
      TRACE_END(70);

      TRACE_START();
      thd->set_query_and_id(beginning_of_next_stmt, length,
                            thd->charset(), next_query_id());
      TRACE_END(71);
      
      TRACE_START();
      statistic_increment(thd->status_var.questions, &LOCK_status);
      TRACE_END(72);
      TRACE_START();
      thd->set_time();
      TRACE_END(73); 
      TRACE_START();
      parser_state.reset(beginning_of_next_stmt, length);
      TRACE_END(74);
      
      TRACE_START();
      mysql_parse(thd, beginning_of_next_stmt, length, &parser_state);
      TRACE_END(75);
    }

    TRACE_START();
    DBUG_PRINT("info",("query ready"));
    TRACE_END(76);
    break;
  }
  case COM_FIELD_LIST:				
#ifdef DONT_ALLOW_SHOW_COMMANDS
    TRACE_START();
    my_message(ER_NOT_ALLOWED_COMMAND, ER(ER_NOT_ALLOWED_COMMAND),
               MYF(0));
    TRACE_END(77);	
    break;
#else
  {
    char *fields, *packet_end= packet + packet_length, *arg_end;
    
    TABLE_LIST table_list;
    LEX_STRING table_name;
    LEX_STRING db;
    
    TRACE_START();
    MDL_savepoint mdl_savepoint= thd->mdl_context.mdl_savepoint();
    TRACE_END(78);

    TRACE_START();
    status_var_increment(thd->status_var.com_stat[SQLCOM_SHOW_FIELDS]);
    TRACE_END(79);
    if (TRACE_S_E( thd->copy_db_to(&db.str, &db.length) ,80))
      break;
    
    TRACE_START();
    arg_end= strend(packet);
    TRACE_END(81);
    uint arg_length= arg_end - packet;

    
    if (arg_length >= packet_length || arg_length > NAME_LEN)
    {
      TRACE_START();
      my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
      TRACE_END(82);
      break;
    }
    TRACE_START();
    thd->convert_string(&table_name, system_charset_info,
			packet, arg_length, thd->charset());
    TRACE_END(83);
    TRACE_START();
    enum_ident_name_check ident_check_status=
      check_table_name(table_name.str, table_name.length, FALSE);
    TRACE_END(84);
    if (ident_check_status == IDENT_NAME_WRONG)
    {
      
      TRACE_START();
      my_error(ER_WRONG_TABLE_NAME, MYF(0), table_name.str);
      TRACE_END(85);
      break;
    }
    else if (ident_check_status == IDENT_NAME_TOO_LONG)
    {
      TRACE_START();
      my_error(ER_TOO_LONG_IDENT, MYF(0), table_name.str);
      TRACE_END(86);
      break;
    }
    packet= arg_end + 1;
    TRACE_START();
    mysql_reset_thd_for_next_command(thd);
    TRACE_END(87);
    TRACE_START();
    lex_start(thd);
    TRACE_END(88);
    
    if (lower_case_table_names) {
      TRACE_START();
      table_name.length= my_casedn_str(files_charset_info, table_name.str);
      TRACE_END(89);
} 
    TRACE_START();
    table_list.init_one_table(db.str, db.length, table_name.str,
                              table_name.length, table_name.str, TL_READ);
    TRACE_END(90);
    
    table_list.select_lex= &(thd->lex->select_lex);
    TRACE_START();
    thd->lex->
      select_lex.table_list.link_in_list(&table_list,
                                         &table_list.next_local);
    TRACE_END(91);
    TRACE_START();
    thd->lex->add_to_query_tables(&table_list);
    TRACE_END(92);

    if (TRACE_S_E( is_infoschema_db(table_list.db, table_list.db_length) ,93))
    {
      TRACE_START();
      ST_SCHEMA_TABLE *schema_table= find_schema_table(thd, table_list.alias);
      TRACE_END(94);
      if (schema_table)
        table_list.schema_table= schema_table;
    }

    uint query_length= (uint) (packet_end - packet); 
    if (TRACE_S_E( !(fields= (char *) thd->memdup(packet, query_length + 1)) ,95))
      break;
    TRACE_START();
    thd->set_query(fields, query_length);
    TRACE_END(96);
    TRACE_START();
    general_log_print(thd, command, "%s %s", table_list.table_name, fields);
    TRACE_END(97);

    if (TRACE_S_E( open_temporary_tables(thd, &table_list) ,98))
      break;

    if (TRACE_S_E( check_table_access(thd, SELECT_ACL, &table_list,
                           TRUE, UINT_MAX, FALSE) ,99))
      break;
    
    thd->lex->sql_command= SQLCOM_SHOW_FIELDS;

    
    TRACE_START();
    Opt_trace_start ots(thd, &table_list, thd->lex->sql_command, NULL,
                        NULL, 0, NULL, NULL);
    TRACE_END(100);

    TRACE_START();
    mysqld_list_fields(thd,&table_list,fields);
    TRACE_END(101);

    TRACE_START();
    thd->lex->unit.cleanup();
    TRACE_END(102);
    
    TRACE_START();
    DBUG_ASSERT(thd->transaction.stmt.is_empty());
    TRACE_END(103);
    TRACE_START();
    close_thread_tables(thd);
    TRACE_END(104);
    TRACE_START();
    thd->mdl_context.rollback_to_savepoint(mdl_savepoint);
    TRACE_END(105);

    if (thd->transaction_rollback_request)
    {
      
      TRACE_START();
      trans_rollback_implicit(thd);
      TRACE_END(106);
      TRACE_START();
      thd->mdl_context.release_transactional_locks();
      TRACE_END(107);
    }

    TRACE_START();
    thd->cleanup_after_query();
    TRACE_END(108);
    break;
  }
#endif
  case COM_QUIT:
    
    TRACE_START();
    general_log_print(thd, command, NullS);
    TRACE_END(109);
    net->error=0;				
    TRACE_START();
    thd->get_stmt_da()->disable_status();
    TRACE_END(110);              
    error=TRUE;					
    break;
#ifndef EMBEDDED_LIBRARY
  case COM_BINLOG_DUMP_GTID:
    TRACE_START();
    error= com_binlog_dump_gtid(thd, packet, packet_length);
    TRACE_END(111);
    break;
  case COM_BINLOG_DUMP:
    TRACE_START();
    error= com_binlog_dump(thd, packet, packet_length);
    TRACE_END(112);
    break;
#endif
  case COM_REFRESH:
  {
    int not_used;

    if (packet_length < 1)
    {
      TRACE_START();
      my_error(ER_MALFORMED_PACKET, MYF(0));
      TRACE_END(113);
      break;
    }

    
    TRACE_START();
    lex_start(thd);
    TRACE_END(114);
    
    TRACE_START();
    status_var_increment(thd->status_var.com_stat[SQLCOM_FLUSH]);
    TRACE_END(115);
    ulong options= (ulong) (uchar) packet[0];
    if (TRACE_S_E( trans_commit_implicit(thd) ,116))
      break;
    TRACE_START();
    thd->mdl_context.release_transactional_locks();
    TRACE_END(117);
    if (TRACE_S_E( check_global_access(thd,RELOAD_ACL) ,118))
      break;
    TRACE_START();
    general_log_print(thd, command, NullS);
    TRACE_END(119);
#ifndef DBUG_OFF
    bool debug_simulate= FALSE;
    TRACE_START();
    DBUG_EXECUTE_IF("simulate_detached_thread_refresh", debug_simulate= TRUE;);
    TRACE_END(120);
    if (debug_simulate)
    {
      
      bool res;
      TRACE_START();
      my_pthread_setspecific_ptr(THR_THD, NULL);
      TRACE_END(121);
      TRACE_START();
      res= reload_acl_and_cache(NULL, options | REFRESH_FAST,
                                NULL, &not_used);
      TRACE_END(122);
      TRACE_START();
      my_pthread_setspecific_ptr(THR_THD, thd);
      TRACE_END(123);
      if (res)
        break;
    }
    else
#endif
    if (TRACE_S_E( reload_acl_and_cache(thd, options, (TABLE_LIST*) 0, &not_used) ,124))
      break;
    if (TRACE_S_E( trans_commit_implicit(thd) ,125))
      break;
    TRACE_START();
    close_thread_tables(thd);
    TRACE_END(126);
    TRACE_START();
    thd->mdl_context.release_transactional_locks();
    TRACE_END(127);
    TRACE_START();
    my_ok(thd);
    TRACE_END(128);
    break;
  }
#ifndef EMBEDDED_LIBRARY
  case COM_SHUTDOWN:
  {
    if (packet_length < 1)
    {
      TRACE_START();
      my_error(ER_MALFORMED_PACKET, MYF(0));
      TRACE_END(129);
      break;
    }
    TRACE_START();
    status_var_increment(thd->status_var.com_other);
    TRACE_END(130);
    if (TRACE_S_E( check_global_access(thd,SHUTDOWN_ACL) ,131))
      break; 
    
    enum mysql_enum_shutdown_level level;
    if (TRACE_S_E( !thd->is_valid_time() ,132))
      level= SHUTDOWN_DEFAULT;
    else
      level= (enum mysql_enum_shutdown_level) (uchar) packet[0];
    if (level == SHUTDOWN_DEFAULT)
      level= SHUTDOWN_WAIT_ALL_BUFFERS; 
    else if (level != SHUTDOWN_WAIT_ALL_BUFFERS)
    {
      TRACE_START();
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "this shutdown level");
      TRACE_END(133);
      break;
    }
    TRACE_START();
    DBUG_PRINT("quit",("Got shutdown command for level %u", level));
    TRACE_END(134);
    TRACE_START();
    general_log_print(thd, command, NullS);
    TRACE_END(135);
    TRACE_START();
    my_eof(thd);
    TRACE_END(136);
    TRACE_START();
    kill_mysql();
    TRACE_END(137);
    error=TRUE;
    break;
  }
#endif
  case COM_STATISTICS:
  {
    STATUS_VAR current_global_status_var;
    ulong uptime;
    TRACE_START();
    uint length __attribute__((unused));
    TRACE_END(138);
    ulonglong queries_per_second1000;
    char buff[250];
    TRACE_START();
    uint buff_len= sizeof(buff);
    TRACE_END(139);

    TRACE_START();
    general_log_print(thd, command, NullS);
    TRACE_END(140);
    TRACE_START();
    status_var_increment(thd->status_var.com_stat[SQLCOM_SHOW_STATUS]);
    TRACE_END(141);
    TRACE_START();
    calc_sum_of_all_status(&current_global_status_var);
    TRACE_END(142);
    if (!(uptime= (ulong) (thd->start_time.tv_sec - server_start_time)))
      queries_per_second1000= 0;
    else
       {
TRACE_START();
queries_per_second1000= thd->query_id * LL(1000) / uptime;
TRACE_END(143);
} 

    TRACE_START();
    length= my_snprintf(buff, buff_len - 1,
                        "Uptime: %lu  Threads: %d  Questions: %lu  "
                        "Slow queries: %llu  Opens: %llu  Flush tables: %lu  "
                        "Open tables: %u  Queries per second avg: %u.%03u",
                        uptime,
                        (int) get_thread_count(), (ulong) thd->query_id,
                        current_global_status_var.long_query_count,
                        current_global_status_var.opened_tables,
                        refresh_version,
                        table_cache_manager.cached_tables(),
                        (uint) (queries_per_second1000 / 1000),
                        (uint) (queries_per_second1000 % 1000));
    TRACE_END(144);
#ifdef EMBEDDED_LIBRARY
    
    TRACE_START();
    my_ok(thd, 0, 0, buff);
    TRACE_END(145);
#else
    TRACE_START();
    (void) my_net_write(net, (uchar*) buff, length);
    TRACE_END(146);
    TRACE_START();
    (void) net_flush(net);
    TRACE_END(147);
    TRACE_START();
    thd->get_stmt_da()->disable_status();
    TRACE_END(148);
#endif
    break;
  }
  case COM_PING:
    TRACE_START();
    status_var_increment(thd->status_var.com_other);
    TRACE_END(149);
    TRACE_START();
    my_ok(thd);
    TRACE_END(150);				
    break;
  case COM_PROCESS_INFO:
    TRACE_START();
    status_var_increment(thd->status_var.com_stat[SQLCOM_SHOW_PROCESSLIST]);
    TRACE_END(151);
    if (!thd->security_ctx->priv_user[0]  && TRACE_S_E( 
        check_global_access(thd, PROCESS_ACL) ,152))
      break;
    TRACE_START();
    general_log_print(thd, command, NullS);
    TRACE_END(153);
    TRACE_START();
    mysqld_list_processes(thd,
			  thd->security_ctx->master_access & PROCESS_ACL ? 
			  NullS : thd->security_ctx->priv_user, 0);
    TRACE_END(154);
    break;
  case COM_PROCESS_KILL:
  {
    if (thread_id & (~0xfffffffful)) {
      TRACE_START();
      my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "thread_id", "mysql_kill()");
      TRACE_END(155);
} 
    else if (packet_length < 4) {
      TRACE_START();
      my_error(ER_MALFORMED_PACKET, MYF(0));
      TRACE_END(156);
} 
    else
    {
      TRACE_START();
      status_var_increment(thd->status_var.com_stat[SQLCOM_KILL]);
      TRACE_END(157);
      TRACE_START();
      ulong id=(ulong) uint4korr(packet);
      TRACE_END(158);
      TRACE_START();
      sql_kill(thd,id,false);
      TRACE_END(159);
    }
    break;
  }
  case COM_SET_OPTION:
  {

    if (packet_length < 2)
    {
      TRACE_START();
      my_error(ER_MALFORMED_PACKET, MYF(0));
      TRACE_END(160);
      break;
    }
    TRACE_START();
    status_var_increment(thd->status_var.com_stat[SQLCOM_SET_OPTION]);
    TRACE_END(161);
    TRACE_START();
    uint opt_command= uint2korr(packet);
    TRACE_END(162);

    switch (opt_command) {
    TRACE_START();
    case (int) MYSQL_OPTION_MULTI_STATEMENTS_ON:
    TRACE_END(163);
      thd->client_capabilities|= CLIENT_MULTI_STATEMENTS;
      TRACE_START();
      my_eof(thd);
      TRACE_END(164);
      break;
    TRACE_START();
    case (int) MYSQL_OPTION_MULTI_STATEMENTS_OFF:
    TRACE_END(165);
      thd->client_capabilities&= ~CLIENT_MULTI_STATEMENTS;
      TRACE_START();
      my_eof(thd);
      TRACE_END(166);
      break;
    default:
      TRACE_START();
      my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
      TRACE_END(167);
      break;
    }
    break;
  }
  case COM_DEBUG:
    TRACE_START();
    status_var_increment(thd->status_var.com_other);
    TRACE_END(168);
    if (TRACE_S_E( check_global_access(thd, SUPER_ACL) ,169))
      break;					
    TRACE_START();
    mysql_print_status();
    TRACE_END(170);
    TRACE_START();
    general_log_print(thd, command, NullS);
    TRACE_END(171);
    TRACE_START();
    my_eof(thd);
    TRACE_END(172);
    break;
  case COM_SLEEP:
  case COM_CONNECT:				
  case COM_TIME:				
  case COM_DELAYED_INSERT:
  case COM_END:
  default:
    TRACE_START();
    my_message(ER_UNKNOWN_COM_ERROR, ER(ER_UNKNOWN_COM_ERROR), MYF(0));
    TRACE_END(173);
    break;
  }

done:
  TRACE_START();
  DBUG_ASSERT(thd->derived_tables == NULL &&
              (thd->open_tables == NULL ||
               (thd->locked_tables_mode == LTM_LOCK_TABLES)));
  TRACE_END(174);

  
  TRACE_START();
  thd->update_server_status();
  TRACE_END(175);
  if (thd->killed) {
    TRACE_START();
    thd->send_kill_message();
    TRACE_END(176);
} 
  TRACE_START();
  thd->protocol->end_statement();
  TRACE_END(177);
  TRACE_START();
  query_cache_end_of_result(thd);
  TRACE_END(178);

  if (TRACE_S_E( !thd->is_error()  ,179) && TRACE_S_E(  !thd->killed_errno() ,180)) {
    TRACE_START();
    mysql_audit_general(thd, MYSQL_AUDIT_GENERAL_RESULT, 0, 0);
    TRACE_END(181);
} 

  TRACE_START();
  mysql_audit_general(thd, MYSQL_AUDIT_GENERAL_STATUS,
                      thd->get_stmt_da()->is_error() ?
                      thd->get_stmt_da()->sql_errno() : 0,
                      command_name[command].str);
  TRACE_END(182);

  TRACE_START();
  log_slow_statement(thd);
  TRACE_END(183);

  TRACE_START();
  THD_STAGE_INFO(thd, stage_cleaning_up);
  TRACE_END(184);

  TRACE_START();
  thd->reset_query();
  TRACE_END(185);
  TRACE_START();
  thd->set_command(COM_SLEEP);
  TRACE_END(186);

  
  TRACE_START();
  MYSQL_END_STATEMENT(thd->m_statement_psi, thd->get_stmt_da());
  TRACE_END(187);
  thd->m_statement_psi= NULL;

  TRACE_START();
  dec_thread_running();
  TRACE_END(188);
  TRACE_START();
  thd->packet.shrink(thd->variables.net_buffer_length);
  TRACE_END(189);	
  TRACE_START();
  free_root(thd->mem_root,MYF(MY_KEEP_PREALLOC));
  TRACE_END(190);
  
  TraceTool::TRACE_START();
get_instance()->end_query();
TRACE_END(191);

  
  if (TRACE_S_E( MYSQL_QUERY_DONE_ENABLED()  ,192) || TRACE_S_E(  MYSQL_COMMAND_DONE_ENABLED() ,193))
  {
    TRACE_START();
    int res __attribute__((unused));
    TRACE_END(194);
    TRACE_START();
    res= (int) thd->is_error();
    TRACE_END(195);
    if (command == COM_QUERY)
    {
      TRACE_START();
      MYSQL_QUERY_DONE(res);
      TRACE_END(196);
    }
    TRACE_START();
    MYSQL_COMMAND_DONE(res);
    TRACE_END(197);
  }

  
#if defined(ENABLED_PROFILING)
  TRACE_START();
  thd->profiling.finish_current_query();
  TRACE_END(198);
#endif

  TRACE_START();
  SESSION_END(true);
  TRACE_END(199);
  TRACE_FUNCTION_END();
  DBUG_RETURN(error);


	TRACE_FUNCTION_END();
}




bool log_slow_applicable(THD *thd)
{
  DBUG_ENTER("log_slow_applicable");

  
  if (unlikely(thd->in_sub_stmt))
    DBUG_RETURN(false);                         

  
  if (thd->enable_slow_log)
  {
    bool warn_no_index= ((thd->server_status &
                          (SERVER_QUERY_NO_INDEX_USED |
                           SERVER_QUERY_NO_GOOD_INDEX_USED)) &&
                         opt_log_queries_not_using_indexes &&
                         !(sql_command_flags[thd->lex->sql_command] &
                           CF_STATUS_COMMAND));
    bool log_this_query=  ((thd->server_status & SERVER_QUERY_WAS_SLOW) ||
                           warn_no_index) &&
                          (thd->get_examined_row_count() >=
                           thd->variables.min_examined_row_limit);
    bool suppress_logging= log_throttle_qni.log(thd, warn_no_index);

    if (!suppress_logging && log_this_query)
      DBUG_RETURN(true);
  }
  DBUG_RETURN(false);
}




void log_slow_do(THD *thd)
{
  DBUG_ENTER("log_slow_do");

  THD_STAGE_INFO(thd, stage_logging_slow_query);
  thd->status_var.long_query_count++;

  if (thd->rewritten_query.length())
    slow_log_print(thd,
                   thd->rewritten_query.c_ptr_safe(),
                   thd->rewritten_query.length());
  else
    slow_log_print(thd, thd->query(), thd->query_length());

  DBUG_VOID_RETURN;
}




void log_slow_statement(THD *thd)
{
  DBUG_ENTER("log_slow_statement");

  if (log_slow_applicable(thd))
    log_slow_do(thd);

  DBUG_VOID_RETURN;
}




int prepare_schema_table(THD *thd, LEX *lex, Table_ident *table_ident,
                         enum enum_schema_tables schema_table_idx)
{
  SELECT_LEX *schema_select_lex= NULL;
  DBUG_ENTER("prepare_schema_table");

  switch (schema_table_idx) {
  case SCH_SCHEMATA:
#if defined(DONT_ALLOW_SHOW_COMMANDS)
    my_message(ER_NOT_ALLOWED_COMMAND,
               ER(ER_NOT_ALLOWED_COMMAND), MYF(0));   
    DBUG_RETURN(1);
#else
    break;
#endif

  case SCH_TABLE_NAMES:
  case SCH_TABLES:
  case SCH_VIEWS:
  case SCH_TRIGGERS:
  case SCH_EVENTS:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    my_message(ER_NOT_ALLOWED_COMMAND,
               ER(ER_NOT_ALLOWED_COMMAND), MYF(0)); 
    DBUG_RETURN(1);
#else
    {
      LEX_STRING db;
      size_t dummy;
      if (lex->select_lex.db == NULL &&
          lex->copy_db_to(&lex->select_lex.db, &dummy))
      {
        DBUG_RETURN(1);
      }
      schema_select_lex= new SELECT_LEX();
      db.str= schema_select_lex->db= lex->select_lex.db;
      schema_select_lex->table_list.first= NULL;
      db.length= strlen(db.str);

      if (check_and_convert_db_name(&db, FALSE) != IDENT_NAME_OK)
        DBUG_RETURN(1);
      break;
    }
#endif
  case SCH_COLUMNS:
  case SCH_STATISTICS:
  {
#ifdef DONT_ALLOW_SHOW_COMMANDS
    my_message(ER_NOT_ALLOWED_COMMAND,
               ER(ER_NOT_ALLOWED_COMMAND), MYF(0)); 
    DBUG_RETURN(1);
#else
    DBUG_ASSERT(table_ident);
    TABLE_LIST **query_tables_last= lex->query_tables_last;
    schema_select_lex= new SELECT_LEX();
    
    schema_select_lex->parent_lex= lex;
    schema_select_lex->init_query();
    if (!schema_select_lex->add_table_to_list(thd, table_ident, 0, 0, TL_READ,
                                              MDL_SHARED_READ))
      DBUG_RETURN(1);
    lex->query_tables_last= query_tables_last;
    break;
  }
#endif
  case SCH_PROFILES:
    
#if defined(ENABLED_PROFILING)
    thd->profiling.discard_current_query();
#endif
    break;
  case SCH_OPTIMIZER_TRACE:
  case SCH_OPEN_TABLES:
  case SCH_VARIABLES:
  case SCH_STATUS:
  case SCH_PROCEDURES:
  case SCH_CHARSETS:
  case SCH_ENGINES:
  case SCH_COLLATIONS:
  case SCH_COLLATION_CHARACTER_SET_APPLICABILITY:
  case SCH_USER_PRIVILEGES:
  case SCH_SCHEMA_PRIVILEGES:
  case SCH_TABLE_PRIVILEGES:
  case SCH_COLUMN_PRIVILEGES:
  case SCH_TABLE_CONSTRAINTS:
  case SCH_KEY_COLUMN_USAGE:
  default:
    break;
  }
  
  SELECT_LEX *select_lex= lex->current_select;
  if (make_schema_select(thd, select_lex, schema_table_idx))
  {
    DBUG_RETURN(1);
  }
  TABLE_LIST *table_list= select_lex->table_list.first;
  table_list->schema_select_lex= schema_select_lex;
  table_list->schema_table_reformed= 1;
  DBUG_RETURN(0);
}




bool alloc_query(THD *thd, const char *packet, uint packet_length)
{
  char *query;
  
  while (packet_length > 0 && my_isspace(thd->charset(), packet[0]))
  {
    packet++;
    packet_length--;
  }
  const char *pos= packet + packet_length;     
  while (packet_length > 0 &&
	 (pos[-1] == ';' || my_isspace(thd->charset() ,pos[-1])))
  {
    pos--;
    packet_length--;
  }
  
  if (! (query= (char*) thd->memdup_w_gap(packet,
                                          packet_length,
                                          1 + sizeof(size_t) + thd->db_length +
                                          QUERY_CACHE_FLAGS_SIZE)))
      return TRUE;
  query[packet_length]= '\0';
  
  char *len_pos = (query + packet_length + 1);
  memcpy(len_pos, (char *) &thd->db_length, sizeof(size_t));
    
  thd->set_query(query, packet_length);
  thd->rewritten_query.free();                 

  
  thd->packet.shrink(thd->variables.net_buffer_length);
  thd->convert_buffer.shrink(thd->variables.net_buffer_length);

  return FALSE;
}

static void reset_one_shot_variables(THD *thd) 
{
  thd->variables.character_set_client=
    global_system_variables.character_set_client;
  thd->variables.collation_connection=
    global_system_variables.collation_connection;
  thd->variables.collation_database=
    global_system_variables.collation_database;
  thd->variables.collation_server=
    global_system_variables.collation_server;
  thd->update_charset();
  thd->variables.time_zone=
    global_system_variables.time_zone;
  thd->variables.lc_time_names= &my_locale_en_US;
  thd->one_shot_set= 0;
}


static
bool sp_process_definer(THD *thd)
{
  DBUG_ENTER("sp_process_definer");

  LEX *lex= thd->lex;

  

  if (!lex->definer)
  {
    Prepared_stmt_arena_holder ps_arena_holder(thd);

    lex->definer= create_default_definer(thd);

    
    if (lex->definer == NULL)
      DBUG_RETURN(TRUE);

    if (thd->slave_thread && lex->sphead)
      lex->sphead->m_chistics->suid= SP_IS_NOT_SUID;
  }
  else
  {
    
    if ((strcmp(lex->definer->user.str, thd->security_ctx->priv_user) ||
         my_strcasecmp(system_charset_info, lex->definer->host.str,
                       thd->security_ctx->priv_host)) &&
        check_global_access(thd, SUPER_ACL))
    {
      my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "SUPER");
      DBUG_RETURN(TRUE);
    }
  }

  

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (!is_acl_user(lex->definer->host.str, lex->definer->user.str))
  {
    push_warning_printf(thd,
                        Sql_condition::WARN_LEVEL_NOTE,
                        ER_NO_SUCH_USER,
                        ER(ER_NO_SUCH_USER),
                        lex->definer->user.str,
                        lex->definer->host.str);
  }
#endif 

  DBUG_RETURN(FALSE);
}




static bool lock_tables_open_and_lock_tables(THD *thd, TABLE_LIST *tables)
{
  Lock_tables_prelocking_strategy lock_tables_prelocking_strategy;
  uint counter;
  TABLE_LIST *table;

  thd->in_lock_tables= 1;

  if (open_tables(thd, &tables, &counter, 0, &lock_tables_prelocking_strategy))
    goto err;

  
  for (table= tables; table; table= table->next_global)
    if (!table->placeholder() && table->table->s->tmp_table)
      table->table->reginfo.lock_type= TL_WRITE;

  if (lock_tables(thd, tables, counter, 0) ||
      thd->locked_tables_list.init_locked_tables(thd))
    goto err;

  thd->in_lock_tables= 0;

  return FALSE;

err:
  thd->in_lock_tables= 0;

  trans_rollback_stmt(thd);
  
  trans_rollback(thd);
  
  close_thread_tables(thd);
  DBUG_ASSERT(!thd->locked_tables_mode);
  thd->mdl_context.release_transactional_locks();
  return TRUE;
}




int
mysql_execute_command(THD *thd)
{
  int res= FALSE;
  int  up_result= 0;
  LEX  *lex= thd->lex;
  
  SELECT_LEX *select_lex= &lex->select_lex;
  
  TABLE_LIST *first_table= select_lex->table_list.first;
  
  TABLE_LIST *all_tables;
  
  SELECT_LEX_UNIT *unit= &lex->unit;
#ifdef HAVE_REPLICATION
  
  bool have_table_map_for_update= FALSE;
#endif
  DBUG_ENTER("mysql_execute_command");
  DBUG_ASSERT(!lex->describe || is_explainable_query(lex->sql_command));

  if (unlikely(lex->is_broken()))
  {
    
    Reprepare_observer *reprepare_observer= thd->get_reprepare_observer();
    if (reprepare_observer &&
        reprepare_observer->report_error(thd))
    {
      DBUG_ASSERT(thd->is_error());
      DBUG_RETURN(1);
    }
  }

#ifdef WITH_PARTITION_STORAGE_ENGINE
  thd->work_part_info= 0;
#endif

  DBUG_ASSERT(thd->transaction.stmt.is_empty() || thd->in_sub_stmt);
  
  DBUG_ASSERT(! thd->transaction_rollback_request || thd->in_sub_stmt);
  
  lex->first_lists_tables_same();
  
  all_tables= lex->query_tables;
  
  select_lex->
    context.resolve_in_table_list_only(select_lex->
                                       table_list.first);

  
  if ((sql_command_flags[lex->sql_command] & CF_DIAGNOSTIC_STMT) != 0)
    thd->get_stmt_da()->set_warning_info_read_only(TRUE);
  else
  {
    thd->get_stmt_da()->set_warning_info_read_only(FALSE);
    if (all_tables)
      thd->get_stmt_da()->opt_clear_warning_info(thd->query_id);
  }

#ifdef HAVE_REPLICATION
  if (unlikely(thd->slave_thread))
  {
    
    if (lex->sql_command != SQLCOM_BEGIN &&
        lex->sql_command != SQLCOM_COMMIT &&
        lex->sql_command != SQLCOM_SAVEPOINT &&
        lex->sql_command != SQLCOM_ROLLBACK &&
        lex->sql_command != SQLCOM_ROLLBACK_TO_SAVEPOINT &&
        !rpl_filter->db_ok(thd->db))
    {
      DBUG_RETURN(0);
    }

    if (lex->sql_command == SQLCOM_DROP_TRIGGER)
    {
      
      add_table_for_trigger(thd, thd->lex->spname, 1, &all_tables);
      
      if (!all_tables)
      {
        
        DBUG_RETURN(0);
      }
      
      
      all_tables->updating= 1;
    }

    
    if (lex->sql_command == SQLCOM_UPDATE_MULTI &&
        thd->table_map_for_update)
    {
      have_table_map_for_update= TRUE;
      table_map table_map_for_update= thd->table_map_for_update;
      uint nr= 0;
      TABLE_LIST *table;
      for (table=all_tables; table; table=table->next_global, nr++)
      {
        if (table_map_for_update & ((table_map)1 << nr))
          table->updating= TRUE;
        else
          table->updating= FALSE;
      }

      if (all_tables_not_ok(thd, all_tables))
      {
        
        my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
        if (thd->one_shot_set)
          reset_one_shot_variables(thd);
        DBUG_RETURN(0);
      }
      
      for (table=all_tables; table; table=table->next_global)
        table->updating= TRUE;
    }
    
    
    if (!(lex->sql_command == SQLCOM_UPDATE_MULTI) &&
	!(lex->sql_command == SQLCOM_SET_OPTION) &&
	!(lex->sql_command == SQLCOM_DROP_TABLE &&
          lex->drop_temporary && lex->drop_if_exists) &&
        all_tables_not_ok(thd, all_tables))
    {
      
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      if (thd->one_shot_set)
      {
        
        reset_one_shot_variables(thd);
      }
      DBUG_RETURN(0);
    }
    
    if (slave_execute_deferred_events(thd))
    {
      DBUG_RETURN(-1);
    }
  }
  else
  {
#endif 
    
    if (deny_updates_if_read_only_option(thd, all_tables))
    {
      my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--read-only");
      DBUG_RETURN(-1);
    }
#ifdef HAVE_REPLICATION
  } 
#endif

  status_var_increment(thd->status_var.com_stat[lex->sql_command]);

  Opt_trace_start ots(thd, all_tables, lex->sql_command, &lex->var_list,
                      thd->query(), thd->query_length(), NULL,
                      thd->variables.character_set_client);

  Opt_trace_object trace_command(&thd->opt_trace);
  Opt_trace_array trace_command_steps(&thd->opt_trace, "steps");

  DBUG_ASSERT(thd->transaction.stmt.cannot_safely_rollback() == FALSE);

  switch (gtid_pre_statement_checks(thd))
  {
  case GTID_STATEMENT_EXECUTE:
    break;
  case GTID_STATEMENT_CANCEL:
    DBUG_RETURN(-1);
  case GTID_STATEMENT_SKIP:
    my_ok(thd);
    DBUG_RETURN(0);
  }

  
  if (stmt_causes_implicit_commit(thd, CF_IMPLICIT_COMMIT_BEGIN))
  {
    
    DBUG_ASSERT(! thd->in_sub_stmt);
    
    DBUG_ASSERT(thd->transaction.stmt.is_empty());

    
    if (trans_check_state(thd))
    {
      DBUG_RETURN(-1);
    }

    
    if (trans_commit_implicit(thd))
      goto error;
    
    thd->mdl_context.release_transactional_locks();
  }

#ifndef DBUG_OFF
  if (lex->sql_command != SQLCOM_SET_OPTION)
    DEBUG_SYNC(thd,"before_execute_sql_command");
#endif

  
  if (thd->tx_read_only &&
      (sql_command_flags[lex->sql_command] & CF_DISALLOW_IN_RO_TRANS))
  {
    my_error(ER_CANT_EXECUTE_IN_READ_ONLY_TRANSACTION, MYF(0));
    goto error;
  }

  
  if (sql_command_flags[lex->sql_command] & CF_HA_CLOSE)
    mysql_ha_rm_tables(thd, all_tables);

  
  if (sql_command_flags[lex->sql_command] & CF_PREOPEN_TMP_TABLES)
  {
    if (open_temporary_tables(thd, all_tables))
      goto error;
  }

  switch (lex->sql_command) {

  case SQLCOM_SHOW_STATUS:
  {
    system_status_var old_status_var= thd->status_var;
    thd->initial_status_var= &old_status_var;

    if (!(res= select_precheck(thd, lex, all_tables, first_table)))
    {
      res= execute_sqlcom_select(thd, all_tables);
    }

    
    thd->server_status&= ~(SERVER_QUERY_NO_INDEX_USED |
                           SERVER_QUERY_NO_GOOD_INDEX_USED);
    
    mysql_mutex_lock(&LOCK_status);
    add_diff_to_status(&global_status_var, &thd->status_var,
                       &old_status_var);
    thd->status_var= old_status_var;
    mysql_mutex_unlock(&LOCK_status);
    break;
  }
  case SQLCOM_SHOW_EVENTS:
#ifndef HAVE_EVENT_SCHEDULER
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "embedded server");
    break;
#endif
  case SQLCOM_SHOW_STATUS_PROC:
  case SQLCOM_SHOW_STATUS_FUNC:
  case SQLCOM_SHOW_DATABASES:
  case SQLCOM_SHOW_TABLES:
  case SQLCOM_SHOW_TRIGGERS:
  case SQLCOM_SHOW_TABLE_STATUS:
  case SQLCOM_SHOW_OPEN_TABLES:
  case SQLCOM_SHOW_PLUGINS:
  case SQLCOM_SHOW_FIELDS:
  case SQLCOM_SHOW_KEYS:
  case SQLCOM_SHOW_VARIABLES:
  case SQLCOM_SHOW_CHARSETS:
  case SQLCOM_SHOW_COLLATIONS:
  case SQLCOM_SHOW_STORAGE_ENGINES:
  case SQLCOM_SHOW_PROFILE:
  case SQLCOM_SELECT:
  {
    thd->status_var.last_query_cost= 0.0;
    thd->status_var.last_query_partial_plans= 0;

    if ((res= select_precheck(thd, lex, all_tables, first_table)))
      break;
    TraceTool::path_count++;
    res= execute_sqlcom_select(thd, all_tables);
    TraceTool::path_count--;
    break;
  }
case SQLCOM_PREPARE:
  {
    mysql_sql_stmt_prepare(thd);
    break;
  }
  case SQLCOM_EXECUTE:
  {
    mysql_sql_stmt_execute(thd);
    break;
  }
  case SQLCOM_DEALLOCATE_PREPARE:
  {
    mysql_sql_stmt_close(thd);
    break;
  }
  case SQLCOM_DO:
    if (check_table_access(thd, SELECT_ACL, all_tables, FALSE, UINT_MAX, FALSE)
        || open_and_lock_tables(thd, all_tables, TRUE, 0))
      goto error;

    res= mysql_do(thd, *lex->insert_list);
    break;

  case SQLCOM_EMPTY_QUERY:
    my_ok(thd);
    break;

  case SQLCOM_HELP:
    res= mysqld_help(thd,lex->help_arg);
    break;

#ifndef EMBEDDED_LIBRARY
  case SQLCOM_PURGE:
  {
    if (check_global_access(thd, SUPER_ACL))
      goto error;
    
    res = purge_master_logs(thd, lex->to_log);
    break;
  }
  case SQLCOM_PURGE_BEFORE:
  {
    Item *it;

    if (check_global_access(thd, SUPER_ACL))
      goto error;
    
    it= (Item *)lex->value_list.head();
    if ((!it->fixed && it->fix_fields(lex->thd, &it)) ||
        it->check_cols(1))
    {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "PURGE LOGS BEFORE");
      goto error;
    }
    it= new Item_func_unix_timestamp(it);
    
    it->quick_fix_field();
    res = purge_master_logs_before_date(thd, (ulong)it->val_int());
    break;
  }
#endif
  case SQLCOM_SHOW_WARNS:
  {
    res= mysqld_show_warnings(thd, (ulong)
			      ((1L << (uint) Sql_condition::WARN_LEVEL_NOTE) |
			       (1L << (uint) Sql_condition::WARN_LEVEL_WARN) |
			       (1L << (uint) Sql_condition::WARN_LEVEL_ERROR)
			       ));
    break;
  }
  case SQLCOM_SHOW_ERRORS:
  {
    res= mysqld_show_warnings(thd, (ulong)
			      (1L << (uint) Sql_condition::WARN_LEVEL_ERROR));
    break;
  }
  case SQLCOM_SHOW_PROFILES:
  {
#if defined(ENABLED_PROFILING)
    thd->profiling.discard_current_query();
    res= thd->profiling.show_profiles();
    if (res)
      goto error;
#else
    my_error(ER_FEATURE_DISABLED, MYF(0), "SHOW PROFILES", "enable-profiling");
    goto error;
#endif
    break;
  }

#ifdef HAVE_REPLICATION
  case SQLCOM_SHOW_SLAVE_HOSTS:
  {
    if (check_global_access(thd, REPL_SLAVE_ACL))
      goto error;
    res= show_slave_hosts(thd);
    break;
  }
  case SQLCOM_SHOW_RELAYLOG_EVENTS:
  {
    if (check_global_access(thd, REPL_SLAVE_ACL))
      goto error;
    res = mysql_show_relaylog_events(thd);
    break;
  }
  case SQLCOM_SHOW_BINLOG_EVENTS:
  {
    if (check_global_access(thd, REPL_SLAVE_ACL))
      goto error;
    res = mysql_show_binlog_events(thd);
    break;
  }
#endif

  case SQLCOM_ASSIGN_TO_KEYCACHE:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_access(thd, INDEX_ACL, first_table->db,
                     &first_table->grant.privilege,
                     &first_table->grant.m_internal,
                     0, 0))
      goto error;
    res= mysql_assign_to_keycache(thd, first_table, &lex->ident);
    break;
  }
  case SQLCOM_PRELOAD_KEYS:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_access(thd, INDEX_ACL, first_table->db,
                     &first_table->grant.privilege,
                     &first_table->grant.m_internal,
                     0, 0))
      goto error;
    res = mysql_preload_keys(thd, first_table);
    break;
  }
#ifdef HAVE_REPLICATION
  case SQLCOM_CHANGE_MASTER:
  {
    if (check_global_access(thd, SUPER_ACL))
      goto error;
    mysql_mutex_lock(&LOCK_active_mi);
    if (active_mi != NULL)
      res= change_master(thd, active_mi);
    else
      my_message(ER_SLAVE_CONFIGURATION, ER(ER_SLAVE_CONFIGURATION),
                 MYF(0));
    mysql_mutex_unlock(&LOCK_active_mi);
    break;
  }
  case SQLCOM_SHOW_SLAVE_STAT:
  {
    
    if (check_global_access(thd, SUPER_ACL | REPL_CLIENT_ACL))
      goto error;
    mysql_mutex_lock(&LOCK_active_mi);
    res= show_slave_status(thd, active_mi);
    mysql_mutex_unlock(&LOCK_active_mi);
    break;
  }
  case SQLCOM_SHOW_MASTER_STAT:
  {
    
    if (check_global_access(thd, SUPER_ACL | REPL_CLIENT_ACL))
      goto error;
    res = show_master_status(thd);
    break;
  }

#endif 
  case SQLCOM_SHOW_ENGINE_STATUS:
    {
      if (check_global_access(thd, PROCESS_ACL))
        goto error;
      res = ha_show_status(thd, lex->create_info.db_type, HA_ENGINE_STATUS);
      break;
    }
  case SQLCOM_SHOW_ENGINE_MUTEX:
    {
      if (check_global_access(thd, PROCESS_ACL))
        goto error;
      res = ha_show_status(thd, lex->create_info.db_type, HA_ENGINE_MUTEX);
      break;
    }
  case SQLCOM_CREATE_TABLE:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    bool link_to_local;
    TABLE_LIST *create_table= first_table;
    TABLE_LIST *select_tables= lex->create_last_non_select_table->next_global;

    
    HA_CREATE_INFO create_info(lex->create_info);
    
    Alter_info alter_info(lex->alter_info, thd->mem_root);

    if (thd->is_fatal_error)
    {
      
      res= 1;
      goto end_with_restore_list;
    }

    if ((res= create_table_precheck(thd, select_tables, create_table)))
      goto end_with_restore_list;

    
    create_info.alias= create_table->alias;

    
    if (append_file_to_dir(thd, &create_info.data_file_name,
			   create_table->table_name) ||
	append_file_to_dir(thd, &create_info.index_file_name,
			   create_table->table_name))
      goto end_with_restore_list;

    
    if (!(create_info.used_fields & HA_CREATE_USED_ENGINE))
      create_info.db_type= create_info.options & HA_LEX_CREATE_TMP_TABLE ?
              ha_default_temp_handlerton(thd) : ha_default_handlerton(thd);
    
    if ((create_info.used_fields &
	 (HA_CREATE_USED_DEFAULT_CHARSET | HA_CREATE_USED_CHARSET)) ==
	HA_CREATE_USED_CHARSET)
    {
      create_info.used_fields&= ~HA_CREATE_USED_CHARSET;
      create_info.used_fields|= HA_CREATE_USED_DEFAULT_CHARSET;
      create_info.default_table_charset= create_info.table_charset;
      create_info.table_charset= 0;
    }

#ifdef WITH_PARTITION_STORAGE_ENGINE
    {
      partition_info *part_info= thd->lex->part_info;
      if (part_info && !(part_info= thd->lex->part_info->get_clone()))
      {
        res= -1;
        goto end_with_restore_list;
      }
      thd->work_part_info= part_info;
    }
#endif

    if (select_lex->item_list.elements)		
    {
      select_result *result;

      
      if(lex->ignore)
        lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_CREATE_IGNORE_SELECT);
      
      if(lex->duplicates == DUP_REPLACE)
        lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_CREATE_REPLACE_SELECT);

      
      if (thd->query_name_consts && 
          mysql_bin_log.is_open() &&
          thd->variables.binlog_format == BINLOG_FORMAT_STMT &&
          !mysql_bin_log.is_query_in_union(thd, thd->query_id))
      {
        List_iterator_fast<Item> it(select_lex->item_list);
        Item *item;
        uint splocal_refs= 0;
        
        while ((item= it++))
        {
          if (item->is_splocal())
            splocal_refs++;
        }
        
        if (splocal_refs != thd->query_name_consts)
          push_warning(thd, 
                       Sql_condition::WARN_LEVEL_WARN,
                       ER_UNKNOWN_ERROR,
"Invoked routine ran a statement that may cause problems with "
"binary log, see 'NAME_CONST issues' in 'Binary Logging of Stored Programs' "
"section of the manual.");
      }
      
      select_lex->options|= SELECT_NO_UNLOCK;
      unit->set_limit(select_lex);

      
      if (create_info.used_fields & HA_CREATE_USED_UNION)
      {
        my_error(ER_WRONG_OBJECT, MYF(0), create_table->db,
                 create_table->table_name, "BASE TABLE");
        res= 1;
        goto end_with_restore_list;
      }

      if (!(res= open_normal_and_derived_tables(thd, all_tables, 0)))
      {
        
        if (create_table->table || create_table->view)
        {
          if (create_info.options & HA_LEX_CREATE_IF_NOT_EXISTS)
          {
            push_warning_printf(thd, Sql_condition::WARN_LEVEL_NOTE,
                                ER_TABLE_EXISTS_ERROR,
                                ER(ER_TABLE_EXISTS_ERROR),
                                create_info.alias);
            my_ok(thd);
          }
          else
          {
            my_error(ER_TABLE_EXISTS_ERROR, MYF(0), create_info.alias);
            res= 1;
          }
          goto end_with_restore_list;
        }

        
        lex->unlink_first_table(&link_to_local);

        
        for (TABLE_LIST *table= lex->query_tables; table;
             table= table->next_global)
          if (table->lock_type >= TL_WRITE_ALLOW_WRITE)
          {
            lex->link_first_table_back(create_table, link_to_local);

            res= 1;
            my_error(ER_CANT_UPDATE_TABLE_IN_CREATE_TABLE_SELECT, MYF(0),
                     table->table_name, create_info.alias);
            goto end_with_restore_list;
          }

        
        if ((result= new select_create(create_table,
                                       &create_info,
                                       &alter_info,
                                       select_lex->item_list,
                                       lex->duplicates,
                                       lex->ignore,
                                       select_tables)))
        {
          
          res= handle_select(thd, result, 0);
          delete result;
        }

        lex->link_first_table_back(create_table, link_to_local);
      }
    }
    else
    {
      
      if (create_info.options & HA_LEX_CREATE_TABLE_LIKE)
      {
        
        res= mysql_create_like_table(thd, create_table, select_tables,
                                     &create_info);
      }
      else
      {
        
        res= mysql_create_table(thd, create_table,
                                &create_info, &alter_info);
      }
      if (!res)
        my_ok(thd);
    }

end_with_restore_list:
    break;
  }
  case SQLCOM_CREATE_INDEX:
    
  case SQLCOM_DROP_INDEX:
  
  {
    
    HA_CREATE_INFO create_info;
    Alter_info alter_info(lex->alter_info, thd->mem_root);

    if (thd->is_fatal_error) 
      goto error;

    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_one_table_access(thd, INDEX_ACL, all_tables))
      goto error; 
    
    thd->enable_slow_log= opt_log_slow_admin_statements;

    memset(&create_info, 0, sizeof(create_info));
    create_info.db_type= 0;
    create_info.row_type= ROW_TYPE_NOT_USED;
    create_info.default_table_charset= thd->variables.collation_database;

    res= mysql_alter_table(thd, first_table->db, first_table->table_name,
                           &create_info, first_table, &alter_info,
                           0, (ORDER*) 0, 0);
    break;
  }
#ifdef HAVE_REPLICATION
  case SQLCOM_SLAVE_START:
  {
    mysql_mutex_lock(&LOCK_active_mi);
    if (active_mi != NULL)
      res= start_slave(thd, active_mi, 1 );
    else
      my_message(ER_SLAVE_CONFIGURATION, ER(ER_SLAVE_CONFIGURATION),
                 MYF(0));
    mysql_mutex_unlock(&LOCK_active_mi);
    break;
  }
  case SQLCOM_SLAVE_STOP:
  
  if (thd->locked_tables_mode ||
      thd->in_active_multi_stmt_transaction() || thd->global_read_lock.is_acquired())
  {
    my_message(ER_LOCK_OR_ACTIVE_TRANSACTION,
               ER(ER_LOCK_OR_ACTIVE_TRANSACTION), MYF(0));
    goto error;
  }
  {
    mysql_mutex_lock(&LOCK_active_mi);
    if (active_mi != NULL)
      res= stop_slave(thd, active_mi, 1 );
    else
      my_message(ER_SLAVE_CONFIGURATION, ER(ER_SLAVE_CONFIGURATION),
                 MYF(0));
    mysql_mutex_unlock(&LOCK_active_mi);
    break;
  }
#endif 

  case SQLCOM_RENAME_TABLE:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    TABLE_LIST *table;
    for (table= first_table; table; table= table->next_local->next_local)
    {
      if (check_access(thd, ALTER_ACL | DROP_ACL, table->db,
                       &table->grant.privilege,
                       &table->grant.m_internal,
                       0, 0) ||
          check_access(thd, INSERT_ACL | CREATE_ACL, table->next_local->db,
                       &table->next_local->grant.privilege,
                       &table->next_local->grant.m_internal,
                       0, 0))
	goto error;
      TABLE_LIST old_list, new_list;
      
      old_list= table[0];
      new_list= table->next_local[0];
      if (check_grant(thd, ALTER_ACL | DROP_ACL, &old_list, FALSE, 1, FALSE) ||
         (!test_all_bits(table->next_local->grant.privilege,
                         INSERT_ACL | CREATE_ACL) &&
          check_grant(thd, INSERT_ACL | CREATE_ACL, &new_list, FALSE, 1,
                      FALSE)))
        goto error;
    }

    if (mysql_rename_tables(thd, first_table, 0))
      goto error;
    break;
  }
#ifndef EMBEDDED_LIBRARY
  case SQLCOM_SHOW_BINLOGS:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    my_message(ER_NOT_ALLOWED_COMMAND, ER(ER_NOT_ALLOWED_COMMAND),
               MYF(0)); 
    goto error;
#else
    {
      if (check_global_access(thd, SUPER_ACL | REPL_CLIENT_ACL))
	goto error;
      res = show_binlogs(thd);
      break;
    }
#endif
#endif 
  case SQLCOM_SHOW_CREATE:
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
#ifdef DONT_ALLOW_SHOW_COMMANDS
    my_message(ER_NOT_ALLOWED_COMMAND, ER(ER_NOT_ALLOWED_COMMAND),
               MYF(0)); 
    goto error;
#else
    {
     

      DBUG_PRINT("debug", ("lex->only_view: %d, table: %s.%s",
                           lex->only_view,
                           first_table->db, first_table->table_name));
      if (lex->only_view)
      {
        if (check_table_access(thd, SELECT_ACL, first_table, FALSE, 1, FALSE))
        {
          DBUG_PRINT("debug", ("check_table_access failed"));
          my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0),
                  "SHOW", thd->security_ctx->priv_user,
                  thd->security_ctx->host_or_ip, first_table->alias);
          goto error;
        }
        DBUG_PRINT("debug", ("check_table_access succeeded"));

        
        first_table->open_type= OT_BASE_ONLY;

      }
      else
      {
        
        if (open_temporary_tables(thd, all_tables))
          goto error;

        
        DBUG_PRINT("debug", ("first_table->grant.privilege: %lx",
                             first_table->grant.privilege));
        if (check_some_access(thd, SHOW_CREATE_TABLE_ACLS, first_table) ||
            (first_table->grant.privilege & SHOW_CREATE_TABLE_ACLS) == 0)
        {
          my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0),
                  "SHOW", thd->security_ctx->priv_user,
                  thd->security_ctx->host_or_ip, first_table->alias);
          goto error;
        }
      }

      
      res= mysqld_show_create(thd, first_table);
      break;
    }
#endif
  case SQLCOM_CHECKSUM:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (check_table_access(thd, SELECT_ACL, all_tables,
                           FALSE, UINT_MAX, FALSE))
      goto error; 

    res = mysql_checksum_table(thd, first_table, &lex->check_opt);
    break;
  }
  case SQLCOM_UPDATE:
  {
    ha_rows found= 0, updated= 0;
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (update_precheck(thd, all_tables))
      break;

    
    if (lex->ignore)
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_UPDATE_IGNORE);

    DBUG_ASSERT(select_lex->offset_limit == 0);
    unit->set_limit(select_lex);
    MYSQL_UPDATE_START(thd->query());
    TraceTool::path_count++;
    res= (up_result= mysql_update(thd, all_tables,
                                  select_lex->item_list,
                                  lex->value_list,
                                  select_lex->where,
                                  select_lex->order_list.elements,
                                  select_lex->order_list.first,
                                  unit->select_limit_cnt,
                                  lex->duplicates, lex->ignore,
                                  &found, &updated));
    TraceTool::path_count--;
    MYSQL_UPDATE_DONE(res, found, updated);
    
    if (up_result != 2)
      break;
    
  }
  case SQLCOM_UPDATE_MULTI:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    
    if (up_result != 2)
    {
      if ((res= multi_update_precheck(thd, all_tables)))
        break;
    }
    else
      res= 0;

    res= mysql_multi_update_prepare(thd);

#ifdef HAVE_REPLICATION
    
    if (unlikely(thd->slave_thread && !have_table_map_for_update))
    {
      if (all_tables_not_ok(thd, all_tables))
      {
        if (res!= 0)
        {
          res= 0;             
          thd->clear_error(); 
        }
        
        my_error(ER_SLAVE_IGNORED_TABLE, MYF(0));
        break;
      }
      if (res)
        break;
    }
    else
    {
#endif 
      if (res)
        break;
      if (opt_readonly &&
	  !(thd->security_ctx->master_access & SUPER_ACL) &&
	  some_non_temp_table_to_be_updated(thd, all_tables))
      {
	my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--read-only");
	break;
      }
#ifdef HAVE_REPLICATION
    }  
#endif
    {
      multi_update *result_obj;
      MYSQL_MULTI_UPDATE_START(thd->query());
      res= mysql_multi_update(thd, all_tables,
                              &select_lex->item_list,
                              &lex->value_list,
                              select_lex->where,
                              select_lex->options,
                              lex->duplicates,
                              lex->ignore,
                              unit,
                              select_lex,
                              &result_obj);
      if (result_obj)
      {
        MYSQL_MULTI_UPDATE_DONE(res, result_obj->num_found(),
                                result_obj->num_updated());
        res= FALSE; 
        delete result_obj;
      }
      else
      {
        MYSQL_MULTI_UPDATE_DONE(1, 0, 0);
      }
    }
    break;
  }
  case SQLCOM_REPLACE:
#ifndef DBUG_OFF
    if (mysql_bin_log.is_open())
    {
      

      Incident incident= INCIDENT_NONE;
      DBUG_PRINT("debug", ("Just before generate_incident()"));
      DBUG_EXECUTE_IF("incident_database_resync_on_replace",
                      incident= INCIDENT_LOST_EVENTS;);
      if (incident)
      {
        Incident_log_event ev(thd, incident);
        if (mysql_bin_log.write_incident(&ev, true))
        {
          res= 1;
          break;
        }
      }
      DBUG_PRINT("debug", ("Just after generate_incident()"));
    }
#endif
  case SQLCOM_INSERT:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);

    
    if (first_table->lock_type != TL_WRITE_DELAYED)
    {
      if ((res= open_temporary_tables(thd, all_tables)))
        break;
    }

    if ((res= insert_precheck(thd, all_tables)))
      break;

    MYSQL_INSERT_START(thd->query());
    res= mysql_insert(thd, all_tables, lex->field_list, lex->many_values,
		      lex->update_list, lex->value_list,
                      lex->duplicates, lex->ignore);
    MYSQL_INSERT_DONE(res, (ulong) thd->get_row_count_func());
    
    if (first_table->view && !first_table->contain_auto_increment)
      thd->first_successful_insert_id_in_cur_stmt=
        thd->first_successful_insert_id_in_prev_stmt;

    DBUG_EXECUTE_IF("after_mysql_insert",
                    {
                      const char act[]=
                        "now "
                        "wait_for signal.continue";
                      DBUG_ASSERT(opt_debug_sync_timeout > 0);
                      DBUG_ASSERT(!debug_sync_set_action(current_thd,
                                                         STRING_WITH_LEN(act)));
                    };);
    break;
  }
  case SQLCOM_REPLACE_SELECT:
  case SQLCOM_INSERT_SELECT:
  {
    select_insert *sel_result;
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if ((res= insert_precheck(thd, all_tables)))
      break;
    
    if (lex->sql_command == SQLCOM_INSERT_SELECT &&
        lex->duplicates == DUP_UPDATE)
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_INSERT_SELECT_UPDATE);

    if (lex->sql_command == SQLCOM_INSERT_SELECT && lex->ignore)
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_INSERT_IGNORE_SELECT);

    if (lex->sql_command == SQLCOM_REPLACE_SELECT)
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_REPLACE_SELECT);

    
    if (first_table->lock_type == TL_WRITE_DELAYED)
      first_table->lock_type= TL_WRITE;

    
    select_lex->options|= SELECT_NO_UNLOCK;

    unit->set_limit(select_lex);

    if (!(res= open_normal_and_derived_tables(thd, all_tables, 0)))
    {
      MYSQL_INSERT_SELECT_START(thd->query());
      
      TABLE_LIST *second_table= first_table->next_local;
      select_lex->table_list.first= second_table;
      select_lex->context.table_list= 
        select_lex->context.first_name_resolution_table= second_table;
      res= mysql_insert_select_prepare(thd);
      if (!res && (sel_result= new select_insert(first_table,
                                                 first_table->table,
                                                 &lex->field_list,
                                                 &lex->field_list,
                                                 &lex->update_list,
                                                 &lex->value_list,
                                                 lex->duplicates,
                                                 lex->ignore)))
      {
        if (lex->describe)
          res= explain_multi_table_modification(thd, sel_result);
        else
        {
          res= handle_select(thd, sel_result, OPTION_SETUP_TABLES_DONE);
          
          if (!res && first_table->lock_type ==  TL_WRITE_CONCURRENT_INSERT &&
              thd->lock)
          {
            
            TABLE_LIST *save_table= first_table->next_local;
            first_table->next_local= 0;
            query_cache_invalidate3(thd, first_table, 1);
            first_table->next_local= save_table;
          }
        }
        delete sel_result;
      }
      
      MYSQL_INSERT_SELECT_DONE(res, (ulong) thd->get_row_count_func());
      select_lex->table_list.first= first_table;
    }
    
    if (first_table->view && !first_table->contain_auto_increment)
      thd->first_successful_insert_id_in_cur_stmt=
        thd->first_successful_insert_id_in_prev_stmt;

    break;
  }
  case SQLCOM_DELETE:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if ((res= delete_precheck(thd, all_tables)))
      break;
    DBUG_ASSERT(select_lex->offset_limit == 0);
    unit->set_limit(select_lex);

    MYSQL_DELETE_START(thd->query());
    res = mysql_delete(thd, all_tables, select_lex->where,
                       &select_lex->order_list,
                       unit->select_limit_cnt, select_lex->options);
    MYSQL_DELETE_DONE(res, (ulong) thd->get_row_count_func());
    break;
  }
  case SQLCOM_DELETE_MULTI:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    TABLE_LIST *aux_tables= thd->lex->auxiliary_table_list.first;
    uint del_table_count;
    multi_delete *del_result;

    if ((res= multi_delete_precheck(thd, all_tables)))
      break;

    
    if (select_lex->item_list.elements != 0)
      select_lex->item_list.empty();
    if (add_item_to_list(thd, new Item_null()))
      goto error;

    THD_STAGE_INFO(thd, stage_init);
    if ((res= open_normal_and_derived_tables(thd, all_tables, 0)))
      break;

    MYSQL_MULTI_DELETE_START(thd->query());
    if ((res= mysql_multi_delete_prepare(thd, &del_table_count)))
    {
      MYSQL_MULTI_DELETE_DONE(1, 0);
      goto error;
    }

    if (!thd->is_fatal_error &&
        (del_result= new multi_delete(aux_tables, del_table_count)))
    {
      if (lex->describe)
        res= explain_multi_table_modification(thd, del_result);
      else
      {
        res= mysql_select(thd,
                          select_lex->get_table_list(),
                          select_lex->with_wild,
                          select_lex->item_list,
                          select_lex->where,
                          (SQL_I_List<ORDER> *)NULL, (SQL_I_List<ORDER> *)NULL,
                          (Item *)NULL,
                          (select_lex->options | thd->variables.option_bits |
                          SELECT_NO_JOIN_CACHE | SELECT_NO_UNLOCK |
                          OPTION_SETUP_TABLES_DONE) & ~OPTION_BUFFER_RESULT,
                          del_result, unit, select_lex);
        res|= thd->is_error();
        if (res)
          del_result->abort_result_set();
      }
      MYSQL_MULTI_DELETE_DONE(res, del_result->num_deleted());
      delete del_result;
    }
    else
    {
      res= TRUE;                                
      MYSQL_MULTI_DELETE_DONE(1, 0);
    }
    break;
  }
  case SQLCOM_DROP_TABLE:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    if (!lex->drop_temporary)
    {
      if (check_table_access(thd, DROP_ACL, all_tables, FALSE, UINT_MAX, FALSE))
	goto error;				
    }
    
    res= mysql_rm_table(thd, first_table, lex->drop_if_exists,
			lex->drop_temporary);
  }
  break;
  case SQLCOM_SHOW_PROCESSLIST:
    if (!thd->security_ctx->priv_user[0] &&
        check_global_access(thd,PROCESS_ACL))
      break;
    mysqld_list_processes(thd,
			  (thd->security_ctx->master_access & PROCESS_ACL ?
                           NullS :
                           thd->security_ctx->priv_user),
                          lex->verbose);
    break;
  case SQLCOM_SHOW_PRIVILEGES:
    res= mysqld_show_privileges(thd);
    break;
  case SQLCOM_SHOW_ENGINE_LOGS:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    my_message(ER_NOT_ALLOWED_COMMAND, ER(ER_NOT_ALLOWED_COMMAND),
               MYF(0));	
    goto error;
#else
    {
      if (check_access(thd, FILE_ACL, any_db, NULL, NULL, 0, 0))
	goto error;
      res= ha_show_status(thd, lex->create_info.db_type, HA_ENGINE_LOGS);
      break;
    }
#endif
  case SQLCOM_CHANGE_DB:
  {
    LEX_STRING db_str= { (char *) select_lex->db, strlen(select_lex->db) };

    if (!mysql_change_db(thd, &db_str, FALSE))
      my_ok(thd);

    break;
  }

  case SQLCOM_LOAD:
  {
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    uint privilege= (lex->duplicates == DUP_REPLACE ?
		     INSERT_ACL | DELETE_ACL : INSERT_ACL) |
                    (lex->local_file ? 0 : FILE_ACL);

    if (lex->local_file)
    {
      if (!(thd->client_capabilities & CLIENT_LOCAL_FILES) ||
          !opt_local_infile)
      {
	my_message(ER_NOT_ALLOWED_COMMAND, ER(ER_NOT_ALLOWED_COMMAND), MYF(0));
	goto error;
      }
    }

    if (check_one_table_access(thd, privilege, all_tables))
      goto error;

    res= mysql_load(thd, lex->exchange, first_table, lex->field_list,
                    lex->update_list, lex->value_list, lex->duplicates,
                    lex->ignore, (bool) lex->local_file);
    break;
  }

  case SQLCOM_SET_OPTION:
  {
    List<set_var_base> *lex_var_list= &lex->var_list;

    if ((check_table_access(thd, SELECT_ACL, all_tables, FALSE, UINT_MAX, FALSE)
         || open_and_lock_tables(thd, all_tables, TRUE, 0)))
      goto error;
    if (!(res= sql_set_variables(thd, lex_var_list)))
    {
      
      thd->one_shot_set|= lex->one_shot_set;
      my_ok(thd);
    }
    else
    {
      
      if (!thd->is_error())
        my_error(ER_WRONG_ARGUMENTS,MYF(0),"SET");
      goto error;
    }

    break;
  }

  case SQLCOM_UNLOCK_TABLES:
    
    if (thd->variables.option_bits & OPTION_TABLE_LOCK)
    {
      
      if (trans_check_state(thd))
      {
        DBUG_RETURN(-1);
      }
      res= trans_commit_implicit(thd);
      thd->locked_tables_list.unlock_locked_tables(thd);
      thd->mdl_context.release_transactional_locks();
      thd->variables.option_bits&= ~(OPTION_TABLE_LOCK);
    }
    if (thd->global_read_lock.is_acquired())
      thd->global_read_lock.unlock_global_read_lock(thd);
    if (res)
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_LOCK_TABLES:
    
    if (trans_check_state(thd))
    {
      DBUG_RETURN(-1);
    }
    
    res= trans_commit_implicit(thd);
    thd->locked_tables_list.unlock_locked_tables(thd);
    
    thd->mdl_context.release_transactional_locks();
    if (res)
      goto error;

    

    if (open_temporary_tables(thd, all_tables))
      goto error;

    if (lock_tables_precheck(thd, all_tables))
      goto error;

    thd->variables.option_bits|= OPTION_TABLE_LOCK;

    res= lock_tables_open_and_lock_tables(thd, all_tables);

    if (res)
    {
      thd->variables.option_bits&= ~(OPTION_TABLE_LOCK);
    }
    else
    {
#ifdef HAVE_QUERY_CACHE
      if (thd->variables.query_cache_wlock_invalidate)
        query_cache.invalidate_locked_for_write(first_table);
#endif 
      my_ok(thd);
    }
    break;
  case SQLCOM_CREATE_DB:
  {
    
    HA_CREATE_INFO create_info(lex->create_info);
    char *alias;
    if (!(alias=thd->strmake(lex->name.str, lex->name.length)) ||
        (check_and_convert_db_name(&lex->name, FALSE) != IDENT_NAME_OK))
      break;
    
#ifdef HAVE_REPLICATION
    if (!db_stmt_db_ok(thd, lex->name.str))
    {
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      break;
    }
#endif
    if (check_access(thd, CREATE_ACL, lex->name.str, NULL, NULL, 1, 0))
      break;
    res= mysql_create_db(thd,(lower_case_table_names == 2 ? alias :
                              lex->name.str), &create_info, 0);
    break;
  }
  case SQLCOM_DROP_DB:
  {
    if (check_and_convert_db_name(&lex->name, FALSE) != IDENT_NAME_OK)
      break;
    
#ifdef HAVE_REPLICATION
    if (!db_stmt_db_ok(thd, lex->name.str))
    {
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      break;
    }
#endif
    if (check_access(thd, DROP_ACL, lex->name.str, NULL, NULL, 1, 0))
      break;
    res= mysql_rm_db(thd, lex->name.str, lex->drop_if_exists, 0);
    break;
  }
  case SQLCOM_ALTER_DB_UPGRADE:
  {
    LEX_STRING *db= & lex->name;
#ifdef HAVE_REPLICATION
    if (!db_stmt_db_ok(thd, lex->name.str))
    {
      res= 1;
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      break;
    }
#endif
    if (check_and_convert_db_name(db, FALSE) != IDENT_NAME_OK)
      break;
    if (check_access(thd, ALTER_ACL, db->str, NULL, NULL, 1, 0) ||
        check_access(thd, DROP_ACL, db->str, NULL, NULL, 1, 0) ||
        check_access(thd, CREATE_ACL, db->str, NULL, NULL, 1, 0))
    {
      res= 1;
      break;
    }
    res= mysql_upgrade_db(thd, db);
    if (!res)
      my_ok(thd);
    break;
  }
  case SQLCOM_ALTER_DB:
  {
    LEX_STRING *db= &lex->name;
    HA_CREATE_INFO create_info(lex->create_info);
    if (check_and_convert_db_name(db, FALSE) != IDENT_NAME_OK)
      break;
    
#ifdef HAVE_REPLICATION
    if (!db_stmt_db_ok(thd, lex->name.str))
    {
      my_message(ER_SLAVE_IGNORED_TABLE, ER(ER_SLAVE_IGNORED_TABLE), MYF(0));
      break;
    }
#endif
    if (check_access(thd, ALTER_ACL, db->str, NULL, NULL, 1, 0))
      break;
    res= mysql_alter_db(thd, db->str, &create_info);
    break;
  }
  case SQLCOM_SHOW_CREATE_DB:
  {
    DBUG_EXECUTE_IF("4x_server_emul",
                    my_error(ER_UNKNOWN_ERROR, MYF(0)); goto error;);
    if (check_and_convert_db_name(&lex->name, TRUE) != IDENT_NAME_OK)
      break;
    res= mysqld_show_create_db(thd, lex->name.str, &lex->create_info);
    break;
  }
  case SQLCOM_CREATE_EVENT:
  case SQLCOM_ALTER_EVENT:
  #ifdef HAVE_EVENT_SCHEDULER
  do
  {
    DBUG_ASSERT(lex->event_parse_data);
    if (lex->table_or_sp_used())
    {
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "Usage of subqueries or stored "
               "function calls as part of this statement");
      break;
    }

    res= sp_process_definer(thd);
    if (res)
      break;

    switch (lex->sql_command) {
    case SQLCOM_CREATE_EVENT:
    {
      bool if_not_exists= (lex->create_info.options &
                           HA_LEX_CREATE_IF_NOT_EXISTS);
      res= Events::create_event(thd, lex->event_parse_data, if_not_exists);
      break;
    }
    case SQLCOM_ALTER_EVENT:
      res= Events::update_event(thd, lex->event_parse_data,
                                lex->spname ? &lex->spname->m_db : NULL,
                                lex->spname ? &lex->spname->m_name : NULL);
      break;
    default:
      DBUG_ASSERT(0);
    }
    DBUG_PRINT("info",("DDL error code=%d", res));
    if (!res)
      my_ok(thd);

  } while (0);
  
  if (!thd->sp_runtime_ctx)
  {
    delete lex->sphead;
    lex->sphead= NULL;
  }
  
  break;
  case SQLCOM_SHOW_CREATE_EVENT:
    res= Events::show_create_event(thd, lex->spname->m_db,
                                   lex->spname->m_name);
    break;
  case SQLCOM_DROP_EVENT:
    if (!(res= Events::drop_event(thd,
                                  lex->spname->m_db, lex->spname->m_name,
                                  lex->drop_if_exists)))
      my_ok(thd);
    break;
#else
    my_error(ER_NOT_SUPPORTED_YET,MYF(0),"embedded server");
    break;
#endif
  case SQLCOM_CREATE_FUNCTION:                  
  {
    if (check_access(thd, INSERT_ACL, "mysql", NULL, NULL, 1, 0))
      break;
#ifdef HAVE_DLOPEN
    if (!(res = mysql_create_function(thd, &lex->udf)))
      my_ok(thd);
#else
    my_error(ER_CANT_OPEN_LIBRARY, MYF(0), lex->udf.dl, 0, "feature disabled");
    res= TRUE;
#endif
    break;
  }
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  case SQLCOM_CREATE_USER:
  {
    if (check_access(thd, INSERT_ACL, "mysql", NULL, NULL, 1, 1) &&
        check_global_access(thd,CREATE_USER_ACL))
      break;
    
    if (!(res= mysql_create_user(thd, lex->users_list)))
      my_ok(thd);
    break;
  }
  case SQLCOM_DROP_USER:
  {
    if (check_access(thd, DELETE_ACL, "mysql", NULL, NULL, 1, 1) &&
        check_global_access(thd,CREATE_USER_ACL))
      break;
    
    if (!(res= mysql_drop_user(thd, lex->users_list)))
      my_ok(thd);
    break;
  }
  case SQLCOM_RENAME_USER:
  {
    if (check_access(thd, UPDATE_ACL, "mysql", NULL, NULL, 1, 1) &&
        check_global_access(thd,CREATE_USER_ACL))
      break;
    
    if (!(res= mysql_rename_user(thd, lex->users_list)))
      my_ok(thd);
    break;
  }
  case SQLCOM_REVOKE_ALL:
  {
    if (check_access(thd, UPDATE_ACL, "mysql", NULL, NULL, 1, 1) &&
        check_global_access(thd,CREATE_USER_ACL))
      break;

    
    thd->binlog_invoker();

    
    if (!(res = mysql_revoke_all(thd, lex->users_list)))
      my_ok(thd);
    break;
  }
  case SQLCOM_REVOKE:
  case SQLCOM_GRANT:
  {
    if (lex->type != TYPE_ENUM_PROXY &&
        check_access(thd, lex->grant | lex->grant_tot_col | GRANT_ACL,
                     first_table ?  first_table->db : select_lex->db,
                     first_table ? &first_table->grant.privilege : NULL,
                     first_table ? &first_table->grant.m_internal : NULL,
                     first_table ? 0 : 1, 0))
      goto error;

    
    thd->binlog_invoker();

    if (thd->security_ctx->user)              
    {
      LEX_USER *user, *tmp_user;
      bool first_user= TRUE;

      List_iterator <LEX_USER> user_list(lex->users_list);
      while ((tmp_user= user_list++))
      {
        if (!(user= get_current_user(thd, tmp_user)))
          goto error;
        if (specialflag & SPECIAL_NO_RESOLVE &&
            hostname_requires_resolving(user->host.str))
          push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                              ER_WARN_HOSTNAME_WONT_WORK,
                              ER(ER_WARN_HOSTNAME_WONT_WORK));
        
        DBUG_ASSERT(user->host.str != 0);

        
        if (lex->type == TYPE_ENUM_PROXY && first_user)
        {
          first_user= FALSE;
          if (acl_check_proxy_grant_access (thd, user->host.str, user->user.str,
                                        lex->grant & GRANT_ACL))
            goto error;
        } 
        else if (is_acl_user(user->host.str, user->user.str) &&
                 user->password.str &&
                 check_change_password (thd, user->host.str, user->user.str, 
                                        user->password.str, 
                                        user->password.length))
          goto error;
      }
    }
    if (first_table)
    {
      if (lex->type == TYPE_ENUM_PROCEDURE ||
          lex->type == TYPE_ENUM_FUNCTION)
      {
        uint grants= lex->all_privileges 
		   ? (PROC_ACLS & ~GRANT_ACL) | (lex->grant & GRANT_ACL)
		   : lex->grant;
        if (check_grant_routine(thd, grants | GRANT_ACL, all_tables,
                                lex->type == TYPE_ENUM_PROCEDURE, 0))
	  goto error;
        
        res= mysql_routine_grant(thd, all_tables,
                                 lex->type == TYPE_ENUM_PROCEDURE, 
                                 lex->users_list, grants,
                                 lex->sql_command == SQLCOM_REVOKE, TRUE);
        if (!res)
          my_ok(thd);
      }
      else
      {
	if (check_grant(thd,(lex->grant | lex->grant_tot_col | GRANT_ACL),
                        all_tables, FALSE, UINT_MAX, FALSE))
	  goto error;
        
        res= mysql_table_grant(thd, all_tables, lex->users_list,
			       lex->columns, lex->grant,
			       lex->sql_command == SQLCOM_REVOKE);
      }
    }
    else
    {
      if (lex->columns.elements || (lex->type && lex->type != TYPE_ENUM_PROXY))
      {
	my_message(ER_ILLEGAL_GRANT_FOR_TABLE, ER(ER_ILLEGAL_GRANT_FOR_TABLE),
                   MYF(0));
        goto error;
      }
      else
      {
        
        res = mysql_grant(thd, select_lex->db, lex->users_list, lex->grant,
                          lex->sql_command == SQLCOM_REVOKE,
                          lex->type == TYPE_ENUM_PROXY);
      }
      if (!res)
      {
	if (lex->sql_command == SQLCOM_GRANT)
	{
	  List_iterator <LEX_USER> str_list(lex->users_list);
	  LEX_USER *user, *tmp_user;
	  while ((tmp_user=str_list++))
          {
            if (!(user= get_current_user(thd, tmp_user)))
              goto error;
	    reset_mqh(user, 0);
          }
	}
      }
    }
    break;
  }
#endif 
  case SQLCOM_RESET:
    
    lex->no_write_to_binlog= 1;
  case SQLCOM_FLUSH:
  {
    int write_to_binlog;
    if (check_global_access(thd,RELOAD_ACL))
      goto error;

    if (first_table && lex->type & REFRESH_READ_LOCK)
    {
      
      if (check_table_access(thd, LOCK_TABLES_ACL | SELECT_ACL, all_tables,
                             FALSE, UINT_MAX, FALSE))
        goto error;
      if (flush_tables_with_read_lock(thd, all_tables))
        goto error;
      my_ok(thd);
      break;
    }
    else if (first_table && lex->type & REFRESH_FOR_EXPORT)
    {
      
      if (check_table_access(thd, LOCK_TABLES_ACL | SELECT_ACL, all_tables,
                             FALSE, UINT_MAX, FALSE))
        goto error;
      if (flush_tables_for_export(thd, all_tables))
        goto error;
      my_ok(thd);
      break;
    }

    
    if (!reload_acl_and_cache(thd, lex->type, first_table, &write_to_binlog))
    {
      
      

      if (write_to_binlog > 0)  
      { 
        if (!lex->no_write_to_binlog)
          res= write_bin_log(thd, FALSE, thd->query(), thd->query_length());
      } else if (write_to_binlog < 0) 
      {
        
        res= 1;
      } 

      if (!res)
        my_ok(thd);
    } 
    
    break;
  }
  case SQLCOM_KILL:
  {
    Item *it= (Item *)lex->value_list.head();

    if (lex->table_or_sp_used())
    {
      my_error(ER_NOT_SUPPORTED_YET, MYF(0), "Usage of subqueries or stored "
               "function calls as part of this statement");
      break;
    }

    if ((!it->fixed && it->fix_fields(lex->thd, &it)) || it->check_cols(1))
    {
      my_message(ER_SET_CONSTANTS_ONLY, ER(ER_SET_CONSTANTS_ONLY),
		 MYF(0));
      goto error;
    }
    sql_kill(thd, (ulong)it->val_int(), lex->type & ONLY_KILL_QUERY);
    break;
  }
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  case SQLCOM_SHOW_GRANTS:
  {
    LEX_USER *grant_user= get_current_user(thd, lex->grant_user);
    if (!grant_user)
      goto error;
    if ((thd->security_ctx->priv_user &&
	 !strcmp(thd->security_ctx->priv_user, grant_user->user.str)) ||
        !check_access(thd, SELECT_ACL, "mysql", NULL, NULL, 1, 0))
    {
      res = mysql_show_grants(thd, grant_user);
    }
    break;
  }
#endif
  case SQLCOM_BEGIN:
    if (trans_begin(thd, lex->start_transaction_opt))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_COMMIT:
  {
    TraceTool::is_commit = true;
    DBUG_ASSERT(thd->lock == NULL ||
                thd->locked_tables_mode == LTM_LOCK_TABLES);
    bool tx_chain= (lex->tx_chain == TVL_YES ||
                    (thd->variables.completion_type == 1 &&
                     lex->tx_chain != TVL_NO));
    bool tx_release= (lex->tx_release == TVL_YES ||
                      (thd->variables.completion_type == 2 &&
                       lex->tx_release != TVL_NO));
    bool commit = trans_commit(thd);
    if (!TraceTool::is_commit)
    {
      TraceTool::is_commit = true;
      TraceTool::commit_successful = false;
    }
    if (commit)
    {
      goto error;
    }
    thd->mdl_context.release_transactional_locks();
    
    if (tx_chain)
    {
      if (trans_begin(thd))
      goto error;
    }
    else
    {
      
      thd->tx_isolation= (enum_tx_isolation) thd->variables.tx_isolation;
      thd->tx_read_only= thd->variables.tx_read_only;
    }
    
    if (tx_release)
      thd->killed= THD::KILL_CONNECTION;
    my_ok(thd);
    break;
  }
  case SQLCOM_ROLLBACK:
  {
    DBUG_ASSERT(thd->lock == NULL ||
                thd->locked_tables_mode == LTM_LOCK_TABLES);
    bool tx_chain= (lex->tx_chain == TVL_YES ||
                    (thd->variables.completion_type == 1 &&
                     lex->tx_chain != TVL_NO));
    bool tx_release= (lex->tx_release == TVL_YES ||
                      (thd->variables.completion_type == 2 &&
                       lex->tx_release != TVL_NO));
    bool rollback = trans_rollback(thd);
    if (!TraceTool::is_commit)
    {
      TraceTool::is_commit = true;
      TraceTool::commit_successful = false;
    }
    if (rollback)
      goto error;
    thd->mdl_context.release_transactional_locks();
    
    if (tx_chain)
    {
      if (trans_begin(thd))
        goto error;
    }
    else
    {
      
      thd->tx_isolation= (enum_tx_isolation) thd->variables.tx_isolation;
      thd->tx_read_only= thd->variables.tx_read_only;
    }
    
    if (tx_release)
      thd->killed= THD::KILL_CONNECTION;
    my_ok(thd);
    break;
  }
  case SQLCOM_RELEASE_SAVEPOINT:
    if (trans_release_savepoint(thd, lex->ident))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_ROLLBACK_TO_SAVEPOINT:
    if (trans_rollback_to_savepoint(thd, lex->ident))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_SAVEPOINT:
    if (trans_savepoint(thd, lex->ident))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_CREATE_PROCEDURE:
  case SQLCOM_CREATE_SPFUNCTION:
  {
    uint namelen;
    char *name;
    int sp_result= SP_INTERNAL_ERROR;

    DBUG_ASSERT(lex->sphead != 0);
    DBUG_ASSERT(lex->sphead->m_db.str); 
    
    if (check_and_convert_db_name(&lex->sphead->m_db, FALSE) != IDENT_NAME_OK)
      goto create_sp_error;

    if (check_access(thd, CREATE_PROC_ACL, lex->sphead->m_db.str,
                     NULL, NULL, 0, 0))
      goto create_sp_error;

    
    if (check_db_dir_existence(lex->sphead->m_db.str))
    {
      my_error(ER_BAD_DB_ERROR, MYF(0), lex->sphead->m_db.str);
      goto create_sp_error;
    }

    name= lex->sphead->name(&namelen);
#ifdef HAVE_DLOPEN
    if (lex->sphead->m_type == SP_TYPE_FUNCTION)
    {
      udf_func *udf = find_udf(name, namelen);

      if (udf)
      {
        my_error(ER_UDF_EXISTS, MYF(0), name);
        goto create_sp_error;
      }
    }
#endif

    if (sp_process_definer(thd))
      goto create_sp_error;

    res= (sp_result= sp_create_routine(thd, lex->sphead));
    switch (sp_result) {
    case SP_OK: {
#ifndef NO_EMBEDDED_ACCESS_CHECKS
      

      Security_context security_context;
      bool restore_backup_context= false;
      Security_context *backup= NULL;
      LEX_USER *definer= thd->lex->definer;
      
      DBUG_ASSERT(thd->transaction.stmt.is_empty());
      close_thread_tables(thd);
      
      if (thd->slave_thread && is_acl_user(definer->host.str, definer->user.str))
      {
        security_context.change_security_context(thd, 
                                                 &thd->lex->definer->user,
                                                 &thd->lex->definer->host,
                                                 &thd->lex->sphead->m_db,
                                                 &backup);
        restore_backup_context= true;
      }

      if (sp_automatic_privileges && !opt_noacl &&
          check_routine_access(thd, DEFAULT_CREATE_PROC_ACLS,
                               lex->sphead->m_db.str, name,
                               lex->sql_command == SQLCOM_CREATE_PROCEDURE, 1))
      {
        if (sp_grant_privileges(thd, lex->sphead->m_db.str, name,
                                lex->sql_command == SQLCOM_CREATE_PROCEDURE))
          push_warning(thd, Sql_condition::WARN_LEVEL_WARN,
                       ER_PROC_AUTO_GRANT_FAIL, ER(ER_PROC_AUTO_GRANT_FAIL));
        thd->clear_error();
      }

       
      if (restore_backup_context)
      {
        DBUG_ASSERT(thd->slave_thread == 1);
        thd->security_ctx->restore_security_context(thd, backup);
      }

#endif
    break;
    }
    case SP_WRITE_ROW_FAILED:
      my_error(ER_SP_ALREADY_EXISTS, MYF(0), SP_TYPE_STRING(lex), name);
    break;
    case SP_BAD_IDENTIFIER:
      my_error(ER_TOO_LONG_IDENT, MYF(0), name);
    break;
    case SP_BODY_TOO_LONG:
      my_error(ER_TOO_LONG_BODY, MYF(0), name);
    break;
    case SP_FLD_STORE_FAILED:
      my_error(ER_CANT_CREATE_SROUTINE, MYF(0), name);
      break;
    default:
      my_error(ER_SP_STORE_FAILED, MYF(0), SP_TYPE_STRING(lex), name);
    break;
    } 

    
create_sp_error:
    if (sp_result != SP_OK )
      goto error;
    my_ok(thd);
    break; 
  } 
  case SQLCOM_CALL:
    {
      sp_head *sp;

      
      if (check_routine_access(thd, EXECUTE_ACL, lex->spname->m_db.str,
                               lex->spname->m_name.str,
                               lex->sql_command == SQLCOM_CALL, 0))
        goto error;

      
      if (check_table_access(thd, SELECT_ACL, all_tables, FALSE,
                             UINT_MAX, FALSE) ||
          open_and_lock_tables(thd, all_tables, TRUE, 0))
       goto error;

      
      if (!(sp= sp_find_routine(thd, SP_TYPE_PROCEDURE, lex->spname,
                                &thd->sp_proc_cache, TRUE)))
      {
	my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "PROCEDURE",
                 lex->spname->m_qname.str);
	goto error;
      }
      else
      {
	ha_rows select_limit;
        
	uint bits_to_be_cleared= 0;
        
        if (thd->in_sub_stmt)
        {
          const char *where= (thd->in_sub_stmt & SUB_STMT_TRIGGER ?
                              "trigger" : "function");
          if (sp->is_not_allowed_in_function(where))
            goto error;
        }

	if (sp->m_flags & sp_head::MULTI_RESULTS)
	{
	  if (! (thd->client_capabilities & CLIENT_MULTI_RESULTS))
	  {
            
	    my_error(ER_SP_BADSELECT, MYF(0), sp->m_qname.str);
	    goto error;
	  }
          
	  bits_to_be_cleared= (~thd->server_status &
                               SERVER_MORE_RESULTS_EXISTS);
	  thd->server_status|= SERVER_MORE_RESULTS_EXISTS;
	}

	if (check_routine_access(thd, EXECUTE_ACL,
				 sp->m_db.str, sp->m_name.str, TRUE, FALSE))
	{
	  goto error;
	}
	select_limit= thd->variables.select_limit;
	thd->variables.select_limit= HA_POS_ERROR;

        
	res= sp->execute_procedure(thd, &lex->value_list);

	thd->variables.select_limit= select_limit;

        thd->server_status&= ~bits_to_be_cleared;

	if (!res)
        {
          my_ok(thd, (thd->get_row_count_func() < 0) ? 0 : thd->get_row_count_func());
        }
	else
        {
          DBUG_ASSERT(thd->is_error() || thd->killed);
	  goto error;		
        }
      }
      break;
    }
  case SQLCOM_ALTER_PROCEDURE:
  case SQLCOM_ALTER_FUNCTION:
    {
      if (check_routine_access(thd, ALTER_PROC_ACL, lex->spname->m_db.str,
                               lex->spname->m_name.str,
                               lex->sql_command == SQLCOM_ALTER_PROCEDURE,
                               false))
        goto error;

      enum_sp_type sp_type= (lex->sql_command == SQLCOM_ALTER_PROCEDURE) ?
                            SP_TYPE_PROCEDURE : SP_TYPE_FUNCTION;
      
      
      int sp_result= sp_update_routine(thd, sp_type, lex->spname,
                                       &lex->sp_chistics);
      if (thd->killed)
        goto error;
      switch (sp_result)
      {
      case SP_OK:
	my_ok(thd);
	break;
      case SP_KEY_NOT_FOUND:
	my_error(ER_SP_DOES_NOT_EXIST, MYF(0),
                 SP_COM_STRING(lex), lex->spname->m_qname.str);
	goto error;
      default:
	my_error(ER_SP_CANT_ALTER, MYF(0),
                 SP_COM_STRING(lex), lex->spname->m_qname.str);
	goto error;
      }
      break;
    }
  case SQLCOM_DROP_PROCEDURE:
  case SQLCOM_DROP_FUNCTION:
    {
#ifdef HAVE_DLOPEN
      if (lex->sql_command == SQLCOM_DROP_FUNCTION &&
          ! lex->spname->m_explicit_name)
      {
        
        udf_func *udf = find_udf(lex->spname->m_name.str,
                                 lex->spname->m_name.length);
        if (udf)
        {
          if (check_access(thd, DELETE_ACL, "mysql", NULL, NULL, 1, 0))
            goto error;

          if (!(res = mysql_drop_function(thd, &lex->spname->m_name)))
          {
            my_ok(thd);
            break;
          }
          my_error(ER_SP_DROP_FAILED, MYF(0),
                   "FUNCTION (UDF)", lex->spname->m_name.str);
          goto error;
        }

        if (lex->spname->m_db.str == NULL)
        {
          if (lex->drop_if_exists)
          {
            push_warning_printf(thd, Sql_condition::WARN_LEVEL_NOTE,
                                ER_SP_DOES_NOT_EXIST, ER(ER_SP_DOES_NOT_EXIST),
                                "FUNCTION (UDF)", lex->spname->m_name.str);
            res= FALSE;
            my_ok(thd);
            break;
          }
          my_error(ER_SP_DOES_NOT_EXIST, MYF(0),
                   "FUNCTION (UDF)", lex->spname->m_name.str);
          goto error;
        }
        
      }
#endif

      char *db= lex->spname->m_db.str;
      char *name= lex->spname->m_name.str;

      if (check_routine_access(thd, ALTER_PROC_ACL, db, name,
                               lex->sql_command == SQLCOM_DROP_PROCEDURE,
                               false))
        goto error;

      enum_sp_type sp_type= (lex->sql_command == SQLCOM_DROP_PROCEDURE) ?
                            SP_TYPE_PROCEDURE : SP_TYPE_FUNCTION;

      
      int sp_result= sp_drop_routine(thd, sp_type, lex->spname);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
      
      DBUG_ASSERT(thd->transaction.stmt.is_empty());
      close_thread_tables(thd);

      if (sp_result != SP_KEY_NOT_FOUND &&
          sp_automatic_privileges && !opt_noacl &&
          sp_revoke_privileges(thd, db, name,
                               lex->sql_command == SQLCOM_DROP_PROCEDURE))
      {
        push_warning(thd, Sql_condition::WARN_LEVEL_WARN,
                     ER_PROC_AUTO_REVOKE_FAIL,
                     ER(ER_PROC_AUTO_REVOKE_FAIL));
        
        goto error;
      }
#endif

      res= sp_result;
      switch (sp_result) {
      case SP_OK:
	my_ok(thd);
	break;
      case SP_KEY_NOT_FOUND:
	if (lex->drop_if_exists)
	{
          res= write_bin_log(thd, TRUE, thd->query(), thd->query_length());
	  push_warning_printf(thd, Sql_condition::WARN_LEVEL_NOTE,
			      ER_SP_DOES_NOT_EXIST, ER(ER_SP_DOES_NOT_EXIST),
                              SP_COM_STRING(lex), lex->spname->m_qname.str);
          if (!res)
            my_ok(thd);
	  break;
	}
	my_error(ER_SP_DOES_NOT_EXIST, MYF(0),
                 SP_COM_STRING(lex), lex->spname->m_qname.str);
	goto error;
      default:
	my_error(ER_SP_DROP_FAILED, MYF(0),
                 SP_COM_STRING(lex), lex->spname->m_qname.str);
	goto error;
      }
      break;
    }
  case SQLCOM_SHOW_CREATE_PROC:
    {
      if (sp_show_create_routine(thd, SP_TYPE_PROCEDURE, lex->spname))
        goto error;
      break;
    }
  case SQLCOM_SHOW_CREATE_FUNC:
    {
      if (sp_show_create_routine(thd, SP_TYPE_FUNCTION, lex->spname))
	goto error;
      break;
    }
  case SQLCOM_SHOW_PROC_CODE:
  case SQLCOM_SHOW_FUNC_CODE:
    {
#ifndef DBUG_OFF
      sp_head *sp;
      enum_sp_type sp_type= (lex->sql_command == SQLCOM_SHOW_PROC_CODE) ?
                            SP_TYPE_PROCEDURE : SP_TYPE_FUNCTION;

      if (sp_cache_routine(thd, sp_type, lex->spname, false, &sp))
        goto error;
      if (!sp || sp->show_routine_code(thd))
      {
        
        my_error(ER_SP_DOES_NOT_EXIST, MYF(0),
                 SP_COM_STRING(lex), lex->spname->m_name.str);
        goto error;
      }
      break;
#else
      my_error(ER_FEATURE_DISABLED, MYF(0),
               "SHOW PROCEDURE|FUNCTION CODE", "--with-debug");
      goto error;
#endif 
    }
  case SQLCOM_SHOW_CREATE_TRIGGER:
    {
      if (lex->spname->m_name.length > NAME_LEN)
      {
        my_error(ER_TOO_LONG_IDENT, MYF(0), lex->spname->m_name.str);
        goto error;
      }

      if (show_create_trigger(thd, lex->spname))
        goto error; 

      break;
    }
  case SQLCOM_CREATE_VIEW:
    {
      
      res= mysql_create_view(thd, first_table, thd->lex->create_view_mode);
      break;
    }
  case SQLCOM_DROP_VIEW:
    {
      if (check_table_access(thd, DROP_ACL, all_tables, FALSE, UINT_MAX, FALSE))
        goto error;
      
      res= mysql_drop_view(thd, first_table, thd->lex->drop_mode);
      break;
    }
  case SQLCOM_CREATE_TRIGGER:
  {
    
    res= mysql_create_or_drop_trigger(thd, all_tables, 1);

    break;
  }
  case SQLCOM_DROP_TRIGGER:
  {
    
    res= mysql_create_or_drop_trigger(thd, all_tables, 0);
    break;
  }
  case SQLCOM_XA_START:
    if (trans_xa_start(thd))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_XA_END:
    if (trans_xa_end(thd))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_XA_PREPARE:
    if (trans_xa_prepare(thd))
      goto error;
    my_ok(thd);
    break;
  case SQLCOM_XA_COMMIT:
    if (trans_xa_commit(thd))
      goto error;
    thd->mdl_context.release_transactional_locks();
    
    thd->tx_isolation= (enum_tx_isolation) thd->variables.tx_isolation;
    thd->tx_read_only= thd->variables.tx_read_only;
    my_ok(thd);
    break;
  case SQLCOM_XA_ROLLBACK:
    if (trans_xa_rollback(thd))
      goto error;
    thd->mdl_context.release_transactional_locks();
    
    thd->tx_isolation= (enum_tx_isolation) thd->variables.tx_isolation;
    thd->tx_read_only= thd->variables.tx_read_only;
    my_ok(thd);
    break;
  case SQLCOM_XA_RECOVER:
    res= mysql_xa_recover(thd);
    break;
  case SQLCOM_ALTER_TABLESPACE:
    if (check_global_access(thd, CREATE_TABLESPACE_ACL))
      break;
    if (!(res= mysql_alter_tablespace(thd, lex->alter_tablespace_info)))
      my_ok(thd);
    break;
  case SQLCOM_INSTALL_PLUGIN:
    if (! (res= mysql_install_plugin(thd, &thd->lex->comment,
                                     &thd->lex->ident)))
      my_ok(thd);
    break;
  case SQLCOM_UNINSTALL_PLUGIN:
    if (! (res= mysql_uninstall_plugin(thd, &thd->lex->comment)))
      my_ok(thd);
    break;
  case SQLCOM_BINLOG_BASE64_EVENT:
  {
#ifndef EMBEDDED_LIBRARY
    mysql_client_binlog_statement(thd);
#else 
    my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "embedded");
#endif 
    break;
  }
  case SQLCOM_CREATE_SERVER:
  {
    if (check_global_access(thd, SUPER_ACL))
      goto error;

    if (create_server(thd, &thd->lex->server_options))
      goto error;

    my_ok(thd, 1);
    break;
  }
  case SQLCOM_ALTER_SERVER:
  {
    if (check_global_access(thd, SUPER_ACL))
      goto error;

    if (alter_server(thd, &thd->lex->server_options))
      goto error;

    my_ok(thd, 1);
    break;
  }
  case SQLCOM_DROP_SERVER:
  {
    if (check_global_access(thd, SUPER_ACL))
      goto error;

    LEX *lex= thd->lex;
    if (drop_server(thd, &lex->server_options, lex->drop_if_exists))
    {
      
      if (thd->is_error() || thd->killed)
        goto error;
      DBUG_ASSERT(lex->drop_if_exists);
      my_ok(thd, 0);
      break;
    }

    my_ok(thd, 1);
    break;
  }
  case SQLCOM_ANALYZE:
  case SQLCOM_CHECK:
  case SQLCOM_OPTIMIZE:
  case SQLCOM_REPAIR:
  case SQLCOM_TRUNCATE:
  case SQLCOM_ALTER_TABLE:
  case SQLCOM_HA_OPEN:
  case SQLCOM_HA_READ:
  case SQLCOM_HA_CLOSE:
    DBUG_ASSERT(first_table == all_tables && first_table != 0);
    
  case SQLCOM_SIGNAL:
  case SQLCOM_RESIGNAL:
  case SQLCOM_GET_DIAGNOSTICS:
    DBUG_ASSERT(lex->m_sql_cmd != NULL);
    res= lex->m_sql_cmd->execute(thd);
    break;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  case SQLCOM_ALTER_USER:
    if (check_access(thd, UPDATE_ACL, "mysql", NULL, NULL, 1, 1) &&
        check_global_access(thd, CREATE_USER_ACL))
      break;
    
    if (!(res= mysql_user_password_expire(thd, lex->users_list)))
      my_ok(thd);
    break;
#endif
  default:
#ifndef EMBEDDED_LIBRARY
    DBUG_ASSERT(0);                             
#endif
    my_ok(thd);
    break;
  }
  THD_STAGE_INFO(thd, stage_query_end);

  
  if (thd->one_shot_set && lex->sql_command != SQLCOM_SET_OPTION)
    reset_one_shot_variables(thd);

  goto finish;

error:
  res= TRUE;

finish:

  DBUG_ASSERT(!thd->in_active_multi_stmt_transaction() ||
               thd->in_multi_stmt_transaction_mode());

  if (! thd->in_sub_stmt)
  {
    
    if (thd->killed_errno())
      thd->send_kill_message();
    if (thd->killed == THD::KILL_QUERY || thd->killed == THD::KILL_BAD_DATA)
    {
      thd->killed= THD::NOT_KILLED;
      thd->mysys_var->abort= 0;
    }
    if (thd->is_error() || (thd->variables.option_bits & OPTION_MASTER_SQL_ERROR))
      trans_rollback_stmt(thd);
    else
    {
      
      thd->get_stmt_da()->set_overwrite_status(true);
      trans_commit_stmt(thd);
      thd->get_stmt_da()->set_overwrite_status(false);
    }
  }

  lex->unit.cleanup();
  
  THD_STAGE_INFO(thd, stage_closing_tables);
  close_thread_tables(thd);

#ifndef DBUG_OFF
  if (lex->sql_command != SQLCOM_SET_OPTION && ! thd->in_sub_stmt)
    DEBUG_SYNC(thd, "execute_command_after_close_tables");
#endif

  if (! thd->in_sub_stmt && thd->transaction_rollback_request)
  {
    
    trans_rollback_implicit(thd);
    thd->mdl_context.release_transactional_locks();
  }
  else if (stmt_causes_implicit_commit(thd, CF_IMPLICIT_COMMIT_END))
  {
    
    DBUG_ASSERT(! thd->in_sub_stmt);
    
    thd->get_stmt_da()->set_overwrite_status(true);
    
    trans_commit_implicit(thd);
    thd->get_stmt_da()->set_overwrite_status(false);
    thd->mdl_context.release_transactional_locks();
  }
  else if (! thd->in_sub_stmt && ! thd->in_multi_stmt_transaction_mode())
  {
    
    thd->mdl_context.release_transactional_locks();
  }
  else if (! thd->in_sub_stmt)
  {
    thd->mdl_context.release_statement_locks();
  }
  DBUG_RETURN(res || thd->is_error());
}

static bool execute_sqlcom_select(THD *thd, TABLE_LIST *all_tables)
{
  LEX	*lex= thd->lex;
  select_result *result= lex->result;
  bool res;
  
  {
    SELECT_LEX *param= lex->unit.global_parameters;
    if (!param->explicit_limit)
      param->select_limit=
        new Item_int((ulonglong) thd->variables.select_limit);
  }
  if (!(res= open_normal_and_derived_tables(thd, all_tables, 0)))
  {
    if (lex->describe)
    {
      
      if (!(result= new select_send()))
        return 1;                               
      res= explain_query_expression(thd, result);
      delete result;
    }
    else
    {
      if (!result && !(result= new select_send()))
        return 1;                               
      select_result *save_result= result;
      select_result *analyse_result= NULL;
      if (lex->proc_analyse)
      {
        if ((result= analyse_result=
               new select_analyse(result, lex->proc_analyse)) == NULL)
          return true;
      }
      TraceTool::path_count++;
      res= handle_select(thd, result, 0);
      TraceTool::path_count--;
      delete analyse_result;
      if (save_result != lex->result)
        delete save_result;
    }
  }
  DEBUG_SYNC(thd, "after_table_open");
  return res;
}


#ifndef NO_EMBEDDED_ACCESS_CHECKS


bool check_single_table_access(THD *thd, ulong privilege, 
                               TABLE_LIST *all_tables, bool no_errors)
{
  Security_context * backup_ctx= thd->security_ctx;

  
  if (all_tables->security_ctx)
    thd->security_ctx= all_tables->security_ctx;

  const char *db_name;
  if ((all_tables->view || all_tables->field_translation) &&
      !all_tables->schema_table)
    db_name= all_tables->view_db.str;
  else
    db_name= all_tables->db;

  if (check_access(thd, privilege, db_name,
                   &all_tables->grant.privilege,
                   &all_tables->grant.m_internal,
                   0, no_errors))
    goto deny;

  
  if (!(all_tables->belong_to_view &&
        (thd->lex->sql_command == SQLCOM_SHOW_FIELDS)) &&
      check_grant(thd, privilege, all_tables, FALSE, 1, no_errors))
    goto deny;

  thd->security_ctx= backup_ctx;
  return 0;

deny:
  thd->security_ctx= backup_ctx;
  return 1;
}



bool check_one_table_access(THD *thd, ulong privilege, TABLE_LIST *all_tables)
{
  if (check_single_table_access (thd,privilege,all_tables, FALSE))
    return 1;

  
  TABLE_LIST *subselects_tables, *view= all_tables->view ? all_tables : 0;
  if ((subselects_tables= all_tables->next_global))
  {
    
    if (view && subselects_tables->belong_to_view == view)
    {
      if (check_single_table_access (thd, privilege, subselects_tables, FALSE))
        return 1;
      subselects_tables= subselects_tables->next_global;
    }
    if (subselects_tables &&
        (check_table_access(thd, SELECT_ACL, subselects_tables, FALSE,
                            UINT_MAX, FALSE)))
      return 1;
  }
  return 0;
}




bool
check_access(THD *thd, ulong want_access, const char *db, ulong *save_priv,
             GRANT_INTERNAL_INFO *grant_internal_info,
             bool dont_check_global_grants, bool no_errors)
{
  Security_context *sctx= thd->security_ctx;
  ulong db_access;

  
  bool  db_is_pattern= ((want_access & GRANT_ACL) && dont_check_global_grants);
  ulong dummy;
  DBUG_ENTER("check_access");
  DBUG_PRINT("enter",("db: %s  want_access: %lu  master_access: %lu",
                      db ? db : "", want_access, sctx->master_access));

  if (save_priv)
    *save_priv=0;
  else
  {
    save_priv= &dummy;
    dummy= 0;
  }

  THD_STAGE_INFO(thd, stage_checking_permissions);
  if ((!db || !db[0]) && !thd->db && !dont_check_global_grants)
  {
    DBUG_PRINT("error",("No database"));
    if (!no_errors)
      my_message(ER_NO_DB_ERROR, ER(ER_NO_DB_ERROR),
                 MYF(0));                       
    DBUG_RETURN(TRUE);				
  }

  if ((db != NULL) && (db != any_db))
  {
    const ACL_internal_schema_access *access;
    access= get_cached_schema_access(grant_internal_info, db);
    if (access)
    {
      switch (access->check(want_access, save_priv))
      {
      case ACL_INTERNAL_ACCESS_GRANTED:
        
        DBUG_RETURN(FALSE);
      case ACL_INTERNAL_ACCESS_DENIED:
        if (! no_errors)
        {
          my_error(ER_DBACCESS_DENIED_ERROR, MYF(0),
                   sctx->priv_user, sctx->priv_host, db);
        }
        DBUG_RETURN(TRUE);
      case ACL_INTERNAL_ACCESS_CHECK_GRANT:
        
        want_access&= ~(*save_priv);
        break;
      }
    }
  }

  if ((sctx->master_access & want_access) == want_access)
  {
    
    if (!(sctx->master_access & SELECT_ACL))
    {
      if (db && (!thd->db || db_is_pattern || strcmp(db, thd->db)))
        db_access= acl_get(sctx->get_host()->ptr(), sctx->get_ip()->ptr(),
                           sctx->priv_user, db, db_is_pattern);
      else
      {
        
        db_access= sctx->db_access;
      }
      
      *save_priv|= sctx->master_access | db_access;
    }
    else
      *save_priv|= sctx->master_access;
    DBUG_RETURN(FALSE);
  }
  if (((want_access & ~sctx->master_access) & ~DB_ACLS) ||
      (! db && dont_check_global_grants))
  {						
    DBUG_PRINT("error",("No possible access"));
    if (!no_errors)
    {
      if (thd->password == 2)
        my_error(ER_ACCESS_DENIED_NO_PASSWORD_ERROR, MYF(0),
                 sctx->priv_user,
                 sctx->priv_host);
      else
        my_error(ER_ACCESS_DENIED_ERROR, MYF(0),
                 sctx->priv_user,
                 sctx->priv_host,
                 (thd->password ?
                  ER(ER_YES) :
                  ER(ER_NO)));                    
    }
    DBUG_RETURN(TRUE);				
  }

  if (db == any_db)
  {
    
    DBUG_RETURN(FALSE);
  }

  if (db && (!thd->db || db_is_pattern || strcmp(db,thd->db)))
    db_access= acl_get(sctx->get_host()->ptr(), sctx->get_ip()->ptr(),
                       sctx->priv_user, db, db_is_pattern);
  else
    db_access= sctx->db_access;
  DBUG_PRINT("info",("db_access: %lu  want_access: %lu",
                     db_access, want_access));

  
  db_access= (db_access | sctx->master_access);
  *save_priv|= db_access;

  
  bool need_table_or_column_check=
    (want_access & (TABLE_ACLS | PROC_ACLS | db_access)) == want_access;

  
  if ( (db_access & want_access) == want_access ||
      (!dont_check_global_grants &&
       need_table_or_column_check))
  {
    
    DBUG_RETURN(FALSE);
  }

  
  DBUG_PRINT("error",("Access denied"));
  if (!no_errors)
    my_error(ER_DBACCESS_DENIED_ERROR, MYF(0),
             sctx->priv_user, sctx->priv_host,
             (db ? db : (thd->db ?
                         thd->db :
                         "unknown")));
  DBUG_RETURN(TRUE);

}




static bool check_show_access(THD *thd, TABLE_LIST *table)
{
  switch (get_schema_table_idx(table->schema_table)) {
  case SCH_SCHEMATA:
    return (specialflag & SPECIAL_SKIP_SHOW_DB) &&
      check_global_access(thd, SHOW_DB_ACL);

  case SCH_TABLE_NAMES:
  case SCH_TABLES:
  case SCH_VIEWS:
  case SCH_TRIGGERS:
  case SCH_EVENTS:
  {
    const char *dst_db_name= table->schema_select_lex->db;

    DBUG_ASSERT(dst_db_name);

    if (check_access(thd, SELECT_ACL, dst_db_name,
                     &thd->col_access, NULL, FALSE, FALSE))
      return TRUE;

    if (!thd->col_access && check_grant_db(thd, dst_db_name))
    {
      my_error(ER_DBACCESS_DENIED_ERROR, MYF(0),
               thd->security_ctx->priv_user,
               thd->security_ctx->priv_host,
               dst_db_name);
      return TRUE;
    }

    return FALSE;
  }

  case SCH_COLUMNS:
  case SCH_STATISTICS:
  {
    TABLE_LIST *dst_table;
    dst_table= table->schema_select_lex->table_list.first;

    DBUG_ASSERT(dst_table);

    
    if (open_temporary_tables(thd, dst_table))
      return TRUE;

    if (check_access(thd, SELECT_ACL, dst_table->db,
                     &dst_table->grant.privilege,
                     &dst_table->grant.m_internal,
                     FALSE, FALSE))
          return TRUE; 

    
    if (check_grant(thd, SELECT_ACL, dst_table, TRUE, UINT_MAX, FALSE))
      return TRUE; 

    close_thread_tables(thd);
    dst_table->table= NULL;

    
    return FALSE;
  }
  default:
    break;
  }

  return FALSE;
}





bool
check_table_access(THD *thd, ulong requirements,TABLE_LIST *tables,
		   bool any_combination_of_privileges_will_do,
                   uint number, bool no_errors)
{
  TABLE_LIST *org_tables= tables;
  TABLE_LIST *first_not_own_table= thd->lex->first_not_own_table();
  uint i= 0;
  Security_context *sctx= thd->security_ctx, *backup_ctx= thd->security_ctx;
  
  for (; i < number && tables != first_not_own_table && tables;
       tables= tables->next_global, i++)
  {
    ulong want_access= requirements;
    if (tables->security_ctx)
      sctx= tables->security_ctx;
    else
      sctx= backup_ctx;

    
    tables->grant.orig_want_privilege= (want_access & ~SHOW_VIEW_ACL);

    
    DBUG_ASSERT(!tables->schema_table_reformed ||
                tables == thd->lex->select_lex.table_list.first);

    DBUG_PRINT("info", ("derived: %d  view: %d", tables->derived != 0,
                        tables->view != 0));

    if (tables->is_anonymous_derived_table())
      continue;

    thd->security_ctx= sctx;

    if (check_access(thd, want_access, tables->get_db_name(),
                     &tables->grant.privilege,
                     &tables->grant.m_internal,
                     0, no_errors))
      goto deny;
  }
  thd->security_ctx= backup_ctx;
  return check_grant(thd,requirements,org_tables,
                     any_combination_of_privileges_will_do,
                     number, no_errors);
deny:
  thd->security_ctx= backup_ctx;
  return TRUE;
}


bool
check_routine_access(THD *thd, ulong want_access,char *db, char *name,
		     bool is_proc, bool no_errors)
{
  TABLE_LIST tables[1];
  
  memset(tables, 0, sizeof(TABLE_LIST));
  tables->db= db;
  tables->table_name= tables->alias= name;
  
  
  DBUG_ASSERT((want_access & CREATE_PROC_ACL) == 0);
  if ((thd->security_ctx->master_access & want_access) == want_access)
    tables->grant.privilege= want_access;
  else if (check_access(thd, want_access, db,
                        &tables->grant.privilege,
                        &tables->grant.m_internal,
                        0, no_errors))
    return TRUE;
  
  return check_grant_routine(thd, want_access, tables, is_proc, no_errors);
}




bool check_some_routine_access(THD *thd, const char *db, const char *name,
                               bool is_proc)
{
  ulong save_priv;
  
  if (thd->security_ctx->master_access & SHOW_PROC_ACLS)
    return FALSE;
  if (!check_access(thd, SHOW_PROC_ACLS, db, &save_priv, NULL, 0, 1) ||
      (save_priv & SHOW_PROC_ACLS))
    return FALSE;
  return check_routine_level_acl(thd, db, name, is_proc);
}




bool check_some_access(THD *thd, ulong want_access, TABLE_LIST *table)
{
  ulong access;
  DBUG_ENTER("check_some_access");

  
  for (access= 1; access < want_access ; access<<= 1)
  {
    if (access & want_access)
    {
      if (!check_access(thd, access, table->db,
                        &table->grant.privilege,
                        &table->grant.m_internal,
                        0, 1) &&
           !check_grant(thd, access, table, FALSE, 1, TRUE))
        DBUG_RETURN(0);
    }
  }
  DBUG_PRINT("exit",("no matching access rights"));
  DBUG_RETURN(1);
}

#else

static bool check_show_access(THD *thd, TABLE_LIST *table)
{
  return false;
}

#endif 




bool check_global_access(THD *thd, ulong want_access)
{
  DBUG_ENTER("check_global_access");
#ifndef NO_EMBEDDED_ACCESS_CHECKS
  char command[128];
  if ((thd->security_ctx->master_access & want_access))
    DBUG_RETURN(0);
  get_privilege_desc(command, sizeof(command), want_access);
  my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), command);
  DBUG_RETURN(1);
#else
  DBUG_RETURN(0);
#endif
}



bool check_fk_parent_table_access(THD *thd,
                                  HA_CREATE_INFO *create_info,
                                  Alter_info *alter_info)
{
  Key *key;
  List_iterator<Key> key_iterator(alter_info->key_list);
  handlerton *db_type= create_info->db_type ? create_info->db_type :
                                             ha_default_handlerton(thd);

  
  if (!ha_check_storage_engine_flag(db_type, HTON_SUPPORTS_FOREIGN_KEYS))
    return false;

  while ((key= key_iterator++))
  {
    if (key->type == Key::FOREIGN_KEY)
    {
      TABLE_LIST parent_table;
      bool is_qualified_table_name;
      Foreign_key *fk_key= (Foreign_key *)key;
      LEX_STRING db_name;
      LEX_STRING table_name= { fk_key->ref_table.str,
                               fk_key->ref_table.length };
      const ulong privileges= (SELECT_ACL | INSERT_ACL | UPDATE_ACL |
                               DELETE_ACL | REFERENCES_ACL);

      
      DBUG_ASSERT(table_name.str != NULL);
      if (check_table_name(table_name.str, table_name.length, false))
      {
        my_error(ER_WRONG_TABLE_NAME, MYF(0), table_name.str);
        return true;
      }

      if (fk_key->ref_db.str)
      {
        is_qualified_table_name= true;
        db_name.str= (char *) thd->memdup(fk_key->ref_db.str,
                                          fk_key->ref_db.length+1);
        db_name.length= fk_key->ref_db.length;

        
        if (fk_key->ref_db.str && check_and_convert_db_name(&db_name, false))
          return true;
      }
      else if (thd->lex->copy_db_to(&db_name.str, &db_name.length))
        return true;
      else
        is_qualified_table_name= false;

      
      if (lower_case_table_names)
      {
        table_name.str= (char *) thd->memdup(fk_key->ref_table.str,
                                             fk_key->ref_table.length+1);
        table_name.length= my_casedn_str(files_charset_info, table_name.str);
      }

      parent_table.init_one_table(db_name.str, db_name.length,
                                  table_name.str, table_name.length,
                                  table_name.str, TL_IGNORE);

      
      if (check_some_access(thd, privileges, &parent_table) ||
          parent_table.grant.want_privilege)
      {
        if (is_qualified_table_name)
        {
          const size_t qualified_table_name_len= NAME_LEN + 1 + NAME_LEN + 1;
          char *qualified_table_name= (char *) thd->alloc(qualified_table_name_len);

          my_snprintf(qualified_table_name, qualified_table_name_len, "%s.%s",
                      db_name.str, table_name.str);
          table_name.str= qualified_table_name;
        }

        my_error(ER_TABLEACCESS_DENIED_ERROR, MYF(0),
                 "REFERENCES",
                 thd->security_ctx->priv_user,
                 thd->security_ctx->host_or_ip,
                 table_name.str);

        return true;
      }
    }
  }

  return false;
}





#if STACK_DIRECTION < 0
#define used_stack(A,B) (long) (A - B)
#else
#define used_stack(A,B) (long) (B - A)
#endif

#ifndef DBUG_OFF
long max_stack_used;
#endif


bool check_stack_overrun(THD *thd, long margin,
			 uchar *buf __attribute__((unused)))
{
  long stack_used;
  DBUG_ASSERT(thd == current_thd);
  if ((stack_used=used_stack(thd->thread_stack,(char*) &stack_used)) >=
      (long) (my_thread_stack_size - margin))
  {
    
    char* ebuff= new (std::nothrow) char[MYSQL_ERRMSG_SIZE];
    if (ebuff) {
      my_snprintf(ebuff, MYSQL_ERRMSG_SIZE, ER(ER_STACK_OVERRUN_NEED_MORE),
                  stack_used, my_thread_stack_size, margin);
      my_message(ER_STACK_OVERRUN_NEED_MORE, ebuff, MYF(ME_FATALERROR));
      delete [] ebuff;
    }
    return 1;
  }
#ifndef DBUG_OFF
  max_stack_used= max(max_stack_used, stack_used);
#endif
  return 0;
}


#define MY_YACC_INIT 1000			
#define MY_YACC_MAX  32000			

bool my_yyoverflow(short **yyss, YYSTYPE **yyvs, ulong *yystacksize)
{
  Yacc_state *state= & current_thd->m_parser_state->m_yacc;
  ulong old_info=0;
  DBUG_ASSERT(state);
  if ((uint) *yystacksize >= MY_YACC_MAX)
    return 1;
  if (!state->yacc_yyvs)
    old_info= *yystacksize;
  *yystacksize= set_zone((*yystacksize)*2,MY_YACC_INIT,MY_YACC_MAX);
  if (!(state->yacc_yyvs= (uchar*)
        my_realloc(state->yacc_yyvs,
                   *yystacksize*sizeof(**yyvs),
                   MYF(MY_ALLOW_ZERO_PTR | MY_FREE_ON_ERROR))) ||
      !(state->yacc_yyss= (uchar*)
        my_realloc(state->yacc_yyss,
                   *yystacksize*sizeof(**yyss),
                   MYF(MY_ALLOW_ZERO_PTR | MY_FREE_ON_ERROR))))
    return 1;
  if (old_info)
  {
    
    memcpy(state->yacc_yyss, *yyss, old_info*sizeof(**yyss));
    memcpy(state->yacc_yyvs, *yyvs, old_info*sizeof(**yyvs));
  }
  *yyss= (short*) state->yacc_yyss;
  *yyvs= (YYSTYPE*) state->yacc_yyvs;
  return 0;
}



void mysql_reset_thd_for_next_command(THD *thd)
{
  thd->reset_for_next_command();
}

void THD::reset_for_next_command()
{
  
  
  THD *thd= this;
  DBUG_ENTER("mysql_reset_thd_for_next_command");
  DBUG_ASSERT(!thd->sp_runtime_ctx); 
  DBUG_ASSERT(! thd->in_sub_stmt);
  thd->free_list= 0;
  thd->select_number= 1;
  
  thd->auto_inc_intervals_in_cur_stmt_for_binlog.empty();
  thd->stmt_depends_on_first_successful_insert_id_in_prev_stmt= 0;

  thd->query_start_used= thd->query_start_usec_used= 0;
  thd->is_fatal_error= thd->time_zone_used= 0;
  
  thd->server_status&= ~SERVER_STATUS_CLEAR_SET;
  
  if (!thd->in_multi_stmt_transaction_mode())
  {
    thd->transaction.all.reset_unsafe_rollback_flags();
  }
  DBUG_ASSERT(thd->security_ctx== &thd->main_security_ctx);
  thd->thread_specific_used= FALSE;

  if (opt_bin_log)
  {
    reset_dynamic(&thd->user_var_events);
    thd->user_var_events_alloc= thd->mem_root;
  }
  thd->clear_error();
  thd->get_stmt_da()->reset_diagnostics_area();
  thd->get_stmt_da()->reset_for_next_command();
  thd->rand_used= 0;
  thd->m_sent_row_count= thd->m_examined_row_count= 0;

  thd->reset_current_stmt_binlog_format_row();
  thd->binlog_unsafe_warning_flags= 0;

  thd->m_trans_end_pos= 0;
  thd->m_trans_log_file= NULL;
  thd->m_trans_fixed_log_file= NULL;
  thd->commit_error= THD::CE_NONE;
  thd->durability_property= HA_REGULAR_DURABILITY;
  thd->set_trans_pos(NULL, 0);

  DBUG_PRINT("debug",
             ("is_current_stmt_binlog_format_row(): %d",
              thd->is_current_stmt_binlog_format_row()));

  DBUG_VOID_RETURN;
}




void
mysql_init_select(LEX *lex)
{
  SELECT_LEX *select_lex= lex->current_select;
  select_lex->init_select();
  lex->wild= 0;
  if (select_lex == &lex->select_lex)
  {
    DBUG_ASSERT(lex->result == 0);
    lex->exchange= 0;
  }
}




bool
mysql_new_select(LEX *lex, bool move_down)
{
  SELECT_LEX *select_lex;
  THD *thd= lex->thd;
  Name_resolution_context *outer_context= lex->current_context();
  DBUG_ENTER("mysql_new_select");

  if (!(select_lex= new (thd->mem_root) SELECT_LEX()))
    DBUG_RETURN(1);
  select_lex->select_number= ++thd->select_number;
  select_lex->parent_lex= lex; 
  select_lex->init_query();
  select_lex->init_select();
  lex->nest_level++;
  if (lex->nest_level > (int) MAX_SELECT_NESTING)
  {
    my_error(ER_TOO_HIGH_LEVEL_OF_NESTING_FOR_SELECT, MYF(0));
    DBUG_RETURN(1);
  }
  select_lex->nest_level= lex->nest_level;
  if (move_down)
  {
    SELECT_LEX_UNIT *unit;
    lex->subqueries= TRUE;
    
    if (!(unit= new (thd->mem_root) SELECT_LEX_UNIT()))
      DBUG_RETURN(1);

    unit->init_query();
    unit->init_select();
    unit->thd= thd;
    unit->include_down(lex->current_select);
    unit->link_next= 0;
    unit->link_prev= 0;
    select_lex->include_down(unit);
    
    if (select_lex->outer_select()->parsing_place == IN_ON)
      
      select_lex->context.outer_context= outer_context;
    else
      select_lex->context.outer_context= &select_lex->outer_select()->context;
  }
  else
  {
    if (lex->current_select->order_list.first && !lex->current_select->braces)
    {
      my_error(ER_WRONG_USAGE, MYF(0), "UNION", "ORDER BY");
      DBUG_RETURN(1);
    }
    select_lex->include_neighbour(lex->current_select);
    SELECT_LEX_UNIT *unit= select_lex->master_unit();                              
    if (!unit->fake_select_lex && unit->add_fake_select_lex(lex->thd))
      DBUG_RETURN(1);
    select_lex->context.outer_context= 
                unit->first_select()->context.outer_context;
  }

  select_lex->master_unit()->global_parameters= select_lex;
  select_lex->include_global((st_select_lex_node**)&lex->all_selects_list);
  lex->current_select= select_lex;
  
  select_lex->context.resolve_in_select_list= TRUE;
  DBUG_RETURN(0);
}



void create_select_for_variable(const char *var_name)
{
  THD *thd;
  LEX *lex;
  LEX_STRING tmp, null_lex_string;
  Item *var;
  char buff[MAX_SYS_VAR_LENGTH*2+4+8], *end;
  DBUG_ENTER("create_select_for_variable");

  thd= current_thd;
  lex= thd->lex;
  mysql_init_select(lex);
  lex->sql_command= SQLCOM_SELECT;
  tmp.str= (char*) var_name;
  tmp.length=strlen(var_name);
  memset(&null_lex_string, 0, sizeof(null_lex_string));
  
  if ((var= get_system_var(thd, OPT_SESSION, tmp, null_lex_string)))
  {
    end= strxmov(buff, "@@session.", var_name, NullS);
    var->item_name.copy(buff, end - buff);
    add_item_to_list(thd, var);
  }
  DBUG_VOID_RETURN;
}


void mysql_init_multi_delete(LEX *lex)
{
  lex->sql_command=  SQLCOM_DELETE_MULTI;
  mysql_init_select(lex);
  lex->select_lex.select_limit= 0;
  lex->unit.select_limit_cnt= HA_POS_ERROR;
  lex->select_lex.table_list.save_and_clear(&lex->auxiliary_table_list);
  lex->query_tables= 0;
  lex->query_tables_last= &lex->query_tables;
}






void mysql_parse(THD *thd, char *rawbuf, uint length,
                 Parser_state *parser_state)
{
  int error __attribute__((unused));
  DBUG_ENTER("mysql_parse");

  DBUG_EXECUTE_IF("parser_debug", turn_parser_debug_on(););

  
  lex_start(thd);
  mysql_reset_thd_for_next_command(thd);

  if (query_cache_send_result_to_client(thd, rawbuf, length) <= 0)
  {
    LEX *lex= thd->lex;

    bool err= parse_sql(thd, parser_state, NULL);

    const char *found_semicolon= parser_state->m_lip.found_semicolon;
    size_t      qlen= found_semicolon
                      ? (found_semicolon - thd->query())
                      : thd->query_length();

    if (!err)
    {
      
      bool general= (opt_log && ! (opt_log_raw || thd->slave_thread));

      if (general || opt_slow_log || opt_bin_log)
      {
        mysql_rewrite_query(thd);

        if (thd->rewritten_query.length())
          lex->safe_to_cache_query= false; 
      }

      if (general)
      {
        if (thd->rewritten_query.length())
          general_log_write(thd, COM_QUERY, thd->rewritten_query.c_ptr_safe(),
                                            thd->rewritten_query.length());
        else
          general_log_write(thd, COM_QUERY, thd->query(), qlen);
      }
    }

    if (!err)
    {
      thd->m_statement_psi= MYSQL_REFINE_STATEMENT(thd->m_statement_psi,
                                                   sql_statement_info[thd->lex->sql_command].m_key);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
      if (mqh_used && thd->get_user_connect() &&
	  check_mqh(thd, lex->sql_command))
      {
	thd->net.error = 0;
      }
      else
#endif
      {
	if (! thd->is_error())
	{
          
          if (found_semicolon && (ulong) (found_semicolon - thd->query()))
            thd->set_query_inner(thd->query(),
                                 (uint32) (found_semicolon -
                                           thd->query() - 1),
                                 thd->charset());
          
          if (found_semicolon)
          {
            lex->safe_to_cache_query= 0;
            thd->server_status|= SERVER_MORE_RESULTS_EXISTS;
          }
          lex->set_trg_event_type_for_tables();
          MYSQL_QUERY_EXEC_START(thd->query(),
                                 thd->thread_id,
                                 (char *) (thd->db ? thd->db : ""),
                                 &thd->security_ctx->priv_user[0],
                                 (char *) thd->security_ctx->host_or_ip,
                                 0);
          if (unlikely(thd->security_ctx->password_expired &&
                       !lex->is_change_password &&
                       lex->sql_command != SQLCOM_SET_OPTION))
          {
            my_error(ER_MUST_CHANGE_PASSWORD, MYF(0));
            error= 1;
          }
          else
          {
            TraceTool::path_count++;
            error= mysql_execute_command(thd);
            TraceTool::path_count--;
          }
          if (error == 0 &&
              thd->variables.gtid_next.type == GTID_GROUP &&
              thd->owned_gtid.sidno != 0 &&
              (thd->lex->sql_command == SQLCOM_COMMIT ||
               stmt_causes_implicit_commit(thd, CF_IMPLICIT_COMMIT_END) ||
               thd->lex->sql_command == SQLCOM_CREATE_TABLE ||
               thd->lex->sql_command == SQLCOM_DROP_TABLE))
          {
            
            error= gtid_empty_group_log_and_cleanup(thd);
          }
          MYSQL_QUERY_EXEC_DONE(error);
	}
      }
    }
    else
    {
      
      thd->m_statement_psi= MYSQL_REFINE_STATEMENT(thd->m_statement_psi,
                                                   sql_statement_info[SQLCOM_END].m_key);

      DBUG_ASSERT(thd->is_error());
      DBUG_PRINT("info",("Command aborted. Fatal_error: %d",
			 thd->is_fatal_error));

      query_cache_abort(&thd->query_cache_tls);
    }

    THD_STAGE_INFO(thd, stage_freeing_items);
    sp_cache_enforce_limit(thd->sp_proc_cache, stored_program_cache_size);
    sp_cache_enforce_limit(thd->sp_func_cache, stored_program_cache_size);
    thd->end_statement();
    thd->cleanup_after_query();
    DBUG_ASSERT(thd->change_list.is_empty());
  }
  else
  {
    
    thd->m_statement_psi= MYSQL_REFINE_STATEMENT(thd->m_statement_psi,
                                                 sql_statement_info[SQLCOM_SELECT].m_key);
    if (!opt_log_raw)
      general_log_write(thd, COM_QUERY, thd->query(), thd->query_length());
    parser_state->m_lip.found_semicolon= NULL;
  }

  DBUG_VOID_RETURN;
}


#ifdef HAVE_REPLICATION


bool mysql_test_parse_for_slave(THD *thd, char *rawbuf, uint length)
{
  LEX *lex= thd->lex;
  bool ignorable= false;
  PSI_statement_locker *parent_locker= thd->m_statement_psi;
  DBUG_ENTER("mysql_test_parse_for_slave");

  DBUG_ASSERT(thd->slave_thread);

  Parser_state parser_state;
  if (parser_state.init(thd, rawbuf, length) == 0)
  {
    lex_start(thd);
    mysql_reset_thd_for_next_command(thd);

    thd->m_statement_psi= NULL;
    if (parse_sql(thd, & parser_state, NULL) == 0)
    {
      if (all_tables_not_ok(thd, lex->select_lex.table_list.first))
        ignorable= true;
      else if (lex->sql_command != SQLCOM_BEGIN &&
               lex->sql_command != SQLCOM_COMMIT &&
               lex->sql_command != SQLCOM_SAVEPOINT &&
               lex->sql_command != SQLCOM_ROLLBACK &&
               lex->sql_command != SQLCOM_ROLLBACK_TO_SAVEPOINT &&
               !rpl_filter->db_ok(thd->db))
        ignorable= true;
    }
    thd->m_statement_psi= parent_locker;
    thd->end_statement();
  }
  thd->cleanup_after_query();
  DBUG_RETURN(ignorable);
}
#endif





bool add_field_to_list(THD *thd, LEX_STRING *field_name, enum_field_types type,
		       char *length, char *decimals,
		       uint type_modifier,
		       Item *default_value, Item *on_update_value,
                       LEX_STRING *comment,
		       char *change,
                       List<String> *interval_list, const CHARSET_INFO *cs,
		       uint uint_geom_type)
{
  register Create_field *new_field;
  LEX  *lex= thd->lex;
  uint8 datetime_precision= decimals ? atoi(decimals) : 0;
  DBUG_ENTER("add_field_to_list");

  if (check_string_char_length(field_name, "", NAME_CHAR_LEN,
                               system_charset_info, 1))
  {
    my_error(ER_TOO_LONG_IDENT, MYF(0), field_name->str); 
    DBUG_RETURN(1);				
  }
  if (type_modifier & PRI_KEY_FLAG)
  {
    Key *key;
    lex->col_list.push_back(new Key_part_spec(*field_name, 0));
    key= new Key(Key::PRIMARY, null_lex_str,
                      &default_key_create_info,
                      0, lex->col_list);
    lex->alter_info.key_list.push_back(key);
    lex->col_list.empty();
  }
  if (type_modifier & (UNIQUE_FLAG | UNIQUE_KEY_FLAG))
  {
    Key *key;
    lex->col_list.push_back(new Key_part_spec(*field_name, 0));
    key= new Key(Key::UNIQUE, null_lex_str,
                 &default_key_create_info, 0,
                 lex->col_list);
    lex->alter_info.key_list.push_back(key);
    lex->col_list.empty();
  }

  if (default_value)
  {
    
    if (default_value->type() == Item::FUNC_ITEM && 
        (static_cast<Item_func*>(default_value)->functype() !=
         Item_func::NOW_FUNC ||
         (!real_type_with_now_as_default(type)) ||
         default_value->decimals != datetime_precision))
    {
      my_error(ER_INVALID_DEFAULT, MYF(0), field_name->str);
      DBUG_RETURN(1);
    }
    else if (default_value->type() == Item::NULL_ITEM)
    {
      default_value= 0;
      if ((type_modifier & (NOT_NULL_FLAG | AUTO_INCREMENT_FLAG)) ==
	  NOT_NULL_FLAG)
      {
	my_error(ER_INVALID_DEFAULT, MYF(0), field_name->str);
	DBUG_RETURN(1);
      }
    }
    else if (type_modifier & AUTO_INCREMENT_FLAG)
    {
      my_error(ER_INVALID_DEFAULT, MYF(0), field_name->str);
      DBUG_RETURN(1);
    }
  }

  if (on_update_value &&
      (!real_type_with_now_on_update(type) ||
       on_update_value->decimals != datetime_precision))
  {
    my_error(ER_INVALID_ON_UPDATE, MYF(0), field_name->str);
    DBUG_RETURN(1);
  }

  if (!(new_field= new Create_field()) ||
      new_field->init(thd, field_name->str, type, length, decimals, type_modifier,
                      default_value, on_update_value, comment, change,
                      interval_list, cs, uint_geom_type))
    DBUG_RETURN(1);

  lex->alter_info.create_list.push_back(new_field);
  lex->last_field=new_field;
  DBUG_RETURN(0);
}




void store_position_for_column(const char *name)
{
  current_thd->lex->last_field->after=(char*) (name);
}




bool add_to_list(THD *thd, SQL_I_List<ORDER> &list, Item *item,bool asc)
{
  ORDER *order;
  DBUG_ENTER("add_to_list");
  if (!(order = (ORDER *) thd->alloc(sizeof(ORDER))))
    DBUG_RETURN(1);
  order->item_ptr= item;
  order->item= &order->item_ptr;
  order->direction= (asc ? ORDER::ORDER_ASC : ORDER::ORDER_DESC);
  order->used_alias= false;
  order->used=0;
  order->counter_used= 0;
  list.link_in_list(order, &order->next);
  DBUG_RETURN(0);
}




TABLE_LIST *st_select_lex::add_table_to_list(THD *thd,
					     Table_ident *table,
					     LEX_STRING *alias,
					     ulong table_options,
					     thr_lock_type lock_type,
					     enum_mdl_type mdl_type,
					     List<Index_hint> *index_hints_arg,
                                             List<String> *partition_names,
                                             LEX_STRING *option)
{
  register TABLE_LIST *ptr;
  TABLE_LIST *previous_table_ref; 
  char *alias_str;
  LEX *lex= thd->lex;
  DBUG_ENTER("add_table_to_list");
  LINT_INIT(previous_table_ref);

  if (!table)
    DBUG_RETURN(0);				
  alias_str= alias ? alias->str : table->table.str;
  if (!MY_TEST(table_options & TL_OPTION_ALIAS))
  {
    enum_ident_name_check ident_check_status=
      check_table_name(table->table.str, table->table.length, FALSE);
    if (ident_check_status == IDENT_NAME_WRONG)
    {
      my_error(ER_WRONG_TABLE_NAME, MYF(0), table->table.str);
      DBUG_RETURN(0);
    }
    else if (ident_check_status == IDENT_NAME_TOO_LONG)
    {
      my_error(ER_TOO_LONG_IDENT, MYF(0), table->table.str);
      DBUG_RETURN(0);
    }
  }
  if (table->is_derived_table() == FALSE && table->db.str &&
      (check_and_convert_db_name(&table->db, FALSE) != IDENT_NAME_OK))
    DBUG_RETURN(0);

  if (!alias)					
  {
    if (table->sel)
    {
      my_message(ER_DERIVED_MUST_HAVE_ALIAS,
                 ER(ER_DERIVED_MUST_HAVE_ALIAS), MYF(0));
      DBUG_RETURN(0);
    }
    if (!(alias_str= (char*) thd->memdup(alias_str,table->table.length+1)))
      DBUG_RETURN(0);
  }
  if (!(ptr = (TABLE_LIST *) thd->calloc(sizeof(TABLE_LIST))))
    DBUG_RETURN(0);				
  if (table->db.str)
  {
    ptr->is_fqtn= TRUE;
    ptr->db= table->db.str;
    ptr->db_length= table->db.length;
  }
  else if (lex->copy_db_to(&ptr->db, &ptr->db_length))
    DBUG_RETURN(0);
  else
    ptr->is_fqtn= FALSE;

  ptr->alias= alias_str;
  ptr->is_alias= alias ? TRUE : FALSE;
  if (lower_case_table_names && table->table.length)
    table->table.length= my_casedn_str(files_charset_info, table->table.str);
  ptr->table_name=table->table.str;
  ptr->table_name_length=table->table.length;
  ptr->lock_type=   lock_type;
  ptr->updating=    MY_TEST(table_options & TL_OPTION_UPDATING);
  
  ptr->force_index= MY_TEST(table_options & TL_OPTION_FORCE_INDEX);
  ptr->ignore_leaves= MY_TEST(table_options & TL_OPTION_IGNORE_LEAVES);
  ptr->derived=	    table->sel;
  if (!ptr->derived && is_infoschema_db(ptr->db, ptr->db_length))
  {
    ST_SCHEMA_TABLE *schema_table;
    if (ptr->updating &&
        
        lex->sql_command != SQLCOM_CHECK &&
        lex->sql_command != SQLCOM_CHECKSUM)
    {
      my_error(ER_DBACCESS_DENIED_ERROR, MYF(0),
               thd->security_ctx->priv_user,
               thd->security_ctx->priv_host,
               INFORMATION_SCHEMA_NAME.str);
      DBUG_RETURN(0);
    }
    schema_table= find_schema_table(thd, ptr->table_name);
    if (!schema_table ||
        (schema_table->hidden && 
         ((sql_command_flags[lex->sql_command] & CF_STATUS_COMMAND) == 0 || 
          
          lex->sql_command == SQLCOM_SHOW_FIELDS ||
          lex->sql_command == SQLCOM_SHOW_KEYS)))
    {
      my_error(ER_UNKNOWN_TABLE, MYF(0),
               ptr->table_name, INFORMATION_SCHEMA_NAME.str);
      DBUG_RETURN(0);
    }
    ptr->schema_table_name= ptr->table_name;
    ptr->schema_table= schema_table;
  }
  ptr->select_lex=  lex->current_select;
  ptr->cacheable_table= 1;
  ptr->index_hints= index_hints_arg;
  ptr->option= option ? option->str : 0;
  
  if (lock_type != TL_IGNORE)
  {
    TABLE_LIST *first_table= table_list.first;
    if (lex->sql_command == SQLCOM_CREATE_VIEW)
      first_table= first_table ? first_table->next_local : NULL;
    for (TABLE_LIST *tables= first_table ;
	 tables ;
	 tables=tables->next_local)
    {
      if (!my_strcasecmp(table_alias_charset, alias_str, tables->alias) &&
	  !strcmp(ptr->db, tables->db))
      {
	my_error(ER_NONUNIQ_TABLE, MYF(0), alias_str); 
	DBUG_RETURN(0);				
      }
    }
  }
  
  if (table_list.elements > 0)
  {
    
    previous_table_ref= (TABLE_LIST*) ((char*) table_list.next -
                                       ((char*) &(ptr->next_local) -
                                        (char*) ptr));
    
    previous_table_ref->next_name_resolution_table= ptr;
  }

  
  table_list.link_in_list(ptr, &ptr->next_local);
  ptr->next_name_resolution_table= NULL;
#ifdef WITH_PARTITION_STORAGE_ENGINE
  ptr->partition_names= partition_names;
#endif 
  
  lex->add_to_query_tables(ptr);

  
  if (!MY_TEST(table_options & TL_OPTION_ALIAS))
  {
    ptr->mdl_request.init(MDL_key::TABLE, ptr->db, ptr->table_name, mdl_type,
                          MDL_TRANSACTION);
  }
  if (table->is_derived_table())
  {
    ptr->effective_algorithm= DERIVED_ALGORITHM_TMPTABLE;
    ptr->derived_key_list.empty();
  }
  DBUG_RETURN(ptr);
}




bool st_select_lex::init_nested_join(THD *thd)
{
  DBUG_ENTER("init_nested_join");

  TABLE_LIST *const ptr=
    TABLE_LIST::new_nested_join(thd->mem_root, "(nested_join)",
                                embedding, join_list, this);
  if (ptr == NULL)
    DBUG_RETURN(true);

  join_list->push_front(ptr);
  embedding= ptr;
  join_list= &ptr->nested_join->join_list;

  DBUG_RETURN(false);
}




TABLE_LIST *st_select_lex::end_nested_join(THD *thd)
{
  TABLE_LIST *ptr;
  NESTED_JOIN *nested_join;
  DBUG_ENTER("end_nested_join");

  DBUG_ASSERT(embedding);
  ptr= embedding;
  join_list= ptr->join_list;
  embedding= ptr->embedding;
  nested_join= ptr->nested_join;
  if (nested_join->join_list.elements == 1)
  {
    TABLE_LIST *embedded= nested_join->join_list.head();
    join_list->pop();
    embedded->join_list= join_list;
    embedded->embedding= embedding;
    join_list->push_front(embedded);
    ptr= embedded;
  }
  else if (nested_join->join_list.elements == 0)
  {
    join_list->pop();
    ptr= 0;                                     
  }
  DBUG_RETURN(ptr);
}




TABLE_LIST *st_select_lex::nest_last_join(THD *thd)
{
  DBUG_ENTER("nest_last_join");

  TABLE_LIST *const ptr=
    TABLE_LIST::new_nested_join(thd->mem_root, "(nest_last_join)",
                                embedding, join_list, this);
  if (ptr == NULL)
    DBUG_RETURN(NULL);

  List<TABLE_LIST> *const embedded_list= &ptr->nested_join->join_list;

  for (uint i=0; i < 2; i++)
  {
    TABLE_LIST *table= join_list->pop();
    table->join_list= embedded_list;
    table->embedding= ptr;
    embedded_list->push_back(table);
    if (table->natural_join)
    {
      ptr->is_natural_join= TRUE;
      
      if (prev_join_using)
        ptr->join_using_fields= prev_join_using;
    }
  }
  join_list->push_front(ptr);

  DBUG_RETURN(ptr);
}




void st_select_lex::add_joined_table(TABLE_LIST *table)
{
  DBUG_ENTER("add_joined_table");
  join_list->push_front(table);
  table->join_list= join_list;
  table->embedding= embedding;
  DBUG_VOID_RETURN;
}




TABLE_LIST *st_select_lex::convert_right_join()
{
  TABLE_LIST *tab2= join_list->pop();
  TABLE_LIST *tab1= join_list->pop();
  DBUG_ENTER("convert_right_join");

  join_list->push_front(tab2);
  join_list->push_front(tab1);
  tab1->outer_join|= JOIN_TYPE_RIGHT;

  DBUG_RETURN(tab1);
}



void st_select_lex::set_lock_for_tables(thr_lock_type lock_type)
{
  bool for_update= lock_type >= TL_READ_NO_INSERT;
  DBUG_ENTER("set_lock_for_tables");
  DBUG_PRINT("enter", ("lock_type: %d  for_update: %d", lock_type,
		       for_update));
  for (TABLE_LIST *tables= table_list.first;
       tables;
       tables= tables->next_local)
  {
    tables->lock_type= lock_type;
    tables->updating=  for_update;
    tables->mdl_request.set_type((lock_type >= TL_WRITE_ALLOW_WRITE) ?
                                 MDL_SHARED_WRITE : MDL_SHARED_READ);
  }
  DBUG_VOID_RETURN;
}




bool st_select_lex_unit::add_fake_select_lex(THD *thd_arg)
{
  SELECT_LEX *first_sl= first_select();
  DBUG_ENTER("add_fake_select_lex");
  DBUG_ASSERT(!fake_select_lex);

  if (!(fake_select_lex= new (thd_arg->mem_root) SELECT_LEX()))
      DBUG_RETURN(1);
  fake_select_lex->include_standalone(this, 
                                      (SELECT_LEX_NODE**)&fake_select_lex);
  fake_select_lex->select_number= INT_MAX;
  fake_select_lex->parent_lex= thd_arg->lex; 
  fake_select_lex->make_empty_select();
  fake_select_lex->linkage= GLOBAL_OPTIONS_TYPE;
  fake_select_lex->select_limit= 0;

  fake_select_lex->context.outer_context=first_sl->context.outer_context;
  
  fake_select_lex->context.resolve_in_select_list= TRUE;
  fake_select_lex->context.select_lex= fake_select_lex;

  if (!is_union())
  {
     
    global_parameters= fake_select_lex;
    fake_select_lex->no_table_names_allowed= 1;
    thd_arg->lex->current_select= fake_select_lex;
  }
  thd_arg->lex->pop_context();
  DBUG_RETURN(0);
}




bool
push_new_name_resolution_context(THD *thd,
                                 TABLE_LIST *left_op, TABLE_LIST *right_op)
{
  Name_resolution_context *on_context;
  if (!(on_context= new (thd->mem_root) Name_resolution_context))
    return TRUE;
  on_context->init();
  on_context->first_name_resolution_table=
    left_op->first_leaf_for_name_resolution();
  on_context->last_name_resolution_table=
    right_op->last_leaf_for_name_resolution();
  on_context->select_lex= thd->lex->current_select;
  
  DBUG_ASSERT(right_op->context_of_embedding == NULL);
  right_op->context_of_embedding= on_context;
  return thd->lex->push_context(on_context);
}




void add_join_on(TABLE_LIST *b, Item *expr)
{
  if (expr)
  {
    if (!b->join_cond())
      b->set_join_cond(expr);
    else
    {
      
      b->set_join_cond(new Item_cond_and(b->join_cond(), expr));
    }
    b->join_cond()->top_level_item();
  }
}




void add_join_natural(TABLE_LIST *a, TABLE_LIST *b, List<String> *using_fields,
                      SELECT_LEX *lex)
{
  b->natural_join= a;
  lex->prev_join_using= using_fields;
}




uint kill_one_thread(THD *thd, ulong id, bool only_kill_query)
{
  THD *tmp= NULL;
  uint error=ER_NO_SUCH_THREAD;
  DBUG_ENTER("kill_one_thread");
  DBUG_PRINT("enter", ("id=%lu only_kill=%d", id, only_kill_query));

  mysql_mutex_lock(&LOCK_thread_count);
  Thread_iterator it= global_thread_list_begin();
  Thread_iterator end= global_thread_list_end();
  for (; it != end; ++it)
  {
    if ((*it)->get_command() == COM_DAEMON)
      continue;
    if ((*it)->thread_id == id)
    {
      tmp= *it;
      mysql_mutex_lock(&tmp->LOCK_thd_data);    
      break;
    }
  }
  mysql_mutex_unlock(&LOCK_thread_count);
  if (tmp)
  {

    

    if ((thd->security_ctx->master_access & SUPER_ACL) ||
        thd->security_ctx->user_matches(tmp->security_ctx))
    {
      
      if (tmp->killed != THD::KILL_CONNECTION)
      {
        tmp->awake(only_kill_query ? THD::KILL_QUERY : THD::KILL_CONNECTION);
      }
      error= 0;
    }
    else
      error=ER_KILL_DENIED_ERROR;
    mysql_mutex_unlock(&tmp->LOCK_thd_data);
  }
  DBUG_PRINT("exit", ("%d", error));
  DBUG_RETURN(error);
}




static
void sql_kill(THD *thd, ulong id, bool only_kill_query)
{
  uint error;
  if (!(error= kill_one_thread(thd, id, only_kill_query)))
  {
    if (! thd->killed)
      my_ok(thd);
  }
  else
    my_error(error, MYF(0), id);
}




bool append_file_to_dir(THD *thd, const char **filename_ptr,
                        const char *table_name)
{
  char buff[FN_REFLEN],*ptr, *end;
  if (!*filename_ptr)
    return 0;					

  
  if (strlen(*filename_ptr)+strlen(table_name) >= FN_REFLEN-1 ||
      !test_if_hard_path(*filename_ptr))
  {
    my_error(ER_WRONG_TABLE_NAME, MYF(0), *filename_ptr);
    return 1;
  }
  
  strmov(buff,*filename_ptr);
  end=convert_dirname(buff, *filename_ptr, NullS);
  if (!(ptr= (char*) thd->alloc((size_t) (end-buff) + strlen(table_name)+1)))
    return 1;					
  *filename_ptr=ptr;
  strxmov(ptr,buff,table_name,NullS);
  return 0;
}




bool check_simple_select()
{
  THD *thd= current_thd;
  LEX *lex= thd->lex;
  if (lex->current_select != &lex->select_lex)
  {
    char command[80];
    Lex_input_stream *lip= & thd->m_parser_state->m_lip;
    strmake(command, lip->yylval->symbol.str,
	    min<size_t>(lip->yylval->symbol.length, sizeof(command)-1));
    my_error(ER_CANT_USE_OPTION_HERE, MYF(0), command);
    return 1;
  }
  return 0;
}


Comp_creator *comp_eq_creator(bool invert)
{
  return invert?(Comp_creator *)&ne_creator:(Comp_creator *)&eq_creator;
}


Comp_creator *comp_ge_creator(bool invert)
{
  return invert?(Comp_creator *)&lt_creator:(Comp_creator *)&ge_creator;
}


Comp_creator *comp_gt_creator(bool invert)
{
  return invert?(Comp_creator *)&le_creator:(Comp_creator *)&gt_creator;
}


Comp_creator *comp_le_creator(bool invert)
{
  return invert?(Comp_creator *)&gt_creator:(Comp_creator *)&le_creator;
}


Comp_creator *comp_lt_creator(bool invert)
{
  return invert?(Comp_creator *)&ge_creator:(Comp_creator *)&lt_creator;
}


Comp_creator *comp_ne_creator(bool invert)
{
  return invert?(Comp_creator *)&eq_creator:(Comp_creator *)&ne_creator;
}



Item * all_any_subquery_creator(Item *left_expr,
				chooser_compare_func_creator cmp,
				bool all,
				SELECT_LEX *select_lex)
{
  if ((cmp == &comp_eq_creator) && !all)       
    return new Item_in_subselect(left_expr, select_lex);

  if ((cmp == &comp_ne_creator) && all)        
    return new Item_func_not(new Item_in_subselect(left_expr, select_lex));

  Item_allany_subselect *it=
    new Item_allany_subselect(left_expr, cmp, select_lex, all);
  if (all)
    return it->upper_item= new Item_func_not_all(it);	

  return it->upper_item= new Item_func_nop_all(it);      
}




bool select_precheck(THD *thd, LEX *lex, TABLE_LIST *tables,
                     TABLE_LIST *first_table)
{
  bool res;
  
  ulong privileges_requested= lex->exchange ? SELECT_ACL | FILE_ACL :
                                              SELECT_ACL;

  if (tables)
  {
    res= check_table_access(thd,
                            privileges_requested,
                            tables, FALSE, UINT_MAX, FALSE) ||
         (first_table && first_table->schema_table_reformed &&
          check_show_access(thd, first_table));
  }
  else
    res= check_access(thd, privileges_requested, any_db, NULL, NULL, 0, 0);

  return res;
}




bool multi_update_precheck(THD *thd, TABLE_LIST *tables)
{
  const char *msg= 0;
  TABLE_LIST *table;
  LEX *lex= thd->lex;
  SELECT_LEX *select_lex= &lex->select_lex;
  DBUG_ENTER("multi_update_precheck");

  if (select_lex->item_list.elements != lex->value_list.elements)
  {
    my_message(ER_WRONG_VALUE_COUNT, ER(ER_WRONG_VALUE_COUNT), MYF(0));
    DBUG_RETURN(TRUE);
  }
  
  for (table= tables; table; table= table->next_local)
  {
    if (table->derived)
      table->grant.privilege= SELECT_ACL;
    else if ((check_access(thd, UPDATE_ACL, table->db,
                           &table->grant.privilege,
                           &table->grant.m_internal,
                           0, 1) ||
              check_grant(thd, UPDATE_ACL, table, FALSE, 1, TRUE)) &&
             (check_access(thd, SELECT_ACL, table->db,
                           &table->grant.privilege,
                           &table->grant.m_internal,
                           0, 0) ||
              check_grant(thd, SELECT_ACL, table, FALSE, 1, FALSE)))
      DBUG_RETURN(TRUE);

    table->table_in_first_from_clause= 1;
  }
  
  if (&lex->select_lex != lex->all_selects_list)
  {
    DBUG_PRINT("info",("Checking sub query list"));
    for (table= tables; table; table= table->next_global)
    {
      if (!table->table_in_first_from_clause)
      {
	if (check_access(thd, SELECT_ACL, table->db,
                         &table->grant.privilege,
                         &table->grant.m_internal,
                         0, 0) ||
	    check_grant(thd, SELECT_ACL, table, FALSE, 1, FALSE))
	  DBUG_RETURN(TRUE);
      }
    }
  }

  if (select_lex->order_list.elements)
    msg= "ORDER BY";
  else if (select_lex->select_limit)
    msg= "LIMIT";
  if (msg)
  {
    my_error(ER_WRONG_USAGE, MYF(0), "UPDATE", msg);
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}



bool multi_delete_precheck(THD *thd, TABLE_LIST *tables)
{
  SELECT_LEX *select_lex= &thd->lex->select_lex;
  TABLE_LIST *aux_tables= thd->lex->auxiliary_table_list.first;
  TABLE_LIST **save_query_tables_own_last= thd->lex->query_tables_own_last;
  DBUG_ENTER("multi_delete_precheck");

  
  for (TABLE_LIST *tl= aux_tables; tl; tl= tl->next_global)
  {
    if (tl->table)
      continue;

    if (tl->correspondent_table)
      tl->table= tl->correspondent_table->table;
  }

  
  DBUG_ASSERT(aux_tables != 0);
  if (check_table_access(thd, SELECT_ACL, tables, FALSE, UINT_MAX, FALSE))
    DBUG_RETURN(TRUE);

  
  thd->lex->query_tables_own_last= 0;
  if (check_table_access(thd, DELETE_ACL, aux_tables, FALSE, UINT_MAX, FALSE))
  {
    thd->lex->query_tables_own_last= save_query_tables_own_last;
    DBUG_RETURN(TRUE);
  }
  thd->lex->query_tables_own_last= save_query_tables_own_last;

  if ((thd->variables.option_bits & OPTION_SAFE_UPDATES) && !select_lex->where)
  {
    my_message(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE,
               ER(ER_UPDATE_WITHOUT_KEY_IN_SAFE_MODE), MYF(0));
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}




static TABLE_LIST *multi_delete_table_match(LEX *lex, TABLE_LIST *tbl,
                                            TABLE_LIST *tables)
{
  TABLE_LIST *match= NULL;
  DBUG_ENTER("multi_delete_table_match");

  for (TABLE_LIST *elem= tables; elem; elem= elem->next_local)
  {
    int cmp;

    if (tbl->is_fqtn && elem->is_alias)
      continue; 
    if (tbl->is_fqtn && elem->is_fqtn)
      cmp= my_strcasecmp(table_alias_charset, tbl->table_name, elem->table_name) ||
           strcmp(tbl->db, elem->db);
    else if (elem->is_alias)
      cmp= my_strcasecmp(table_alias_charset, tbl->alias, elem->alias);
    else
      cmp= my_strcasecmp(table_alias_charset, tbl->table_name, elem->table_name) ||
           strcmp(tbl->db, elem->db);

    if (cmp)
      continue;

    if (match)
    {
      my_error(ER_NONUNIQ_TABLE, MYF(0), elem->alias);
      DBUG_RETURN(NULL);
    }

    match= elem;
  }

  if (!match)
    my_error(ER_UNKNOWN_TABLE, MYF(0), tbl->table_name, "MULTI DELETE");

  DBUG_RETURN(match);
}




bool multi_delete_set_locks_and_link_aux_tables(LEX *lex)
{
  TABLE_LIST *tables= lex->select_lex.table_list.first;
  TABLE_LIST *target_tbl;
  DBUG_ENTER("multi_delete_set_locks_and_link_aux_tables");

  for (target_tbl= lex->auxiliary_table_list.first;
       target_tbl; target_tbl= target_tbl->next_local)
  {
    
    TABLE_LIST *walk= multi_delete_table_match(lex, target_tbl, tables);
    if (!walk)
      DBUG_RETURN(TRUE);
    if (!walk->derived)
    {
      target_tbl->table_name= walk->table_name;
      target_tbl->table_name_length= walk->table_name_length;
    }
    walk->updating= target_tbl->updating;
    walk->lock_type= target_tbl->lock_type;
    
    DBUG_ASSERT(walk->lock_type >= TL_WRITE_ALLOW_WRITE);
    walk->mdl_request.set_type(MDL_SHARED_WRITE);
    target_tbl->correspondent_table= walk;	
  }
  DBUG_RETURN(FALSE);
}




bool update_precheck(THD *thd, TABLE_LIST *tables)
{
  DBUG_ENTER("update_precheck");
  if (thd->lex->select_lex.item_list.elements != thd->lex->value_list.elements)
  {
    my_message(ER_WRONG_VALUE_COUNT, ER(ER_WRONG_VALUE_COUNT), MYF(0));
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(check_one_table_access(thd, UPDATE_ACL, tables));
}




bool delete_precheck(THD *thd, TABLE_LIST *tables)
{
  DBUG_ENTER("delete_precheck");
  if (check_one_table_access(thd, DELETE_ACL, tables))
    DBUG_RETURN(TRUE);
  
  tables->grant.want_privilege=(SELECT_ACL & ~tables->grant.privilege);
  DBUG_RETURN(FALSE);
}




bool insert_precheck(THD *thd, TABLE_LIST *tables)
{
  LEX *lex= thd->lex;
  DBUG_ENTER("insert_precheck");

  
  ulong privilege= (INSERT_ACL |
                    (lex->duplicates == DUP_REPLACE ? DELETE_ACL : 0) |
                    (lex->value_list.elements ? UPDATE_ACL : 0));

  if (check_one_table_access(thd, privilege, tables))
    DBUG_RETURN(TRUE);

  if (lex->update_list.elements != lex->value_list.elements)
  {
    my_message(ER_WRONG_VALUE_COUNT, ER(ER_WRONG_VALUE_COUNT), MYF(0));
    DBUG_RETURN(TRUE);
  }
  DBUG_RETURN(FALSE);
}




void create_table_set_open_action_and_adjust_tables(LEX *lex)
{
  TABLE_LIST *create_table= lex->query_tables;

  if (lex->create_info.options & HA_LEX_CREATE_TMP_TABLE)
    create_table->open_type= OT_TEMPORARY_ONLY;
  else
    create_table->open_type= OT_BASE_ONLY;

  if (!lex->select_lex.item_list.elements)
  {
    
    create_table->lock_type= TL_READ;
  }
}




bool create_table_precheck(THD *thd, TABLE_LIST *tables,
                           TABLE_LIST *create_table)
{
  LEX *lex= thd->lex;
  SELECT_LEX *select_lex= &lex->select_lex;
  ulong want_priv;
  bool error= TRUE;                                 
  DBUG_ENTER("create_table_precheck");

  

  want_priv= (lex->create_info.options & HA_LEX_CREATE_TMP_TABLE) ?
             CREATE_TMP_ACL :
             (CREATE_ACL | (select_lex->item_list.elements ? INSERT_ACL : 0));

  if (check_access(thd, want_priv, create_table->db,
                   &create_table->grant.privilege,
                   &create_table->grant.m_internal,
                   0, 0))
    goto err;

  
  if (lex->create_info.merge_list.first)
  {
    

    if (check_table_access(thd, SELECT_ACL | UPDATE_ACL | DELETE_ACL,
                           lex->create_info.merge_list.first,
                           FALSE, UINT_MAX, FALSE))
      goto err;
  }

  if (want_priv != CREATE_TMP_ACL &&
      check_grant(thd, want_priv, create_table, FALSE, 1, FALSE))
    goto err;

  if (select_lex->item_list.elements)
  {
    
    if (tables && check_table_access(thd, SELECT_ACL, tables, FALSE,
                                     UINT_MAX, FALSE))
      goto err;
  }
  else if (lex->create_info.options & HA_LEX_CREATE_TABLE_LIKE)
  {
    if (check_table_access(thd, SELECT_ACL, tables, FALSE, UINT_MAX, FALSE))
      goto err;
  }

  if (check_fk_parent_table_access(thd, &lex->create_info, &lex->alter_info))
    goto err;

  error= FALSE;
err:
  DBUG_RETURN(error);
}




static bool lock_tables_precheck(THD *thd, TABLE_LIST *tables)
{
  TABLE_LIST *first_not_own_table= thd->lex->first_not_own_table();

  for (TABLE_LIST *table= tables; table != first_not_own_table && table;
       table= table->next_global)
  {
    if (is_temporary_table(table))
      continue;

    if (check_table_access(thd, LOCK_TABLES_ACL | SELECT_ACL, table,
                           FALSE, 1, FALSE))
      return TRUE;
  }

  return FALSE;
}




Item *negate_expression(THD *thd, Item *expr)
{
  Item *negated;
  if (expr->type() == Item::FUNC_ITEM &&
      ((Item_func *) expr)->functype() == Item_func::NOT_FUNC)
  {
    
    Item *arg= ((Item_func *) expr)->arguments()[0];
    enum_parsing_place place= thd->lex->current_select->parsing_place;
    if (arg->is_bool_func() || place == IN_WHERE || place == IN_HAVING)
      return arg;
    
    return new Item_func_ne(arg, new Item_int_0());
  }

  if ((negated= expr->neg_transformer(thd)) != 0)
    return negated;
  return new Item_func_not(expr);
}


 
void get_default_definer(THD *thd, LEX_USER *definer)
{
  const Security_context *sctx= thd->security_ctx;

  definer->user.str= (char *) sctx->priv_user;
  definer->user.length= strlen(definer->user.str);

  definer->host.str= (char *) sctx->priv_host;
  definer->host.length= strlen(definer->host.str);

  definer->password= null_lex_str;
  definer->plugin= empty_lex_str;
  definer->auth= empty_lex_str;
  definer->uses_identified_with_clause= false;
  definer->uses_identified_by_clause= false;
  definer->uses_authentication_string_clause= false;
  definer->uses_identified_by_password_clause= false;
}




LEX_USER *create_default_definer(THD *thd)
{
  LEX_USER *definer;

  if (! (definer= (LEX_USER*) thd->alloc(sizeof(LEX_USER))))
    return 0;

  thd->get_definer(definer);

  return definer;
}




LEX_USER *create_definer(THD *thd, LEX_STRING *user_name, LEX_STRING *host_name)
{
  LEX_USER *definer;

  

  if (! (definer= (LEX_USER*) thd->alloc(sizeof(LEX_USER))))
    return 0;

  definer->user= *user_name;
  definer->host= *host_name;
  definer->password.str= NULL;
  definer->password.length= 0;
  definer->uses_authentication_string_clause= false;
  definer->uses_identified_by_clause= false;
  definer->uses_identified_by_password_clause= false;
  definer->uses_identified_with_clause= false;
  return definer;
}




LEX_USER *get_current_user(THD *thd, LEX_USER *user)
{
  if (!user->user.str)  
  {
    LEX_USER *default_definer= create_default_definer(thd);
    if (default_definer)
    {
      
      default_definer->uses_authentication_string_clause=
        user->uses_authentication_string_clause;
      default_definer->uses_identified_by_clause=
        user->uses_identified_by_clause;
      default_definer->uses_identified_by_password_clause=
        user->uses_identified_by_password_clause;
      default_definer->uses_identified_with_clause=
        user->uses_identified_with_clause;
      default_definer->plugin.str= user->plugin.str;
      default_definer->plugin.length= user->plugin.length;
      default_definer->auth.str= user->auth.str;
      default_definer->auth.length= user->auth.length;
      return default_definer;
    }
  }

  return user;
}




bool check_string_byte_length(LEX_STRING *str, const char *err_msg,
                              uint max_byte_length)
{
  if (str->length <= max_byte_length)
    return FALSE;

  my_error(ER_WRONG_STRING_LENGTH, MYF(0), str->str, err_msg, max_byte_length);

  return TRUE;
}





bool check_string_char_length(LEX_STRING *str, const char *err_msg,
                              uint max_char_length, const CHARSET_INFO *cs,
                              bool no_error)
{
  int well_formed_error;
  uint res= cs->cset->well_formed_len(cs, str->str, str->str + str->length,
                                      max_char_length, &well_formed_error);

  if (!well_formed_error &&  str->length == res)
    return FALSE;

  if (!no_error)
  {
    ErrConvString err(str->str, str->length, cs);
    my_error(ER_WRONG_STRING_LENGTH, MYF(0), err.ptr(), err_msg, max_char_length);
  }
  return TRUE;
}



C_MODE_START

int test_if_data_home_dir(const char *dir)
{
  char path[FN_REFLEN];
  int dir_len;
  DBUG_ENTER("test_if_data_home_dir");

  if (!dir)
    DBUG_RETURN(0);

  (void) fn_format(path, dir, "", "",
                   (MY_RETURN_REAL_PATH|MY_RESOLVE_SYMLINKS));
  dir_len= strlen(path);
  if (mysql_unpacked_real_data_home_len<= dir_len)
  {
    if (dir_len > mysql_unpacked_real_data_home_len &&
        path[mysql_unpacked_real_data_home_len] != FN_LIBCHAR)
      DBUG_RETURN(0);

    if (lower_case_file_system)
    {
      if (!my_strnncoll(default_charset_info, (const uchar*) path,
                        mysql_unpacked_real_data_home_len,
                        (const uchar*) mysql_unpacked_real_data_home,
                        mysql_unpacked_real_data_home_len))
        DBUG_RETURN(1);
    }
    else if (!memcmp(path, mysql_unpacked_real_data_home,
                     mysql_unpacked_real_data_home_len))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}

C_MODE_END




bool check_host_name(LEX_STRING *str)
{
  const char *name= str->str;
  const char *end= str->str + str->length;
  if (check_string_byte_length(str, ER(ER_HOSTNAME), HOSTNAME_LENGTH))
    return TRUE;

  while (name != end)
  {
    if (*name == '@')
    {
      my_printf_error(ER_UNKNOWN_ERROR, 
                      "Malformed hostname (illegal symbol: '%c')", MYF(0),
                      *name);
      return TRUE;
    }
    name++;
  }
  return FALSE;
}


extern int MYSQLparse(class THD *thd); 




bool parse_sql(THD *thd,
               Parser_state *parser_state,
               Object_creation_ctx *creation_ctx)
{
  bool ret_value;
  DBUG_ASSERT(thd->m_parser_state == NULL);
  DBUG_ASSERT(thd->lex->m_sql_cmd == NULL);

  MYSQL_QUERY_PARSE_START(thd->query());
  

  Object_creation_ctx *backup_ctx= NULL;

  if (creation_ctx)
    backup_ctx= creation_ctx->set_n_backup(thd);

  

  thd->m_parser_state= parser_state;

#ifdef HAVE_PSI_STATEMENT_DIGEST_INTERFACE
  
  thd->m_parser_state->m_lip.m_digest_psi= MYSQL_DIGEST_START(thd->m_statement_psi);
#endif

  

  bool mysql_parse_status= MYSQLparse(thd) != 0;

  

  DBUG_ASSERT(!mysql_parse_status ||
              (mysql_parse_status && thd->is_error()) ||
              (mysql_parse_status && thd->get_internal_handler()));

  

  thd->m_parser_state= NULL;

  

  if (creation_ctx)
    creation_ctx->restore_env(thd, backup_ctx);

  

  ret_value= mysql_parse_status || thd->is_fatal_error;
  MYSQL_QUERY_PARSE_DONE(ret_value);
  return ret_value;
}








const CHARSET_INFO*
merge_charset_and_collation(const CHARSET_INFO *cs, const CHARSET_INFO *cl)
{
  if (cl)
  {
    if (!my_charset_same(cs, cl))
    {
      my_error(ER_COLLATION_CHARSET_MISMATCH, MYF(0), cl->name, cs->csname);
      return NULL;
    }
    return cl;
  }
  return cs;
}
