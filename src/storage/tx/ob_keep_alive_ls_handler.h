/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#ifndef OCEANBASE_TRANSACTION_KEEP_ALIVE_LS_HANDLER
#define OCEANBASE_TRANSACTION_KEEP_ALIVE_LS_HANDLER

#include "logservice/palf/palf_callback.h"
#include "logservice/ob_log_base_header.h"
#include "logservice/ob_log_base_type.h"
#include "logservice/ob_append_callback.h"
#include "share/ob_ls_id.h"

namespace oceanbase
{

namespace logservice
{
class ObLogHandler;
}

namespace transaction
{

enum class MinStartScnStatus
{
  UNKOWN = 0, // collect failed
  NO_CTX,
  HAS_CTX,

  MAX
};

class ObKeepAliveLogBody
{
public:
  OB_UNIS_VERSION(1);

public:
  ObKeepAliveLogBody()
      : compat_bit_(1), min_start_scn_(OB_INVALID_TIMESTAMP),
        min_start_status_(MinStartScnStatus::UNKOWN)
  {}
  ObKeepAliveLogBody(int64_t compat_bit, int64_t min_start_scn, MinStartScnStatus min_status)
      : compat_bit_(compat_bit), min_start_scn_(min_start_scn), min_start_status_(min_status)
  {}

  static int64_t get_max_serialize_size();
  int64_t get_min_start_scn() { return min_start_scn_; };
  MinStartScnStatus get_min_start_status() { return min_start_status_; }

  TO_STRING_KV(K_(compat_bit), K_(min_start_scn), K_(min_start_status));

private:
  int64_t compat_bit_; // not used, only for compatibility
  int64_t min_start_scn_;
  MinStartScnStatus min_start_status_;
};

struct KeepAliveLsInfo
{
  int64_t log_ts_;
  palf::LSN lsn_;
  int64_t min_start_scn_;
  MinStartScnStatus min_start_status_;

  void reset()
  {
    log_ts_ = OB_INVALID_TIMESTAMP;
    lsn_.reset();
    min_start_scn_ = OB_INVALID_TIMESTAMP;
    min_start_status_ = MinStartScnStatus::UNKOWN;
  }

  TO_STRING_KV(K(log_ts_), K(lsn_), K(min_start_scn_), K(min_start_status_));
};

class ObLSKeepAliveStatInfo
{
public:
  ObLSKeepAliveStatInfo() { reset(); }
  void reset()
  {
    cb_busy_cnt = 0;
    not_master_cnt = 0;
    near_to_gts_cnt = 0;
    other_error_cnt = 0;
    submit_succ_cnt = 0;
    stat_keepalive_info_.reset();
  }

  void clear_cnt()
  {
    cb_busy_cnt = 0;
    not_master_cnt = 0;
    near_to_gts_cnt = 0;
    other_error_cnt = 0;
    submit_succ_cnt = 0;
  }

  int64_t cb_busy_cnt;
  int64_t not_master_cnt;
  int64_t near_to_gts_cnt;
  int64_t other_error_cnt;
  int64_t submit_succ_cnt;
  KeepAliveLsInfo stat_keepalive_info_;

private:
  // none
};

// after init , we should register in logservice
class ObKeepAliveLSHandler : public logservice::ObIReplaySubHandler,
                             public logservice::ObICheckpointSubHandler,
                             public logservice::ObIRoleChangeSubHandler,
                             public logservice::AppendCb 
{
public:
  const int64_t KEEP_ALIVE_GTS_INTERVAL_NS = 100 * 1000 * 1000; 
public:
  ObKeepAliveLSHandler() : submit_buf_(nullptr) { reset(); }
  int init(const share::ObLSID &ls_id,logservice::ObLogHandler * log_handler_ptr);

  void stop();
  // false - can not safe destroy
  bool check_safe_destory();
  void destroy();

  void reset();
  
  int try_submit_log(int64_t min_start_scn, MinStartScnStatus status);
  void print_stat_info();
public:

  bool is_busy() { return ATOMIC_LOAD(&is_busy_); }
  int on_success();
  int on_failure();

  int replay(const void *buffer, const int64_t nbytes, const palf::LSN &lsn, const int64_t ts_ns);
  void switch_to_follower_forcedly()
  {
   ATOMIC_STORE(&is_master_, false); 
  }
  int switch_to_leader() { ATOMIC_STORE(&is_master_,true); return OB_SUCCESS;}
  int switch_to_follower_gracefully() { ATOMIC_STORE(&is_master_,false); return OB_SUCCESS;}
  int resume_leader() { ATOMIC_STORE(&is_master_,true);return OB_SUCCESS; }
  int64_t get_rec_log_ts() { return INT64_MAX; }
  int flush(int64_t rec_log_ts) { return OB_SUCCESS;}

  void get_min_start_scn(int64_t &min_start_scn, int64_t &keep_alive_scn, MinStartScnStatus &status);
private:
  bool check_gts_();
  int serialize_keep_alive_log_(int64_t min_start_scn, MinStartScnStatus status);
private : 
  SpinRWLock lock_;

  bool is_busy_;
  bool is_master_;
  bool is_stopped_;

  share::ObLSID ls_id_;
  logservice::ObLogHandler * log_handler_ptr_;

  char *submit_buf_;
  int64_t submit_buf_len_;
  int64_t submit_buf_pos_;

  int64_t last_gts_;

  KeepAliveLsInfo tmp_keep_alive_info_;
  KeepAliveLsInfo durable_keep_alive_info_;

  ObLSKeepAliveStatInfo stat_info_;
};

}
}
#endif
