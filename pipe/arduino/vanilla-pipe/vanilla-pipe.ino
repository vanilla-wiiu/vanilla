#include <DHCPLite.h>
#include <Ethernet.h>

#include <WiFi.h>
#include <WiFiUdp.h>
#include <wifi_drv.h>

extern "C" {
#include <wifi/wifi_conf.h>
#include <wifi/rtw_wpa_supplicant/src/wps/wps_defs.h>
#include <wifi/rtw_wpa_supplicant/wpa_supplicant/wifi_wps_config.h>
}

#include "def.h"

EthernetUDP DhcpServer;

WiFiUDP VidInUdp;
WiFiUDP AudInUdp;
WiFiUDP HidInUdp;
WiFiUDP MsgInUdp;
WiFiUDP CmdInUdp;

EthernetUDP VidOutUdp;
EthernetUDP AudOutUdp;
EthernetUDP HidOutUdp;
EthernetUDP MsgOutUdp;
EthernetUDP CmdOutUdp;

EthernetUDP CtrlUdp;

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 43, 1);
char domainName[] = "mshome.net";

IPAddress client(0, 0, 0, 0);

enum States
{
  STATE_IDLE,
  STATE_SYNC,
  STATE_CONNECT
};

int pipeState = STATE_IDLE;

uint16_t syncCode = 0;

unsigned int htonl(unsigned int x) {
#if ( SYSTEM_ENDIAN == _ENDIAN_LITTLE_ )
  return (
      (x & 0x000000ff) << 24u
    | (x & 0x0000ff00) << 8u
    | (x & 0x00ff0000) >> 8u
    | (x & 0xff000000) >> 24u
  );

  return ((x)<<8) | (((x)>>8)&0xFF);
#else
  return x;
#endif          
}

unsigned int ntohl(unsigned int x) {
  return htonl(x);
}

void setup() {
  Serial.begin(115200);

  while (!Serial) {
    ;
  }

  // BW16 hosts CS pin on 9
  Ethernet.init(9);

  // Initialize ethernet with specific IP address
  Ethernet.begin(mac, ip);

  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    while (true) {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }

  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }

  // Start DHCP server for client
  Serial.println("Hosting DHCP server");
  DhcpServer.begin(DHCP_SERVER_PORT);

  // Connect to Wii U
  // wiFiDrv.wifiDriverInit();
  // wifi_set_channel_plan(0x76);

  VidOutUdp.begin(50220);
  AudOutUdp.begin(50221);
  HidOutUdp.begin(50222);
  MsgOutUdp.begin(50210);
  CmdOutUdp.begin(50223);

  // VidInUdp.begin(50120);
  // AudInUdp.begin(50121);
  // HidInUdp.begin(50122);
  // MsgInUdp.begin(50110);
  // CmdInUdp.begin(50123);

  CtrlUdp.begin(51000);
}

void doRelay(UDP *in, UDP *out, IPAddress toAddr, uint16_t toPort) {
  uint8_t consoleBuf[2048];
  if (in->parsePacket()) {
    int readSize = in->read(consoleBuf, sizeof(consoleBuf));
    if (readSize) {
      Serial.print("Received packet from ");
      Serial.print(in->remoteIP());
      Serial.print(":");
      Serial.print(in->remotePort());
      Serial.print(", forwarding to ");
      Serial.print(toAddr);
      Serial.print(":");
      Serial.print(toPort);
      Serial.print(" size ");
      Serial.println(readSize);
      
      if (!out->beginPacket(toAddr, toPort)) {
        Serial.println("  Failed to begin packet");
      }
      if (!out->write(consoleBuf, readSize)) {
        Serial.println("  Failed to write packet");
      }
      if (!out->endPacket()) {
        Serial.println("  Failed to end packet");
      }
    }
  }
}

void doDhcp() {
  uint8_t dhcpBuf[DHCP_MESSAGE_SIZE];
  if (DhcpServer.parsePacket()) {
    int readSize = DhcpServer.read(dhcpBuf, DHCP_MESSAGE_SIZE);
    if (readSize) {
      byte serverIP[] = {192, 168, 43, 1};
      readSize = DHCPreply((RIP_MSG *) dhcpBuf, readSize, serverIP, domainName);
      if (readSize) {
        DhcpServer.beginPacket({255, 255, 255, 255}, DhcpServer.remotePort());
        DhcpServer.write(dhcpBuf, readSize);
        DhcpServer.endPacket();

        RIP_MSG *rip = (RIP_MSG *) dhcpBuf;

        client = rip->yiaddr;
        Serial.println(client);
      }
    }
  }
}

void doRelays() {
  doRelay(&VidInUdp, &VidOutUdp, client, 50320);
  doRelay(&VidOutUdp, &VidInUdp, IPAddress(192, 168, 1, 10), 50020);
  doRelay(&AudInUdp, &AudOutUdp, client, 50321);
  doRelay(&AudOutUdp, &AudInUdp, IPAddress(192, 168, 1, 10), 50021);
  doRelay(&MsgInUdp, &MsgOutUdp, client, 50310);
  doRelay(&MsgOutUdp, &MsgInUdp, IPAddress(192, 168, 1, 10), 50010);
  doRelay(&HidInUdp, &HidOutUdp, client, 50322);
  doRelay(&HidOutUdp, &HidInUdp, IPAddress(192, 168, 1, 10), 50022);
  doRelay(&CmdInUdp, &CmdOutUdp, client, 50323);
  doRelay(&CmdOutUdp, &CmdInUdp, IPAddress(192, 168, 1, 10), 50023);
}

void doSync() {
  Serial.println("Searching for Wii U...");
  
  int networks = WiFi.scanNetworks();
  if (networks == -1) {
    Serial.println("  Scan Error");
  }

  for (int i = 0; i < networks; i++) {
    if (memcmp(WiFi.SSID(i), "WiiU", 4) == 0) {
      Serial.println("  Found Wii U candidate, attempting WPS authentication...");

      char buf[9];
      snprintf(buf, sizeof(buf), "%04d5678", syncCode);

      int ret = wps_start(WPS_CONFIG_KEYPAD, buf, 0, WiFi.SSID(i));
      if (ret == 0) {
        Serial.println("  Connected");
        break;
      }
    }
  }
}

void doCtrl() {
  uint32_t cc;
  if (CtrlUdp.parsePacket()) {
    int readSize = CtrlUdp.read((byte *) &cc, sizeof(cc));
    if (readSize == sizeof(cc)) {
      cc = ntohl(cc);
      if ((cc >> 16) == (VANILLA_PIPE_CC_SYNC >> 16)) {
        // Do sync
        Serial.println("Received sync request...");
        
        pipeState = STATE_SYNC;
        syncCode = cc & 0xFFFF;

        cc = htonl(VANILLA_PIPE_CC_BIND_ACK);
        CtrlUdp.beginPacket(CtrlUdp.remoteIP(), CtrlUdp.remotePort());
        CtrlUdp.write((const char *) &cc, sizeof(cc));
        CtrlUdp.endPacket();
      } else if (cc == VANILLA_PIPE_CC_CONNECT) {
        // Do connect
        Serial.println("Received connect request...");

        if (((uint32_t) client) == 0) {
          client = CtrlUdp.remoteIP();
        }

        pipeState = STATE_CONNECT;
        WiFi.begin("WiiU9ce63589f813", "e6a31b46fd78b758e15245486e6ca81d12d7aec63ace0007a742b91500b7d40c");

        VidInUdp.begin(50120);
        AudInUdp.begin(50121);
        HidInUdp.begin(50122);
        MsgInUdp.begin(50110);
        CmdInUdp.begin(50123);

        cc = htonl(VANILLA_PIPE_CC_BIND_ACK);
        CtrlUdp.beginPacket(CtrlUdp.remoteIP(), CtrlUdp.remotePort());
        CtrlUdp.write((const char *) &cc, sizeof(cc));
        CtrlUdp.endPacket();
      } else if (cc == VANILLA_PIPE_CC_UNBIND) {
        Serial.println("Received interrupt...");

        VidInUdp.stop();
        AudInUdp.stop();
        HidInUdp.stop();
        MsgInUdp.stop();
        CmdInUdp.stop();

        WiFi.disconnect();

        pipeState = STATE_IDLE;
      }
    }
  }
}

void loop() {
  // Handle control codes from the client
  doCtrl();

  switch (pipeState) {
  case STATE_IDLE:
    // Handle DHCP packets
    doDhcp();
    break;
  case STATE_SYNC:
    // Do sync
    doSync();
    break;
  case STATE_CONNECT:
    // Handle connection to Wii U
    doRelays();
    break;
  }
}
