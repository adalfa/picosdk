// Copyright (c) 2024 Cesanta Software Limited
// All rights reserved

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "FreeRTOS.h"
#include <task.h>

#include "mongoose.h"
#include "net.h"

#define WIFI_SSID "XXXX-WiFi_XXXX"
#define WIFI_PASS "XXXXXX"

static const char *s_url =
    "mqtt://192.168.1.190";
static const char *s_rx_topic = "rw";
static const char *s_tx_topic = "rw";
static int s_qos = 1;




static void timer_sntp_fn(void *param) {  // SNTP timer function. Sync up time
  mg_sntp_connect(param, "udp://time.google.com:123", NULL, NULL);
}


static void fn(struct mg_connection *c, int ev, void *ev_data);
static void mongoose(void *args) {
  struct mg_mgr mgr;        // Initialise Mongoose event manager
  mg_mgr_init(&mgr);        // and attach it to the interface
  mg_log_set(MG_LL_VERBOSE);  // Set log level

  cyw43_arch_init();
  cyw43_arch_enable_sta_mode();
  cyw43_arch_wifi_connect_blocking(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK);

  MG_INFO(("Initialising application..."));
  struct mg_mqtt_opts opts = {.clean = true,
                              
                              };
  
  bool done = false;
  MG_INFO(("Connecting to %s", s_url));  
  mg_timer_add(&mgr, 3600 * 1000, MG_TIMER_RUN_NOW | MG_TIMER_REPEAT,timer_sntp_fn, &mgr);
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
    // c->is_hexdumping = 1;
  } else if (ev == MG_EV_CONNECT) {
    if (mg_url_is_ssl(s_url)) {
      struct mg_tls_opts opts = { .ca = mg_unpacked("/ca.crt"),
                                 //.cert = mg_unpacked("/crt.pem"),
                                 //.key = mg_unpacked("/key.pem"),
                                 .skip_verification=1,
                                 .name = mg_url_host(s_url)};

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
   } else if (ev == MG_EV_POLL && c->data[0] == 'X' ) { 
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
  MG_INFO(("Start"));
  xTaskCreate(mongoose, "mongoose", 2048, 0, configMAX_PRIORITIES - 1, NULL);

  vTaskStartScheduler();  // This blocks

  return 0;
}

