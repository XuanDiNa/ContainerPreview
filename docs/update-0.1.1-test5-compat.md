# ContainerPreview 0.1.1-test5-compat

## English

This update fixes refusal of the reviewed current **1.26.51.01 LeviLauncher
image**. Support for exact **1.26.45.01 / 1.26.52.03** and all seven ModMenu
Ctrl / Shift / Left Alt combinations is retained. ModMenu's binding editor
uses **0.6.0-test5-leftalt** or a separately verified compatible version.

Only ContainerPreview is included. Close Minecraft, back up the old plugin
folder outside `mods`, then merge the ZIP's `mods/ContainerPreview`. Replace
the DLL and matching metadata together; keep your existing `config.json` and
logs. No user configuration is included. For a first installation, install
stable 0.1.0 before applying this update. Do not load two DLL copies.

The 26.51 current launcher retains every byte and section layout of the reviewed
original image and adds a non-executable `.levildr` import section. Both reviewed
launcher hashes remain pinned; file size, PE identity, complete `.text`, native
entries and loaded-header checks remain required. An arbitrary EXE with the
same version or matching code section is still refused.

### Focused manual check

1. Start 26.45, then 26.51 separately. Confirm the log starts with
   `0.1.1-test5-compat` and reaches Ready.
2. In a single-player world, aim at an ordinary chest and hold Left Alt. Release
   it to hide the preview, then open and close the chest normally.
3. Apply `CTRL+K` in Configure. Hold Ctrl before K to preview, release either to
   hide, and reopen Configure to confirm persistence. K alone must not activate.
4. Check an Ender Chest and a permitted server/friend-world session when available.
   Report the version and context for a refusal, stuck UI or crash.

Hold-to-preview, unbinding with `NONE`, and the Shift/native-sneak background-open
pause are unchanged. The full optional binding checklist remains in
[keybindings.md](keybindings.md); its test4 artifact hashes are historical.
To roll back, exit Minecraft and restore the previous DLL, metadata and config
backup together.

Fresh offline tests cover all six current original/launcher images. This **new
DLL still awaits gameplay acceptance**; previous manual results do not certify
it. See [verification](verification.md) for artifact hashes and coverage.

## 繁體中文

此版修復目前已核實的 **1.26.51.01 LeviLauncher 映像被拒絕載入**，保留精確
**1.26.45.01／1.26.52.03** 支援，以及 ModMenu Ctrl／Shift／左 Alt 的七種組合。
綁定介面使用 **0.6.0-test5-leftalt**，其他版本需另行驗證。

包內僅有 ContainerPreview。退出遊戲，將舊插件備份到 mods 外，再合併包內
`mods/ContainerPreview`，DLL 與配套描述檔一起更新；保留原本 `config.json`
及日誌。本包不含使用者設定；首次安裝先裝正式版 0.1.0。不要同時載入兩份 DLL。

目前 26.51 啟動器保留已核實原版的全部原有區段與位元組，只新增不可執行的
`.levildr` 匯入區段。保留兩個精確啟動器雜湊，仍檢查完整檔案、大小、PE 身分、
程式碼、原生入口與載入標頭；不是只看版本名稱或程式碼相同就放行。

### 最小人工測試

1. 分別啟動 26.45、26.51，確認日誌版本為 `0.1.1-test5-compat` 並到達 Ready。
2. 單人世界對準普通箱子，按住左 Alt 預覽，放開隱藏，再正常開關一次箱子。
3. Configure 套用 Ctrl＋K，先按 Ctrl 再按 K 預覽，放開任一鍵隱藏；重開設定
   確認保留，單按 K 不觸發。
4. 再檢查終界箱；方便時測允許使用的伺服器／朋友世界。若拒絕、介面卡住或
   閃退，請附遊戲版本與環境。

仍維持按住預覽、`NONE` 解除綁定，以及 Shift／潛行時暫停新的背景開箱。
完整可選組合鍵清單見 [keybindings.md](keybindings.md)，其中 test4 雜湊為歷史紀錄。
回退時退出遊戲，一起還原舊 DLL、配套描述檔與設定備份。

六個目前原始／啟動器映像均完成離線檢查，**此新 DLL 尚待遊戲驗收**；舊版人工
結果不直接套用。檔案雜湊與覆蓋範圍見[驗證紀錄](verification.md)。
