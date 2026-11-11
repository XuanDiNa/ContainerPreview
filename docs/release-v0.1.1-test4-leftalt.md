# ContainerPreview 0.1.1-test4-leftalt

## English

**Prerelease — keyboard-binding update; gameplay acceptance pending.**
This release updates ContainerPreview only. ModMenu and the other six plugins
are not included.

- Add ModMenu v5 keyboard bindings: all seven Ctrl / Shift / Left Alt modifier
  combinations, plus `NONE` to unbind. Requires the binding editor from
  ModMenuBedrock **0.6.0-test5-leftalt** or separately verified compatible UI.
- Keep hold-to-preview: press modifiers before the main key and release any
  required key to stop. Extra modifiers and late modifier presses do not activate
  a binding. The existing Shift/native-sneak background-opening guard stays on.
- Include the earlier friend-invitation route correction, retaining session and
  ownership validation. Exact native image/entry checks remain unchanged.

**Update an existing installation:** exit Minecraft, back up the current plugin
folder outside `mods`, then merge the archive's `mods/ContainerPreview`. Replace
the DLL, manifest and matching descriptions together. Keep your existing
`config.json` and logs; this update ZIP includes no user configuration. Do not
delete the old folder or load two DLL copies. For a first installation, use
stable 0.1.0 first. Roll back the DLL, metadata and config backup as a set.

Retains the existing exact profiles for 1.26.45.01 / 1.26.51.01 / 1.26.52.03;
it does not support arbitrary builds sharing those labels. The currently
available unreviewed 26.51 launcher image is deliberately rejected, not added
to the whitelist. Use 26.52 for the initial binding smoke test.

Fresh Debug/Release checks and package verification are offline evidence, not
gameplay proof. Earlier acceptance of stable 0.1.0 or the separate friend-world
candidate is not transferred to this DLL. Stable 0.1.0 remains unchanged.
See the [README](https://github.com/XuanDiNa/ContainerPreview),
[focused manual checks](https://github.com/XuanDiNa/ContainerPreview/blob/v0.1.1-test4-leftalt/docs/keybindings.md)
and [verification](https://github.com/XuanDiNa/ContainerPreview/blob/v0.1.1-test4-leftalt/docs/verification.md).

Download `ContainerPreview-0.1.1-test4-leftalt-update.zip`, not GitHub's automatic
source-code archives. SHA-256:
`614E5CE9660190E152E6DAF79C0A7DA9540BC021843876638645D9D2E95D6755`.
The shipped DLL is the frozen matching-bundle file, not the verification rebuild:
`9C38BD8AF77D461F6BB940ED80F485DAAF69A8825486CCC19F3D36E0848A0083`.

## 繁體中文

**預發佈版：組合鍵配套更新，遊戲驗收尚未完成。**
此包只更新 ContainerPreview，不包含 ModMenu 或其餘六個插件。

- 新增 ModMenu v5 組合鍵：Ctrl／Shift／左 Alt 的七種組合，以及 `NONE` 解除
  綁定。編輯介面使用 ModMenuBedrock **0.6.0-test5-leftalt**，其他版本需另行驗證。
- 維持按住預覽，先按修飾鍵再按主鍵，放開任一必要鍵即停止。多按修飾鍵或
  先按主鍵再補修飾鍵不觸發；保留原有 Shift／潛行背景開箱保護。
- 包含較早的朋友邀請世界路徑修正，仍檢查工作階段與擁有者，沒有放寬原生
  映像或入口保護。

**升級既有安裝：**退出遊戲，將目前插件備份到 mods 外，再合併包內
`mods/ContainerPreview`；DLL、manifest 與設定描述檔一起更新。保留原本的
`config.json` 與日誌，本升級包不含使用者設定。不要刪除整個舊資料夾，也不要
同時載入兩份 DLL。首次安裝先使用正式版 0.1.0。回退時一併還原 DLL、中繼資料
及設定備份。

保留 1.26.45.01／1.26.51.01／1.26.52.03 的既有精確映像設定，不是所有同名
版本都相容。目前本機 26.51 啟動器映像雜湊未經核實，刻意保持拒絕，沒有擴大
白名單。首次組合鍵測試請用 26.52。

Debug／Release 及封裝檢查僅為離線證據，不代表遊戲驗收。正式版或較早朋友世界
候選版的實測結果不套用到此 DLL。原正式版 0.1.0 與下載資產保持不變。
下載插件 ZIP，不是自動產生的 Source code；ZIP 與 DLL 的 SHA-256 見上方。
安裝、人工測試與回退細節附於包內中英文 `UPDATE.md`。
