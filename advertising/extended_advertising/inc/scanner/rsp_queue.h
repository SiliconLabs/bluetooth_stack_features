#ifndef _RSP_QUEUE_H_
#define _RSP_QUEUE_H_

#include "sl_bt_api.h"

typedef struct rsp{
  struct rsp *next, *prev;
  uint32_t counter;
  sl_bt_evt_scanner_extended_advertisement_report_t data;
}rsp_t;

typedef struct rsp_queue{
  uint8_t num;
  rsp_t *head;
}rsp_queue_t;

int __match(const sl_bt_evt_scanner_extended_advertisement_report_t *rsp, rsp_t *r);
void __copy(const sl_bt_evt_scanner_extended_advertisement_report_t *rsp, rsp_t *r);
void update_counter(rsp_t *r);
rsp_t *find_rsp(rsp_queue_t *rsp_queue, const sl_bt_evt_scanner_extended_advertisement_report_t *rsp);
void head_item(rsp_queue_t *rsp_queue, rsp_t *r);
void remove_item(rsp_queue_t *rsp_queue, rsp_t *r);
int insert_rsp(rsp_queue_t *rsp_queue, const sl_bt_evt_scanner_extended_advertisement_report_t *rsp);

#endif
