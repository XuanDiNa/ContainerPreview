# ContainerPreview

[English](#english) · [繁體中文](#繁體中文)

## English

Hold **Left Alt** while looking at a world container to preview its contents with native item icons and stack counts, without showing its inventory screen.

A client-side native DLL loaded through LeviLauncher. No behavior pack, server plugin, or companion protocol is required.

[Stable 0.1.0](https://github.com/XuanDiNa/ContainerPreview/releases/tag/v0.1.0) · [Latest prerelease](https://github.com/XuanDiNa/ContainerPreview/releases/tag/v0.1.1-test5-compat) · [Report an issue](https://github.com/XuanDiNa/ContainerPreview/issues)

### Features

- **World-container preview:** ordinary and double chests, plus your own 27-slot Ender Chest.
- **Native background opening:** request current contents from normally accessible containers, including supported held-item states.
- **Game modes:** Survival, Creative and Adventure; sneaking pauses new background requests.
- **Additional single-player containers:** read-only adapters for more vanilla blocks and container entities.
- **Hive death crates:** experimental preview for `hivesky:death_crate`, not arbitrary custom entities.
- **Configurable controls:** enable/disable and hold-key settings with automatic reload; the prerelease adds ModMenu Ctrl / Shift / Left Alt combinations and unbinding.

### Requirements

- Windows x64
- Minecraft **1.26.45.01 / 1.26.51.01 / 1.26.52.03**
- LeviLauncher with native DLL plugin support

One DLL supports these three exact game versions. Other builds are not supported; failed image or native-entry checks leave the plugin inactive.

The keyboard-binding prerelease uses ModMenuBedrock **0.6.0-test5-leftalt** for its binding editor. **0.1.1-test5-compat** also supports the reviewed current 26.51 launcher image; all six currently available original/launcher images pass offline checks. See [current verification](docs/verification.md).

### Installation

1. Fully close Minecraft. If upgrading, back up the complete old plugin folder **outside the mods directory**.
2. Download the plugin ZIP from [Releases](https://github.com/XuanDiNa/ContainerPreview/releases). The automatic source-code archives are not installable packages.
3. For stable 0.1.0, create `mods/ContainerPreview` and extract its eight flat files. The prerelease is an **update-only** ZIP: merge its `mods/ContainerPreview` into an existing installation.
4. When upgrading, replace the DLL, manifest and matching settings descriptions together. Keep your existing `config.json` and logs; the update-only ZIP includes no user config. Use the stable default config only for a first installation.

Do not replace DLLs while Minecraft is running or load old and new copies together. To roll back, close the game and restore the backup. This plugin cannot be imported as a `.mcpack`.

### Configuration

Edit `config.json` beside the DLL. The default is `{"enabled": true, "holdKey": "LALT"}`. Hold the key to preview; release it or look away to hide the preview. Settings reload automatically.

In the prerelease, Configure can save `CTRL+K`, `LALT+K`, or `CTRL+SHIFT+LALT+K`. Hold the modifiers before the main key; release any required key to stop. Esc during capture selects `NONE` (Unbound), saved only on Apply. Shift/sneaking still pauses new background opens. See [update and testing instructions](docs/update-0.1.1-test5-compat.md).

See the [default config](config.json) and [setting descriptions](config.schema.json). Release downloads include archive and per-file SHA-256 checksums.

### Notes and feedback

**0.1.1-test5-compat is a prerelease awaiting gameplay acceptance.** It retains the friend-world route correction and ModMenu keyboard bindings, and adds the reviewed current 26.51 launcher hash. The earlier friend-world candidate was accepted on 26.52; that result is not transferred to this DLL, and 26.45/26.51 friend-world gameplay remains untested.

Stable **0.1.0** remains available with its original three-version focused manual results. Neither release certifies all servers, containers, plugin combinations or long sessions; the additional single-player adapters have not all been individually tested.

Background opening follows normal interaction and access rules. It can play sounds, animate lids, trigger opening events and generate/save unopened loot. Not every **No data** result can be resolved automatically. Unverified special held items remain refused, and manual interaction takes priority.

Players, other players' inventories, arbitrary custom menus/entities, item-tooltip previews and Spectator mode are not supported. Hive support remains experimental; see [compatibility and limitations](https://github.com/XuanDiNa/ContainerPreview/blob/main/docs/compatibility.md).

The plugin does not move items, write inventory memory, simulate input, construct packets, modify the game EXE or bypass server permissions. Use it only where server rules allow.

Feedback and issue reports are welcome! Include the exact game version, single-player/server context, container and held-item type, and steps to reproduce. Review `ContainerPreview.log` before sharing: diagnostics stay local but can include coordinates and numeric container/nonplayer entity IDs.

The stable release retains its tested DLL (`0.1.0-test31` startup line). The prerelease ships the new verified compatibility build, with its own recorded hash. See [verification and binary provenance](docs/verification.md) and [build instructions](docs/building.md).

Inspired by [Tweakeroo Inventory Preview](https://github.com/maruohon/tweakeroo). This is an independent Bedrock adaptation, not an official port. See [third-party notices](THIRD_PARTY_NOTICES.txt).

## 繁體中文

準星看著世界容器，**按住左 Alt**，不用顯示容器介面就能查看原生物品圖示與堆疊數量。

透過 LeviLauncher 載入的客戶端原生 DLL 插件，不需要行為包、伺服器插件或配套協定。

[正式版 0.1.0](https://github.com/XuanDiNa/ContainerPreview/releases/tag/v0.1.0) · [最新預發佈版](https://github.com/XuanDiNa/ContainerPreview/releases/tag/v0.1.1-test5-compat) · [回報問題](https://github.com/XuanDiNa/ContainerPreview/issues)

### 主要功能

- **世界容器預覽**：普通箱子、大箱子，以及自己 27 格的終界箱。
- **原生背景開啟**：向正常允許存取的容器取得目前內容，支援已核實的持物狀態。
- **遊戲模式**：支援生存、創造與冒險；蹲下時暫停新的背景請求。
- **額外單人容器**：提供更多原版方塊及容器實體的唯讀適配。
- **Hive 死亡箱**：實驗性支援 `hivesky:death_crate`，不適用於任意自定義實體。
- **可自定義控制**：預覽鍵、啟用／停用與自動重載；預發佈版新增 ModMenu Ctrl／Shift／左 Alt 組合鍵與解除綁定。

### 環境需求

- Windows x64
- Minecraft **1.26.45.01／1.26.51.01／1.26.52.03**
- 支援原生 DLL 插件的 LeviLauncher

同一個 DLL 支援上述三個精確版本，不適用於其他小版本；遊戲映像或原生入口校驗不符時，插件不會啟用。

組合鍵預發佈版配合 ModMenuBedrock **0.6.0-test5-leftalt** 的綁定介面使用。**0.1.1-test5-compat** 已加入核實後的目前 26.51 啟動器映像；三個版本目前共六個原始／啟動器映像均通過離線檢查，詳見[目前驗證狀態](docs/verification.md)。

### 安裝方式

1. 完整退出 Minecraft。升級前，將整個舊插件資料夾備份到 **mods 目錄外面**。
2. 從 [Releases](https://github.com/XuanDiNa/ContainerPreview/releases) 下載插件 ZIP，不要下載自動產生的 Source code 壓縮檔。
3. 正式版 0.1.0：建立 `mods/ContainerPreview` 並解壓八個平鋪檔案。預發佈版是**僅供升級**的 ZIP，將包內 `mods/ContainerPreview` 合併到原有安裝。
4. 升級時一起更新 DLL、manifest 與配套設定描述檔，保留原有的 `config.json` 與日誌。升級包不含使用者設定；首次安裝才使用正式版預設設定。

不要在遊戲執行中替換 DLL，也不要同時載入新舊插件。需要回退時，退出遊戲後還原備份。本插件不能當作 `.mcpack` 匯入。

### 設定

編輯 DLL 同目錄的 `config.json`，預設為 `{"enabled": true, "holdKey": "LALT"}`。按住預覽鍵查看，放開或移開準星即隱藏；設定會自動重新載入。

預發佈版可在 Configure 設定 `CTRL+K`、`LALT+K` 或 `CTRL+SHIFT+LALT+K`。先按修飾鍵再按主鍵，放開任一必要鍵即停止。擷取時按 Esc 選 `NONE`（無綁定），套用才儲存；Shift／潛行仍會暫停新的背景開箱。詳見[更新與測試說明](docs/update-0.1.1-test5-compat.md)。

可參考[預設設定](config.json)與[設定欄位說明](config.schema.json)。Release 附有 ZIP 及包內各檔案的 SHA-256 校驗值。

### 注意事項與回饋

**0.1.1-test5-compat 是待遊戲驗收的預發佈版**，保留朋友世界路徑修正及 ModMenu 組合鍵，並加入核實後的目前 26.51 啟動器雜湊。較早的朋友世界候選版曾於 26.52 實測通過，但不將結果套用到此次 DLL；26.45／26.51 的朋友世界仍未人工測試。

正式版 **0.1.0** 及其原有三版本聚焦測試結果保持不變。兩者均不保證所有伺服器、容器、插件組合或長時間情境；額外單人適配尚未全部逐項實測。

背景開啟沿用正常互動與存取規則，可能播放聲音、顯示箱蓋動畫、觸發開箱事件，以及生成／保存未開過的戰利品。不是所有 **No data** 都能自動處理。未核實的特殊持物仍會拒絕，手動操作優先。

不支援玩家、其他玩家背包、任意自定義選單／實體、物品提示預覽或旁觀模式。Hive 支援仍屬實驗性，詳見[相容性與限制](https://github.com/XuanDiNa/ContainerPreview/blob/main/docs/compatibility.md)。

插件不搬物品、不直接改背包記憶體、不模擬輸入、不自製封包、不修改遊戲 EXE，也不繞過伺服器權限。請遵守所在伺服器的使用規則。

歡迎回報問題或提出建議！請附精確遊戲版本、單人／伺服器環境、容器及持物類型，以及重現步驟。分享 `ContainerPreview.log` 前請先檢查：診斷只保存在本機，但可能包含座標與數字容器／非玩家實體 ID。

正式版保留實際驗收的原 DLL（啟動日誌 `0.1.0-test31`）；預發佈版使用這輪通過離線驗證的相容性建置，並單獨記錄雜湊。詳見[驗證與 DLL 來源](docs/verification.md)及[建置說明](docs/building.md)。

本專案參考 [Tweakeroo Inventory Preview](https://github.com/maruohon/tweakeroo)，是獨立的基岩版適配實作，並非官方移植。第三方授權見[相關聲明](THIRD_PARTY_NOTICES.txt)。
