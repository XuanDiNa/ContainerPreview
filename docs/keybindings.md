# ContainerPreview keyboard-binding update

## English

Version: **0.1.1-test4-leftalt**, manual-test candidate. This standalone update
contains only ContainerPreview. Use the v5 binding UI in ModMenuBedrock
**0.6.0-test5-leftalt** (or a separately verified compatible version).

The DLL is the exact frozen ContainerPreview file from the matching binding
test bundle, not the rebuild used for offline verification. DLL SHA-256:
`9C38BD8AF77D461F6BB940ED80F485DAAF69A8825486CCC19F3D36E0848A0083`.

### Install

1. Exit Minecraft. Back up the existing ContainerPreview folder outside the
   active mods directory.
2. Merge this archive's `mods/ContainerPreview` into the existing plugin folder.
   Replace the DLL, `manifest.json`, `mod.json`, `modmenu.settings.json`,
   `config.schema.json`, notices and checksums together. Do not delete the folder.
3. Keep your existing `config.json` and logs. No user config is included.
   Restart Minecraft for DLL changes; Apply retains the 200 ms JSON reload.

### Focused manual test

- Keep the previous `LALT` binding first. Aim at an accessible ordinary chest
  and hold Left Alt; release it to hide the preview.
- In Configure, capture `CTRL+K` and Apply. Reopen to verify persistence.
  Hold Ctrl before pressing K: keep the full combination held to preview.
  K alone, extra Shift/Left Alt, or adding Ctrl after K must not activate it.
- Repeat with `LALT+K` and `CTRL+LALT+K`. Release any required key to stop.
  Returning from another application with K held must require a fresh release
  and press. Right Alt is not Left Alt.
- Esc during binding capture selects Unbound; Apply persists `NONE` and disables
  the preview key. Restore your preferred binding afterward. Cancel must not save.
- Check one normal chest open/close, held building blocks and, where permitted,
  a server/friend-world preview. No inventory changes, stuck UI or crash expected.

All seven Ctrl/Shift/Left Alt modifier combinations are readable. **The existing
Shift/native-sneak guard still pauses new background container opens**; a
Shift-containing binding is not a bypass. Use Ctrl+K or Left Alt+K for background
opening tests. The action remains hold-to-preview, not a toggle.

Native image/entry guards and the existing exact-version boundary are retained:
1.26.45.01 / 1.26.51.01 / 1.26.52.03, not arbitrary builds with those labels.
This round verifies the current 26.52 original/launcher pair, 26.45 pair and
26.51 original image offline. The currently available 26.51 launcher EXE has an
unreviewed hash and is deliberately rejected; do not treat it as supported.
No game, other plugin, IDA database or installed user setting is changed here.

Fresh build, contract, fixture and package checks are offline evidence only;
this keyboard-binding candidate still needs in-game acceptance. To roll back,
exit Minecraft and restore the old DLL, metadata and config backup as one set.
An older DLL may not understand newly saved combinations.

## 繁體中文

版本：**0.1.1-test4-leftalt**，人工測試候選版。這是僅含 ContainerPreview 的獨立
更新包，配合 ModMenuBedrock **0.6.0-test5-leftalt** 的 v5 組合鍵介面使用；其他
版本需另行驗證相容性。

保留配套測試包內原本的 DLL，不用這輪重新建置的 DLL 替換；其 SHA-256 見上方。

### 更新方式

1. 完全退出遊戲，將現有 ContainerPreview 資料夾備份到 mods 以外。
2. 合併包內 `mods/ContainerPreview`，一起更新 DLL、manifest、mod.json、
   `modmenu.settings.json`、`config.schema.json`、授權及校驗檔；不要刪除整個資料夾。
3. 保留原有 `config.json` 與日誌。本包沒有使用者設定檔。更換 DLL 後重啟遊戲；
   透過 Apply 儲存設定後，維持原有約 200 ms 熱重載。

### 最小人工測試

- 先保留原本左 Alt，對準可正常開啟的普通箱子，按住顯示、放開隱藏。
- Configure 綁定 Ctrl＋K 並套用，重開設定確認保留。先按 Ctrl 再按 K，持續按住
  完整組合才預覽；單按 K、多按 Shift／左 Alt、先按 K 再補 Ctrl 均不觸發。
- 再測左 Alt＋K、Ctrl＋左 Alt＋K；放開任一必要鍵即停止。切到其他程式再返回，
  主鍵仍按住時不得自行恢復，須放開再按；右 Alt 不可代替左 Alt。
- 擷取快捷鍵時按 Esc 選無綁定，套用後 `NONE` 應停用預覽鍵；之後改回慣用鍵。
  另試一次取消，確認不儲存。
- 確認正常開關箱、手持普通建築方塊，以及允許使用的伺服器／朋友世界預覽，
  沒有物品變動、介面卡住或閃退。

支援 Ctrl／Shift／左 Alt 的七種組合，但**原有 Shift／潛行保護仍會暫停新的背景
開箱**，不是用組合鍵繞過保護。背景開箱請用 Ctrl＋K 或左 Alt＋K 測試。功能仍是
按住預覽，不是切換開關。

保留既有三個精確版本的原生雜湊／入口檢查。本輪離線檢查包含 26.52、26.45 的
原始與啟動器映像，以及 26.51 原始映像；目前本機 26.51 啟動器 EXE 雜湊未經核實，
會繼續安全拒絕，不能僅因版本名稱相同就視為支援。

這輪未改動遊戲、其他插件、IDA 或已安裝設定。離線建置與測試不等於遊戲驗收。
回退時退出遊戲，一併還原舊 DLL、中繼資料與設定備份；舊 DLL 可能讀不了新組合鍵。
