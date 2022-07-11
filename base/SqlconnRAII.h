/*
 * @Author: Zhibin Yin
 * @Date:   2022-05-24
 * @copyleft Apache 2.0
 */ 
#pragma once
#include "Sqlconnpool.h"

class SqlConnRAII {
 public:
  SqlConnRAII(MYSQL **sql, SqlConnPool *connpool) {
    assert(connpool);
    *sql = connpool->GetConn();
    sql_ = *sql;
    connpool_ = connpool;
  }

  ~SqlConnRAII() {
    if (sql_) { connpool_->FreeConn(sql); }
  }
 private:
  MYSQL *sql_;
};