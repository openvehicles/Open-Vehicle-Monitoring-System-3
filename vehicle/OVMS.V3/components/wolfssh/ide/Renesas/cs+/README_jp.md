# wolfSSH シンプル SSH サーバ セットアップガイド

このデモは以下の環境でテストしています。  

* Renesas : CS+ v8.01
* Board   : Alpha Project AP-RX71M-0A w/ Sample program v2.0
* wolfSSL : 4.0.0
* wolfSSH : 1.3.1

## セットアップ手順：
### １ ソフトウェアの入手

- APボード付属のソフトウェア一式を適当なフォルダー下に解凍します。  
- 同じフォルダー下にwolfssl一式を解凍します。
- 同じフォルダー下にwolfssh一式を解答します。
### ２ wolfSSL及びwolfSSHのセットアップ

- CS+にてwolfssh\ide\Renesas\cs+\下のwolfssl_lib\wolfssl_lib.mtpjを開き  
  wolfSSLライブラリーのビルドをします。
- CS+にてwolfssh\ide\Renesas\cs+\下のwolfssh_lib\wolfssj_lib.mtpjを開き  
  wolfSShライブラリーのビルドをします。
- 同じフォルダの下のdemo_server.mtpjを開き、デモプログラムのビルドをします。  
  このプログラムもライブラリー形式でビルドされます。

### ３ AlphaProject側のセットアップ
デモはap_rx71m_0a_sample_cs\Sample\ap_rx71m_0a_usbfunc_sample_csフォルダ下の  
ap_rx71m_0a_usbfunc_sample_cs.mtpjプロジェクトを利用します。

- ap_rx71m_0a_sample_cs\Sample\ap_rx71m_0a_ether_sample_cs\srcフォルダ下のAP_RX71M_0A.cファイルを開き、  
  UsbfInit()の下にwolfSSL_init()を挿入します。

```
        CanInit();
        SciInit();
        EthernetAppInit();
        UsbfInit();
        wolfSSL_init(); <- この行を挿入
```
- ap_rx71m_0a_sample_cs\Sample\ap_rx71m_0a_usbfunc_sample_cs\src\smc_gen\r_config\r_bsp_config.h  
  を開き、スタックサイズとヒープサイズを以下のように設定します。  
　154行目 #pragma stacksize su=0x2000  
　175行目 #define BSP_CFG_HEAP_BYTES  (0xa000)  

- IPアドレスのデフォルト値は以下のようになっています。  
　必要があれば、Sample\ap_rx71m_0a_ether_sample_cs\src\tcp_sample\config_tcpudp.c
　内の139行目からの定義を変更します。

```
       #define MY_IP_ADDR0     192,168,1,200           /* Local IP address  */
       #define GATEWAY_ADDR0   192,168,1,254           /* Gateway address (invalid if all 0s) */
       #define SUBNET_MASK0    255,255,255,0           /* Subnet mask  */
```
- CS+でap_rx71m_0a_usbfunc_sample_cs.mtpjプロジェクトを開き、wolfSSL、wolfSSH及びデモライブラリを  
　登録します。CC-RX(ビルドツール)->リンク・オプションタブ->使用するライブラリに  
　以下の二つのファイルを登録します。

 - CC-RX(ビルドツール)->ライブラリージェネレーションタブ->ライブラリー構成を「C99」に、  
    ctype.hを有効にするを「はい」に設定します。

- プロジェクトのビルド、ターゲットへのダウンロードをしたのち、表示->デバッグ・コンソール  
　からコンソールを表示させます。実行を開始するとコンソールに以下の表示が出力されます。
```
    Start server_test
```
- シンプル wolfSSH サーバは、50000番のポートを開いて待ちます。サーバへは、wolfSSHに付サンプルクライアントを  
使って以下のように接続することができます。
```
    $ ./examples/client/client -h 192.168.1.200 -p 50000 -u jill
    Sample public key check callback
    public key = 0x55a0890864ea
    public key size = 279
    ctx = You've been sampled!
    Password: <---- input "upthehill"
    Server said: Hello, wolfSSH!
```

##　サポート
サポートが必要な場合は、[support@wolfssl.com](mailto:support@wolfssl.com)へご連絡ください。

以上
