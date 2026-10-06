#include <assert.h>
#include "../../../firmware/components/app_services/racebox_service.c"
static int stopped, publish_calls, fail_on_call;
static uint32_t published_session_index[8], published_session_total[8];
static bool connected;
bool app_mqtt_is_connected(void){return connected;}
app_mqtt_status_t app_mqtt_status(void){return connected ? APP_MQTT_CONNECTED : APP_MQTT_FAILED;}
const char *app_mqtt_status_text(void){return connected ? "MQTT 已连接" : "MQTT 连接失败";}
int app_mqtt_start(void){return 0;}
void app_mqtt_stop(void){}
int app_mqtt_publish_confirmed(const char *t,const char *p,int timeout){
 (void)t;(void)p;(void)timeout;assert(!"RaceBox must publish RBX2 binary payloads");return 1;
}
int app_mqtt_publish_bytes_confirmed(const char *t,const void *payload,size_t len,int timeout){
 const uint8_t *p=payload;assert(len>=136);assert(memcmp(p,"RBX2",4)==0);assert(p[4]==2 && p[5]<=3);
 assert(u16(p+6)==136 && u16(p+8)==80);int count=u16(p+10);assert(len==136+(size_t)count*80);
 assert(u32(p+12)==(uint32_t)s_uploaded);
 bool sync_final=p[5]&1;bool session_final=p[5]&2;
 assert(sync_final==(s_download_done && s_uploaded+count==s_received));
 assert(u32(p+16)==(uint32_t)(sync_final?s_received:s_total));
 assert(session_final==(u32(p+96)>0 && u32(p+92)+(uint32_t)count==u32(p+96)));
 assert(u32(p+128)==crc32_ieee(p+136,len-136));assert(u32(p+132)==crc32_ieee(p,132));
 if(publish_calls<8){published_session_index[publish_calls]=u32(p+88);published_session_total[publish_calls]=u32(p+96);}
 publish_calls++;return fail_on_call<0 || publish_calls==fail_on_call;
}
void racebox_ble_init(void){}
void racebox_ble_start(void){}
void racebox_ble_stop(void){stopped++;}
void racebox_ble_set_conn_cb(racebox_ble_conn_cb_t cb){}
void racebox_ble_set_rx_cb(racebox_ble_rx_cb_t cb){}
void racebox_ble_set_disc_cb(racebox_ble_disc_cb_t cb){}
void racebox_ble_set_scan_done_cb(racebox_ble_scan_done_cb_t cb){}
void racebox_ble_set_filter(const char *p,const char *l){}
static uint8_t last_cmd[16];static int last_cmd_len;
int racebox_ble_send(const uint8_t *p,int n){memcpy(last_cmd,p,n);last_cmd_len=n;return 0;}
static void frame(uint8_t id,const uint8_t *body,int n) {
 uint8_t raw[100]={0xb5,0x62,0xff,id,n,0};memcpy(raw+6,body,n);
 uint8_t a=0,b=0;for(int i=2;i<6+n;i++){a+=raw[i];b+=a;}raw[6+n]=a;raw[7+n]=b;
 // Simulate fragmentation across BLE notifications.
 on_ble_rx(raw,3);on_ble_rx(raw+3,n+5);
}
int main(void){
 s_upload_tcb=(StaticTask_t*)1;s_upload_stack=(StackType_t*)1;
 racebox_service_day_tick(2026,9,25);racebox_service_trigger();s_state=RACEBOX_DOWNLOADING;
 uint8_t max[4]={3,0,0,0};frame(0x23,max,4);
 uint8_t record[80]={0};record[4]=0xea;record[5]=7;record[6]=9;record[7]=25;
 record[20]=3;record[23]=12;record[48]=0xe8;record[49]=3; // 1000 mm/s
 record[24]=1;record[28]=1; // 非零经纬度：坐标全 0 会被无定位过滤当作无效记录丢弃
 frame(0x01,record,80);assert(s_received==1);
 frame(0x21,record,80);assert(s_received==2);
 uint8_t copied[18*80];assert(copy_record_range(0,1,copied));
 racebox_record_t decoded;decode_record(copied,&decoded);
 assert(decoded.year==2026 && decoded.speed==1.0 && decoded.numberof_svs==12);
 uint8_t unrelated[2]={0xff,0x24};frame(0x02,unrelated,2);assert(!s_download_done);
 uint8_t ack[2]={0xff,0x23};frame(0x02,ack,2);
 assert(s_download_done && s_point_count==0 && !s_synced_today && s_uploaded==0 && stopped==1);
 connected=true;fail_on_call=0;upload_worker(NULL);
 assert(s_state==RACEBOX_DONE && s_point_count==2 && s_synced_today);
 frame(0x01,record,80);assert(s_received==2);
 racebox_service_day_tick(2026,9,25);assert(racebox_service_synced_today() && racebox_service_point_count()==2);
 racebox_service_day_tick(2026,9,26);assert(!racebox_service_synced_today() && racebox_service_point_count()==0);
 racebox_service_trigger();assert(s_received==0 && !s_download_done);
 publish_calls=0;
 max[0]=65;s_state=RACEBOX_DOWNLOADING;frame(0x23,max,4);
 for(int i=0;i<65;i++)frame(0x21,record,80);
 assert(s_chunk_count==2);assert(copy_record_range(47,18,copied));
 frame(0x02,ack,2);connected=true;fail_on_call=1;upload_worker(NULL);
 assert(s_uploaded==65 && s_state==RACEBOX_DONE && publish_calls==2);
 assert(s_cached_offset==64 && s_chunk_count==1 && s_point_count==65);
 // With erase enabled, only the erase ACK finalizes success and daily points accumulate.
 // ACK now triggers a status query to verify the memory is really empty.
 racebox_service_trigger();s_auto_erase=true;publish_calls=0;fail_on_call=0;
 max[0]=5;s_state=RACEBOX_DOWNLOADING;frame(0x23,max,4);
 for(int i=0;i<5;i++) frame(0x21,record,80);
 frame(0x02,ack,2);upload_worker(NULL);
 assert(s_state==RACEBOX_SCANNING && s_point_count==65);
 on_ble_conn(true);assert(s_state==RACEBOX_ERASING);
 uint8_t erase_ack[2]={0xff,0x24};frame(0x02,erase_ack,2);
 assert(s_state==RACEBOX_ERASING && last_cmd_len==(int)sizeof(CMD_STATUS) && memcmp(last_cmd,CMD_STATUS,sizeof(CMD_STATUS))==0);
 // Device does not support the status query (NACK FF 22): fall back to trusting the ACK.
 uint8_t status_nack[2]={0xff,0x22};frame(0x03,status_nack,2);
 assert(s_state==RACEBOX_DONE && s_point_count==5 && s_count_accum_mode);
 racebox_service_trigger();max[0]=7;s_state=RACEBOX_DOWNLOADING;frame(0x23,max,4);
 for(int i=0;i<7;i++) frame(0x21,record,80);
 frame(0x02,ack,2);upload_worker(NULL);
 on_ble_conn(true);frame(0x02,erase_ack,2);
 // Status reports the memory is really empty -> done.
 uint8_t status_empty[12]={1,0,0,0,0,0,0,0,0xe1,0x11,0,0};frame(0x22,status_empty,12);
 assert(s_state==RACEBOX_DONE && s_point_count==12);
 // Status still reports records: erase is re-sent, verified again, then honestly failed.
 racebox_service_trigger();s_state=RACEBOX_DOWNLOADING;publish_calls=0;fail_on_call=0;
 max[0]=3;frame(0x23,max,4);
 for(int i=0;i<3;i++) frame(0x21,record,80);
 frame(0x02,ack,2);upload_worker(NULL);
 on_ble_conn(true);frame(0x02,erase_ack,2);
 uint8_t status_stored[12]={1,0,0,0,100,0,0,0,0xe1,0x11,0,0};
 frame(0x22,status_stored,12);
 assert(s_state==RACEBOX_ERASING && last_cmd[3]==0x24);   // erase re-sent
 frame(0x02,erase_ack,2);frame(0x22,status_stored,12);
 assert(s_state==RACEBOX_ERASING && last_cmd[3]==0x24);   // second retry
 frame(0x02,erase_ack,2);frame(0x22,status_stored,12);
 assert(s_state==RACEBOX_FAILED && !strcmp(s_message,"上传完成，但设备清理未生效"));
 s_auto_erase=false;s_received=9;finish_success();assert(s_point_count==9 && !s_count_accum_mode);
 // 0x26 closes an exact session: two device sessions become two independent RBX2 messages.
 racebox_service_trigger();publish_calls=0;fail_on_call=0;connected=true;
 max[0]=4;max[1]=max[2]=max[3]=0;s_state=RACEBOX_DOWNLOADING;frame(0x23,max,4);
 frame(0x21,record,80);frame(0x21,record,80);frame(0x26,record,0);
 record[9]=1;frame(0x21,record,80);frame(0x21,record,80);frame(0x26,record,0);
 frame(0x02,ack,2);upload_worker(NULL);
 assert(publish_calls==2 && published_session_index[0]==0 && published_session_index[1]==1);
 assert(published_session_total[0]==2 && published_session_total[1]==2);
 // 无定位记录（fix_status<2 或经纬度全 0）不入库；整次同步只有无定位记录时按失败收口并提示。
 racebox_service_trigger();s_state=RACEBOX_DOWNLOADING;publish_calls=0;fail_on_call=0;connected=true;
 max[0]=3;max[1]=max[2]=max[3]=0;frame(0x23,max,4);
 uint8_t nofix[80];memcpy(nofix,record,80);nofix[20]=0; // fix_status=0：无定位
 frame(0x01,nofix,80);assert(s_received==0 && s_nofix==1);
 uint8_t zeropos[80];memcpy(zeropos,record,80);zeropos[24]=0;zeropos[28]=0; // 3D 修复但坐标全 0
 frame(0x01,zeropos,80);assert(s_received==0 && s_nofix==2);
 frame(0x02,ack,2);assert(s_download_done);
 upload_worker(NULL);
 assert(s_state==RACEBOX_FAILED && s_uploaded==0 && publish_calls==0);
 assert(!strcmp(s_message,"无位置数据") && s_sound==RB_SND_NO_DATA);
 racebox_service_trigger();s_state=RACEBOX_DOWNLOADING;
 max[0]=0x51;max[1]=0xc3;max[2]=0;max[3]=0;frame(0x23,max,4);
 assert(s_state==RACEBOX_DOWNLOADING && s_total==50001 && s_chunk_count==0);
 free_record_chunks();free_sessions();puts("RaceBox chunking/fragmentation/session/RBX2/ACK/count/decode/reboot/day rollover passed");
}
