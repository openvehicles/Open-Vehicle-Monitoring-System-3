# MQX向けビルド方法
## 概要
このMakefileはwolfSSHライブラリーとサンプルプログラムをMQX向けにビルドするためのものです。
以下のターゲットを含んでいます。
 - wolfsshlib: wolfSSH静的ライブラリー
 - echoserver: Echoサーバサンプルプログラム


## 準備
- 事前にMQXをインストールしておいてください。
- 事前にwolfSSHを有効化したwolfSSLの静的ライブラリーをビルドしておいてください。

## 設定
- wolfSSH コンフィグレーションオプション
　<wolfSSH-root>/ide/MQX/user_settings.hファイルに必要なオプションを追加または削除してください。

- Makefileの設定
  WOLFSSL_ROOT:wolfSSLソースコードのルート
  WOLFSSH_ROOT:Makefileの格納位置を変える場合はこの定義を変更してください
  MQX_ROOT: MQX のインストールパス
  MQXLIB:   リンクするMQX ライブラリのパス
  CC:       コンパイラコマンド
  AR:       ARコマンド

  