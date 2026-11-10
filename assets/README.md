# サンプル・Factory素材

素材の正本は生成コードとsource lockです。現在のProjectコンテナは[MGXC](../docs/project-format.md)。新規保存名は曲`.mgxs`・Kit`.mgxk`・Preset`.mgxp`。`.mgx`も含め、読込み種別は内部TYPEを正とします。

- `samples/demo-slices.wav`: オリジナルのmono PCM16 four-tone sample。[生成スクリプト](../scripts/create-demo-assets.py)で再現します。
- `factory/`と開発用 `kits/Techno-01.mgxk`: `make factory-content`で生成する検証素材。既存115件の公式原本は `factory-library/`。追加のVA／FMドラム原本と焼いたKitは [factory-synth-drums](factory-synth-drums/README.md)、DAISY単体6音色は `src/future_factory_presets.h`。いずれも共通Factory catalogから設置する。旧 `presets/`生成は停止。外部素材の固定元とlicenseは[source lock](../tools/content/factory_sources.lock.json)と各NOTICEを参照してください。
- 旧デモ曲の制作・移行は中止済みです。標準Kit・Debugは`make factory-catalog`で`build/factory-data/FACTORY/`へ生成し、実機も同じ定義から準備します。旧`demo-projects/`と非互換コンテナは廃止。[DATA構造](../docs/data-content.md)。
- `fonts/`: Barlow Semi Condensedの共有A4 glyph atlasとOFL notice。全役割で同じ書体を使い、数字はtnumを焼込み。日本語の説明書とは別に、製品UIは英語です。

Projectの素材参照はコンテナ相対で保存し、runtimeでcanonical pathへ解決します。持ち出しは[説明書のCOLLECT](../docs/user-manual.ja.md)を使い、フォルダー全体を渡します。音声bufferとUndo履歴はProjectへ埋め込みません。

デモ生成が追跡対象素材を変更する場合があります。テストの副作用を製品変更としてcommitしないでください。評価曲を意図して変更する場合はrevision／hash／負荷を更新し、別workloadの結果を同一条件として比較しません。

`assets/`は来歴を保持する生成入力です。配布するDATAは`make canonical-catalog`で`build/factory-data/`へ作り、ディレクトリ名を大文字に統一します。FactoryのNOTICEと対応するlibrary.jsonは出典のため同梱し、測定CSVや開発用画像はSDへコピーしません。

旧4エンジン撤去に伴い、開発用WAVの旧生成元をDAISY／FM／VAへ変更した。`factory/`の808／909等のファイル名は検証fixtureの識別名であり、ベンダー実機互換の音を主張しない。この変更で再生成した8 WAVは意図した新fixtureで、既存115原本やその共有manifest／packは変更しない。

起動表示は`boot/splash-yadokari-480x272.png`を原寸のままRGB565へ量子化します。`tools/ui/boot_splash.py`でinclude／preview／manifestを生成し、版数は本体が実行時に追加。[契約と出典](../docs/splash-yadokari.md)。
