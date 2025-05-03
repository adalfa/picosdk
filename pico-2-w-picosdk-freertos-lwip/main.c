// Copyright (c) 2024 Cesanta Software Limited
// All rights reserved

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "FreeRTOS.h"
#include <task.h>
#include <semphr.h>

#include "mongoose.h"
#include "net.h"

#define WIFI_SSID ""
#define WIFI_PASS ""

static const char *s_url =
    "mqtts://evgn-adf-mcu.italynorth-1.ts.eventgrid.azure.net";
static const char *s_rx_topic = "test/test2";
static const char *s_tx_topic = "test/test1";
static const char *deviceid="rp2350_1";
static int s_qos = 0;
static struct mg_connection *s_sntp_conn = NULL;
static time_t s_boot_timestamp = 0;




static void sfn(struct mg_connection *c, int ev, void *ev_data) {
  if (ev == MG_EV_SNTP_TIME) {
    // Time received, the internal protocol handler updates what mg_now() returns
    uint64_t curtime = mg_now();
    MG_INFO(("SNTP-updated current time is: %llu ms from epoch", curtime));
    
    {
      uint64_t t = *(uint64_t *) ev_data;
      time_t tt = (time_t) ((t - mg_millis()) / 1000);
      //s_boot_timestamp=tt;
      MG_INFO(("Got SNTP time: %llu ms from epoch, and set  %llu ", t,s_boot_timestamp));
      struct timeval tv;
               tv.tv_sec=tt;
               tv.tv_usec=0;
              settimeofday(&tv,NULL);
    }
    
  } else if (ev == MG_EV_CLOSE) {
    s_sntp_conn = NULL;
  }
 // xSemaphoreGive(bin_time);
  (void) c;
}

static void timer_sntp_fn(void *param) {  // SNTP timer function. Sync up time
  //xSemaphoreTake(bin_time,portMAX_DELAY);
  mg_sntp_connect(param, "udp://time.google.com:123", sfn, NULL);
  mg_sntp_request(s_sntp_conn);
  
}
static char* getCurrentLocalTimeString()
{
  time_t now = time(NULL);
  
  return ctime(&now);
}

static char* getCurrentLocalTimeStringMG()
{
  time_t now = mg_now();
  
  return ctime(&now);
}

static void printCurrentTime()
{
  MG_INFO(("Current time system: %s\b",getCurrentLocalTimeString()));
  MG_INFO(("Current time MG: %s\n",getCurrentLocalTimeStringMG()));
  
}


static void fn(struct mg_connection *c, int ev, void *ev_data);

static void mongooseTime(void *args) {
  struct mg_mgr mgrt; 
  bool done = false;
  vTaskDelay(pdMS_TO_TICKS(500));
  //xSemaphoreTake(bin_conn,portMAX_DELAY);
  mg_timer_add(&mgrt, 3600 * 1000, MG_TIMER_RUN_NOW | MG_TIMER_REPEAT,timer_sntp_fn, &mgrt);
  
  printCurrentTime();
  while (!done) mg_mgr_poll(&mgrt, 1000);

}
static void mongoose(void *args) {
  //xSemaphoreTake(bin_conn,portMAX_DELAY);
  struct mg_mgr mgr;        // Initialise Mongoose event manager
  mg_mgr_init(&mgr);        // and attach it to the interface
  mg_log_set(MG_LL_VERBOSE);  // Set log level
  
  cyw43_arch_init();
  cyw43_arch_enable_sta_mode();
  cyw43_arch_wifi_connect_blocking(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK);
  //xSemaphoreGive(bin_conn);
  //xSemaphoreTake(bin_time,portMAX_DELAY);
  MG_INFO(("Initialising application sntp"));
  mg_timer_add(&mgr, 3600 * 1000, MG_TIMER_RUN_NOW | MG_TIMER_REPEAT,timer_sntp_fn, &mgr);
  
  printCurrentTime();

  time_t now = time(NULL);
  while (now < 1510592825)
  { mg_mgr_poll(&mgr, 1000);
  now =  time(NULL);
  printCurrentTime();
  }
  MG_INFO(("Initialising application..."));
  //for Azure seehttps://learn.microsoft.com/en-us/azure/event-grid/mqtt-client-certificate-authentication#self-signed-client-certificate---thumbprint
  struct mg_mqtt_opts opts = {.clean = true,
                              .user=(char *)deviceid,
                              .client_id=(char *)deviceid,
                             .version= 5
                              
                              //.pass=NULL,
                              //.topic=(char *)s_tx_topic
                              
                              };
  
  bool done = false;
  MG_INFO(("Connecting to %s", mg_url_host(s_url)));


  vTaskDelay(pdMS_TO_TICKS(2000));

  printCurrentTime();
  //web_init(&mgr);
   
  struct mg_connection *conn;          // Inform that we're starting
  conn = mg_mqtt_connect(&mgr, s_url, &opts, fn, &done);
  MG_INFO(("Accettata?  %d", conn->is_accepted)); 
  //mg_http_connect(&mgr, sh_url, fn2, NULL); 
  while (!done) mg_mgr_poll(&mgr, 1000);           // Loop until done
  mg_mgr_free(&mgr);      

  (void) args;
}


#include "mongoose.h"

static void fn(struct mg_connection *c, int ev, void *ev_data) {
  MG_INFO(("EV: %d",ev));
  if (ev == MG_EV_OPEN) {
    MG_INFO(("EV_OPEN"));
     c->is_hexdumping = 1;
  } else if (ev == MG_EV_CONNECT) {
    if (mg_url_is_ssl(s_url)) {
      struct mg_tls_opts opts = { .ca = mg_unpacked("/DigiCertGlobalRootG3.crt.pem"),
                                 .cert = mg_unpacked("/65DF206050F998666C9DF4FAE3E6390B.pem"),
                                 .key = mg_unpacked("/rp2350_1.key"),
                                 
                                 //.skip_verification=1,
                                 .name =mg_url_host(s_url)
                                };

                                 MG_INFO(("TLS_INIT start"));
                                 mg_tls_init(c, &opts);
                                 MG_INFO(("TLS_INIT done"));
    }
  } else if (ev == MG_EV_ERROR) {
    // On error, log error message
    MG_ERROR(("%p %s", c->fd, (char *) ev_data));
  } else if (ev == MG_EV_MQTT_OPEN) {
    // MQTT connect is successful
    struct mg_str topic = mg_str(s_rx_topic);
    MG_INFO(("Connected to %s", s_url));
    
    MG_INFO(("Subscribing to %s", s_rx_topic));
    struct mg_mqtt_opts sub_opts;
    memset(&sub_opts, 0, sizeof(sub_opts));
    sub_opts.topic = topic;
    sub_opts.qos = s_qos;
    mg_mqtt_sub(c, &sub_opts);
    
    c->data[0] = 'X';  // Set a label that we're logged in
  } else if (ev == MG_EV_MQTT_MSG) {
    // When we receive MQTT message, print it
    struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
    MG_INFO(("Received on %.*s : %.*s", (int) mm->topic.len, mm->topic.buf,
             (int) mm->data.len, mm->data.buf));
   } else if (ev == MG_EV_POLL  && c->data[0] == 'X') { 
  //else if (ev == MG_EV_POLL && c->data[0] == 'X') {
    static unsigned long prev_second;
    unsigned long now_second = (*(unsigned long *) ev_data) / 1000;
    MG_INFO(("EV_POLL"));
    if (now_second != prev_second) {
      struct mg_str topic = mg_str(s_tx_topic), data = mg_str("{\"a\":123}");
      MG_INFO(("Publishing to %s", s_tx_topic));
      struct mg_mqtt_opts pub_opts;
      memset(&pub_opts, 0, sizeof(pub_opts));
      pub_opts.topic = topic;
      pub_opts.message = data;
      pub_opts.qos = s_qos, pub_opts.retain = false;
      mg_mqtt_pub(c, &pub_opts);
      prev_second = now_second;
    }
  }

  if (ev == MG_EV_ERROR || ev == MG_EV_CLOSE) {
    MG_INFO(("Got event %d, stopping...", ev));
    *(bool *) c->fn_data = true;  // Signal that we're done
  }
}


int main(void) {
  // initialize stdio
  stdio_init_all(); 
  //bin_time = xSemaphoreCreateBinary(); 
  //bin_conn = xSemaphoreCreateBinary(); 
  MG_INFO(("Start"));
  
  
  xTaskCreate(mongoose, "mongoose", 2048, 0, configMAX_PRIORITIES - 1, NULL);
 
  //xTaskCreate(mongooseTime, "mongooseTime", 2048, 0, configMAX_PRIORITIES - 1, NULL);
  vTaskStartScheduler();  // This blocks

  return 0;
}

