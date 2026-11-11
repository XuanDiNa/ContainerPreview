# Current verification — 0.1.1-test5-compat

The compatibility candidate accepts all six currently available exact images:
original and launcher for **1.26.45.01 / 1.26.51.01 / 1.26.52.03**. The newly
reviewed 26.51 launcher preserves every original section descriptor and payload,
including code, vtables, data, imports and relocations. It adds a non-executable
loader import section. See the [static comparison](launcher-26.51-review.json).
The existing 26.51 native RVAs and entry guards are unchanged. Both reviewed
launcher hashes are pinned; no version-label or text-only admission was added.

Fresh public-copy Debug and Release runs each passed **24 executions / 4,427
checks / zero failures**, plus 18 Python artifact tests and 431 shared binding
checks. A regression first reproduced refusal of the current 26.51 launcher
(450 checks, one failure), then passed after the compatibility fix. Mutating
non-code bytes still fails complete-file verification. The first parallel
Debug build hit MSVC C1069; a serial continuation succeeded before testing.
The clean serial Release rebuild succeeded.

The new Release DLL is **554,496 bytes**, SHA-256
`E9D315D953C8ABFA2484D3304BE0AA0AD4CD2B7BC36F0BD374717C753BAC814F`.
The update ZIP is **271,694 bytes**, SHA-256
`BD4D067D6AAC899A41EA9B226E19DC5B3358B6F4F7DFC09EAF2ED0898276360D`.
The eight-entry update package contains only ContainerPreview and instructions;
user config is omitted. All 63 public/development build and runtime inputs
match byte-for-byte. Detailed hashes and the six-image matrix are in the
[verification record](release-artifacts-0.1.1-test5-compat.json).

**Gameplay acceptance of this new DLL is pending for all three versions.**
Earlier manual results are not transferred. The historical 26.51 launcher
hash remains supported in source but was unavailable for a fresh file check.
Stable 0.1.0 and test4 retain their original artifacts. See
[installation and focused testing](update-0.1.1-test5-compat.md).

繁體中文：本版修復目前已核實的 26.51 啟動器映像拒絕載入；26.45／26.51／26.52
目前六個原始與啟動器映像均通過離線檢查。靜態比對確認 26.51 原有九個區段
完全相同，僅增加不可執行的載入器匯入區段，因此沿用原有精確位址與入口保護，
並保留兩個完整啟動器雜湊，沒有只按版本或程式碼雜湊放行。
Debug／Release 各 4,427 項、18 項封裝測試、431 項組合鍵檢查通過，新增的
非程式碼竄改檢查也維持拒絕。Debug 初次並行建置遇到 C1069，改為循序建置後
成功再執行測試；Release 乾淨循序建置成功。新 DLL 與 ZIP 雜湊見上方，
63 個公開／開發輸入逐位元組一致。**三版本均仍待此新 DLL 的遊戲驗收**。
舊 26.51 啟動器雜湊保留，但這輪缺少該舊檔，未重新核對；正式版及 test4 資產不變。

# Historical verification — 0.1.1-test4-leftalt

This is a **prerelease**, not a replacement gameplay-accepted release. The
frozen matching-bundle DLL is 553,984 bytes, SHA-256
`9C38BD8AF77D461F6BB940ED80F485DAAF69A8825486CCC19F3D36E0848A0083`.
The update ZIP is 282,008 bytes, SHA-256
`614E5CE9660190E152E6DAF79C0A7DA9540BC021843876638645D9D2E95D6755`.
It contains only ContainerPreview, matching metadata and bilingual instructions;
no user settings, game files, private analysis or other plugins are included.

The development-copy Debug/Release verification on 2026-11-11 completed 24 test
executions / 4,412 checks per mode, all passing, plus 18 artifact tests and
431 shared binding-contract checks. Five reviewed exact images were accepted;
the currently available unreviewed 26.51 launcher was correctly rejected.
No guard was relaxed. These offline results are not gameplay acceptance.

Public-copy results and the clean source comparison are recorded in the
[prerelease verification record](release-artifacts-0.1.1-test4-leftalt.json).
The SDK header retains its MIT notice and [license](../include/sdk/LICENSE).
The earlier 26.52 friend-world acceptance belongs to a different DLL; 26.45/51
friend-world manual testing was waived and remains untested.

繁體中文：此版為預發佈候選版，沿用配套包原 DLL；檔案大小及獨立雜湊見上方。
開發副本 Debug／Release 各 4,412 項檢查、18 項封裝測試及 431 項共用組合鍵
檢查通過，不代表遊戲驗收。接受五個已核實映像，正確拒絕目前未核實的 26.51
啟動器映像，沒有放寬保護。朋友世界的舊版人工結果不套用到新 DLL；26.45／51
朋友世界仍未實測。公開副本驗證與重編檔來源另記於上述紀錄。

See [installation and focused testing](keybindings.md). Stable 0.1.0 and its
original assets remain unchanged; its historical evidence follows.

# Historical verification and binary provenance — 0.1.0

## English

### Fresh public-copy verification — 2026-10-23

The full public-copy verifier completed with exit 0. Both clean Debug/Release
builds succeeded: **48 C++ executions / 8742 checks / zero failures**, plus
**18 Python artifact tests**. No compiler warning/error was observed. All six
exact game images passed their profile tests and post-run hash checks.

The release ZIP is `ContainerPreview_0.1.0_MC26.45-26.51-26.52.zip`, 276631
bytes; SHA-256:
`BC2A899DEB38E457CCE8A58CEB34671E615FA672A1DB233EAF4D608251C7AC45`.
All eight unique flat member streams were checked against the frozen package;
all seven payloads match public inputs or the independently pinned retained DLL.
README SHA-256:
`2394C74398336AE6EE1DEC387EC8AA5A0C914D845CFCFC27BB459FCEA941A4F7`.

The fresh public Release DLL has SHA-256
`39178AB2DB5EF8AFC6183CF0A06702BBD47CB8051F983C90F5E0C51869D5984B`.
It differs from the accepted artifact and is not published. The release DLL
keeps the accepted hash below. Native files (36) and test files (16) match the
development inputs byte-for-byte. Runtime package checksums/defaults/metadata
and independent DLL provenance pass. Release identity is also recorded in
[release-artifacts-0.1.0.json](release-artifacts-0.1.0.json).

### Manual acceptance

The retained test31 DLL completed the focused manual round separately on
Minecraft 1.26.45.01, 1.26.51.01 and 1.26.52.03. Installed artifact hashes were
checked, and attributable startup/Ready, snapshot/render and background-session
observations corroborate the reports. Diagnostic counters are not independent
proof of every container, mode or request lifecycle.

| Exact game version | Focused manual result | Installed DLL |
|---|---|---|
| 1.26.45.01 | User accepted | Same retained test31 hash |
| 1.26.51.01 | User accepted | Same retained test31 hash |
| 1.26.52.03 | User accepted | Same retained test31 hash |

This does not certify all servers, custom containers, plugin combinations or
extended sessions. Additional single-player adapters do not all have individual
manual coverage. Experimental Hive owner-UID correlation limits remain; see
[compatibility.md](compatibility.md).

### Accepted release binary

- Package version: **0.1.0**.
- Binary startup identifier: **0.1.0-test31**, intentionally unchanged.
- DLL size: **544256 bytes**.
- DLL SHA-256:
  `92F2C95A28BB317FA6B2F6E2B02119CF300760E20769C1CCBCF4934F250324A8`.
- AMD64; exactly `ContainerPreviewInit` / `ContainerPreviewStatus` exports.
- Import libraries: bcrypt.dll, kernel32.dll and user32.dll.

Native source and the accepted DLL were not edited to rename the release.
Public source files and tests are copied byte-for-byte from the validated
development version. Clean rebuilds have separate provenance and are offline
verification outputs, not replacement gameplay-tested release artifacts.

### Artifact policy

The plugin ZIP contains exactly eight flat files: `ContainerPreview.dll`,
`config.json`, `config.schema.json`, `mod.json`, `manifest.json`, `README.md`,
`THIRD_PARTY_NOTICES.txt` and `SHA256SUMS.txt`. The internal checksum file covers
the seven payloads. External release assets add an archive `.sha256` and a
`SHA256SUMS.txt` covering the ZIP plus those seven payloads.

Both folder and ZIP verification check the exact metadata/game-version list,
config defaults/schema, PE architecture/exports/imports, complete checksums and
an independent accepted-DLL hash pin. Replacing the DLL and recalculating the
package checksums cannot satisfy that separate pin.

The repository is a clean allowlisted copy: source, tests, portable build and
verification tools, JSON dependency header and notices, settings and curated
documentation. No game binaries/assets, private analysis/IDA/decompilation,
runtime logs, credentials or machine-specific paths are distributed. The only
Windows user-path literal in the tests is a deliberately fictional negative
privacy fixture, not a real machine path. No installation or game automation
is part of publication.

## 繁體中文

### 公開副本離線驗證

公開副本完整流程已退出 0：Debug／Release 共 48 次 C++ 執行、8742 項檢查
零失敗，另有 18 項 Python 測試通過，未觀察到編譯警告／錯誤。六個精確
遊戲映像及執行後雜湊皆通過。ZIP、README、已驗收 DLL 及未發佈重編 DLL
的獨立雜湊見上方與[資產紀錄](release-artifacts-0.1.0.json)。36 個原生檔案
與 16 個測試檔逐檔一致；新 README 不改變插件行為。

### 人工驗收與 DLL 來源

保留的 test31 DLL 已在 1.26.45.01、1.26.51.01、1.26.52.03 分別完成本輪
人工驗收；已核對安裝雜湊及可歸屬的 Ready、快照／繪製、背景流程紀錄。
診斷計數不代表每個容器、模式或請求流程皆已獨立證實，也不保證所有
伺服器、插件組合或長時間情境。額外單人容器仍未全部逐項人工測試，Hive
擁有者 UID 限制也仍保留，詳見[相容性說明](compatibility.md)。

正式套件資訊為 0.1.0，原 DLL 啟動識別仍是 0.1.0-test31，大小與 SHA-256
見上方。沒有為改版本字串修改原生程式碼或 DLL。公開原始碼與測試逐檔
對應驗收時開發版本；乾淨重編僅作離線驗證，不取代已驗收的發佈 DLL。

### 封裝原則

插件 ZIP 僅八個平鋪檔案，內附七項內容校驗碼；Release 另提供 ZIP 校驗
檔，以及包含 ZIP 與七項內容的總校驗檔。驗證同時檢查版本、設定、PE、
匯出／依賴及獨立的 DLL 來源雜湊，不能重算套件校驗碼就替換已驗收 DLL。

公開儲存庫只包含白名單原始碼、測試、可攜工具、JSON 標頭與授權、設定
及整理後文件，不含遊戲檔、私人分析／IDA／反編譯資料、日誌、憑證或
實際電腦路徑。測試中的使用者路徑是故意虛構的隱私拒絕案例。發佈不涉及
安裝插件或自動操作遊戲。
