/*
 * This module implements secure access
 *
 * Copyright (c) 2019, 2020 Danny Backx
 *
 * License (GNU Lesser General Public License) :
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 3 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <Arduino.h>
#include "secrets.h"
#include "esp_log.h"
#include "sdkconfig.h"

#ifdef HARDCODED_CERTIFICATES
#include "private_key.h"
#endif

#include "Kippen.h"
#include "Secure.h"
#include <sys/socket.h>
#include <lwip/etharp.h>

Secure::Secure() {
  tbl_max = tbl_inc;
  secure_tbl = (struct secure_device *)malloc(sizeof(struct secure_device) * tbl_max);
  ndevices = ntrusts = 0;
  tlsTask = 0;
  srvcert = 0;
  pkey = 0;
  ssl = 0;
  conf = 0;
  WaitForAcmeCertificate = false;
  client_certs = 0;

  // Whitelist (up to 16) devices specified in secrets.h
#ifdef SECURE_WHITELIST_1_MAC
  AddDevice(SECURE_WHITELIST_1_MAC);
#elif defined(SECURE_WHITELIST_1_IP)
  AddDevice(inet_addr(SECURE_WHITELIST_1_IP));
#endif
#ifdef SECURE_WHITELIST_2_MAC
  AddDevice(SECURE_WHITELIST_2_MAC);
#elif defined(SECURE_WHITELIST_2_IP)
  AddDevice(inet_addr((const char *)SECURE_WHITELIST_2_IP));
#endif
#ifdef SECURE_WHITELIST_3_MAC
  AddDevice(SECURE_WHITELIST_3_MAC);
#elif defined(SECURE_WHITELIST_3_IP)
  AddDevice(inet_addr((const char *)SECURE_WHITELIST_3_IP));
#endif
#ifdef SECURE_WHITELIST_4_MAC
  AddDevice(SECURE_WHITELIST_4_MAC);
#elif defined(SECURE_WHITELIST_4_IP)
  AddDevice(inet_addr((const char *)SECURE_WHITELIST_4_IP));
#endif
#ifdef SECURE_WHITELIST_5_MAC
  AddDevice(SECURE_WHITELIST_5_MAC);
#elif defined(SECURE_WHITELIST_5_IP)
  AddDevice(inet_addr((const char *)SECURE_WHITELIST_5_IP));
#endif
#ifdef SECURE_WHITELIST_6_MAC
  AddDevice(SECURE_WHITELIST_6_MAC);
#elif defined(SECURE_WHITELIST_6_IP)
  AddDevice(inet_addr((const char *)SECURE_WHITELIST_6_IP));
#endif
#ifdef SECURE_WHITELIST_7_MAC
  AddDevice(SECURE_WHITELIST_7_MAC);
#elif defined(SECURE_WHITELIST_7_IP)
  AddDevice(inet_addr((const char *)SECURE_WHITELIST_7_IP));
#endif
#ifdef SECURE_WHITELIST_8_MAC
  AddDevice(SECURE_WHITELIST_8_MAC);
#elif defined(SECURE_WHITELIST_8_IP)
  AddDevice(inet_addr((const char *)SECURE_WHITELIST_8_IP));
#endif
#ifdef SECURE_WHITELIST_9_MAC
  AddDevice(SECURE_WHITELIST_9_MAC);
#elif defined(SECURE_WHITELIST_9_IP)
  AddDevice(inet_addr((const char *)SECURE_WHITELIST_9_IP));
#endif
#ifdef SECURE_WHITELIST_10_MAC
  AddDevice(SECURE_WHITELIST_10_MAC);
#elif defined(SECURE_WHITELIST_10_IP)
  AddDevice(inet_addr((const char *)SECURE_WHITELIST_10_IP));
#endif
#ifdef SECURE_WHITELIST_11_MAC
  AddDevice(SECURE_WHITELIST_11_MAC);
#elif defined(SECURE_WHITELIST_11_IP)
  AddDevice(inet_addr((const char *)SECURE_WHITELIST_11_IP));
#endif
#ifdef SECURE_WHITELIST_12_MAC
  AddDevice(SECURE_WHITELIST_12_MAC);
#elif defined(SECURE_WHITELIST_12_IP)
  AddDevice(inet_addr((const char *)SECURE_WHITELIST_12_IP));
#endif
#ifdef SECURE_WHITELIST_13_MAC
  AddDevice(SECURE_WHITELIST_13_MAC);
#elif defined(SECURE_WHITELIST_13_IP)
  AddDevice(inet_addr((const char *)SECURE_WHITELIST_13_IP));
#endif
#ifdef SECURE_WHITELIST_14_MAC
  AddDevice(SECURE_WHITELIST_14_MAC);
#elif defined(SECURE_WHITELIST_14_IP)
  AddDevice(inet_addr((const char *)SECURE_WHITELIST_14_IP));
#endif
#ifdef SECURE_WHITELIST_15_MAC
  AddDevice(SECURE_WHITELIST_15_MAC);
#elif defined(SECURE_WHITELIST_15_IP)
  AddDevice(inet_addr((const char *)SECURE_WHITELIST_15_IP));
#endif
#ifdef SECURE_WHITELIST_16_MAC
  AddDevice(SECURE_WHITELIST_16_MAC);
#elif defined(SECURE_WHITELIST_16_IP)
  AddDevice(inet_addr((const char *)SECURE_WHITELIST_16_IP));
#endif
}

Secure::~Secure() {
  TlsServerStopTask();
  if (client_certs) {
    // FIX ME clear stuff pointed to as well
    free(client_certs);
    client_certs = 0;
  }
}

void Secure::loop(time_t nowts) {
  if (WaitForAcmeCertificate && acme && acme->HaveValidCertificate()) {
    TlsServerStartTask();
    WaitForAcmeCertificate = false;
  }
}

void Secure::NetworkConnected(void *ctx, system_event_t *event) {
  if (acme) {
    // With ACME, we may have to delay stuff until we have a certificate
    if (acme->HaveValidCertificate()) {
      TlsServerStartTask();
    } else {
      WaitForAcmeCertificate = true;
    }
  } else {
    // No delay
    TlsServerStartTask();
  }
}

void Secure::NetworkDisconnected(void *ctx, system_event_t *event) {
  TlsServerStopTask();
}

boolean Secure::isIPSecure(struct sockaddr_in *sender) {
  boolean ok = CheckPeerIP(sender);
  ESP_LOGD(secure_tag, "%s -> %s", __FUNCTION__, ok ? "safe" : "no");
  return ok;
}

boolean Secure::isPeerSecure(int sock) {
  struct sockaddr_in sa;
  socklen_t al = sizeof(sa);

  errno = 0;
  if (getpeername(sock, (struct sockaddr *)&sa, &al) != 0) {
    ESP_LOGE(secure_tag, "%s: getpeername failed, errno %d", __FUNCTION__, errno);
    return false;
  }

  if (sa.sin_addr.s_addr == 0) {
    // Try ipv6, see https://www.esp32.com/viewtopic.php?t=8317
    struct sockaddr_in6 sa6;
    al = sizeof(sa6);
    if (getpeername(sock, (struct sockaddr *)&sa6, &al) != 0) {
      ESP_LOGE(secure_tag, "%s: getpeername6 failed, errno %d", __FUNCTION__, errno);
      return false;
    }

    sa.sin_addr.s_addr = sa6.sin6_addr.un.u32_addr[3];
  }

  ESP_LOGD(secure_tag, "%s: IP address is %s, errno %d", __FUNCTION__, inet_ntoa(sa.sin_addr), errno);
  return isIPSecure(&sa);
}

/*
 * List ARP table either to log or to MQTT, depending on parameter.
 * 1 -> to MQTT
 */
void Secure::ListARP(boolean tomqtt) {
  int i;
  ip4_addr_t		*ip;
  struct eth_addr	*ea;
  struct netif		*netif;

  for (i=0; i<ARP_TABLE_SIZE; i++) {
    if (etharp_get_entry(i, &ip, &netif, &ea)) {
      char msg[80];
      sprintf(msg, "[%02d] %02X:%02X:%02X:%02X:%02X:%02X\t%s",
        i,
	ea->addr[0], ea->addr[1], ea->addr[2],
        ea->addr[3], ea->addr[4], ea->addr[5],
	ip4addr_ntoa(ip));
      if (tomqtt) {
        kippen->Report(msg);
      } else {
        ESP_LOGE(secure_tag, "%s", msg);
      }
    }
  }
}

/*
 * If a peer is contacting us, then the ARP table has its IP / MAC mapping.
 * So when we get a connection in the application (with IP address), we can look
 * up the corresponding MAC in ARP, and verify if this is one of our peers (secrets.h).
 *
 * Note it looks like the first connection is always lost because the ARP table is still too empty at the time.
 * Initial entries appear to include the ESP itself and the network's router.
 */
boolean Secure::CheckPeerIP(struct sockaddr_in *sap) {
  struct eth_addr ea, *eap = &ea;
  const ip4_addr_t *iap;
  s8_t ix;

  ESP_LOGD(secure_tag, "%s looking up %s", __FUNCTION__, inet_ntoa(sap->sin_addr));

  ix = etharp_find_addr(NULL, (ip4_addr_t *)&sap->sin_addr.s_addr, &eap, &iap);

#if 0
  ListARP(false);
#endif

  ESP_LOGD(secure_tag, "%s -> %d", __FUNCTION__, ix);

  if (ix < 0) {
    ESP_LOGE(secure_tag, "%s(%s) unknown, errno %d", __FUNCTION__, inet_ntoa(sap->sin_addr), errno);
    return false;
  }

  char mac[20];
  sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", eap->addr[0], eap->addr[1],
    eap->addr[2], eap->addr[3], eap->addr[4], eap->addr[5]);

  for (int i=0; i<ndevices; i++)
    if (secure_tbl[i].mac && (strcasecmp(mac, secure_tbl[i].mac) == 0)) {
      ESP_LOGD(secure_tag, "%s(%s) MAC %s is secure", __FUNCTION__, inet_ntoa(sap->sin_addr), mac);
      return true;
    } else if ((secure_tbl[i].ip.addr != 0) && (sap->sin_addr.s_addr == secure_tbl[i].ip.addr)) {
      ESP_LOGD(secure_tag, "%s(%s) IP is secure", __FUNCTION__, inet_ntoa(sap->sin_addr));
      return true;
    }

  ESP_LOGE(secure_tag, "%s(%s) has MAC %s, unknown", __FUNCTION__, inet_ntoa(sap->sin_addr), mac);
  return false;
}

void Secure::AddDevice(const char *mac) {
  ESP_LOGD(secure_tag, "Add Device %s", mac);

  if (ndevices == tbl_max) {
    tbl_max += tbl_inc;
    secure_tbl = (struct secure_device *)realloc(secure_tbl, sizeof(struct secure_device) * tbl_max);
  }
  secure_tbl[ndevices].mac = mac;
  secure_tbl[ndevices].ip.addr = 0;
  ndevices++;
}

void Secure::AddDevice(in_addr_t ip) {
  ESP_LOGD(secure_tag, "Add Device %s", inet_ntoa(ip));

  if (ndevices == tbl_max) {
    tbl_max += tbl_inc;
    secure_tbl = (struct secure_device *)realloc(secure_tbl, sizeof(struct secure_device) * tbl_max);
  }
  secure_tbl[ndevices].mac = 0;
  secure_tbl[ndevices].ip.addr = ip;
  ndevices++;
}

/********************************************************************************
 *  
 * TLS server
 *
 ********************************************************************************/
static void LocalTlsTaskLoop(void *ptr) {
  security->TlsTaskLoop(ptr);
}

void Secure::TlsServerStartTask() {
  int server_port = CONFIG_JSON_SERVERPORT;

  if (server_port < 0)
    return;

  ESP_LOGI(secure_tag, "TlsServerStartTask(port %d)", server_port);

  /*
   * This priority is described in the manual, even though portPRIVILEGE_BIT is 0 on esp32.
   * Other documentation describes that the idle task prio is 0, any value up to 32 is valid,
   * and lower number means lower priority.
   * See e.g. https://www.freertos.org/RTOS-task-priority.html .
   *
   * We appear to use <4K of task stack so allocating 5K
   * With 5K, sometimes there's a stack overflow
   */
  xTaskCreate(LocalTlsTaskLoop, "TLS task", 6000, 0, 2 | portPRIVILEGE_BIT, &tlsTask);
}

void Secure::TlsServerStopTask() {
  ESP_LOGI(secure_tag, "TlsServerStopTask()");

  // Do this at the end.
  if (tlsTask)
    vTaskDelete(tlsTask);
  tlsTask = 0;

  ESP_LOGI(secure_tag, "TlsServerStopTask end");
}

int Secure::TlsSetup() {
  if (acme && acme->HaveValidCertificate()) {
    return TlsSetupAcme();
  } else {
    return TlsSetupLocal();
  }
}

/*
 * Get our ACME certificate info
 */
int Secure::TlsSetupAcme() {
  int ret = 0;
  char path[80];	// FIX ME

  ESP_LOGI(tls_tag, "TLS setup, ACME version ...");

  ssl = (mbedtls_ssl_context *)calloc(sizeof(mbedtls_ssl_context), 1);
  mbedtls_ssl_init(ssl);
  conf = (mbedtls_ssl_config *)calloc(sizeof(mbedtls_ssl_config), 1);
  mbedtls_ssl_config_init(conf);
  // srvcert = (mbedtls_x509_crt *)calloc(sizeof(mbedtls_x509_crt), 1);
  // mbedtls_x509_crt_init(srvcert);
  // mbedtls_pk_init(pkey);
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);

  // Server certificate
  srvcert = acme->getCertificate();
  if (srvcert == 0) {
    ESP_LOGE(tls_tag, "No server certificate");
    return ESP_ERR_NOT_FOUND;
  }

  // Server private key
  pkey = acme->getCertificateKey();
  if (pkey == 0) {
    ESP_LOGE(tls_tag, "No server private key");
    return ESP_ERR_NOT_FOUND;
  }

  ESP_LOGD(tls_tag, "Read server key %s", path);
  ESP_LOGI(tls_tag, "Load server cert and key : OK" );

  if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
      (const unsigned char *) pers, strlen(pers))) != 0) {
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    ESP_LOGE(tls_tag, "Failed to seed random number generator, error %d (%s)", ret, error_buf);
    TlsShutdown();
    return ret;
  }
  ESP_LOGI(tls_tag, "Seeded the random number generator");

  if ((ret = mbedtls_ssl_config_defaults(conf, MBEDTLS_SSL_IS_SERVER,
      MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    ESP_LOGE(tls_tag, "Failed to set up SSL data, error %d (%s)", ret, error_buf);
    TlsShutdown();
    return ret;
  }
  ESP_LOGI(tls_tag, "SSL data setup OK");

  // Force certificate passing
  // See https://tls.mbed.org/discussions/generic/how-to-establish-a-handshake-with-two-way-authentication
  mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_REQUIRED);

  mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, &ctr_drbg);

  mbedtls_ssl_conf_ca_chain(conf, srvcert->next, NULL);
  if ((ret = mbedtls_ssl_conf_own_cert(conf, srvcert, pkey)) != 0) {
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    ESP_LOGE(tls_tag, "mbedtls_ssl_conf_own_cert failed, error %d (%s)", ret, error_buf);
    TlsShutdown();
    return ret;
  }

  if ((ret = mbedtls_ssl_setup(ssl, conf)) != 0) {
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    ESP_LOGE(tls_tag, "mbedtls_ssl_setup failed, error %d (%s)", ret, error_buf);
    TlsShutdown();
    return ret;
  }

  ESP_LOGI(tls_tag, "TLS/SSL setup complete" );

  // Gather a list of client certificates ?
  client_certs = (mbedtls_x509_crt *)calloc(sizeof(mbedtls_x509_crt), 1);
  mbedtls_x509_crt_init(client_certs);

#if 0
  const char *trusts[] = {
    "/spiffs/trust-client.crt",
    "/spiffs/trust-client2.crt",
    "/spiffs/certificate.pem",
    "/spiffs/trust-cert.pem",
    // "/spiffs/trust-client.key",
    // "/spiffs/trust-client2.key",
    0
  };

  ntrusts = 0;
  for (int i = 0; trusts[i]; i++) {
    const char *t = trusts[i];
    ret = mbedtls_x509_crt_parse_file(client_certs, t);
    if (ret != 0) {
      mbedtls_strerror(ret, error_buf, sizeof(error_buf));
      ESP_LOGE(tls_tag, "%s : failed to load trusted key %s, error %s", __FUNCTION__, t, error_buf);
    } else {
      ntrusts++;
      ESP_LOGI(tls_tag, "%s: loaded trusted key %s", __FUNCTION__, t);
    }
  }
  ESP_LOGI(tls_tag, "Loaded %d trusted keys", ntrusts);
#else
  ESP_LOGD(tls_tag, "Not loading local trusted keys");
#endif

  return ESP_OK;
}

/*
 * Can be made to work with hardcoded certificates as well as from files.
 * Hardcoded version commented out to save the space and build dependencies.
 */
int Secure::TlsSetupLocal() {
#if 0
  int ret;
  char path[80];	// FIX ME

  ssl = (mbedtls_ssl_context *)calloc(sizeof(mbedtls_ssl_context), 1);
  mbedtls_ssl_init(ssl);
  conf = (mbedtls_ssl_config *)calloc(sizeof(mbedtls_ssl_config), 1);
  mbedtls_ssl_config_init(conf);
  srvcert = (mbedtls_x509_crt *)calloc(sizeof(mbedtls_x509_crt), 1);
  mbedtls_x509_crt_init(srvcert);
  mbedtls_pk_init(pkey);
  mbedtls_entropy_init(&entropy);
  mbedtls_ctr_drbg_init(&ctr_drbg);

  if (CONFIG_CERTIFICATE_FN) {		// Read certificate from file
    sprintf(path, CONFIG_FS_BASEDIR, CONFIG_CERTIFICATE_FN);
    ret = mbedtls_x509_crt_parse_file(srvcert, path);
  } else {				// Use self signed certificate
#ifdef HARDCODED_CERTIFICATES
    ret = mbedtls_x509_crt_parse(srvcert, (const unsigned char *) alarm_crt_DER,
      alarm_crt_DER_len);
#else
    ret = ESP_ERR_NOT_FOUND;
#endif
  }
  if (ret != 0) {
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    ESP_LOGE(tls_tag, "Failed to load server cert+key from %s, error %s", path, error_buf);
    TlsShutdown();
    return ret;
  }
  ESP_LOGD(tls_tag, "Read server certificate %s", path);

  if (CONFIG_CACERT_FN) {
    sprintf(path, CONFIG_FS_BASEDIR, CONFIG_CACERT_FN);
    ret = mbedtls_x509_crt_parse_file(srvcert, path);
  } else {
#ifdef HARDCODED_CERTIFICATES
    ret = mbedtls_x509_crt_parse(srvcert, (const unsigned char *)my_ca_crt_DER,
      my_ca_crt_DER_len);
#else
    ret = ESP_ERR_NOT_FOUND;
#endif
  }
  if (ret != 0) {
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    ESP_LOGE(tls_tag, "Failed to parse CA key from %s, error %s", path, error_buf);
    TlsShutdown();
    return ret;
  }
  ESP_LOGD(tls_tag, "Read CA key %s", path);

  if (CONFIG_MYKEY_FN) {
    sprintf(path, CONFIG_FS_BASEDIR, CONFIG_MYKEY_FN);
    ret = mbedtls_pk_parse_keyfile(pkey, path, 0);
  } else {
#ifdef HARDCODED_CERTIFICATES
    ret = mbedtls_pk_parse_key(pkey, (const unsigned char *) alarm_key_DER,
      alarm_key_DER_len, NULL, 0);
#else
    ret = ESP_ERR_NOT_FOUND;
#endif
  }
  if (ret != 0) {
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    ESP_LOGE(tls_tag, "Failed to parse server key from %s, error %s", path, error_buf);
    TlsShutdown();
    return ret;
  }
  ESP_LOGD(tls_tag, "Read server key %s", path);
  ESP_LOGI(tls_tag, "Load server cert and key : OK" );

  if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
      (const unsigned char *) pers, strlen(pers))) != 0) {
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    ESP_LOGE(tls_tag, "Failed to seed random number generator, error %d (%s)", ret, error_buf);
    TlsShutdown();
    return ret;
  }
  ESP_LOGI(tls_tag, "Seeded the random number generator");

  if ((ret = mbedtls_ssl_config_defaults(conf, MBEDTLS_SSL_IS_SERVER,
      MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    ESP_LOGE(tls_tag, "Failed to set up SSL data, error %d (%s)", ret, error_buf);
    TlsShutdown();
    return ret;
  }
  ESP_LOGI(tls_tag, "SSL data setup OK");

  // Force certificate passing
  // See https://tls.mbed.org/discussions/generic/how-to-establish-a-handshake-with-two-way-authentication
  mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_REQUIRED);

  mbedtls_ssl_conf_rng(conf, mbedtls_ctr_drbg_random, &ctr_drbg);

  mbedtls_ssl_conf_ca_chain(conf, srvcert->next, NULL);
  if ((ret = mbedtls_ssl_conf_own_cert(conf, srvcert, pkey)) != 0) {
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    ESP_LOGE(tls_tag, "mbedtls_ssl_conf_own_cert failed, error %d (%s)", ret, error_buf);
    TlsShutdown();
    return ret;
  }

  if ((ret = mbedtls_ssl_setup(ssl, conf)) != 0) {
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    ESP_LOGE(tls_tag, "mbedtls_ssl_setup failed, error %d (%s)", ret, error_buf);
    TlsShutdown();
    return ret;
  }

  ESP_LOGI(tls_tag, "TLS/SSL setup complete" );

  // Gather a list of client certificates ?
  client_certs = (mbedtls_x509_crt *)calloc(sizeof(mbedtls_x509_crt), 1);
  mbedtls_x509_crt_init(client_certs);

#if 0
  if (config->getTrustedKeyStore()) {
    sprintf(path, config->base_fmt, config->getTrustedKeyStore());
    // FIXME which file type does this support ?
    ret = mbedtls_x509_crt_parse_file(client_certs, path);
    if (ret != 0) {
      mbedtls_strerror(ret, error_buf, sizeof(error_buf));
      ESP_LOGE(tls_tag, "Failed to load Trusted Key Store %s, error %s", path, error_buf);

      TlsShutdown();
      return ret;
    }
    ESP_LOGI(tls_tag, "Loaded Trusted Key Store %s", path);
  } else {
    ESP_LOGE(tls_tag, "Not starting JSON server without trusted key store");
    TlsShutdown();
    return -1;
  }
#else
  const char *trusts[] = {
    "/spiffs/trust-client.crt",
    "/spiffs/trust-client2.crt",
    "/spiffs/trust-cert.pem",
    // "/spiffs/trust-client.key",
    // "/spiffs/trust-client2.key",
    0
  };


  ntrusts = 0;
  for (int i = 0; trusts[i]; i++) {
    const char *t = trusts[i];
    ret = mbedtls_x509_crt_parse_file(client_certs, t);
    if (ret != 0) {
      mbedtls_strerror(ret, error_buf, sizeof(error_buf));
      ESP_LOGE(tls_tag, "%s: failed to load trusted key %s, error %s", __FUNCTION__, t, error_buf);
    } else {
      ntrusts++;
      ESP_LOGI(tls_tag, "%s: loaded trusted key %s", __FUNCTION__, t);
    }
  }
  ESP_LOGI(tls_tag, "Loaded %d trusted keys", ntrusts);
#endif
  return ESP_OK;
#else
  return ESP_ERR_NOT_FOUND;
#endif
}

void Secure::TlsShutdown() {
  mbedtls_x509_crt_free(srvcert);
  free(srvcert);
  srvcert = 0;
  mbedtls_pk_free(pkey);
  if (ssl) {
    mbedtls_ssl_free(ssl);
    free(ssl);
    ssl = 0;
  }
  if (conf != 0) {
    mbedtls_ssl_config_free(conf);
    free(conf);
    conf = 0;
  }
  mbedtls_ctr_drbg_free(&ctr_drbg);
  mbedtls_entropy_free(&entropy);
}

/*
 * Simplistic whitelist
 */
int MyTlsCNVerification(const char *cn) {
  if (strcmp(cn, "dannybackx.hopto.org") == 0) {
    return 0;
  }
  return MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
}

int MyKeyVerification(void *param, mbedtls_x509_crt *cert, int depth, uint32_t *flags) {
  static const char *stls_tag = "TLS MyKeyVerification";

  ESP_LOGI(stls_tag, "%s being called(crt %p)", __FUNCTION__, cert);

  if (cert) {
    unsigned char buf[1024];
    int ret;

    // Get human-readable certificate info
    if ((ret = mbedtls_x509_crt_info((char *) buf, sizeof(buf) - 1, "", cert)) >= 0) {
      // Show it
      // ESP_LOGI(stls_tag, "TLS client %s", buf);
    }

    // Issuer ?
    mbedtls_x509_name issuer = cert->issuer;
    if (mbedtls_x509_dn_gets((char *)buf, sizeof(buf), &issuer) > 0) {
      ESP_LOGI(stls_tag, "TLS CA : %s", buf);
    }

    // Return OK if this is the CA (meaning it's not the client)
    if (cert->ca_istrue == 1) {
      ESP_LOGI(stls_tag, "TLS this is a CA -> ok");
      return 0;
    }

    // Subject (= calling node)
    mbedtls_x509_name subject = cert->subject;
    if (mbedtls_x509_dn_gets((char *)buf, sizeof(buf), &subject) > 0) {
      ESP_LOGD(stls_tag, "TLS Node : %s", buf);

      char *pcn = strstr((const char *)buf, "CN=");
      if (pcn == 0)
	ESP_LOGE(stls_tag, "TLS Node not scanned");
      else {
	char *pcn3 = strdup(pcn+3);
	ESP_LOGI(stls_tag, "TLS CN : %s", pcn3);
	return MyTlsCNVerification(pcn3);
      }
    }
  }
  return 0;
}

/*
 * Note this can not return (unless vTaskDelete() was called) or FreeRTOS deliberately panics.
 */
void Secure::TlsTaskLoop(void *ptr) {
  int ret, len;
  mbedtls_net_context listen_fd;
  unsigned char buf[1024];
  char *caller = 0;
  char port[6];
  TaskHandle_t tsk;

  if (TlsSetup() != 0) {
    TlsShutdown();
    tsk = tlsTask;
    tlsTask = 0;
    vTaskDelete(tsk);
    return;
  }

  sprintf(port, "%d", CONFIG_JSON_SERVERPORT);

  mbedtls_net_init(&listen_fd);
  if ((ret = mbedtls_net_bind(&listen_fd, NULL, port, MBEDTLS_NET_PROTO_TCP)) != 0) {
    mbedtls_strerror(ret, error_buf, sizeof(error_buf));
    ESP_LOGE(tls_tag, "Failed to bind to https://localhost:%d, error %d (%s)",
      CONFIG_JSON_SERVERPORT, ret, error_buf);
    TlsShutdown();
    tsk = tlsTask;
    tlsTask = 0;
    vTaskDelete(tsk);
    return;
  }
  ESP_LOGI(tls_tag, "Bind on https://localhost:%d ok", CONFIG_JSON_SERVERPORT);

  // Accept incoming connections
  while (1) {
    mbedtls_net_context client_fd;
    ret = 0;

    mbedtls_net_init(&client_fd);
    mbedtls_ssl_session_reset(ssl);

    ESP_LOGI(tls_tag, "Waiting for a remote connection ..." );
    if ((ret = mbedtls_net_accept(&listen_fd, &client_fd, NULL, 0, NULL)) != 0) {
      ESP_LOGE(tls_tag, "Accept failed, return %d", ret);
      break;
    }

    ESP_LOGI(tls_tag, "Got a remote connection ..." );
    mbedtls_ssl_set_bio(ssl, &client_fd, mbedtls_net_send, mbedtls_net_recv, NULL );

    ESP_LOGI(tls_tag, "Start handshake ..." );
    if ((ret = mbedtls_ssl_handshake(ssl)) != 0) {
      if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
	mbedtls_strerror(ret, error_buf, sizeof(error_buf));
	ESP_LOGE(tls_tag, "SSL/TLS handshake failed, error %d (%s)", ret, error_buf);
	mbedtls_net_free(&client_fd);
	continue;
      }
    }

    // Who are we talking to ?
    mbedtls_x509_crt *crt = (mbedtls_x509_crt *)mbedtls_ssl_get_peer_cert(ssl);

    // No client certificate --> no connection
    if (crt == 0) {
#if 1
      if ((ret = mbedtls_ssl_write(ssl, (unsigned char *)json_not_authenticated, strlen(json_not_authenticated))) <= 0) {
	mbedtls_strerror(ret, error_buf, sizeof(error_buf));
	ESP_LOGE(tls_tag, "Write to client failed, error %d (%s)", ret, error_buf);
      }
      if ((ret = mbedtls_ssl_close_notify(ssl)) < 0) {
	mbedtls_strerror(ret, error_buf, sizeof(error_buf));
	ESP_LOGE(tls_tag, "Connection close error %d (%s)", ret, error_buf);
      }
      mbedtls_net_free(&client_fd);
      ESP_LOGE(tls_tag, "No client certificate, disconnect");
      continue;
#else
      ESP_LOGE(tls_tag, "No client certificate, continuing anyway");
#endif
      }

    ESP_LOGI(tls_tag, "SSL/TLS handshake ok");

    // FIXME to be upgraded to certificate / security check : authenticate the peer
    if (crt) {
      unsigned char buf[1024];
      int ret;

      // Get human-readable certificate info
      if ((ret = mbedtls_x509_crt_info((char *) buf, sizeof(buf) - 1, "", crt)) >= 0) {
	// Show it
	// ESP_LOGI(tls_tag, "TLS client %s", buf);
      }

      // Issuer ?
      mbedtls_x509_name issuer = crt->issuer;
      if (mbedtls_x509_dn_gets((char *)buf, sizeof(buf), &issuer) > 0) {
	ESP_LOGI(tls_tag, "TLS CA : %s", buf);
      }

      // Subject (= calling node)
      mbedtls_x509_name subject = crt->subject;
      if (mbedtls_x509_dn_gets((char *)buf, sizeof(buf), &subject) > 0) {
	ESP_LOGD(tls_tag, "TLS Node : %s", buf);

	char *pcn = strstr((const char *)buf, "CN=");
	if (pcn == 0)
	  ESP_LOGE(tls_tag, "TLS Node not scanned");
	else {
	  caller = strdup(pcn+3);
	  ESP_LOGI(tls_tag, "TLS Caller : %s", caller);
	}
      }
    }

    int (*vfn)(void *, mbedtls_x509_crt *, int, uint32_t *);
    vfn = MyKeyVerification;
    /*
     * A CRL is a certificate revocation list.
     * For now, we're using an empty list.
     */
    mbedtls_x509_crl client_crl;
    mbedtls_x509_crl_init(&client_crl);

    /*
     * There are two ways to use this module.
     * 1. With self signed certificates in the alarm modules as well as in the remote devices
     *    (phones). In that case, getting here already proves that the remote device knows
     *    "the secret" so they're ok, no further validation required.
     * 2. With regular certificates, we still need to validate whether the remote client is
     *    on our whitelist. That's what the code below does.
     */
#if defined(CONFIG_CHECK_LOCALCERTIFICATES)
    if (crt) {
      boolean ok = false;

      // Validate whether the client is authorized
      // for (int i=0; i<ntrusts; i++) {
      // }
      uint32_t flags = 0;
      uint32_t vrfy = 0;

      ESP_LOGI(tls_tag, "%s: mbedtls_x509_crt_verify(%p, %p, ..)", __FUNCTION__, (void *)crt, (void *)client_certs);
      {
        mbedtls_x509_crt * p = client_certs;
	ESP_LOGI(tls_tag, "initial %p", p);
	for (; p; p = p->next)
	  ESP_LOGI(tls_tag, "next %p", p->next);

      }
      ret = mbedtls_x509_crt_verify(crt, client_certs, &client_crl, 0, &flags, vfn, &vrfy);
      if (ret == 0 && flags == 0)	// Chain was verified and is valid
	ok = true;
      else if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
        char error[120];
	int len = mbedtls_x509_crt_verify_info(error, sizeof(error), "", flags);
	if (len > 0) {
#if 1
	  ESP_LOGE(tls_tag, "Unverified connection : %s, refusing", error);
#else
	  ESP_LOGE(tls_tag, "Unverified connection : %s, flags 0x%X, allowing for now", error, flags);
	  ok = true;
#endif
	}
      } else {
	mbedtls_strerror(ret, error_buf, sizeof(error_buf));
	ESP_LOGE(tls_tag, "mbedtls_x509_crt_verify error : %s", error_buf);
      }
 
      if (! ok) {	// Unsecure connection, abort
	mbedtls_net_free(&client_fd);
	continue;
      }
    }
#endif

    /*
     * Make JSON request/reply server
     */
    // Read the HTTP Request
    do {
      len = sizeof(buf) - 1;
      memset(buf, 0, sizeof(buf));
      ret = mbedtls_ssl_read(ssl, buf, len);

      if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
	continue;
      if (ret <= 0) {
	switch (ret) {
	case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
	  ESP_LOGI(tls_tag, " connection was closed gracefully");
	  break;

	case MBEDTLS_ERR_NET_CONN_RESET:
	  ESP_LOGE(tls_tag, " connection was reset by peer");
	  break;

	default:
	  mbedtls_strerror(ret, error_buf, sizeof(error_buf));
	  ESP_LOGI(tls_tag, "mbedtls_ssl_read returned -0x%x (%s)", -ret, error_buf);
	  break;
	}

	break;
      }

      len = ret;
      if (ret > 0)
	break;
    } while (1);

    // Hack : to accomodate HTML requests, skip everything until "{"
    char *p;
    for (p=(char *)buf; *p && *p != '{'; p++) ;

    // Ok, interpret it now
    char *q = kippen->HandleQueryAuthenticated(p, caller);
    ESP_LOGD(tls_tag, "HandleQuery(%s) -> %s", p, q);

    if (q) {	// Only write back if non-null, obviously
      if ((ret = mbedtls_ssl_write(ssl, (unsigned char *)q, strlen(q))) <= 0) {
	mbedtls_strerror(ret, error_buf, sizeof(error_buf));
	ESP_LOGE(tls_tag, "Write to client failed, error %d (%s)", ret, error_buf);
      }

      // Note : no need to free q, see Peers::HandleQueryInternal.
    }

    // Free caller info
    if (caller) {
      free(caller);
      caller = 0;
    }

    if ((ret = mbedtls_ssl_close_notify(ssl)) < 0) {
      if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
	mbedtls_strerror(ret, error_buf, sizeof(error_buf));
	ESP_LOGE(tls_tag, "Failed to close connection, error %d (%s)", ret, error_buf);
      } else {
	ESP_LOGI(tls_tag, "Connection closed" );
      }
    }
    mbedtls_net_free(&client_fd);
  }

  if (ret != 0) {
    char error_buf[100];
    mbedtls_strerror(ret, error_buf, 100);
    ESP_LOGE(tls_tag, "Last error was: %d - %s", ret, error_buf);
  }

  mbedtls_net_free(&listen_fd);
  TlsShutdown();
}
