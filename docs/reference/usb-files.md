# USB-Cファイル転送（MGF1 v1）

有線の開発・検証用入口。日常の転送はWi‑Fi Webでも同じ検証・保護・上書き規則を使う。USB storageとしてmountせず、GBX1へファイル命令を追加しない。認証codeは不要。物理ケーブルを持つホストがOPENし、録音／ファイル処理／更新が動いていなければ演奏を停止して専用画面へ入る。CLOSE、画面のCLOSE／BROWSER、切断または30秒無通信で終了。通常操作と音声装置の再接続はその後に戻る。Wi‑FiとUSBの同時セッションは不可。

## CLI

Python 3.11以上とPyUSB／libusbを使う（`python3 -m pip install pyusb`）。LinuxのUSB権限が必要な場合は既存updateツールと同じudev設定またはsudoを使う。CDC ttyのreaderと同時に開かない。CLIはinterface 1を一時claimし、終了時にcdc_acmを戻す。

```sh
python3 tools/files/usb_files.py list projects
python3 tools/files/usb_files.py put Song.mgxs
python3 tools/files/usb_files.py get PROJECTS/Song.mgxs received.mgxs
python3 tools/files/usb_files.py delete PROJECTS/Song.mgxs
```

各呼出しはOPEN→操作→CLOSE。PUT／GETはbyte数・SHA256・時間をJSON表示し、エラーはstderrと非zero終了。GETはhostの`.part`へ書いて完了後に指定出力名へ置換し、中断した自分の`.part`だけ除去する。機器側のPUTも完全受信・保存済みhash・型検証を終えてから同名を置換する。現在Project／全宣言参照／標準素材の同一filesystem entryはPUT・DELETEとも拒否。許可棚はPROJECTS／PRESETS／KITS／ASSETSのSAMPLES・SOUNDFONTS・MIDI／RECORDINGS。PUTはContentKindやRIFF等の中身に従い自動配置し、任意pathは受け取らない。旧MGXCは拒否。拡張子とMGXC ContentKindの食い違いでは中身を正とする。

## wire

VID:PID=1209:0001、CDC data interface 1、OUT=0x02／IN=0x83。24-byte little endian header: `MGF1`、version u8=1、opcode u8、status u16（要求0）、request ID u32、payload長u32、payload FNV-1a32、先頭20 bytesのFNV-1a32。応答opcodeは要求|0x80、IDは一致。FNVはframe破損検出で、PUTの内容保証はSHA256。request payload最大4096、response最大32776。

| opcode | payload | 成功応答 |
|---|---|---|
| 1 OPEN | 空 | 空 |
| 2 CLOSE | 空 | 空 |
| 3 LIST | shelf u8、first u32 | total u32、next u32、各entry: bytes u64・path長u8・UTF-8 path |
| 4 GET | offset u64、wanted u32≤32752、NUL path | total u64、offset u64、data |
| 5 PUT BEGIN | length u64、SHA256 32 bytes、NUL filename | 空 |
| 6 PUT DATA | offset u64、data≤4088 | 空 |
| 7 PUT COMMIT | 空 | NUL destination |
| 8 DELETE | NUL path | 空 |
| 9 CANCEL | 空 | 空 |

requestは1本ずつ応答を待つ。4096-byte上限はCircle CDCの8192-byte receive ringに合わせ、bulk受信overrunを避ける。DATAは機器側32KiBへ結合してSDへ保存し、100MBでも全量をRAMへ載せない。DATAの応答は受信確認で、永続化完了を意味しない。COMMIT成功のみ保存完了。LISTは最大1024 entries／96 directories、境界を超えるとLIMIT。statusはUserFileErrorの値、128=INVALID_FRAME、129=UNSUPPORTED_VERSION。専用frameの途中はGBX1やGUP1へ渡さない。更新中のOPENはBUSY、ファイル中のupdate ENTERも拒否する。GBX1 STATUS／LOGは従来どおり制御用。更新の署名・journal・recoveryは変更しない。

## 置換と復旧の限界

MRP1 plan・旧候補・新候補はサービス専用の一時transactionで、成功／次回起動復旧で消去する。復旧はProjectのloadより先、未検証候補で曲を開始しない。復旧の前提は、各filesystem操作が原子的・永続であること。FAT metadataの破断やSD controller内部cacheまで含む実電源断は保証しない。

MGF1は通常appの機能。既設のimmutable recoveryは署名更新専用で、MGF1からは書き換えない。appの転送モードからのコンテンツ修復と、recoveryからのファームウェア復旧を区別する。

INVALID_FRAMEに24-byte詳細が付く場合は、reason u32（1=header、2=payload checksum、3=DATA extent、4=要求flags）、expected u32、actual u32、payload長u32、受理済みoffset u64。CLIはこの診断をエラーへ添えます。エラーを成功へ読み替えたり自動的に再送して上書きしたりしません。

専用frameのpumpは利用可能な入出力を1 turn最大16操作・各4096 bytesへまとめ、空read／backpressureで即座に戻ります。通常演奏中のI/Oや無制限のpoll loopを増やしません。USB gadgetのOUT／SETUPはDMA完了後にcache invalidateを行ってからCPUへ引き渡します。

