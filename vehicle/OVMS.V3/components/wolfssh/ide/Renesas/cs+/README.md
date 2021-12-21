# wolfSSH/AlphaProject Boad Simple Ssh Example Server Setup Guide
This demo is tested with the following condition.

* Renesas : CS+ v8.01
* Board   : Alpha Project AP-RX71M-0A w/ Sample program v2.0
* wolfSSL : 4.0.0
* wolfSSH : 1.3.1

## Setup process:
### 1. Download software

  - Unzip AlphaProject firmware
  - Unzip wolfssl under the same directory
  - Unzip wolfssh under the same directory

### 2. Set up wolfSSL and wolfSSH
  - Open wolfssh\ide\Renesas\cs+\wolfssl_lib\wolfssl_lib.mtpj with CS+ and build
  - Open wolfssh\ide\Renesas\cs+\wolfssh_lib\wolfssh_lib.mtpj with CS+ and build
  - Open demo_server.mtpj and build. This create demo program library.

### 3. Set up AlphaProject
  - The demo uses ap_rx71m_0a_sample_cs\Sample\ap_rx71m_0a_usbfunc_sample_cs\ap_rx71m_0a_ether_sample_cs.mtpj
  - Open and edit ap_rx71m_0a_sample_cs\Sample\ap_rx71m_0a_usbfunc_sample_cs\src\AP_RX71M_0A.c  
    insert wolfSSL_init() in UsbfInit().
```
    CanInit();
    SciInit();
    EthernetAppInit();
    UsbfInit();
    wolfSSL_init(); <- insert this line
```

  - Modify stack and heap size in ap_rx71m_0a_sample_cs\Sample\ap_rx71m_0a_usbfunc_sample_cs\src\smc_gen\r_config\r_bsp_config.h
```
    Line 154 #pragma stacksize su=0x2000
    Line 175 #define BSP_CFG_HEAP_BYTES  (0xa000)
```
  - Modify IP address Sample\ap_rx71m_0a_usbfunc_sample_cs\src\tcp_sample\config_tcpudp.c as needed

```
   #define MY_IP_ADDR0     192,168,1,200           /* Local IP address  */
   #define GATEWAY_ADDR0   192,168,1,254           /* Gateway address (invalid if all 0s) */
   #define SUBNET_MASK0    255,255,255,0
```
  - Add project properties of linking library in ap_rx71m_0a_usbfunc_sample_cs.mtpj  
    wolfssh\ide\Renesas\cs+\Projects\wolfssl_lib\DefaultBuild\wolfssl_lib.lib  
    wolfssh\ide\Renesas\cs+\Projects\wolfssh_lib\DefaultBuild\wolfssh_lib.lib  
　  wolfssh\ide\Renesas\cs+\Projects\demo_server\DefaultBuild\demo_sever.lib 
    
  - Set CC-RX(Build Tool)->Library Geberation->Library Configuration to"C99" and enable ctype.h.

  - Build the project and start execut. You see message on the console prompting command.
```
    Start server_test
```
  - wolfSSH simple server will be open on port 50000 which can be connected to by using the example client bundled with wolfSSH
```
    $ ./examples/client/client -h 192.168.1.200 -p 50000 -u jill
    Sample public key check callback
    public key = 0x55a0890864ea
    public key size = 279
    ctx = You've been sampled!
    Password: <---- input "upthehill"
    Server said: Hello, wolfSSH!
```
## Support

Email us at [support@wolfssl.com](mailto:support@wolfssl.com).
