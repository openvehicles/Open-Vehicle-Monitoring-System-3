wolfSSL/AlphaProject�{�[�h�f���@�Z�b�g�A�b�v�K�C�h

���̃f���͈ȉ��̊��Ńe�X�g���Ă��܂��B

  Renesas : CS+ v6.01, v8.01
  Board   : AP-RX71M-0A
  wolfSSL : 3.15.3, 4.0.0

�Z�b�g�A�b�v�菇�F

�P�D�\�t�g�E�F�A�̓���
�@- AP�{�[�h�t���̃\�t�g�E�F�A�ꎮ��K���ȃt�H���_�[���ɉ𓀂��܂��B
�@- �����t�H���_�[����wolfssl�ꎮ���𓀂��܂��B

�Q�DwolfSSL�̃Z�b�g�A�b�v
�@- CS+�ɂ�wolfssl\IDE\Renesas\cs+\Project����wolfssl\wolfssl_lib.mtpj���J��
�@�@wolfSSL���C�u�����[�̃r���h�����܂��B
�@- �����t�H���_�̉���t4_demo.mtpj���J���A�f���v���O�����̃r���h�����܂��B
�@���̃v���O���������C�u�����[�`���Ńr���h����܂��B

�R�DAlphaProject���̃Z�b�g�A�b�v
  
  !!** �T���v���v���O���� v2.0 ���g�p����ꍇ�́A_ether_ => _usbfunc_ **!!
  !!** �ƒu�������Ă�������                                           **!!

�@�f����ap_rx71m_0a_sample_cs\Sample\ap_rx71m_0a_ether_sample_cs�t�H���_����
�@ap_rx71m_0a_ether_sample_cs.mtpj�v���W�F�N�g�𗘗p���܂��B
�@
�@- ap_rx71m_0a_sample_cs\Sample\ap_rx71m_0a_ether_sample_cs\src�t�H���_����
�@AP_RX71M_0A.c�t�@�C�����J���A
�@�X�V�s�ڂ�echo_srv_init()�̉���wolfSSL_init()��}�����܂��B

===
        sci_init();
        can_init();
        echo_srv_init();
        wolfSSL_init(); <- ���̍s��}��
===

!!** �T���v���v���O���� v2.0 ���g�p����ꍇ�́A���L                   **!!
===
        CanInit();
        SciInit();
        EthernetAppInit();
        UsbfInit();
        wolfSSL_init(); <- ���̍s��}��
===
!!**********************************************************************!!

�@- ap_rx71m_0a_sample_cs\Sample\ap_rx71m_0a_ether_sample_cs\src\smc_gen\r_bsp_config.h
�@���J���A�X�^�b�N�T�C�Y�ƃq�[�v�T�C�Y���ȉ��̂悤�ɐݒ肵�܂��B
�@
�@120�s�� #pragma stacksize su=0x2000
�@139�s�� #define BSP_CFG_HEAP_BYTES  (0xa000)

!!** �T���v���v���O���� v2.0 ���g�p����ꍇ�́A���L                   **!!
�@- ap_rx71m_0a_sample_cs\Sample\ap_rx71m_0a_usbfunc_sample_cs\src\smc_gen\r_bsp_config.h
�@���J���A�X�^�b�N�T�C�Y�ƃq�[�v�T�C�Y���ȉ��̂悤�ɐݒ肵�܂��B
�@154�s�� #pragma stacksize su=0x2000
�@175�s�� #define BSP_CFG_HEAP_BYTES  (0xa000)
!!**********************************************************************!!

�@- IP�A�h���X�̃f�t�H���g�l�͈ȉ��̂悤�ɂȂ��Ă��܂��B
�@�K�v������΁ASample\ap_rx71m_0a_ether_sample_cs\src\r_t4_rx\src\config_tcpudp.c
�@����139�s�ڂ���̒�`��ύX���܂��B
�@!!** �T���v���v���O���� v2.0 ���g�p����ꍇ�́A���L                   **!!
  Sample\ap_rx71m_0a_usbfunc_sample_cs\src\tcp_sample\src\config_tcpudp.c
  ����166�s�ڂ���̒�`��ύX���܂��B
  !!**********************************************************************!!

===
#define MY_IP_ADDR0     192,168,1,200           /* Local IP address  */
#define GATEWAY_ADDR0   192,168,1,254           /* Gateway address (invalid if all 0s) */
#define SUBNET_MASK0    255,255,255,0           /* Subnet mask  */
===


�@- CS+��ap_rx71m_0a_ether_sample_cs.mtpj�v���W�F�N�g���J���AwolfSSL�ƃf�����C�u������
�@�o�^���܂��BCC-RX(�r���h�c�[��)->�����N�E�I�v�V�����^�u->�g�p���郉�C�u������
�@�ȉ��̓�̃t�@�C����o�^���܂��B
�@wolfssl\IDE\Renesas\cs+\Projects\wolfssl_lib\DefaultBuild\wolfssl_lib.lib
�@wolfssl\IDE\Renesas\cs+\Projects\t4_demo\DefaultBuild\t4_demo.lib

- CC-RX(�r���h�c�[��)->���C�u�����[�W�F�l���[�V�����^�u->���C�u�����[�\�����uC99�v�ɁA
ctype.h��L���ɂ�����u�͂��v�ɐݒ肵�܂��B

�@- �v���W�F�N�g�̃r���h�A�^�[�Q�b�g�ւ̃_�E�����[�h�������̂��A�\��->�f�o�b�O�E�R���\�[��
�@����R���\�[����\�������܂��B���s���J�n����ƃR���\�[���Ɉȉ��̕\�����o�͂���܂��B
�@
===
�@wolfSSL Demo
t: test, b: benchmark, s: server, or c <IP addr> <Port>: client
$
===

t�R�}���h�F�e�Í����A���S���Y���̊ȒP�ȃe�X�g�����s���܂��B���v�̃A���S���Y����
�@�g�ݍ��܂�Ă��邩�m�F���邱�Ƃ��ł��܂��B�g�ݍ��ރA���S���Y���̓r���h�I�v�V����
�@�ŕύX���邱�Ƃ��ł��܂��B�ڂ����̓��[�U�}�j���A�����Q�Ƃ��Ă��������B
b�R�}���h�F�e�Í��A���S���Y�����Ƃ̊ȒP�ȃx���`�}�[�N�����s���܂��B
s�R�}���h�F�ȒP��TLS�T�[�o���N�����܂��B�N������ƃr���h����IP�A�h���X�A
�@�|�[�g50000�ɂ�TLS�ڑ���҂��܂��B
c�R�}���h�F�ȒP��TLS�N���C�A���g���N�����܂��B�N������Ƒ��A�[�M�������g�Ŏw�肳�ꂽ
�@IP�A�h���X�A���A�[�M�������g�Ŏw�肳�ꂽ�|�[�g�ɑ΂���TLS�ڑ����܂��B

������̃R�}���h���P��̂ݎ��s���܂��B�J��Ԃ����s�������ꍇ�́AMPU�����Z�b�g����
�ċN�����܂��B

�S�D�Ό��e�X�g
�@�f���̂��A���R�}���h���g���āA���̋@��ƊȒP�ȑΌ��e�X�g�����邱�Ƃ��ł��܂��B
�@Ubuntu�Ȃǂ�GCC, make���AWindows��Visual Studio�Ȃǂ�
�@�Ό��e�X�g�p�̃T�[�o�A�N���C�A���g���r���h���邱�Ƃ��ł��܂��B

�@GCC,make�R�}���h���ł́A�_�E�����[�h�𓀂���wolfssl�̃f�B���N�g�����ňȉ���
�@�R�}���h�𔭍s����ƁA���C�u�����A�e�X�g�p�̃N���C�A���g�A�T�[�o�Ȃǈꎮ���r���h
�@����܂��B
�@
�@$ ./configure
�@$ make check
�@
�@���̌�A�ȉ��̂悤�Ȏw��ŃN���C�A���g�܂��̓T�[�o���N�����āA�{�[�h���
�@�f���ƑΌ��e�X�g���邱�Ƃ��ł��܂��B
�@
�@PC���F
�@$ ./examples/server/server -b -d
�@�{�[�h���F
�@�@> c <IP�A�h���X> 11111

�@�{�[�h���F
�@�@> s
�@PC���F�@
�@$ ./examples/client/client -h <IP�A�h���X> -p 50000
�@
�@
�@Windows��Visual Studio�ł́A�_�E�����[�h�𓀂���wolfssl�t�H���_����wolfssl64.sln
�@���J���A�\�����[�V�������r���h���܂��BDebug�t�H���_���Ƀr���h�����client.exe��
�@server.exe�𗘗p���܂��B
�@
  PC���F
�@Debug> .\server -b -d
�@�{�[�h���F
�@�@> c <IP�A�h���X> 11111

�@�{�[�h���F
�@�@> s
�@PC���F
�@Debug> .\client  -h <IP�A�h���X> -p 50000

�ȏ�A